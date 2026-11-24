# Connecting

[Contents](README.md)

A program connects to a server by three things: where the server is, a
*target*; the user it connects as; and a *mechanism* that proves the
user. A connection is a session of the server's, held until the program
ends it. This chapter shows each target and mechanism, TLS, what a
connection asks for and how long it waits, what it is told, and how it
ends or its statement is cancelled. [Connections](../client/21-connections.md)
gives each class in full, and [Settings](09-settings.md) how to connect
as a URI, the environment and the files say.

## A first connection

A server listens on TCP port 7422 unless its operator chose another; the
examples here reach one on port 17439. A server whose setting
SERVER.TRUST_LOCAL is ON takes a client of its own machine as the user
it names, by the mechanism `trust`:

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  try {
    turborel::mechanism trust = turborel::mechanism::trust();
    turborel::connection conn =
        turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
    std::cout << conn.server() << ", as " << conn.authorization() << '\n';
    std::cout << "time zone " << conn.setting("SESSION.TIME_ZONE").value_or("unknown") << '\n';
    conn.terminate();
  } catch (const turborel::error& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
```

```
turborel 0.0.0 (alpha), as ANN
time zone +00:00
```

The user is an identifier, so `ann` is ANN. Every failure is thrown as a
`turborel::error`, a `std::runtime_error` whose `what()` reads as
Turborel's own client writes a failure; [Errors and
Diagnostics](10-errors.md) tells what else it holds. A connection
destroyed closes, as a program that ends does; `terminate()` ends the
session first, its transaction as the session says.

## By a URI

`connection::open` connects as a `turborel://` URI says, the rest of the
connection's settings laid under it, the environment's and a service
file's among them, as [Settings](09-settings.md) shows:

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  try {
    turborel::connection conn =
        turborel::connection::open("turborel://ann@127.0.0.1:17439/?tls_mode=disable&application_name=reports");
    std::cout << conn.server() << ", as " << conn.authorization() << '\n';
  } catch (const turborel::error& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
```

```
turborel 0.0.0 (alpha), as ANN
```

## Targets

A target is where the server is:

- `target::tcp(host, port)`, TCP, at each address the host's name
  resolves to, in turn; the port is 7422 unless given;
- `target::local(path)`, a Unix socket, the path the server's
  SERVER.SOCKET names, or on Windows a named pipe, `\\.\pipe\turborel`
  unless the operator chose another;
- `target::over_tls(host, port, tls, server_name)`, TLS on TCP, begun on
  the server's own port, its certificate checked against `server_name`,
  else against the host.

## Mechanisms

A mechanism proves the user, and is made for one login at a time:

- `mechanism::trust()`: the server trusts the user as it names itself,
  as SERVER.TRUST_LOCAL lets it for its own machine;
- `mechanism::password(secret)`: the password, sent whole, only over
  TLS, a Unix socket or a named pipe;
- `mechanism::scram(secret)`: SCRAM-SHA-256, the password proved and
  never sent, the server proving in turn that it knows the user's
  verifier; over TLS it is SCRAM-SHA-256-PLUS, bound to the TLS
  connection carrying it;
- `mechanism::certificate()`: the user the common name of the
  certificate TLS presents names.

Over a Unix socket a password is sent, or proved by SCRAM:

Made beforehand, as the owner:

```sql
CREATE USER BOB PASSWORD 'bob''s secret';
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  const turborel::target local = turborel::target::local("turborel.sock");
  turborel::mechanism password = turborel::mechanism::password("bob's secret");
  std::cout << "password: " << turborel::connection::connect(local, "bob", password).authorization() << '\n';
  turborel::mechanism scram = turborel::mechanism::scram("bob's secret");
  turborel::connection conn = turborel::connection::connect(local, "bob", scram);
  std::cout << "SCRAM: " << conn.authorization() << '\n';
}
```

```
password: BOB
SCRAM: BOB
```

Over plain TCP, where anyone between may read what is sent, the driver
refuses a password, and SCRAM, before anything is sent, SQLSTATE 28000;
`in_the_clear()` lets SCRAM go to a server whose setting
SERVER.SCRAM_WITHOUT_TLS is ON. A password the server refuses fails the
login, SQLSTATE 28000 too, and ends the connection:

Made beforehand, as the owner:

```sql
CREATE USER BOB PASSWORD 'bob''s secret';
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  turborel::mechanism scram = turborel::mechanism::scram("bob's secret");
  try {
    static_cast<void>(turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "bob", scram));
  } catch (const turborel::error& e) {
    std::cout << e.what() << '\n';
  }
  turborel::mechanism wrong = turborel::mechanism::scram("not bob's");
  try {
    static_cast<void>(turborel::connection::connect(turborel::target::local("turborel.sock"), "bob", wrong));
  } catch (const turborel::error& e) {
    std::cout << e.sqlcode() << ' ' << e.sqlstate() << '\n';
  }
}
```

```
SQLCODE -919 (SQLSTATE 28000): scram-sha-256 is sent only over TLS, a Unix socket or a named pipe, never over plain TCP [DESIGN]
-919 28000
```

## TLS

A `turborel::tls` holds what TLS trusts and presents: the authorities of
a PEM file, `trust_file`, the platform's, `trust_platform`, or both; and
a certificate chain and its key to present, `present`. The platform's
are its store, or the files SSL_CERT_FILE and SSL_CERT_DIR name instead.
Every target made of the settings shares them.

Over TLS 1.3, SCRAM binds its exchange by the keys TLS exports,
`tls-exporter`; over TLS 1.2, by the server's certificate,
`tls-server-end-point`. `bind_channel(channel_binding::require)` refuses
a login that is not bound, SQLSTATE 28000. The keys SCRAM derives from a
password may be kept in a `turborel::key_cache`, so a later login of the
same salt does not derive them again:

Made beforehand, as the owner:

```sql
CREATE USER BOB PASSWORD 'bob''s secret';
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  turborel::tls settings;
  settings.trust_file("ca.pem");
  const turborel::target server = turborel::target::over_tls("localhost", 17439, settings);
  turborel::key_cache cache;
  turborel::mechanism scram = turborel::mechanism::scram("bob's secret");
  scram.bind_channel(turborel::channel_binding::require).with_cache(cache);
  for (int login = 0; login < 2; ++login) {
    turborel::connection conn = turborel::connection::connect(server, "bob", scram);
    std::cout << "bound to the channel: " << conn.authorization() << '\n';
  }
  std::cout << "cache hits " << cache.counts().hits << ", misses " << cache.counts().misses << '\n';
  turborel::tls presenting;
  presenting.trust_file("ca.pem").present("client.pem", "client.key");
  turborel::mechanism certificate = turborel::mechanism::certificate();
  const turborel::target by_certificate = turborel::target::over_tls("localhost", 17439, presenting);
  std::cout << "certificate: " << turborel::connection::connect(by_certificate, "ann", certificate).authorization()
            << '\n';
}
```

```
bound to the channel: BOB
bound to the channel: BOB
cache hits 1, misses 1
certificate: ANN
```

A program that opens its own stream, as a TLS stream of a library of its
own, connects over it by `connection::open(stream, user, mechanism)`,
the stream a `turborel::byte_stream` it implements; SCRAM is bound to
it first by `mechanism::bind`, of the server's certificate and the keys
the stream exports. The connection owns the stream, and closes it.

## What a connection asks for, and waits

A connection asks for every capability of the protocol but compression,
and the server grants those it has. `turborel::connect_options` changes
that: `ask_none()` asks for none, the protocol's first form; `ask` asks
for one more, as `capability::compression`; `attribute` sends a
connection attribute, which the server shows in
INFORMATION_SCHEMA.CONNECTION_ATTRIBUTES; `compress_from` sets the
shortest frame sent compressed. A call that needs a capability not
granted fails before it sends anything, SQLSTATE 08006, the connection
kept.

Connecting, TLS and each step of the login wait 30 seconds unless
`timeouts` says otherwise, and reads and writes after, none, unless it
says a read timeout; a wait that runs out loses the connection, SQLSTATE
08006. `set_read_timeout` changes the read timeout of a connection made.

```cpp
#include <turborel/turborel.hpp>

#include <chrono>
#include <iostream>

int main() {
  using namespace std::chrono_literals;
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connect_options options;
  options.ask(turborel::capability::compression).attribute("application", "nightly-report").timeouts(5s, 30s);
  turborel::connection conn =
      turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust, options);
  for (const std::string_view name : conn.granted()) {
    std::cout << name << ' ';
  }
  std::cout << '\n' << "read timeout " << conn.read_timeout().count() << " ms\n";
  for (const auto& [name, value] : conn.settings()) {
    if (name.find('.') == std::string_view::npos) {
      std::cout << name << " = " << value << '\n';
    }
  }
  turborel::connect_options none;
  none.ask_none();
  turborel::connection plain =
      turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust, none);
  try {
    plain.ping();
  } catch (const turborel::error& e) {
    std::cout << e.what() << '\n';
  }
}
```

```
notices status diagnostics describe cursors ping reset change-user query-attributes connection-attributes multi-statement compression commit-position copy row-counts generated-values fetch-ends conditions declared-types parameter-modes 
read timeout 30000 ms
SERVER_VERSION = turborel 0.0.0 (alpha)
AUTHORIZATION = ANN
SCHEMA = ANN
READ_ONLY = NO
SERVER_READ_ONLY = NO
CATALOG = TURBOREL
OWNER = YES
SQLCODE -919 (SQLSTATE 08006): the server did not grant ping [DESIGN]
```

## What the server tells

At login, and as statements change them, the server tells the session's
settings: its authorization identifier, schema and catalog, whether it is
read-only, whether its server is, whether its user owns the database,
the server's version, and every setting a session may set, as
SESSION.TIME_ZONE and TRANSACTION.ISOLATION. `setting(name)` gives one
as last told, and `settings()` all of them, with no round trip; neither
is had without `status`. `server()` and `authorization()` are as Ready
told them.

## Ending, and cancelling

`terminate()` ends the session: its transaction is committed or rolled
back as SESSION.COMMIT_AT_END says, a commit that fails gives its
failure, and the server closes. The connection is lost after, every
call on it failing.

A statement runs until it completes, unless it is cancelled from
another thread by a `turborel::canceller`, which `make_canceller()`
makes of the connection, holding the key Ready gave. `cancel()` sends it
over a connection of its own, giving up after 10 seconds, or the time
given; the statement fails, SQLCODE -920, SQLSTATE TB920, and the
connection goes on:

```cpp
#include <turborel/turborel.hpp>

#include <chrono>
#include <iostream>
#include <thread>

int main() {
  using namespace std::chrono_literals;
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  turborel::canceller canceller = conn.make_canceller();
  std::thread cancelling{[&canceller] {
    std::this_thread::sleep_for(300ms);
    canceller.cancel();
  }};
  try {
    static_cast<void>(conn.query("WITH RECURSIVE T(N) AS (SELECT CAST(1 AS BIGINT) FROM (VALUES (1)) AS V(X) "
                                 "UNION ALL SELECT N + 1 FROM T WHERE N < 100000) SELECT COUNT(*) FROM T A, T B"));
  } catch (const turborel::error& e) {
    std::cout << e.what() << '\n';
  }
  cancelling.join();
  std::cout << conn.query("VALUES 1").at(0).get<std::int64_t>(0) << '\n';
  conn.terminate();
  std::cout << std::boolalpha << conn.lost() << '\n';
}
```

```
SQLCODE -920 (SQLSTATE TB920): the statement was cancelled
1
true
```
