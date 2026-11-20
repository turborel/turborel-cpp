/// @file
/// @brief Streams the live tests open themselves (POSIX alone): a TCP
/// stream to the server, plain or over TLS by OpenSSL, as a program that
/// opens its own stream does [CLIENT Connecting]; and a server of the
/// test's own that answers with bytes it was given, then says nothing, so
/// a wait runs out [CLIENT Timeouts].

#ifndef TURBOREL_TEST_SUPPORT_SOCKETS_HPP
#define TURBOREL_TEST_SUPPORT_SOCKETS_HPP

#include "frames.hpp"

#include <turborel/connection.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>

namespace test::sockets {

/// Whether the platform has the sockets these streams need.
/// @return Whether.
bool available();

/// A TCP stream to 127.0.0.1:`port`, changing the first frame it writes by
/// `rewrite`, if given, as a test does to send what no client would.
/// @param port The port.
/// @param rewrite Changes the first write.
/// @return The stream; none if it cannot connect.
std::unique_ptr<turborel::byte_stream> tcp(std::uint16_t port, std::function<void(test::bytes&)> rewrite = nullptr);

/// A TLS stream to 127.0.0.1:`port`, the server's certificate checked
/// against the authority `ca`, as the program's own; it gives what binds
/// SCRAM to it.
struct tls_opened {
  /// The stream.
  std::unique_ptr<turborel::byte_stream> stream;
  /// The server's certificate, DER.
  test::bytes certificate;
  /// The 32 bytes exported under "EXPORTER-Channel-Binding".
  test::bytes exporter;
};

/// Opens a TLS stream.
/// @param port The port.
/// @param ca The PEM file of the authority trusted.
/// @param at_most_tls12 Whether to take TLS 1.2 at most, where no keys are
///   exported, so none is given.
/// @return The stream; none if it cannot connect, or where OpenSSL is not
///   had.
std::optional<tls_opened> tls(std::uint16_t port, const std::string& ca, bool at_most_tls12 = false);

/// A server of the test's own on a free port of 127.0.0.1 that takes one
/// client's TLS of 1.2 at most, by the live server's own certificate and
/// key, and passes what it reads over TLS to the live server and back, so
/// a client sees TLS 1.2 and the server's certificate [CLIENT Connecting].
class tls12_proxy {
public:
  /// Listens, and serves in a thread of its own.
  /// @param upstream The live server's port.
  /// @param chain The PEM file of the server's certificate chain.
  /// @param key The PEM file of its key.
  tls12_proxy(std::uint16_t upstream, const std::string& chain, const std::string& key);
  tls12_proxy(const tls12_proxy&) = delete;
  tls12_proxy(tls12_proxy&&) = delete;
  tls12_proxy& operator=(const tls12_proxy&) = delete;
  tls12_proxy& operator=(tls12_proxy&&) = delete;
  /// Stops listening and waits for the thread.
  ~tls12_proxy();
  /// The port it listens on.
  /// @return The port.
  [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

private:
  std::uint16_t port_{};
  int listener_;
  std::thread serving_;
};

/// A server of the test's own on a free port of 127.0.0.1: it takes one
/// connection, sends it `reply`, and then says nothing until it is
/// destroyed.
class silent_server {
public:
  /// Listens, and answers in a thread of its own.
  /// @param reply What it sends once a client connects.
  explicit silent_server(test::bytes reply);
  silent_server(const silent_server&) = delete;
  silent_server(silent_server&&) = delete;
  silent_server& operator=(const silent_server&) = delete;
  silent_server& operator=(silent_server&&) = delete;
  /// Closes the connection and stops the thread.
  ~silent_server();

  /// The port it listens on.
  /// @return The port.
  [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

private:
  std::uint16_t port_{};
  int listener_;
  int accepted_{-1};
  std::thread answering_;
};

/// A server of the test's own on a free port of 127.0.0.1 between the
/// client and the live server: the first connection closed as it comes,
/// so a TLS handshake over it fails; the second passed to `upstream` and
/// back until either side ends [CLIENT TLS by mode].
class refusing_proxy {
public:
  /// Listens, and serves in a thread of its own.
  /// @param upstream The port of the server it passes to.
  explicit refusing_proxy(std::uint16_t upstream);
  refusing_proxy(const refusing_proxy&) = delete;
  refusing_proxy(refusing_proxy&&) = delete;
  refusing_proxy& operator=(const refusing_proxy&) = delete;
  refusing_proxy& operator=(refusing_proxy&&) = delete;
  /// Stops listening and waits for the thread.
  ~refusing_proxy();
  /// The port it listens on.
  /// @return The port.
  [[nodiscard]] std::uint16_t port() const noexcept { return port_; }
  /// The connections it took, once it is done with both.
  /// @return The count.
  [[nodiscard]] int accepted() noexcept;

private:
  std::uint16_t port_{};
  int listener_;
  int accepted_{};
  std::thread serving_;
};

} // namespace test::sockets

#endif // TURBOREL_TEST_SUPPORT_SOCKETS_HPP
