#include "test_framework.hpp"
#include "crux/identity/identity.hpp"

namespace {

TEST(identity_generate_valid_keypair) {
    auto kp = crux::identity::generate("CRUX-Test");
    CHECK(kp.valid());
    CHECK(!kp.fingerprint.empty());
    CHECK_EQ(kp.fingerprint.size(), 64u); // SHA-256 hex is 64 characters
}

TEST(identity_fingerprint_deterministic) {
    auto kp = crux::identity::generate("CRUX-Test-2");
    std::string fp1 = kp.fingerprint;
    std::string fp2 = crux::identity::compute_fingerprint(kp.pkey);
    CHECK_EQ(fp1, fp2);
}

TEST(identity_sign_and_verify) {
    auto kp = crux::identity::generate("CRUX-SignTest");
    CHECK(kp.valid());

    std::string cert_pem = crux::identity::cert_to_pem(kp);
    CHECK(!cert_pem.empty());

    std::string msg = "Hello CRUX TLS 1.3";
    auto sig = crux::identity::sign(kp, reinterpret_cast<const std::uint8_t*>(msg.data()), msg.size());
    CHECK_EQ(sig.size(), 64u); // Ed25519 signature is 64 bytes

    bool ok = crux::identity::verify(cert_pem,
                                     reinterpret_cast<const std::uint8_t*>(msg.data()),
                                     msg.size(),
                                     sig);
    CHECK(ok);

    // Tampered data should fail verification
    std::string tampered = "Hello CRUX TLS 1.4";
    bool bad = crux::identity::verify(cert_pem,
                                      reinterpret_cast<const std::uint8_t*>(tampered.data()),
                                      tampered.size(),
                                      sig);
    CHECK(!bad);
}

TEST(identity_cert_fingerprint_match) {
    auto kp = crux::identity::generate("CRUX-FPTest");
    std::string cert_pem = crux::identity::cert_to_pem(kp);
    std::string fp_from_cert = crux::identity::fingerprint_from_cert_pem(cert_pem);
    CHECK_EQ(kp.fingerprint, fp_from_cert);
}

} // namespace
