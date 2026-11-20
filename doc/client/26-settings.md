# Settings

[Contents](README.md)

Connection settings, laid from the program's options or a URI, a service
file, the environment and the defaults, and the connections they make.
Every setting is in [Appendix C: Settings](../appendix/c-settings.md).

## Options

### NAME

`turborel::options`, `turborel::setting`: settings a program sets.

### SYNOPSIS

```cpp
#include <turborel/settings.hpp>

namespace turborel {
namespace setting {
inline constexpr std::string_view host = "host";  // and every setting of Appendix C
}

class options {
public:
  options();
  static options from_uri(const std::string& uri);
  options& set(std::string_view key, std::string_view value);
  std::optional<std::string_view> get(std::string_view key) const;
};
}
```

### DESCRIPTION

`options` holds settings by name, as a URI's query and a service file
name them. `from_uri` reads a URI's: `turborel://user:password@hosts/?
setting=value&...`, each part percent-encoded, an IPv6 address in
brackets, a Unix socket's path percent-encoded as a host. `get` gives a
setting as set.

### ERRORS

A key that names no setting, a URI of another scheme, of a path, of a
pair without `=`, or of a `%` not followed by two hexadecimal digits
fails with SQLCODE -919, SQLSTATE 08001, saying which.

## Settings laid

### NAME

`turborel::settings`, `turborel::server_address`, `turborel::tls_mode`,
`turborel::target_role`, `turborel::load_balance`.

### SYNOPSIS

```cpp
namespace turborel {
enum class tls_mode : std::uint8_t { disable, prefer, require, verify_ca, verify_full };
enum class target_role : std::uint8_t { any, read_write, read_only };
enum class load_balance : std::uint8_t { disable, random };
struct server_address { std::string_view name; std::uint16_t port{}; bool local{}; };

class settings {
public:
  using variables = std::function<std::optional<std::string>(std::string_view name)>;
  static settings resolve(const options& layer = options{});
  static settings resolve(const options& layer, const variables& environment);
  static settings from_uri(const std::string& uri);
  std::size_t host_count() const noexcept;
  server_address host(std::size_t index) const;
  std::string_view user() const noexcept;
  std::optional<std::string_view> get(std::string_view key) const;
  turborel::tls_mode tls_mode() const noexcept;
  turborel::channel_binding channel_binding() const noexcept;
  std::chrono::milliseconds connect_timeout() const noexcept;
  target_role target() const noexcept;
  turborel::load_balance load_balance() const noexcept;
  bool tls_system_roots() const noexcept;
  std::optional<std::string> password_for(std::size_t host) const;
  key_cache::lookups key_cache_lookups() const noexcept;
  connection connect() const;
};
}
```

### DESCRIPTION

`resolve` lays settings: each from the first layer that has it, the
options given, the section of the service file `service` names, the
environment's variables, then the defaults. The environment is read
through `environment`, a function of a variable's name, or the
process's own; a variable set but empty gives nothing. `from_uri` lays a
URI's options so. The service file is `service_file`, else
`~/.turborel_service.conf`: sections `[name]`, lines `setting = value`,
`#` comments and blank lines skipped. The password file is `passfile`,
else `~/.turborel_pass`: lines `host:port:user:password`, `*` matching
anything, `\` escaping `:` and `\`, `#` comments; the first line that
matches a server and user gives its password, a Unix socket matched by
its path and the default port, read only if a regular file, or a link to
one.

Each accessor gives a setting laid, `get` any by its name but the
password, which `password_for` gives of a server: the settings' own, else
the password file's. `connect` connects as they say: each host in turn,
or in a random order of `load_balance=random`, at each address it
resolves to, one not reached, refusing the user or not of the `target`
role passed for the next; TLS as `tls_mode` says, connecting again over
plain TCP after a failed handshake of `prefer`; a password proved by
SCRAM, else the certificate named presented, else trust. Every
connection one `settings` makes shares one SCRAM key cache, whose
lookups `key_cache_lookups` gives.

### ERRORS

A setting that cannot be read, a service file that cannot be read or
lacks its section, a malformed line of one, naming its file and line, a
word a setting does not take, naming those it takes, or no user, fails
with SQLCODE -919, SQLSTATE 08001. `connect` with no server kept fails
with -919, 08001, naming each address and why, or 28000 if the last
refused the user; a secret over plain TCP, or binding required where it
cannot bind, with -919, 28000, before anything is sent; a certificate
that does not check, with -919, 08006. `host(i)` past the last fails with
-910, TB910.

### EXAMPLES

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  const turborel::settings laid = turborel::settings::from_uri(
      "turborel://ann@127.0.0.1:1,127.0.0.1:17439/?tls_mode=disable&application_name=reports&connect_timeout=5");
  for (std::size_t i = 0; i < laid.host_count(); ++i) {
    std::cout << laid.host(i).name << ':' << laid.host(i).port << '\n';
  }
  std::cout << laid.user() << ", " << laid.connect_timeout().count() << " ms\n";
  turborel::connection conn = laid.connect();
  std::cout << conn.authorization() << ", " << (conn.role() == turborel::server_role::read_write ? "read-write" : "not")
            << '\n';
}
```

```
127.0.0.1:1
127.0.0.1:17439
ann, 5000 ms
ANN, read-write
```

Made beforehand, as the owner:

```sql
CREATE USER BOB PASSWORD 'bob''s secret';
```

```cpp
#include <turborel/turborel.hpp>

#include <fstream>
#include <iostream>
#include <optional>
#include <string>

int main() {
  std::ofstream{"services.conf"} << "# servers\n[local]\nhost = 127.0.0.1\nport = 17439\nuser = bob\n";
  std::ofstream{"passwords"} << "127.0.0.1:17439:bob:bob's secret\n";
  const turborel::settings::variables environment = [](std::string_view name) -> std::optional<std::string> {
    if (name == "TURBOREL_SERVICE_FILE") return "services.conf";
    if (name == "TURBOREL_PASSFILE") return "passwords";
    return std::nullopt;
  };
  turborel::options program;
  program.set(turborel::setting::service, "local").set(turborel::setting::tls_mode, "require");
  const turborel::settings laid = turborel::settings::resolve(program, environment);
  std::cout << laid.host(0).name << ':' << laid.host(0).port << " as " << laid.user() << ", password "
            << (laid.password_for(0) ? "found" : "none") << '\n';
  std::cout << laid.connect().authorization() << " by SCRAM over TLS\n";
  try {
    static_cast<void>(turborel::settings::from_uri("turborel://bob@db/?tls_mode=always"));
  } catch (const turborel::error& e) {
    std::cout << e.sqlstate() << ": " << e.message() << '\n';
  }
}
```

```
127.0.0.1:17439 as bob, password found
BOB by SCRAM over TLS
08001: tls_mode is always, not one of disable, prefer, require, verify-ca, verify-full
```

## STANDARD

The Client Library's Settings, Pipelines and Pools, its sections
Connecting by settings, Where settings come from, The connection URI,
The service file, The password file, Several servers and TLS by mode.

## SEE ALSO

[Settings](../user/09-settings.md),
[Appendix C: Settings](../appendix/c-settings.md),
[Connections](21-connections.md)
