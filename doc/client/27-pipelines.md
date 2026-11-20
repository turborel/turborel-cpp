# Pipelines

[Contents](README.md)

## NAME

`turborel::pipeline`, `turborel::on_error`, `turborel::piped`: requests
sent before their answers come.

## SYNOPSIS

```cpp
#include <turborel/pipeline.hpp>

namespace turborel {
inline constexpr std::size_t pipeline_window = 65536;
enum class on_error : std::uint8_t { keep_going, skip };
enum class piped : std::uint8_t { done, failed, skipped };

class pipeline {
public:
  explicit pipeline(connection& conn);
  pipeline& query(const std::string& sql);
  pipeline& execute(const statement& prepared, const params& values = params{});
  pipeline& when_one_fails(on_error what);
  std::size_t size() const noexcept;
  void run();
  piped outcome(std::size_t request) const noexcept;
  result result_of(std::size_t request) const;
  std::optional<failure> failure_of(std::size_t request) const;
};
}
```

## DESCRIPTION

A pipeline queues queries and executions of prepared statements, which
`run` sends at once, a window of `pipeline_window` bytes at a time, the
window's replies read in turn before the next is written, so neither
the client nor the server waits on the other. Each request has its
outcome: `done`, its `result_of`; `failed`, its `failure_of`; or
`skipped`. Of `on_error::keep_going`, the default, a failure is its
request's own and the requests after it run; under autocommit each
request is its own transaction. Of `on_error::skip`, a savepoint goes
before each request but the first, so the requests after the first
failure are rolled back and skipped, those not yet written never sent;
the batch is one transaction, committed as it ends under autocommit
where it began none, and within a transaction its failure keeps the
transaction's earlier work. A pipeline runs once; a result it gives is
valid while it is.

## ERRORS

`run` of a pipeline already run, or a request added after, fails with
SQLCODE -910, SQLSTATE TB910; a connection lost as it runs fails it with
-919, 08006, as does a skipping batch that cannot be undone. `result_of`
of a request that did not run fails with -910, TB910.

## EXAMPLES

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

## STANDARD

The Native Protocol, version 1, its section Requests; The Client
Library's Settings, Pipelines and Pools, its section Pipelines.

## SEE ALSO

[Pipelines and Pools](../user/08-pipelines-and-pools.md),
[Statements and Cursors](22-statements.md)
