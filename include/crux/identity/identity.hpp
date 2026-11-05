#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Forward-declare OpenSSL types so callers don't need to include OpenSSL headers
struct evp_pkey_st;
using EVP_PKEY = evp_pkey_st;

struct x509_st;
using X509 = x509_st;

namespace crux::identity {

// A complete local identity: Ed25519 key pair + self-signed X.509 certificate.
// The certificate is used for TLS mutual authentication.
// The fingerprint is the stable peer identifier pinned by friends.
struct KeyPair {
    // Raw key material (held in OpenSSL structures)
    EVP_PKEY* pkey = nullptr;  // Ed25519 key pair (owned)
    X509*     cert = nullptr;  // Self-signed certificate (owned)

    // Human-readable fingerprint: SHA-256 of DER-encoded public key, lowercase hex
    // Example: "a3f1b2c4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1a2"
    std::string fingerprint;

    KeyPair() = default;
    ~KeyPair();

    // Non-copyable (owns OpenSSL heap objects)
    KeyPair(const KeyPair&) = delete;
    KeyPair& operator=(const KeyPair&) = delete;

    // Movable
    KeyPair(KeyPair&& other) noexcept;
    KeyPair& operator=(KeyPair&& other) noexcept;

    bool valid() const { return pkey != nullptr && cert != nullptr && !fingerprint.empty(); }
};

// Generate a new Ed25519 key pair and a self-signed X.509 certificate.
// Returns an empty KeyPair (valid() == false) on failure.
KeyPair generate(const std::string& common_name = "CRUX");

// Load identity from PEM files. If files don't exist, generate and save.
// key_path: path to the private key PEM file
// cert_path: path to the certificate PEM file
// Returns an empty KeyPair on unrecoverable failure.
KeyPair load_or_generate(const std::string& key_path, const std::string& cert_path,
                          const std::string& common_name = "CRUX");

// Save key pair to PEM files.
// Private key file is created with restrictive permissions on Linux.
bool save(const KeyPair& kp, const std::string& key_path, const std::string& cert_path);

// Compute fingerprint string from an EVP_PKEY's public component.
// Returns empty string on failure.
std::string compute_fingerprint(EVP_PKEY* pkey);

// Sign arbitrary data with the Ed25519 private key.
// Returns the 64-byte signature, or empty vector on failure.
std::vector<std::uint8_t> sign(const KeyPair& kp,
                                const std::uint8_t* data, std::size_t len);

// Verify an Ed25519 signature against a public key extracted from a certificate PEM.
// cert_pem: PEM-encoded X.509 certificate of the signer
// data / data_len: the signed data
// signature: the 64-byte signature
bool verify(const std::string& cert_pem,
             const std::uint8_t* data, std::size_t data_len,
             const std::vector<std::uint8_t>& signature);

// Extract the fingerprint from a PEM-encoded X.509 certificate.
// Returns empty string on failure.
std::string fingerprint_from_cert_pem(const std::string& cert_pem);

// Get the PEM encoding of the certificate (for pinning comparison).
std::string cert_to_pem(const KeyPair& kp);

} // namespace crux::identity
