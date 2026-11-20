/// @file
/// @brief Connection settings from outside the program [CLIENT Connecting
/// by settings]: where the servers are, as whom to connect, how, and what a
/// server must be, laid from the program's options or a URI, a service's
/// section of the service file, the environment and the defaults, a
/// password none gives looked up in the password file (RULES.md D2).

#ifndef TURBOREL_SETTINGS_HPP
#define TURBOREL_SETTINGS_HPP

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/export.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

struct trl_options;
struct trl_settings;

namespace turborel {

/// The settings' names, as the program, a URI's query and a service file
/// write them [CLIENT Where settings come from].
namespace setting {
/// The servers.
inline constexpr std::string_view host = "host";
/// The port of each host that carries none.
inline constexpr std::string_view port = "port";
/// The user to connect as.
inline constexpr std::string_view user = "user";
/// The user's password.
inline constexpr std::string_view password = "password";
/// How a connection over TCP uses TLS.
inline constexpr std::string_view tls_mode = "tls_mode";
/// Whether SCRAM binds its exchange to the TLS connection.
inline constexpr std::string_view channel_binding = "channel_binding";
/// The program's name, sent as the connection attribute `application`.
inline constexpr std::string_view application_name = "application_name";
/// Whole seconds each step of connecting waits.
inline constexpr std::string_view connect_timeout = "connect_timeout";
/// The section of the service file to read.
inline constexpr std::string_view service = "service";
/// The service file.
inline constexpr std::string_view service_file = "service_file";
/// The password file.
inline constexpr std::string_view passfile = "passfile";
/// What a server must be.
inline constexpr std::string_view target = "target";
/// Whether servers are tried in order.
inline constexpr std::string_view load_balance = "load_balance";
/// A PEM file of the certificate authorities trusted.
inline constexpr std::string_view tls_roots = "tls_roots";
/// Whether the platform's authorities are trusted too.
inline constexpr std::string_view tls_system_roots = "tls_system_roots";
/// A PEM file of the client's certificate chain.
inline constexpr std::string_view tls_certificate = "tls_certificate";
/// A PEM file of the client's key.
inline constexpr std::string_view tls_key = "tls_key";
/// The name the server's certificate is checked against.
inline constexpr std::string_view server_name = "server_name";
} // namespace setting

/// How a connection over TCP uses TLS [CLIENT TLS by mode].
enum class tls_mode : std::uint8_t {
  /// Never: plain TCP.
  disable,
  /// TLS if the server takes it, else plain TCP; the certificate not checked.
  prefer,
  /// TLS or no connection; the certificate not checked.
  require,
  /// TLS, the certificate signed by an authority trusted.
  verify_ca,
  /// TLS, the certificate signed by an authority trusted and of the
  /// server's name.
  verify_full,
};

/// What a server must be for a connection to it to be kept [CLIENT Several
/// servers].
enum class target_role : std::uint8_t {
  /// Any server.
  any,
  /// One that takes changes.
  read_write,
  /// One that takes none: a replica following its group.
  read_only,
};

/// Whether servers are tried in the order given.
enum class load_balance : std::uint8_t {
  /// In order.
  disable,
  /// In an order drawn at random for each connection.
  random,
};

/// A layer of connection settings, each by its name, as text.
class TURBOREL_CPP_EXPORT options {
public:
  /// A layer of no settings.
  /// @throws std::bad_alloc If none can be made.
  options();

  /// The layer a connection URI writes [CLIENT The connection URI]:
  /// `turborel://[user[:password]@][host[:port][,host[:port]]...][/][?setting=value[&...]]`.
  /// @param uri The URI.
  /// @return The layer.
  /// @throws turborel::error Of another scheme, a path, a setting not in
  ///   the table, a pair without `=`, or bad percent-encoding (08001).
  [[nodiscard]] static options from_uri(const std::string& uri);

  /// Sets a setting, by its name.
  /// @param key The setting's name, as setting::host.
  /// @param value Its value.
  /// @return The layer.
  /// @throws turborel::error Of a name of no setting (08001).
  options& set(std::string_view key, std::string_view value);

  /// A setting's value.
  /// @param key The setting's name.
  /// @return Its value, valid until it is set again; none if not set.
  [[nodiscard]] std::optional<std::string_view> get(std::string_view key) const;

  /// turborel-c's layer.
  /// @return It.
  [[nodiscard]] const trl_options* c_options() const noexcept { return options_.get(); }

private:
  /// Frees a layer.
  struct freer {
    /// Frees `made`.
    /// @param made The layer.
    void operator()(trl_options* made) const noexcept;
  };

  explicit options(trl_options* made) noexcept : options_{made} {}

  std::unique_ptr<trl_options, freer> options_;
};

/// A server the settings name.
struct server_address {
  /// Its host name or address, or the path of its Unix socket or pipe.
  std::string_view name;
  /// Its port, 0 of a local one.
  std::uint16_t port{};
  /// Whether it is a Unix socket or a named pipe.
  bool local{};
};

/// How to connect, as the settings say [CLIENT Connecting by settings].
/// Settings connect from several threads at once, every connection they
/// make sharing their SCRAM key cache.
class TURBOREL_CPP_EXPORT settings {
public:
  /// Reads one of the environment's variables, as the program gives them.
  using variables = std::function<std::optional<std::string>(std::string_view name)>;

  /// Lays settings: `layer`, then the service it or the environment names,
  /// then the environment, then the defaults [CLIENT Where settings come
  /// from].
  /// @param layer The program's layer.
  /// @return The settings.
  /// @throws turborel::error Of a setting that cannot be read, a service
  ///   file unread or without the section, or no user (08001).
  [[nodiscard]] static settings resolve(const options& layer = options{});

  /// Lays settings as resolve() does, `environment` reading the
  /// environment's variables.
  /// @param layer The program's layer.
  /// @param environment The reader of the environment.
  /// @return The settings.
  /// @throws turborel::error As resolve() does.
  [[nodiscard]] static settings resolve(const options& layer, const variables& environment);

  /// Lays the settings of a connection URI, as resolve() lays a layer.
  /// @param uri The URI.
  /// @return The settings.
  /// @throws turborel::error As options::from_uri and resolve() do.
  [[nodiscard]] static settings from_uri(const std::string& uri);

  /// How many servers they name.
  /// @return How many.
  [[nodiscard]] std::size_t host_count() const noexcept;

  /// A server they name, in the order given.
  /// @param index Which, from 0.
  /// @return The server.
  /// @throws turborel::error Past the last (-910).
  [[nodiscard]] server_address host(std::size_t index) const;

  /// The user to connect as.
  /// @return The user.
  [[nodiscard]] std::string_view user() const noexcept;

  /// A setting's value, as laid; of `password`, none.
  /// @param key The setting's name.
  /// @return Its value, or none if no layer set it.
  [[nodiscard]] std::optional<std::string_view> get(std::string_view key) const;

  /// How TLS is used.
  /// @return The mode.
  [[nodiscard]] turborel::tls_mode tls_mode() const noexcept;

  /// Whether SCRAM binds its channel.
  /// @return How.
  [[nodiscard]] turborel::channel_binding channel_binding() const noexcept;

  /// How long each step of connecting waits; zero for no limit.
  /// @return The timeout.
  [[nodiscard]] std::chrono::milliseconds connect_timeout() const noexcept;

  /// What a server must be.
  /// @return The role.
  [[nodiscard]] target_role target() const noexcept;

  /// Whether the servers are tried in order.
  /// @return Which.
  [[nodiscard]] turborel::load_balance load_balance() const noexcept;

  /// Whether the platform's authorities are trusted too.
  /// @return Whether.
  [[nodiscard]] bool tls_system_roots() const noexcept;

  /// The password to prove at a server: the settings', else the password
  /// file's for that server, its port and the user [CLIENT The password
  /// file].
  /// @param host Which server, from 0.
  /// @return The password, or none.
  [[nodiscard]] std::optional<std::string> password_for(std::size_t host) const;

  /// The lookups of the SCRAM key cache every connection they make shares.
  /// @return Its hits and misses.
  [[nodiscard]] key_cache::lookups key_cache_lookups() const noexcept;

  /// Connects as the settings say: to the first server, in their order, at
  /// each address its name resolves to, that is reached, logs the user in
  /// and is of the role asked [CLIENT Several servers].
  /// @return The connection.
  /// @throws turborel::error Of one server tried, its own failure; of
  ///   several, 08001 naming each address and why, 28000 where the last
  ///   refused the user.
  [[nodiscard]] connection connect() const;

  /// turborel-c's settings.
  /// @return Them.
  [[nodiscard]] const trl_settings* get() const noexcept { return settings_.get(); }

private:
  /// Frees settings.
  struct freer {
    /// Frees `made`.
    /// @param made The settings.
    void operator()(trl_settings* made) const noexcept;
  };

  explicit settings(trl_settings* made) noexcept : settings_{made} {}

  std::unique_ptr<trl_settings, freer> settings_;
};

} // namespace turborel

#endif // TURBOREL_SETTINGS_HPP
