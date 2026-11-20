# The SQL-agent

[Contents](README.md)

## NAME

`turborel::agent`: a program's connections, each named, one current.

## SYNOPSIS

```cpp
#include <turborel/agent.hpp>

namespace turborel {
class agent {
public:
  using connector =
      std::function<connection(const std::optional<std::string>& server, const std::optional<std::string>& user)>;
  explicit agent(connector make);
  agent(connector make, connection first);
  result query(const std::string& sql);
  result query_batches(const std::string& sql, const std::function<void(const result&)>& handler);
  std::optional<std::string_view> current_name() const noexcept;
  bool has_current() const noexcept;
  std::size_t size() const noexcept;
  void terminate();
};
}
```

## DESCRIPTION

An agent runs SQL's connection statements itself, as the standard's
SQL-agent does: `CONNECT TO address AS name USER user`, or `CONNECT TO
DEFAULT`, makes a connection through the connector, given the address
and the user named, none of the default; `SET CONNECTION name` or
`DEFAULT` makes another current; `DISCONNECT name`, `CURRENT`, `DEFAULT`
or `ALL` ends connections. Every other statement is sent to the current
connection, `query_batches` giving a query's rows a batch at a time.
An agent begun with a connection holds it as the default. `current_name`
is the current connection's name, none of the default; `size` counts
those held; `terminate` ends each by Terminate, every one ended though
one fails. An agent is moved, not copied.

## ERRORS

Each refusal has its SQLSTATE: no such connection, 08003; a name in use,
08002; a name not an identifier, 2E000; a user not one, 28000; CONNECT
or SET CONNECTION in a transaction, 0A001; DISCONNECT of a connection
whose transaction is active, 25000. A connector's failure refuses
CONNECT, SQLCODE -919: 08001 of a server not reached, lost or never
found, else 08004; what it throws that is no `turborel::error` is thrown
as it was, and a connection it made over the program's own stream fails
with -910, TB910.

## EXAMPLES

Made beforehand, as the owner:

```sql
CREATE TABLE T (K INTEGER);
INSERT INTO T VALUES (1);
GRANT SELECT ON T TO PUBLIC;
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>
#include <optional>
#include <string>

int main() {
  const turborel::agent::connector dial = [](const std::optional<std::string>& server,
                                             const std::optional<std::string>& user) {
    std::string host = "127.0.0.1";
    std::uint16_t port = 17439;
    if (server) {
      const std::size_t colon = server->rfind(':');
      host = server->substr(0, colon);
      port = static_cast<std::uint16_t>(std::stoul(server->substr(colon + 1)));
    }
    turborel::mechanism trust = turborel::mechanism::trust();
    return turborel::connection::connect(turborel::target::tcp(host, port), user.value_or("ann"), trust);
  };
  turborel::agent agent{dial};
  static_cast<void>(agent.query("CONNECT TO DEFAULT"));
  static_cast<void>(agent.query("CONNECT TO '127.0.0.1:17439' AS 'other' USER 'y'"));
  std::cout << agent.current_name().value_or("(default)") << ": "
            << agent.query("VALUES CURRENT_USER").at(0).at(0).to_string() << '\n';
  static_cast<void>(agent.query("COMMIT"));
  static_cast<void>(agent.query("SET CONNECTION DEFAULT"));
  std::cout << agent.current_name().value_or("(default)") << ": "
            << agent.query("VALUES CURRENT_USER").at(0).at(0).to_string() << ", " << agent.size() << " held\n";
  static_cast<void>(agent.query("COMMIT"));
  try {
    static_cast<void>(agent.query("SET CONNECTION 'NOPE'"));
  } catch (const turborel::error& e) {
    std::cout << e.sqlstate() << ": " << e.message() << '\n';
  }
  static_cast<void>(agent.query("DISCONNECT ALL"));
  std::cout << agent.size() << " held\n";
}
```

```
OTHER: Y
(default): ANN, 2 held
08003: there is no connection NOPE
0 held
```

## STANDARD

ISO/IEC 9075:1992 §15, the connection management statements; The Client
Library, its section The SQL-agent.

## SEE ALSO

[Pipelines and Pools](../user/08-pipelines-and-pools.md),
[Connections](21-connections.md)
