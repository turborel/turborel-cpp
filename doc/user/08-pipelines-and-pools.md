# Pipelines and Pools

[Contents](README.md)

A pipeline sends many requests before their answers come, so a batch of
statements costs one wait for the server rather than one each. A pool
keeps connections a program's threads share. The SQL-agent holds a
program's connections by name, as SQL's CONNECT, SET CONNECTION and
DISCONNECT make, choose and end them. This chapter shows each;
[Pipelines](../client/27-pipelines.md), [Pools](../client/28-pools.md)
and [The SQL-agent](../client/29-sql-agent.md) give their calls in full.

## Pipelines

A `turborel::pipeline` of a connection queues queries and executions of
prepared statements; `run()` sends them, a window of 64 KiB at a time,
reading each window's replies before the next, and gives one outcome
each, in order. A failure is its request's own, and the requests after it
run. Under autocommit each request is its own transaction.
`when_one_fails(on_error::skip)` makes a batch one transaction: a
savepoint before each request but the first, those after a failure
rolled back and skipped, those not yet written never sent, and the batch
committed as it ends under autocommit where it began none:

Made beforehand, as the owner:

```sql
CREATE TABLE T (K INTEGER PRIMARY KEY, V VARCHAR(10));
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  const turborel::statement insert = conn.prepare("INSERT INTO T VALUES (?, 'v')");
  turborel::pipeline piped{conn};
  piped.query("INSERT INTO T VALUES (1, 'a')")
      .query("INSERT INTO T VALUES (1, 'again')")
      .execute(insert, turborel::params::of(2))
      .query("SELECT K, V FROM T ORDER BY K");
  piped.run();
  for (std::size_t i = 0; i < piped.size(); ++i) {
    const char* outcome = piped.outcome(i) == turborel::piped::done ? "done" : "failed";
    std::cout << i << ' ' << outcome;
    if (const auto failed = piped.failure_of(i)) {
      std::cout << ' ' << failed->sqlstate;
    }
    std::cout << '\n';
  }
  std::cout << piped.result_of(3).size() << " rows\n";
  static_cast<void>(conn.query("ROLLBACK"));
  turborel::pipeline skipping{conn};
  skipping.when_one_fails(turborel::on_error::skip);
  skipping.query("INSERT INTO T VALUES (3, 'kept')").query("INSERT INTO T VALUES (3, 'twice')")
      .query("INSERT INTO T VALUES (4, 'undone')");
  skipping.run();
  std::cout << (skipping.outcome(2) == turborel::piped::skipped ? "skipped" : "not skipped") << ", "
            << conn.query("SELECT K FROM T").size() << " row\n";
}
```

```
0 done
1 failed 23000
2 done
3 done
2 rows
skipped, 1 row
```

## Pools

A `turborel::pool`, made of settings or of a function that makes a
connection, keeps at most `max` connections, 10 by default, which a
program's threads share; its copies are the same pool. `get()` hands out
an idle connection that answers its check, or makes one while fewer than
`max` are made, or waits for one to come back, 30 seconds by default.
The `pooled` handle it gives is used as the connection, and gives it
back as it is destroyed, reset to its login: its transaction rolled
back, its statements gone, autocommit off. A connection lost, failing its
reset, past its lifetime or beyond `max_idle` is closed; one idle past
`idle_timeout`, or failing its check, closed as `get` meets it. The pool
starts no thread; each wait is the waiting thread's own:

Made beforehand, as the owner:

```sql
CREATE TABLE T (K INTEGER);
```

```cpp
#include <turborel/turborel.hpp>

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

int main() {
  using namespace std::chrono_literals;
  turborel::pool_config config;
  config.max = 3;
  config.wait = 5s;
  turborel::pool pool =
      turborel::pool::of(turborel::settings::from_uri("turborel://ann@127.0.0.1:17439/?tls_mode=disable"), config);
  std::vector<std::thread> workers;
  for (int t = 0; t < 8; ++t) {
    workers.emplace_back([&pool] {
      for (int round = 0; round < 5; ++round) {
        turborel::pooled conn = pool.get();
        static_cast<void>(conn->query("SELECT COUNT(*) FROM T"));
      }
    });
  }
  for (std::thread& worker : workers) {
    worker.join();
  }
  std::cout << pool.made() << " made, " << pool.idle() << " idle\n";
  {
    turborel::pooled conn = pool.get();
    static_cast<void>(conn->query("INSERT INTO T VALUES (1)"));
    conn->set_autocommit(true);
  }
  turborel::pooled again = pool.get();
  std::cout << "given back reset: active " << std::boolalpha << again->in_transaction() << ", autocommit "
            << again->autocommit() << ", " << again->query("SELECT COUNT(*) FROM T").at(0).at(0).to_string()
            << " rows\n";
}
```

```
3 made, 3 idle
given back reset: active false, autocommit false, 0 rows
```

## The SQL-agent

A `turborel::agent` holds a program's connections, each by its name, one
of them current, as the SQL-agent of SQL's standard does. CONNECT makes
one, through a connector the program gives, to the address and as the
user it names; SET CONNECTION makes another current; DISCONNECT ends
one; every other statement is sent to the current connection:

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
