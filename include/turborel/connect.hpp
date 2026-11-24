/// @file
/// @brief What a connection is made of [CLIENT Connecting]: where the
/// server is, the TLS it is reached over, the mechanism that proves the
/// user, the SCRAM key cache, and what to ask for and how long to wait
/// (RULES.md D2).

#ifndef TURBOREL_CONNECT_HPP
#define TURBOREL_CONNECT_HPP

#include <turborel/export.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

struct trl_tls;
struct trl_target;
struct trl_key_cache;
struct trl_mechanism;
struct trl_conn_options;

namespace turborel {

/// The port a server listens on unless told another [PROTOCOL The Native
/// Protocol].
inline constexpr std::uint16_t default_port = 7422;
/// The named pipe of a server on Windows.
inline constexpr std::string_view default_pipe = R"(\\.\pipe\turborel)";
/// How long connecting, TLS and each step of the login wait, by default
/// [CLIENT Timeouts].
inline constexpr std::chrono::seconds connect_within{30};
/// How long Terminate waits for the server to close, with no read timeout.
inline constexpr std::chrono::seconds terminate_within{60};
/// How long a cancel waits for the server, by default.
inline constexpr std::chrono::seconds cancel_within{10};
/// The longest reply a connection takes, by default: the protocol's
/// longest body, 16 MiB [PROTOCOL Frames].
inline constexpr std::uint32_t max_body_bytes = 16777216;

/// The names of the protocol's capabilities [PROTOCOL Capabilities].
namespace capability {
/// A Notice before a statement's completion.
inline constexpr std::string_view notices = "notices";
/// A Status of each session setting.
inline constexpr std::string_view status = "status";
/// A failure's diagnostics.
inline constexpr std::string_view diagnostics = "diagnostics";
/// The request Describe.
inline constexpr std::string_view describe = "describe";
/// Several numbered cursors of one statement.
inline constexpr std::string_view cursors = "cursors";
/// The request Ping.
inline constexpr std::string_view ping = "ping";
/// The request Reset.
inline constexpr std::string_view reset = "reset";
/// The request ChangeUser.
inline constexpr std::string_view change_user = "change-user";
/// Attributes on Query and Execute.
inline constexpr std::string_view query_attributes = "query-attributes";
/// Hello carries attributes.
inline constexpr std::string_view connection_attributes = "connection-attributes";
/// A Query of several statements.
inline constexpr std::string_view multi_statement = "multi-statement";
/// Compressed frames.
inline constexpr std::string_view compression = "compression";
/// A commit's log position.
inline constexpr std::string_view commit_position = "commit-position";
/// CopyIn, CopyData and CopyEnd.
inline constexpr std::string_view copy = "copy";
/// A statement's row count and time.
inline constexpr std::string_view row_counts = "row-counts";
/// An INSERT's generated identity values.
inline constexpr std::string_view generated_values = "generated-values";
/// A transaction ended with its cursor.
inline constexpr std::string_view fetch_ends = "fetch-ends";
/// A statement's conditions in its completion.
inline constexpr std::string_view conditions = "conditions";
/// Each column's declared type.
inline constexpr std::string_view declared_types = "declared-types";
/// A CALL's procedure, and each `?`'s mode and position.
inline constexpr std::string_view parameter_modes = "parameter-modes";
} // namespace capability

/// TLS settings: the certificate authorities trusted, and the certificate
/// the client presents, if any. Every target made of them shares them, so
/// they are set before the first target is made.
class TURBOREL_CPP_EXPORT tls {
public:
  /// Settings of TLS 1.2 and 1.3 that trust no authority yet, checking the
  /// server's certificate and its name.
  /// @throws turborel::error Of a turborel-c built without TLS.
  tls();

  /// Trusts the certificate authorities of a PEM file.
  /// @param path The file.
  /// @return The settings.
  /// @throws turborel::error If it cannot be read or holds no certificate.
  tls& trust_file(const std::string& path);

  /// Trusts the platform's certificate authorities: its store, or the
  /// files SSL_CERT_FILE and SSL_CERT_DIR name instead.
  /// @return The settings.
  /// @throws turborel::error If they cannot be had.
  tls& trust_platform();

  /// Presents a certificate chain and its key, of two PEM files, as the
  /// mechanism `certificate` needs.
  /// @param chain The file of the chain, the client's first.
  /// @param key The file of its private key.
  /// @return The settings.
  /// @throws turborel::error If either cannot be read, or they do not fit.
  tls& present(const std::string& chain, const std::string& key);

  /// turborel-c's settings.
  /// @return Them.
  [[nodiscard]] trl_tls* get() const noexcept { return tls_.get(); }

private:
  /// Frees settings.
  struct freer {
    /// Frees `tls`.
    /// @param tls The settings.
    void operator()(trl_tls* tls) const noexcept;
  };

  std::unique_ptr<trl_tls, freer> tls_;
};

/// Where a server is: TCP, a Unix socket or named pipe, or TLS.
class TURBOREL_CPP_EXPORT target {
public:
  /// A server over TCP, at each address its host resolves to.
  /// @param host The host's name or address.
  /// @param port Its port.
  /// @return The target.
  /// @throws turborel::error Of an empty host (-910).
  [[nodiscard]] static target tcp(const std::string& host, std::uint16_t port = default_port);

  /// A server over a Unix socket, or on Windows a named pipe.
  /// @param path The socket's path, or the pipe's name.
  /// @return The target.
  /// @throws turborel::error Of an empty path (-910).
  [[nodiscard]] static target local(const std::string& path);

  /// A server over TLS on TCP, its certificate checked against
  /// `server_name`, else `host`.
  /// @param host The host's name or address.
  /// @param port Its port.
  /// @param settings The TLS settings, which the target shares.
  /// @param server_name The name its certificate must have.
  /// @return The target.
  /// @throws turborel::error If the settings trust no authority, or failed.
  [[nodiscard]] static target over_tls(const std::string& host, std::uint16_t port, const tls& settings,
                                       const std::optional<std::string>& server_name = std::nullopt);

  /// Whether it is TCP without TLS, over which no secret goes.
  /// @return Whether it is.
  [[nodiscard]] bool is_plain_tcp() const noexcept;

  /// Whether it is over TLS.
  /// @return Whether it is.
  [[nodiscard]] bool is_tls() const noexcept;

  /// Its host, of TCP or TLS; none of a local target.
  /// @return The host, valid while the target is.
  [[nodiscard]] std::optional<std::string_view> host() const noexcept;

  /// turborel-c's target.
  /// @return It.
  [[nodiscard]] const trl_target* get() const noexcept { return target_.get(); }

private:
  /// Frees a target.
  struct freer {
    /// Frees `target`.
    /// @param target The target.
    void operator()(trl_target* target) const noexcept;
  };

  explicit target(trl_target* made) noexcept : target_{made} {}

  std::unique_ptr<trl_target, freer> target_;
};

/// A cache of the keys SCRAM derives from a password [CLIENT The SCRAM key
/// cache], which every thread may use at once. Its copies are the same
/// cache.
class TURBOREL_CPP_EXPORT key_cache {
public:
  /// Its lookups.
  struct lookups {
    /// Those that found keys.
    std::uint64_t hits{};
    /// Those that derived them.
    std::uint64_t misses{};
  };

  /// An empty cache.
  /// @throws std::bad_alloc If none can be made.
  key_cache();

  /// How many entries it keeps, at most 64.
  /// @return The count.
  [[nodiscard]] std::size_t size() const noexcept;

  /// Its lookups' hits and misses.
  /// @return Them.
  [[nodiscard]] lookups counts() const noexcept;

  /// Drops every entry.
  void clear() noexcept;

  /// turborel-c's cache.
  /// @return It.
  [[nodiscard]] trl_key_cache* get() const noexcept { return cache_.get(); }

private:
  std::shared_ptr<trl_key_cache> cache_;
};

/// How SCRAM binds its exchange to the TLS connection carrying it.
enum class channel_binding : std::uint8_t {
  /// Binds where the connection has a binding.
  prefer,
  /// Binds nothing.
  disable,
  /// Refuses a login not bound (SQLSTATE 28000).
  require,
};

/// What proves the user [PROTOCOL Mechanisms]; it keeps its exchange's
/// state, so it serves one login at a time.
class TURBOREL_CPP_EXPORT mechanism {
public:
  /// `trust`: the server trusts the user as it names itself.
  /// @return The mechanism.
  [[nodiscard]] static mechanism trust();

  /// `password`: the password sent whole, only over TLS, a Unix socket or
  /// a named pipe.
  /// @param secret The password, UTF-8.
  /// @return The mechanism.
  [[nodiscard]] static mechanism password(const std::string& secret);

  /// `certificate`: the user proved by the certificate TLS presents.
  /// @return The mechanism.
  [[nodiscard]] static mechanism certificate();

  /// SCRAM-SHA-256 of a password, SCRAM-SHA-256-PLUS over TLS.
  /// @param secret The password, UTF-8.
  /// @return The mechanism.
  /// @throws turborel::error Of a turborel-c built without TLS's hashes.
  [[nodiscard]] static mechanism scram(const std::string& secret);

  /// Lets SCRAM go over plain TCP, to a server whose
  /// SERVER.SCRAM_WITHOUT_TLS is ON.
  /// @return The mechanism.
  /// @throws turborel::error If it is not SCRAM (-910).
  mechanism& in_the_clear();

  /// Says how SCRAM binds its exchange.
  /// @param how How.
  /// @return The mechanism.
  /// @throws turborel::error If it is not SCRAM (-910).
  mechanism& bind_channel(channel_binding how);

  /// Gives SCRAM a key cache, which it shares.
  /// @param cache The cache.
  /// @return The mechanism.
  /// @throws turborel::error If it is not SCRAM (-910).
  mechanism& with_cache(const key_cache& cache);

  /// Binds SCRAM to a TLS stream the program opened, before
  /// connection::open: by its exported keys over TLS 1.3, else by the
  /// server's certificate.
  /// @param certificate The server's certificate, DER; empty if not had.
  /// @param exporter The 32 bytes exported under
  ///   "EXPORTER-Channel-Binding"; empty if not had.
  /// @return The mechanism.
  mechanism& bind(std::span<const std::byte> certificate, std::span<const std::byte> exporter);

  /// The name it is sent by, as "scram-sha-256-plus" once bound.
  /// @return The name, valid while the mechanism is.
  [[nodiscard]] std::string_view name() const noexcept;

  /// turborel-c's mechanism.
  /// @return It.
  [[nodiscard]] trl_mechanism* get() const noexcept { return mechanism_.get(); }

private:
  /// Frees a mechanism, its password cleared first.
  struct freer {
    /// Frees `mechanism`.
    /// @param mechanism The mechanism.
    void operator()(trl_mechanism* mechanism) const noexcept;
  };

  explicit mechanism(trl_mechanism* made) noexcept : mechanism_{made} {}

  std::unique_ptr<trl_mechanism, freer> mechanism_;
};

/// What a connection asks for as it opens, and how long it waits [CLIENT
/// What a connection asks for] [CLIENT Timeouts]. Made, it asks for every
/// capability but compression, waits 30 seconds for each step of
/// connecting, and has no read timeout.
class TURBOREL_CPP_EXPORT connect_options {
public:
  /// The defaults.
  /// @throws std::bad_alloc If none can be made.
  connect_options();

  /// Asks for no capability: the protocol's first form alone.
  /// @return The options.
  connect_options& ask_none();

  /// Asks for a capability too, as capability::compression.
  /// @param name Its name.
  /// @return The options.
  /// @throws turborel::error Of an empty name (-910).
  connect_options& ask(std::string_view name);

  /// Adds a connection attribute, shown in
  /// INFORMATION_SCHEMA.CONNECTION_ATTRIBUTES.
  /// @param name Its name, 1 to 64 bytes.
  /// @param value Its value, at most 1,024 bytes.
  /// @return The options.
  /// @throws turborel::error Past the protocol's limits, 32 attributes and
  ///   those lengths (-910).
  connect_options& attribute(std::string_view name, std::string_view value);

  /// The shortest frame sent compressed, once compression is granted.
  /// @param bytes The bytes.
  /// @return The options.
  connect_options& compress_from(std::size_t bytes);

  /// How long a connection waits: `connect` to connect, begin TLS and for
  /// each step of the login, `read` for each read or write after. Zero is
  /// no limit.
  /// @param connect The connect timeout.
  /// @param read The read timeout.
  /// @return The options.
  connect_options& timeouts(std::chrono::milliseconds connect, std::chrono::milliseconds read);

  /// turborel-c's options.
  /// @return Them.
  [[nodiscard]] const trl_conn_options* get() const noexcept { return options_.get(); }

private:
  /// Frees options.
  struct freer {
    /// Frees `options`.
    /// @param options The options.
    void operator()(trl_conn_options* options) const noexcept;
  };

  std::unique_ptr<trl_conn_options, freer> options_;
};

} // namespace turborel

#endif // TURBOREL_CONNECT_HPP
