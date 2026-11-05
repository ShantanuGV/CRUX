#include "crux/identity/identity.hpp"

#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/bio.h>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <ctime>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace crux::identity {

// ─── KeyPair lifecycle ───────────────────────────────────────

KeyPair::~KeyPair() {
    if (pkey) { EVP_PKEY_free(pkey); pkey = nullptr; }
    if (cert) { X509_free(cert);     cert = nullptr; }
}

KeyPair::KeyPair(KeyPair&& other) noexcept
    : pkey(other.pkey), cert(other.cert), fingerprint(std::move(other.fingerprint)) {
    other.pkey = nullptr;
    other.cert = nullptr;
}

KeyPair& KeyPair::operator=(KeyPair&& other) noexcept {
    if (this != &other) {
        if (pkey) EVP_PKEY_free(pkey);
        if (cert) X509_free(cert);
        pkey = other.pkey;
        cert = other.cert;
        fingerprint = std::move(other.fingerprint);
        other.pkey = nullptr;
        other.cert = nullptr;
    }
    return *this;
}

// ─── Helpers ─────────────────────────────────────────────────

static std::string bytes_to_hex(const std::uint8_t* data, std::size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < len; ++i)
        oss << std::setw(2) << static_cast<unsigned>(data[i]);
    return oss.str();
}

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    return f.good();
}

// ─── Fingerprint ─────────────────────────────────────────────

std::string compute_fingerprint(EVP_PKEY* pkey) {
    if (!pkey) return {};

    // Get DER-encoded SubjectPublicKeyInfo
    int len = i2d_PUBKEY(pkey, nullptr);
    if (len <= 0) return {};

    std::vector<std::uint8_t> der(static_cast<std::size_t>(len));
    std::uint8_t* ptr = der.data();
    if (i2d_PUBKEY(pkey, &ptr) <= 0) return {};

    // SHA-256 of the DER encoding
    std::uint8_t hash[SHA256_DIGEST_LENGTH];
    SHA256(der.data(), der.size(), hash);

    return bytes_to_hex(hash, SHA256_DIGEST_LENGTH);
}

std::string fingerprint_from_cert_pem(const std::string& cert_pem) {
    if (cert_pem.empty()) return {};

    BIO* bio = BIO_new_mem_buf(cert_pem.data(), static_cast<int>(cert_pem.size()));
    if (!bio) return {};

    X509* x = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!x) return {};

    EVP_PKEY* pub = X509_get_pubkey(x);
    X509_free(x);
    if (!pub) return {};

    std::string fp = compute_fingerprint(pub);
    EVP_PKEY_free(pub);
    return fp;
}

// ─── Generation ──────────────────────────────────────────────

KeyPair generate(const std::string& common_name) {
    KeyPair kp;

    // Generate Ed25519 key pair
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (!ctx) return kp;

    if (EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_keygen(ctx, &kp.pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return kp;
    }
    EVP_PKEY_CTX_free(ctx);

    // Create self-signed X.509 certificate
    kp.cert = X509_new();
    if (!kp.cert) {
        EVP_PKEY_free(kp.pkey); kp.pkey = nullptr;
        return kp;
    }

    // Serial number
    ASN1_INTEGER_set(X509_get_serialNumber(kp.cert), 1);

    // Validity: 20 years (fits in 32-bit signed long seconds)
    X509_gmtime_adj(X509_get_notBefore(kp.cert), 0);
    X509_gmtime_adj(X509_get_notAfter(kp.cert),
                    static_cast<long>(60L * 60 * 24 * 365 * 20));

    // Public key
    X509_set_pubkey(kp.cert, kp.pkey);

    // Subject / Issuer (self-signed: same)
    X509_NAME* name = X509_get_subject_name(kp.cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>(common_name.c_str()),
        -1, -1, 0);
    X509_set_issuer_name(kp.cert, name);

    // Sign the certificate with our own private key
    if (X509_sign(kp.cert, kp.pkey, nullptr) == 0) {
        // Ed25519 uses nullptr for digest (it's a pure EdDSA signature)
        // Some versions need EVP_DigestSign flow. Try again with EVP_md_null:
        // Actually for Ed25519, X509_sign with EVP_DigestSign should work.
        // If sign fails, cert is invalid.
        X509_free(kp.cert);  kp.cert = nullptr;
        EVP_PKEY_free(kp.pkey); kp.pkey = nullptr;
        return kp;
    }

    kp.fingerprint = compute_fingerprint(kp.pkey);
    return kp;
}

// ─── Persistence ─────────────────────────────────────────────

bool save(const KeyPair& kp, const std::string& key_path,
          const std::string& cert_path) {
    if (!kp.valid()) return false;

    // Write private key (PEM)
    {
        BIO* bio = BIO_new(BIO_s_mem());
        if (!bio) return false;
        bool ok = PEM_write_bio_PrivateKey(bio, kp.pkey,
                                            nullptr, nullptr, 0, nullptr, nullptr) == 1;
        if (ok) {
            char* data = nullptr;
            long len = BIO_get_mem_data(bio, &data);
            ok = write_file(key_path, std::string(data, static_cast<std::size_t>(len)));
        }
        BIO_free(bio);
        if (!ok) return false;

#ifndef _WIN32
        // Restrict private key file permissions
        chmod(key_path.c_str(), 0600);
#endif
    }

    // Write certificate (PEM)
    {
        BIO* bio = BIO_new(BIO_s_mem());
        if (!bio) return false;
        bool ok = PEM_write_bio_X509(bio, kp.cert) == 1;
        if (ok) {
            char* data = nullptr;
            long len = BIO_get_mem_data(bio, &data);
            ok = write_file(cert_path, std::string(data, static_cast<std::size_t>(len)));
        }
        BIO_free(bio);
        if (!ok) return false;
    }

    return true;
}

KeyPair load_or_generate(const std::string& key_path, const std::string& cert_path,
                          const std::string& common_name) {
    // Try to load existing files
    std::string key_pem  = read_file(key_path);
    std::string cert_pem = read_file(cert_path);

    if (!key_pem.empty() && !cert_pem.empty()) {
        KeyPair kp;

        // Load private key
        BIO* bio = BIO_new_mem_buf(key_pem.data(), static_cast<int>(key_pem.size()));
        if (bio) {
            kp.pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
            BIO_free(bio);
        }

        // Load certificate
        bio = BIO_new_mem_buf(cert_pem.data(), static_cast<int>(cert_pem.size()));
        if (bio) {
            kp.cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
            BIO_free(bio);
        }

        if (kp.pkey && kp.cert) {
            kp.fingerprint = compute_fingerprint(kp.pkey);
            if (!kp.fingerprint.empty()) {
                return kp;
            }
        }
        // Fall through to regeneration if loading failed
    }

    // Generate new identity
    KeyPair kp = generate(common_name);
    if (!kp.valid()) return kp;

    save(kp, key_path, cert_path);
    return kp;
}

// ─── Signing ─────────────────────────────────────────────────

std::vector<std::uint8_t> sign(const KeyPair& kp,
                                const std::uint8_t* data, std::size_t len) {
    if (!kp.valid()) return {};

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return {};

    std::vector<std::uint8_t> sig;
    std::size_t sig_len = 0;

    if (EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, kp.pkey) == 1) {
        // Get required signature length
        if (EVP_DigestSign(ctx, nullptr, &sig_len, data, len) == 1) {
            sig.resize(sig_len);
            if (EVP_DigestSign(ctx, sig.data(), &sig_len, data, len) != 1) {
                sig.clear();
            } else {
                sig.resize(sig_len);
            }
        }
    }

    EVP_MD_CTX_free(ctx);
    return sig;
}

// ─── Verification ────────────────────────────────────────────

bool verify(const std::string& cert_pem,
             const std::uint8_t* data, std::size_t data_len,
             const std::vector<std::uint8_t>& signature) {
    if (cert_pem.empty() || signature.empty()) return false;

    // Load certificate and extract public key
    BIO* bio = BIO_new_mem_buf(cert_pem.data(), static_cast<int>(cert_pem.size()));
    if (!bio) return false;

    X509* x = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!x) return false;

    EVP_PKEY* pub = X509_get_pubkey(x);
    X509_free(x);
    if (!pub) return false;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    bool ok = false;

    if (ctx && EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pub) == 1) {
        ok = (EVP_DigestVerify(ctx, signature.data(), signature.size(),
                               data, data_len) == 1);
    }

    if (ctx) EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pub);
    return ok;
}

// ─── PEM export ──────────────────────────────────────────────

std::string cert_to_pem(const KeyPair& kp) {
    if (!kp.cert) return {};

    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) return {};

    std::string result;
    if (PEM_write_bio_X509(bio, kp.cert) == 1) {
        char* data = nullptr;
        long len = BIO_get_mem_data(bio, &data);
        result.assign(data, static_cast<std::size_t>(len));
    }

    BIO_free(bio);
    return result;
}

} // namespace crux::identity
