/// @file
/// @brief Test certificates of ECDSA P-256 with SHA-256, each signed by the
/// run's own authority, made by OpenSSL where the build found it.

#include "certs.hpp"

#include <string>

#ifdef TURBOREL_CPP_TEST_CERTS

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <filesystem>
#include <iomanip>
#include <ios>
#include <memory>
#include <sstream>
#include <system_error>
#include <utility>

namespace test::live {

namespace {

/// An hour, in seconds: how long before now a certificate is valid from.
constexpr long hour = 3600;
/// A day, in seconds: how long after now it is valid to.
constexpr long day = 86400;

/// Owners of OpenSSL's objects.
struct key_free {
  void operator()(EVP_PKEY* key) const noexcept { EVP_PKEY_free(key); }
};
struct cert_free {
  void operator()(X509* cert) const noexcept { X509_free(cert); }
};
using key_owner = std::unique_ptr<EVP_PKEY, key_free>;
using cert_owner = std::unique_ptr<X509, cert_free>;

/// A key pair of P-256.
key_owner new_key() {
  EVP_PKEY_CTX* context = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
  EVP_PKEY* key = nullptr;
  if (context == nullptr || EVP_PKEY_keygen_init(context) != 1 ||
      EVP_PKEY_CTX_set_ec_paramgen_curve_nid(context, NID_X9_62_prime256v1) != 1 ||
      EVP_PKEY_keygen(context, &key) != 1) {
    key = nullptr;
  }
  EVP_PKEY_CTX_free(context);
  return key_owner{key};
}

/// Adds the extension `nid` of `value` to `cert`, issued by `issuer`.
bool extend(X509* cert, X509* issuer, int nid, const std::string& value) {
  X509V3_CTX context;
  X509V3_set_ctx(&context, issuer, cert, nullptr, nullptr, 0);
  X509_EXTENSION* extension = X509V3_EXT_conf_nid(nullptr, &context, nid, value.c_str());
  const bool added = extension != nullptr && X509_add_ext(cert, extension, -1) == 1;
  X509_EXTENSION_free(extension);
  return added;
}

/// A certificate of `name` and `key`, not yet signed, issued by `issuer`,
/// or by itself.
cert_owner new_cert(const std::string& name, long serial, EVP_PKEY* key, X509 const* issuer) {
  cert_owner cert{X509_new()};
  X509_NAME* subject = X509_NAME_new();
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): OpenSSL takes a name as unsigned octets.
  const auto* text = reinterpret_cast<const unsigned char*>(name.c_str());
  const bool made =
      cert && subject != nullptr && X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, text, -1, -1, 0) == 1 &&
      X509_set_version(cert.get(), 2) == 1 && ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), serial) == 1 &&
      X509_gmtime_adj(X509_getm_notBefore(cert.get()), -hour) != nullptr &&
      X509_gmtime_adj(X509_getm_notAfter(cert.get()), day) != nullptr && X509_set_pubkey(cert.get(), key) == 1 &&
      X509_set_subject_name(cert.get(), subject) == 1 &&
      X509_set_issuer_name(cert.get(), issuer == nullptr ? subject : X509_get_subject_name(issuer)) == 1;
  X509_NAME_free(subject);
  return made ? std::move(cert) : cert_owner{};
}

/// Writes `cert`, and `key` if not null, to `dir`/`name`.pem and .key.
bool write_out(const std::string& dir, const std::string& name, X509 const* cert, EVP_PKEY const* key) {
  BIO* file = BIO_new_file((dir + "/" + name + ".pem").c_str(), "w");
  bool written = file != nullptr && PEM_write_bio_X509(file, cert) == 1;
  BIO_free(file);
  if (written && key != nullptr) {
    file = BIO_new_file((dir + "/" + name + ".key").c_str(), "w");
    written = file != nullptr && PEM_write_bio_PrivateKey(file, key, nullptr, nullptr, 0, nullptr, nullptr) == 1;
    BIO_free(file);
  }
  return written;
}

/// An authority: its key and certificate.
struct authority {
  key_owner key;
  cert_owner cert;
};

/// An authority of `name`.
authority make_authority(const std::string& name, long serial) {
  authority made{.key = new_key(), .cert = nullptr};
  made.cert = made.key ? new_cert(name, serial, made.key.get(), nullptr) : cert_owner{};
  X509* cert = made.cert.get();
  const bool good = cert != nullptr && extend(cert, cert, NID_basic_constraints, "critical,CA:TRUE") &&
                    extend(cert, cert, NID_key_usage, "critical,keyCertSign,cRLSign") &&
                    extend(cert, cert, NID_subject_key_identifier, "hash") &&
                    X509_sign(cert, made.key.get(), EVP_sha256()) > 0;
  return good ? std::move(made) : authority{};
}

/// What a certificate an authority signs is of.
struct leaf_of {
  /// The files' name, before .pem and .key.
  std::string file;
  /// Its subject's common name.
  std::string name;
  /// Its extended key usage.
  std::string usage;
  /// Its subject's other names, or none.
  std::string names;
  /// Its serial number.
  long serial;
};

/// A certificate `ca` signs, as `of` says, written to `dir`.
bool leaf(const std::string& dir, const leaf_of& of, const authority& ca) {
  const key_owner key = new_key();
  const cert_owner cert = key ? new_cert(of.name, of.serial, key.get(), ca.cert.get()) : cert_owner{};
  X509* made = cert.get();
  return made != nullptr && extend(made, ca.cert.get(), NID_basic_constraints, "CA:FALSE") &&
         extend(made, ca.cert.get(), NID_key_usage, "critical,digitalSignature") &&
         extend(made, ca.cert.get(), NID_ext_key_usage, of.usage) &&
         extend(made, ca.cert.get(), NID_authority_key_identifier, "keyid") &&
         (of.names.empty() || extend(made, ca.cert.get(), NID_subject_alt_name, of.names)) &&
         X509_sign(made, ca.key.get(), EVP_sha256()) > 0 && write_out(dir, of.file, made, key.get());
}

/// Writes `cert` to `dir`/certs/<hash>.0, as a directory of authorities
/// OpenSSL reads, SSL_CERT_DIR's, names each.
bool write_hashed(const std::string& dir, X509 const* cert) {
  std::ostringstream name;
  name << std::hex << std::setw(8) << std::setfill('0')
       << X509_NAME_hash_ex(X509_get_subject_name(cert), nullptr, nullptr, nullptr) << ".0";
  std::error_code made;
  std::filesystem::create_directories(dir + "/certs", made);
  return !made && write_out(dir + "/certs", name.str(), cert, nullptr);
}

} // namespace

bool make_certificates(const std::string& dir) {
  const authority ca = make_authority("turborel-cpp test authority", 1);
  const authority other = make_authority("turborel-cpp untrusted authority", 2);
  return ca.cert && other.cert && write_out(dir, "ca", ca.cert.get(), nullptr) && write_hashed(dir, ca.cert.get()) &&
         leaf(dir,
              leaf_of{
                  .file = "server",
                  .name = "localhost",
                  .usage = "serverAuth",
                  .names = "DNS:localhost,IP:127.0.0.1",
                  .serial = 10,
              },
              ca) &&
         leaf(dir, leaf_of{.file = "client", .name = "ann", .usage = "clientAuth", .names = "", .serial = 11}, ca) &&
         write_out(dir, "other-ca", other.cert.get(), nullptr);
}

} // namespace test::live

#else

namespace test::live {

bool make_certificates(const std::string& dir) {
  static_cast<void>(dir);
  return false;
}

} // namespace test::live

#endif
