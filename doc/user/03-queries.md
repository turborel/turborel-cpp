# Running Queries

[Contents](README.md)

A statement runs by one call of the connection, and gives its outcome as
a `turborel::result`: a query's columns and rows, or another statement's
completion. This chapter shows statements run whole, with parameters,
their rows read as they come, and several statements as one request.
[Statements and Cursors](../client/22-statements.md) gives each call in
full, and [Values](../client/23-values.md) each value's C++ type.

## A statement and its result

`query(sql)` runs one statement. A query's result has its columns, each
a name and a type, and its rows, which the result holds whole; it is a
range of rows, read in a range-based `for` or by the standard library's
algorithms, each row's values read by `get<T>(i)` as the C++ type asked
for. Another statement's result is its completion: SQLCODE 0 and SQLSTATE
00000, or a warning's. A query that returns no row completes with
SQLCODE 100, SQLSTATE 02000:

Made beforehand, as the owner:

```sql
CREATE TABLE STAFF (ID INTEGER PRIMARY KEY, NAME VARCHAR(20), JOINED DATE);
```

```cpp
#include <turborel/turborel.hpp>

#include <cstdint>
#include <iostream>
#include <string_view>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  const turborel::result inserted = conn.query(
      "INSERT INTO STAFF VALUES (1, 'Ada', DATE '2024-03-01'), (2, 'Grace', DATE '2025-06-15'), (3, 'Edsger', NULL)");
  std::cout << inserted.sqlcode() << ' ' << inserted.sqlstate() << ", " << conn.rows_affected().value_or(0)
            << " rows\n";
  const turborel::result staff = conn.query("SELECT ID, NAME, JOINED FROM STAFF ORDER BY ID");
  for (const turborel::column& column : staff.columns()) {
    std::cout << column.name.value_or("?") << ' ' << column.type_written << '\n';
  }
  for (const turborel::row row : staff) {
    std::cout << row.get<std::int64_t>(0) << ' ' << row.get<std::string_view>(1) << ' '
              << row.at(2).to_string() << '\n';
  }
  const turborel::result none = conn.query("SELECT ID FROM STAFF WHERE ID > 9");
  std::cout << none.size() << " rows, " << none.sqlcode() << ' ' << none.sqlstate() << '\n';
  static_cast<void>(conn.query("COMMIT"));
}
```

```
0 00000, 3 rows
ID INTEGER
NAME CHARACTER VARYING(20)
JOINED DATE
1 Ada 2024-03-01
2 Grace 2025-06-15
3 Edsger NULL
0 rows, 100 02000
```

A statement's failure is thrown, as [Errors and Diagnostics](10-errors.md)
shows; the connection goes on, its transaction as the server left it.

## Parameters

A statement's `?` marks take values: `query(sql, values...)` prepares
the statement, executes it with the values given, each of the C++ type
of its kind, and deallocates it. `query_text(sql, texts)` takes each
value as text, the server assigning it to its parameter's type, as SQL's
CAST of the text does, an empty optional being the null value:

Made beforehand, as the owner:

```sql
CREATE TABLE STAFF (ID INTEGER PRIMARY KEY, NAME VARCHAR(20));
INSERT INTO STAFF VALUES (1, 'Ada'), (2, 'Grace'), (3, 'Edsger');
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>
#include <optional>
#include <string>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  for (const turborel::row row : conn.query("SELECT NAME FROM STAFF WHERE ID >= ? AND NAME <> ? ORDER BY ID", 2, "Edsger")) {
    std::cout << row.get<std::string>(0) << '\n';
  }
  const turborel::result typed =
      conn.query_text("VALUES (CAST(? AS INTEGER) * 2, CAST(? AS VARCHAR(3)))", {std::string{"21"}, std::nullopt});
  std::cout << typed.at(0).at(0).to_string() << ' ' << typed.at(0).at(1).to_string() << '\n';
}
```

```
Grace
42 NULL
```

A statement run with its values by `query` is prepared, executed and
deallocated, so what the connection tells of the last statement after
it, its rows affected, generated values and conditions, is the
deallocation's; after `query_text` it is the statement's own. A program
that wants an INSERT's with values of their C++ types prepares and
executes it, as [Prepared Statements](04-prepared-statements.md) shows.

## Rows as they come

A query's rows come in batches. `query_batches(sql, handler)` gives each
batch to the handler as it comes, so a query of many rows is read in
bounded memory; the result it returns holds the columns and the
completion, and no rows. What the handler throws is thrown once the
statement's replies are read, and it is given no more batches after:

```cpp
#include <turborel/turborel.hpp>

#include <cstddef>
#include <iostream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  std::size_t rows = 0;
  std::size_t batches = 0;
  const turborel::result done = conn.query_batches(
      "WITH RECURSIVE T(N) AS (VALUES CAST(1 AS INTEGER) UNION ALL SELECT N + 1 FROM T WHERE N < 100000) "
      "SELECT N FROM T",
      [&](const turborel::result& batch) {
        rows += batch.size();
        ++batches;
      });
  std::cout << rows << " rows in " << (batches > 1 ? "several batches" : "one batch") << ", "
            << done.columns().size() << " column\n";
}
```

```
100000 rows in several batches, 1 column
```

## Several statements as one request

`query_many(sql)` runs a text of several statements, apart by `;`, as one
request, with `multi-statement`: each in order, until one fails. Its
`ran` gives each statement's result, and `stopped_by()` the failure that
stopped the rest:

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  const turborel::ran ran = conn.query_many("CREATE TABLE T (K INTEGER); INSERT INTO T VALUES (1); SELECT NOPE FROM T; VALUES 2");
  std::cout << ran.size() << " ran\n";
  for (std::size_t i = 0; i < ran.size(); ++i) {
    std::cout << i << ": " << ran.at(i).sqlstate() << '\n';
  }
  if (const auto stopped = ran.stopped_by()) {
    std::cout << "stopped by " << stopped->sqlstate << ": " << stopped->message << '\n';
  }
}
```

```
2 ran
0: 00000
1: 00000
stopped by 2A000: no table in the FROM clause has a column NOPE
```

## Cancelling

A statement runs until it completes, unless another thread cancels it
by a `turborel::canceller`, as [Connecting](02-connecting.md#ending-and-cancelling)
shows; it then fails with SQLCODE -920, SQLSTATE TB920.
