/// @file
/// @brief The live tests' TLS certificates, made for the run by OpenSSL:
/// an authority, the server's of localhost and 127.0.0.1, ann's client
/// certificate, and an authority no one trusts. No secret is committed.

#ifndef TURBOREL_TEST_SUPPORT_CERTS_HPP
#define TURBOREL_TEST_SUPPORT_CERTS_HPP

#include <string>

namespace test::live {

/// Makes the certificates in `dir`: ca.pem, server.pem and .key,
/// client.pem and .key, other-ca.pem.
/// @param dir The directory.
/// @return Whether they were made; false where OpenSSL was not found.
bool make_certificates(const std::string& dir);

} // namespace test::live

#endif // TURBOREL_TEST_SUPPORT_CERTS_HPP
