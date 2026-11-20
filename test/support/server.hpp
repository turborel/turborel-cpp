/// @file
/// @brief The live tests' server (R-TEST-8): turboreld, as the environment
/// variable TURBOREL_CPP_TEST_SERVER names it, a path of it or a command
/// that runs it, started once for the run on port 17439, this driver's
/// own, with a database kept nowhere, its owner ann, TLS of certificates
/// made for the run, a Unix socket, and trusted local clients; stopped by
/// its own process ID as the tests end. Unset, the live tests are skipped,
/// and the unit tests stay deterministic (R-TEST-5).

#ifndef TURBOREL_TEST_SUPPORT_SERVER_HPP
#define TURBOREL_TEST_SUPPORT_SERVER_HPP

#include "frames.hpp"

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>

#include <cstdint>
#include <string>

namespace test::live {

/// The port the live tests' server listens on, this driver's own.
inline constexpr std::uint16_t port = 17439;
/// The owner's name.
inline constexpr const char* owner = "ann";
/// The owner's password.
inline constexpr const char* password = "ann's secret";

/// Whether the server runs; false if TURBOREL_CPP_TEST_SERVER names none,
/// or it could not start. Started on the first call.
/// @return Whether it runs.
bool running();

/// Why the server does not run, for the skip's message.
/// @return The reason.
std::string why_not();

/// Whether the run made its TLS certificates, which need OpenSSL.
/// @return Whether it did.
bool has_certificates();

/// A path of the server's directory.
/// @param name The file's name.
/// @return Its path.
std::string path(const std::string& name);

/// TLS settings trusting the run's authority, presenting ann's
/// certificate if `present`.
/// @param present Whether to present it.
/// @return The settings.
turborel::tls tls(bool present = false);

/// A connection by trust over TCP, as ann.
/// @param options What to ask for.
/// @return The connection.
turborel::connection trusted(const turborel::connect_options& options = turborel::connect_options{});

/// The server's TCP target.
/// @return The target.
turborel::target tcp();

/// A user made for a test, with a password, by the owner, if not made yet.
/// @param name The user, as an identifier.
/// @param secret Its password.
void user(const std::string& name, const std::string& secret);

} // namespace test::live

/// Skips a live test when the server does not run.
#define LIVE_OR_SKIP()                                                                                                 \
  do {                                                                                                                 \
    if (!test::live::running()) {                                                                                      \
      SKIP(test::live::why_not());                                                                                     \
    }                                                                                                                  \
  } while (false)

/// Skips a test that needs the run's TLS certificates, where OpenSSL was
/// not found to make them.
#define CERTIFIED_OR_SKIP()                                                                                            \
  do {                                                                                                                 \
    LIVE_OR_SKIP();                                                                                                    \
    TLS_OR_SKIP();                                                                                                     \
    if (!test::live::has_certificates()) {                                                                             \
      SKIP("no OpenSSL made the run's certificates, so the TLS tests are skipped");                                    \
    }                                                                                                                  \
  } while (false)

#endif // TURBOREL_TEST_SUPPORT_SERVER_HPP
