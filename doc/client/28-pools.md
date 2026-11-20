# Pools

[Contents](README.md)

## NAME

`turborel::pool`, `turborel::pooled`, `turborel::pool_config`: a bounded
set of connections a program's threads share.

## SYNOPSIS

```cpp
#include <turborel/pool.hpp>

namespace turborel {
inline constexpr std::size_t pool_max = 10;
inline constexpr std::chrono::minutes pool_idle_timeout{10};
inline constexpr std::chrono::hours pool_max_lifetime{1};
inline constexpr std::chrono::seconds pool_wait{30};
inline constexpr std::size_t pool_reset_most_statements = 256;

struct pool_config {
  std::size_t max = pool_max;
  std::size_t max_idle = pool_max;
  std::chrono::milliseconds idle_timeout = pool_idle_timeout;
  std::chrono::milliseconds max_lifetime = pool_max_lifetime;
  std::chrono::milliseconds wait = pool_wait;
  bool check = true;
  std::vector<std::string> reset;
};

class pooled {
public:
  connection& operator*() noexcept;
  connection* operator->() noexcept;
};

class pool {
public:
  using connector = std::function<connection()>;
  static pool of(settings how, const pool_config& config = pool_config{});
  static pool of(connector make, const pool_config& config = pool_config{});
  pooled get();
  std::size_t made() const;
  std::size_t idle() const;
};
}
```

## DESCRIPTION

A pool is made of settings, which it keeps, or of a function that makes a
connection; its copies are the same pool, for other threads. `get` hands
out an idle connection that answers its check, by Ping, or by ROLLBACK of
a server that grants no Ping; or makes one while fewer than `max` are
made; or waits for one to come back, at most `wait`. A `pooled` is used
as its connection, moved and not copied, and gives it back as it is
destroyed, reset to its login: by Reset, else, of a server that grants
none, by statements, one pipeline, of a connection with at most
`pool_reset_most_statements` statements left prepared; autocommit off;
then the `reset` statements. A connection lost, failing its reset, past
`max_lifetime` or beyond `max_idle` is closed as it comes back; one idle
past `idle_timeout`, or failing its check, closed as `get` meets it.
`made()` and `idle()` count the connections made and idle. A pool starts
no thread: each wait is the waiting thread's own, and a connector is
called on the thread that gets.

## ERRORS

`get` fails with SQLCODE -919, SQLSTATE 08001 if none comes back within
`wait`, or with the failure of making one; what a connector throws is
thrown as it was, and a connection it made over the program's own stream,
which a pool cannot keep, fails with -910, TB910. A `max` of 0, or a time
below zero or past 2^32 - 1 ms, fails `of` with -910, TB910.

## EXAMPLES

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

## STANDARD

The Client Library's Settings, Pipelines and Pools, its section Pools.

## SEE ALSO

[Pipelines and Pools](../user/08-pipelines-and-pools.md),
[Settings](26-settings.md)
