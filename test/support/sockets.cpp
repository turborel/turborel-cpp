/// @file
/// @brief The live tests' own streams and silent server, on POSIX sockets,
/// TLS by OpenSSL where the build found it.

#include "sockets.hpp"

#include "frames.hpp"

#include <turborel/connection.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#ifndef _WIN32
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#ifdef TURBOREL_CPP_TEST_CERTS
#include <openssl/crypto.h>
#include <openssl/prov_ssl.h>
#include <openssl/ssl.h>
#include <openssl/tls1.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#endif

namespace test::sockets {

#ifndef _WIN32

namespace {

/// The address 127.0.0.1:`port`.
sockaddr_in loopback(std::uint16_t port) {
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return address;
}

/// A socket connected to 127.0.0.1:`port`, or -1.
int connected(std::uint16_t port) {
  const int socket = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in address = loopback(port);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the sockets API takes its addresses so.
  if (socket < 0 || ::connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) {
    if (socket >= 0) {
      ::close(socket);
    }
    return -1;
  }
  return socket;
}

/// A socket listening on a free port of 127.0.0.1, `backlog` deep, its
/// port into `port`; -1 if none could be had.
int listening(int backlog, std::uint16_t& port) {
  const int socket = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in address = loopback(0);
  socklen_t length = sizeof address;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the sockets API takes its addresses so.
  auto* named = reinterpret_cast<sockaddr*>(&address);
  if (socket < 0 || ::bind(socket, named, sizeof address) != 0 || ::listen(socket, backlog) != 0 ||
      ::getsockname(socket, named, &length) != 0) {
    if (socket >= 0) {
      ::close(socket);
    }
    return -1;
  }
  port = ntohs(address.sin_port);
  return socket;
}

/// A plain TCP stream, its first write changed by a hook.
class tcp_stream : public turborel::byte_stream {
public:
  tcp_stream(int socket, std::function<void(test::bytes&)> rewrite) : socket_{socket}, rewrite_{std::move(rewrite)} {}

  std::optional<std::size_t> read(std::span<std::byte> into) override {
    const ssize_t got = ::recv(socket_, into.data(), into.size(), 0);
    return got < 0 ? std::nullopt : std::optional<std::size_t>{static_cast<std::size_t>(got)};
  }

  std::optional<std::size_t> write(std::span<const std::byte> bytes) override {
    if (rewrite_) {
      test::bytes changed{bytes.begin(), bytes.end()};
      std::exchange(rewrite_, nullptr)(changed);
      const ssize_t sent = ::send(socket_, changed.data(), changed.size(), 0);
      return sent < 0 ? std::nullopt : std::optional<std::size_t>{bytes.size()};
    }
    const ssize_t sent = ::send(socket_, bytes.data(), bytes.size(), 0);
    return sent < 0 ? std::nullopt : std::optional<std::size_t>{static_cast<std::size_t>(sent)};
  }

  void close() noexcept override { ::close(socket_); }

private:
  int socket_;
  std::function<void(test::bytes&)> rewrite_;
};

} // namespace

bool available() { return true; }

std::unique_ptr<turborel::byte_stream> tcp(std::uint16_t port, std::function<void(test::bytes&)> rewrite) {
  const int socket = connected(port);
  if (socket < 0) {
    return nullptr;
  }
  return std::make_unique<tcp_stream>(socket, std::move(rewrite));
}

silent_server::silent_server(test::bytes reply) : listener_{listening(1, port_)} {
  answering_ = std::thread{[this, reply = std::move(reply)] {
    accepted_ = ::accept(listener_, nullptr, nullptr);
    if (accepted_ >= 0 && !reply.empty()) {
      static_cast<void>(::send(accepted_, reply.data(), reply.size(), 0));
    }
  }};
}

silent_server::~silent_server() {
  ::shutdown(listener_, SHUT_RDWR);
  ::close(listener_);
  answering_.join();
  if (accepted_ >= 0) {
    ::close(accepted_);
  }
}

namespace {

/// Passes bytes each way between `a` and `b` until either ends, or ten
/// seconds pass with none.
void pump(int a, int b) {
  constexpr int wait_ms = 10000;
  std::array<pollfd, 2> ends{
      pollfd{.fd = a, .events = POLLIN, .revents = 0},
      pollfd{.fd = b, .events = POLLIN, .revents = 0},
  };
  std::array<std::byte, 4096> buffer{};
  for (;;) {
    if (::poll(ends.data(), ends.size(), wait_ms) <= 0) {
      return;
    }
    for (std::size_t i = 0; i < ends.size(); ++i) {
      if ((static_cast<unsigned>(ends.at(i).revents) & (static_cast<unsigned>(POLLIN) | POLLHUP)) != 0U) {
        const ssize_t got = ::read(ends.at(i).fd, buffer.data(), buffer.size());
        if (got <= 0 || ::write(ends.at(1 - i).fd, buffer.data(), static_cast<std::size_t>(got)) != got) {
          return;
        }
      }
    }
  }
}

} // namespace

refusing_proxy::refusing_proxy(std::uint16_t upstream) : listener_{listening(4, port_)} {
  serving_ = std::thread{[this, upstream] {
    for (int n = 0; n < 2; ++n) {
      const int client = ::accept(listener_, nullptr, nullptr);
      if (client < 0) {
        return;
      }
      ++accepted_;
      if (n == 1) {
        const int server = connected(upstream);
        if (server >= 0) {
          pump(client, server);
          ::close(server);
        }
      }
      ::close(client);
    }
  }};
}

refusing_proxy::~refusing_proxy() {
  ::shutdown(listener_, SHUT_RDWR);
  ::close(listener_);
  if (serving_.joinable()) {
    serving_.join();
  }
}

int refusing_proxy::accepted() noexcept {
  if (serving_.joinable()) {
    serving_.join();
  }
  return accepted_;
}

#ifdef TURBOREL_CPP_TEST_CERTS

namespace {

/// The label of tls-exporter's keys [RFC 9266].
constexpr std::string_view exporter_label = "EXPORTER-Channel-Binding";
/// The bytes exported.
constexpr std::size_t exporter_bytes = 32;

/// A TLS stream over a socket, by OpenSSL.
class tls_stream : public turborel::byte_stream {
public:
  tls_stream(int socket, SSL_CTX* context, SSL* ssl) : socket_{socket}, context_{context}, ssl_{ssl} {}

  std::optional<std::size_t> read(std::span<std::byte> into) override {
    std::size_t got = 0;
    const int read = SSL_read_ex(ssl_, into.data(), into.size(), &got);
    if (read != 1) {
      const int why = SSL_get_error(ssl_, read);
      return why == SSL_ERROR_ZERO_RETURN ? std::optional<std::size_t>{0} : std::nullopt;
    }
    return got;
  }

  std::optional<std::size_t> write(std::span<const std::byte> bytes) override {
    std::size_t sent = 0;
    return SSL_write_ex(ssl_, bytes.data(), bytes.size(), &sent) == 1 ? std::optional<std::size_t>{sent} : std::nullopt;
  }

  void close() noexcept override {
    SSL_shutdown(ssl_);
    SSL_free(ssl_);
    SSL_CTX_free(context_);
    ::close(socket_);
  }

private:
  int socket_;
  SSL_CTX* context_;
  SSL* ssl_;
};

} // namespace

std::optional<tls_opened> tls(std::uint16_t port, const std::string& ca, bool at_most_tls12) {
  const int socket = connected(port);
  SSL_CTX* context = SSL_CTX_new(TLS_client_method());
  if (socket < 0 || context == nullptr || SSL_CTX_load_verify_file(context, ca.c_str()) != 1 ||
      (at_most_tls12 && SSL_CTX_set_max_proto_version(context, TLS1_2_VERSION) != 1)) {
    return std::nullopt;
  }
  SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
  SSL* ssl = SSL_new(context);
  // The server is told apart from plain TCP by the handshake's first byte.
  SSL_set_fd(ssl, socket);
  X509_VERIFY_PARAM_set1_host(SSL_get0_param(ssl), "localhost", 0);
  if (SSL_connect(ssl) != 1) {
    SSL_free(ssl);
    SSL_CTX_free(context);
    ::close(socket);
    return std::nullopt;
  }
  tls_opened opened{
      .stream = std::make_unique<tls_stream>(socket, context, ssl),
      .certificate = {},
      .exporter = test::bytes(at_most_tls12 ? 0 : exporter_bytes),
  };
  X509* peer = SSL_get1_peer_certificate(ssl);
  unsigned char* der = nullptr;
  const int length = i2d_X509(peer, &der);
  if (length > 0) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): DER's octets as bytes.
    const auto* first = reinterpret_cast<const std::byte*>(der);
    opened.certificate.assign(first, std::next(first, length));
  }
  OPENSSL_free(der);
  X509_free(peer);
  if (!at_most_tls12) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the exported keys' octets.
    SSL_export_keying_material(ssl, reinterpret_cast<unsigned char*>(opened.exporter.data()), exporter_bytes,
                               exporter_label.data(), exporter_label.size(), nullptr, 0, 0);
  }
  return opened;
}

namespace {

/// Passes what each TLS end reads to the other until either ends, or ten
/// seconds pass with nothing.
void pump(SSL* a, SSL* b) noexcept {
  constexpr int wait_ms = 10000;
  std::array<pollfd, 2> fds{
      pollfd{.fd = SSL_get_fd(a), .events = POLLIN, .revents = 0},
      pollfd{.fd = SSL_get_fd(b), .events = POLLIN, .revents = 0},
  };
  /// One way of the two: what reads, what is written, and its poll.
  struct way {
    SSL* from;
    SSL* to;
    pollfd* polled;
  };
  const std::array<way, 2> ways{
      way{.from = a, .to = b, .polled = &fds.front()},
      way{.from = b, .to = a, .polled = &fds.back()},
  };
  std::array<std::byte, 16384> buffer{};
  for (;;) {
    const bool pending = SSL_pending(a) > 0 || SSL_pending(b) > 0;
    if (!pending && ::poll(fds.data(), fds.size(), wait_ms) <= 0) {
      return;
    }
    for (const way& one : ways) {
      const bool ready = (static_cast<unsigned>(one.polled->revents) & (static_cast<unsigned>(POLLIN) | POLLHUP)) != 0U;
      one.polled->revents = 0;
      if (ready || SSL_pending(one.from) > 0) {
        std::size_t got = 0;
        std::size_t sent = 0;
        if (SSL_read_ex(one.from, buffer.data(), buffer.size(), &got) != 1 ||
            SSL_write_ex(one.to, buffer.data(), got, &sent) != 1) {
          return;
        }
      }
    }
  }
}

/// Serves one client of `listener` as tls12_proxy says.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): a certificate chain then its key, as OpenSSL takes them.
void serve_tls12(int listener, std::uint16_t upstream, const std::string& chain, const std::string& key) noexcept {
  const int client = ::accept(listener, nullptr, nullptr);
  const int server = client < 0 ? -1 : connected(upstream);
  SSL_CTX* outer = SSL_CTX_new(TLS_server_method());
  SSL_CTX* inner = SSL_CTX_new(TLS_client_method());
  SSL* from = nullptr;
  SSL* to = nullptr;
  if (server >= 0 && outer != nullptr && inner != nullptr &&
      SSL_CTX_set_max_proto_version(outer, TLS1_2_VERSION) == 1 &&
      SSL_CTX_use_certificate_chain_file(outer, chain.c_str()) == 1 &&
      SSL_CTX_use_PrivateKey_file(outer, key.c_str(), SSL_FILETYPE_PEM) == 1) {
    from = SSL_new(outer);
    to = SSL_new(inner);
    SSL_set_fd(from, client);
    SSL_set_fd(to, server);
    if (SSL_accept(from) == 1 && SSL_connect(to) == 1) {
      pump(from, to);
    }
  }
  SSL_free(from);
  SSL_free(to);
  SSL_CTX_free(outer);
  SSL_CTX_free(inner);
  for (const int socket : {client, server}) {
    if (socket >= 0) {
      ::close(socket);
    }
  }
}

} // namespace

tls12_proxy::tls12_proxy(std::uint16_t upstream, const std::string& chain, const std::string& key)
    : listener_{listening(1, port_)} {
  serving_ = std::thread{serve_tls12, listener_, upstream, chain, key};
}

tls12_proxy::~tls12_proxy() {
  ::shutdown(listener_, SHUT_RDWR);
  ::close(listener_);
  serving_.join();
}

#else

std::optional<tls_opened> tls(std::uint16_t port, const std::string& ca, bool at_most_tls12) {
  static_cast<void>(port);
  static_cast<void>(ca);
  static_cast<void>(at_most_tls12);
  return std::nullopt;
}

tls12_proxy::tls12_proxy(std::uint16_t upstream, const std::string& chain, const std::string& key) : listener_{-1} {
  static_cast<void>(upstream);
  static_cast<void>(chain);
  static_cast<void>(key);
}

tls12_proxy::~tls12_proxy() = default;

#endif

#else

bool available() { return false; }

std::unique_ptr<turborel::byte_stream> tcp(std::uint16_t port, std::function<void(test::bytes&)> rewrite) {
  static_cast<void>(port);
  static_cast<void>(rewrite);
  return nullptr;
}

std::optional<tls_opened> tls(std::uint16_t port, const std::string& ca, bool at_most_tls12) {
  static_cast<void>(port);
  static_cast<void>(ca);
  static_cast<void>(at_most_tls12);
  return std::nullopt;
}

tls12_proxy::tls12_proxy(std::uint16_t upstream, const std::string& chain, const std::string& key) : listener_{-1} {
  static_cast<void>(upstream);
  static_cast<void>(chain);
  static_cast<void>(key);
}

tls12_proxy::~tls12_proxy() = default;

silent_server::silent_server(test::bytes reply) : listener_{-1} { static_cast<void>(reply); }

silent_server::~silent_server() = default;

refusing_proxy::refusing_proxy(std::uint16_t upstream) : listener_{-1} { static_cast<void>(upstream); }

refusing_proxy::~refusing_proxy() = default;

int refusing_proxy::accepted() noexcept { return accepted_; }

#endif

} // namespace test::sockets
