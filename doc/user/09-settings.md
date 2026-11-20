# Settings

[Contents](README.md)

A program may connect as settings say rather than by a target and a
mechanism of its own: a URI, options it sets, a service file, a password
file and the environment, laid in that order. This chapter shows each,
several servers and the role asked of them, and how TLS modes and channel
binding protect a connection. [Settings](../client/26-settings.md) gives
each call, and [Appendix C: Settings](../appendix/c-settings.md) every
setting.

## A URI

`settings::from_uri` reads a `turborel://` URI: its user and password,
its hosts, each with its port, and its query's settings, laid over the
rest; `connect()` connects as they say. `connection::open(uri)` does both
at once. Hosts are tried in the order given, one that cannot be reached
passed for the next:

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

## Where settings come from

`turborel::options` holds settings set by name, as `setting::tls_mode`;
`settings::resolve(options)` lays them, taking each setting from the
first layer that has it: the program's options, the service file's
section the `service` setting names, the environment's variables, then
the defaults. The environment is read through a function the program
gives, or the process's own. The password, unless given, is the password
file's first line matching the server and user:

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

A setting that cannot be read, a service file that cannot be read or
lacks its section, or no user, is refused, SQLSTATE 08001, before
anything is sent.

## How a connection is proved

A password is proved by SCRAM-SHA-256, never sent over plain TCP,
SQLSTATE 28000 before anything is sent; with none, the certificate
`tls_certificate` and `tls_key` name is presented, else the user is
trusted. Every connection the same settings make shares one SCRAM key
cache.

## Several servers and roles

Each host is tried in turn, at each address its name resolves to, within
the connect timeout; one that cannot be reached, refuses the user, or is
not of the role asked is passed for the next, and with none kept the
failure names each address and why, SQLSTATE 08001, or 28000 if the last
refused the user. `load_balance=random` tries them in a random order for
each connection. `target=read-write` takes a server alone or its group's
primary, and `target=read-only` a replica, as the server's
SERVER_READ_ONLY tells it, or, of one that does not, as
INFORMATION_SCHEMA.REPLICAS shows; `role()` gives a connection's.

## TLS modes and channel binding

`tls_mode` is how TCP is protected: `disable`, plain TCP; `prefer`, TLS
if the server takes it, else plain TCP, connecting again after a failed
handshake; `require`, TLS, its certificate not checked; `verify-ca`, its
certificate checked against the authorities trusted, `tls_roots` and,
with `tls_system_roots=true`, the platform's; `verify-full`, its name
too, against `server_name`, else the host's. A Unix socket or named pipe
uses no TLS. `channel_binding=require` refuses, before connecting,
settings that cannot bind SCRAM to TLS, SQLSTATE 28000:

Made beforehand, as the owner:

```sql
CREATE USER BOB PASSWORD 'bob''s secret';
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  const turborel::settings verified = turborel::settings::from_uri(
      "turborel://bob:bob's%20secret@localhost:17439/?tls_mode=verify-full&tls_roots=ca.pem&channel_binding=require");
  std::cout << verified.connect().authorization() << " over verified TLS, bound\n";
  try {
    static_cast<void>(
        turborel::settings::from_uri("turborel://ann@127.0.0.1:17439/?tls_mode=verify-full&tls_roots=ca.pem&server_name=other")
            .connect());
  } catch (const turborel::error& e) {
    std::cout << e.sqlstate() << ": " << e.message() << '\n';
  }
  try {
    static_cast<void>(turborel::settings::from_uri("turborel://ann@127.0.0.1:17439/?tls_mode=disable&target=read-only").connect());
  } catch (const turborel::error& e) {
    std::cout << e.sqlstate() << ": " << e.message() << '\n';
  }
}
```

```
BOB over verified TLS, bound
08006: the TLS handshake failed: certificate verify failed (hostname mismatch)
08001: the server is read-write, and read-only is asked
```
