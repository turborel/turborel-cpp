# Connections

[Contents](README.md)

How a connection is made, what it asks for and is told, how its session
is controlled, and how it ends or its statement is cancelled. The
statements a connection runs are in [Statements and
Cursors](22-statements.md), and connecting by settings in
[Settings](26-settings.md).

## Targets

### NAME

`turborel::target`: where a server is.

### SYNOPSIS

```cpp
#include <turborel/connect.hpp>

namespace turborel {
inline constexpr std::uint16_t default_port = 7422;
inline constexpr std::string_view default_pipe = R"(\\.\pipe\turborel)";

class target {
public:
  static target tcp(const std::string& host, std::uint16_t port = default_port);
  static target local(const std::string& path);
  static target over_tls(const std::string& host, std::uint16_t port, const tls& settings,
                         const std::optional<std::string>& server_name = std::nullopt);
  bool is_plain_tcp() const noexcept;
  bool is_tls() const noexcept;
  std::optional<std::string_view> host() const noexcept;
};
}
```

### DESCRIPTION

`tcp` is a server over TCP, tried at each address its host resolves to,
in turn. `local` is a server over a Unix socket of that path, or on
Windows over the named pipe of that name, `default_pipe` its default.
`over_tls` is a server over TLS begun on its TCP port, as the server
tells it by the handshake's first byte; its certificate is checked
against `server_name`, else against `host`. `is_plain_tcp()` says
whether it is TCP without TLS, over which no secret goes; `host()` is
its host, none of a local target.

### ERRORS

`over_tls` of settings that trust no authority fails with SQLCODE -919,
SQLSTATE 08006, saying so. A target is made of anything else; what it
cannot reach fails as it is connected to.

## TLS

### NAME

`turborel::tls`: what TLS trusts, and presents.

### SYNOPSIS

```cpp
namespace turborel {
class tls {
public:
  tls();
  tls& trust_file(const std::string& path);
  tls& trust_platform();
  tls& present(const std::string& chain, const std::string& key);
};
}
```

### DESCRIPTION

Settings of TLS 1.2 and 1.3, made trusting no authority, checking the
server's certificate and its name. `trust_file` trusts the authorities
of a PEM file; `trust_platform` the platform's: its store, or the files
SSL_CERT_FILE and SSL_CERT_DIR name instead; both may be trusted.
`present` presents a certificate chain and its key, of two PEM files, as
the mechanism `certificate` needs. The settings are set before the first
target made of them, which share them.

### ERRORS

A file that cannot be read, a key that is not the certificate's, or a
platform with no authorities fails with SQLCODE -919, SQLSTATE 08006,
saying which. Of a turborel-c built with no TLS library, `tls()` fails
with SQLCODE -900, SQLSTATE 0A000.

## Mechanisms

### NAME

`turborel::mechanism`, `turborel::channel_binding`: what proves the
user.

### SYNOPSIS

```cpp
namespace turborel {
enum class channel_binding : std::uint8_t { prefer, disable, require };

class mechanism {
public:
  static mechanism trust();
  static mechanism password(const std::string& secret);
  static mechanism certificate();
  static mechanism scram(const std::string& secret);
  mechanism& in_the_clear();
  mechanism& bind_channel(channel_binding how);
  mechanism& with_cache(const key_cache& cache);
  mechanism& bind(std::span<const std::byte> certificate, std::span<const std::byte> exporter);
  std::string_view name() const noexcept;
};
}
```

### DESCRIPTION

`trust` is accepted at once, by a server that trusts the client, as
SERVER.TRUST_LOCAL lets it for its own machine. `password` sends the
password whole, only over TLS, a Unix socket or a named pipe.
`certificate` is proved by the certificate TLS presents, its common name
the user's. `scram` is SCRAM-SHA-256 (RFC 5802, RFC 7677): the client's
first message in Hello, the server's first, the client's final, the
server's final, whose signature is checked before the last, empty
Answer; over TLS it is SCRAM-SHA-256-PLUS, bound by `tls-exporter` over
TLS 1.3 and by `tls-server-end-point` over TLS 1.2. Its password is
cleared from memory as the mechanism is destroyed.

`in_the_clear` lets SCRAM go over plain TCP, to a server whose
SERVER.SCRAM_WITHOUT_TLS is ON. `bind_channel` says how SCRAM binds:
where it can, `prefer`; never, `disable`; or the login refused unless it
is bound, `require`. `with_cache` gives it a key cache. `bind` binds it
to a TLS stream the program opened, before `connection::open`: by the
keys exported under `EXPORTER-Channel-Binding` over TLS 1.3, else by the
server's certificate, DER. `name()` is the name the mechanism is sent
by, `scram-sha-256-plus` once bound by `bind`.

### ERRORS

A setter of SCRAM's of another mechanism fails with SQLCODE -910,
SQLSTATE TB910. Of a turborel-c built with no TLS library, `scram` fails
with -900, 0A000, having no hashes.

## The SCRAM key cache

### NAME

`turborel::key_cache`: the keys SCRAM derived, kept for the next login.

### SYNOPSIS

```cpp
namespace turborel {
class key_cache {
public:
  struct lookups { std::uint64_t hits{}; std::uint64_t misses{}; };
  key_cache();
  std::size_t size() const noexcept;
  lookups counts() const noexcept;
  void clear() noexcept;
};
}
```

### DESCRIPTION

A login whose server proved itself keeps the keys its password derived,
found by the salt, the iterations and a tag of the password under a
secret of the cache's own; a later login of the same salt and iterations
skips the derivation, a hit, where another derives, a miss. An entry a
failed login used is dropped. At most 64 entries are kept, the oldest
dropped first, in the process's memory alone. Its copies are the same
cache, which every thread may use at once; settings share one among
every connection they make.

## Options

### NAME

`turborel::connect_options`: what a connection asks for, and how long
it waits.

### SYNOPSIS

```cpp
namespace turborel {
inline constexpr std::chrono::seconds connect_within{30};
inline constexpr std::uint32_t max_body_bytes = 16777216;
namespace capability {
inline constexpr std::string_view notices = "notices";  // and every other capability
}

class connect_options {
public:
  connect_options();
  connect_options& ask_none();
  connect_options& ask(std::string_view name);
  connect_options& attribute(std::string_view name, std::string_view value);
  connect_options& compress_from(std::size_t bytes);
  connect_options& timeouts(std::chrono::milliseconds connect, std::chrono::milliseconds read);
};
}
```

### DESCRIPTION

Made, options ask for every capability but compression, wait
`connect_within` for connecting, TLS and each step of the login, and
have no read timeout. `ask_none` asks for no capability, the protocol's
first form; `ask` for one more, of the names `capability` holds.
`attribute` adds a connection attribute, sent in Hello, which every
server reads, granted or not; `compress_from` sets the shortest frame
sent compressed, once compression is granted; `timeouts` sets both
waits, zero being none.

### ERRORS

More than 32 attributes, a name of more than 64 bytes or none, or a value
of more than 1024, fails with SQLCODE -919, SQLSTATE 08006, before
anything is sent, as the server would fail the connection. A capability
of no name, or a timeout below zero or past 2^32 - 1 ms, fails with -910,
TB910.

## Connections

### NAME

`turborel::connection`: a connection, and the session it holds.

### SYNOPSIS

```cpp
#include <turborel/connection.hpp>

namespace turborel {
struct timing { std::chrono::microseconds total{}, queued{}, locks{}; };

class connection {
public:
  using notice_handler = std::function<void(const failure&)>;
  static connection connect(const target& where, const std::string& user, mechanism& proof,
                            const connect_options& options = connect_options{});
  static connection open(std::unique_ptr<byte_stream> stream, const std::string& user, mechanism& proof,
                         const connect_options& options = connect_options{});
  static connection open(const std::string& uri);
  bool lost() const noexcept;
  std::string_view server() const noexcept;
  std::string_view authorization() const noexcept;
  bool granted(std::string_view name) const;
  std::vector<std::string_view> granted() const;
  std::optional<std::string_view> setting(std::string_view name) const;
  std::vector<std::pair<std::string_view, std::string_view>> settings() const;
  void set_read_timeout(std::chrono::milliseconds wait);
  std::chrono::milliseconds read_timeout() const noexcept;
  void set_max_body(std::uint32_t bytes);
  void ping();
  void reset();
  void change_user(const std::string& user, mechanism& proof);
  void set_query_attributes(const std::vector<std::pair<std::string, std::string>>& attributes);
  void on_notice(notice_handler handler);
  std::vector<failure> notices() const;
  void clear_notices() noexcept;
  canceller make_canceller();
  void terminate();
  // the statements of Statements and Cursors
};

class byte_stream {
public:
  virtual std::optional<std::size_t> read(std::span<std::byte> into) = 0;
  virtual std::optional<std::size_t> write(std::span<const std::byte> bytes) = 0;
  virtual void close() noexcept = 0;
  virtual bool bounds() const noexcept;
  virtual bool bound(std::chrono::milliseconds wait) noexcept;
};
}
```

### DESCRIPTION

`connect` connects to a target as a user, proved by a mechanism, as the
options ask: Hello, the mechanism's Challenges and Answers, a Status of
each session setting, then Ready, which tells the session's
authorization identifier, the cancel key, the capabilities granted and
the server's product. `open(stream, ...)` connects over a stream the
program opened, which the connection then owns and closes; a stream that
`bounds()` itself is bounded as the options' timeouts say, through
`bound`, and its read timeout set as a connection's; one that does not
waits as its maker set it. `open(uri)` connects as a URI says, with the
rest of the settings ([Settings](26-settings.md)).

A connection is used by one thread at a time; a canceller of it cancels
from another. It is moved, not copied; destroyed, it closes without
Terminate. `lost()` says whether it is lost, every call on it then
failing.

`granted` says whether the server granted a capability, or gives all it
granted, in the protocol's order. `setting` gives a session setting as
the server last told it, and `settings()` every one, in the order first
told, with no round trip; neither is had without `status`.
`set_read_timeout` bounds each read and write after, zero being none;
`set_max_body` the longest reply taken, 16 MiB at first.

`ping` asks whether the server and the connection are alive, Ping
answered by Pong, during a copy too. `reset` puts the session back as at
login: its transaction rolled back, told by notice 01T01, its prepared
statements, cursors, a copy, temporary tables and settings gone, its
user and cancel key kept. `change_user` authenticates the connection
again as another user, whose session it then is, renewed as by Reset
under a new cancel key; a refusal loses the connection.
`set_query_attributes` sets attributes every later statement carries,
read in SQL by QUERY_ATTRIBUTE(name), until set again; none clears them.

A Notice comes before its statement's completion. `on_notice` gives each
to a handler, as a failure of SQLCODE 0, on the thread that reads it; a
handler that throws has its exception thrown once that call returns.
Without one, the last 64 are kept, `notices()` the oldest first.

`terminate` ends the session, its transaction as SESSION.COMMIT_AT_END
says, a Notice or Status before the end read past, and waits up to 60
seconds for the server to close. The connection is lost after.

### ERRORS

Every failure is thrown as `turborel::error` ([Errors](30-errors.md)).
A secret mechanism over plain TCP fails with SQLCODE -919, SQLSTATE
28000, before anything is sent; a server that cannot be reached, a reply
that is malformed or longer than taken, or a wait that runs out loses
the connection, -919, 08006; a login the server refuses gives the
server's failure, of class 28 or 08, and ends the connection. A call that
needs a capability not granted fails with -919, 08006, before it sends
anything, the connection kept; a read timeout of a stream that bounds
not itself, likewise. A commit that fails as the session ends gives its
failure.

### EXAMPLES

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

## Cancel

### NAME

`turborel::canceller`: what cancels a connection's statement from
another thread.

### SYNOPSIS

```cpp
namespace turborel {
inline constexpr std::chrono::seconds cancel_within{10};

class canceller {
public:
  void cancel();
  void cancel(std::chrono::milliseconds within);
};
}
```

### DESCRIPTION

`connection::make_canceller` makes one of a connection, holding the key
its Ready gave, cleared from memory as the canceller is destroyed.
`cancel` sends Cancel, that key, as the first and only message of a
connection of its own, which the server closes with no reply; it gives
up after `cancel_within`, or the time given. The statement the
connection is running fails with SQLCODE -920, SQLSTATE TB920, and the
connection goes on.

### ERRORS

A canceller of a connection over a stream the program opened cannot be
made, having no target, SQLCODE -910, SQLSTATE TB910. A cancel whose
server cannot be reached fails with -919, 08006; the connection it would
cancel is kept.

### EXAMPLES

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

## STANDARD

The Native Protocol, version 1, its sections Opening a connection,
Mechanisms, Capabilities, Hello 'H', Ready 'R', Status 'T', Cancel 'Z',
Terminate 'X', Ping 'G', Reset 'W' and ChangeUser 'U'; The Client
Library, its sections Connecting, Timeouts, What a connection asks for,
What the server tells and Controlling the session.

## SEE ALSO

[Connecting](../user/02-connecting.md), [Settings](26-settings.md),
[Errors](30-errors.md)
