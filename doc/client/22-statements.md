# Statements and Cursors

[Contents](README.md)

The statements a connection runs: queries and other statements whole,
with parameters, a batch at a time, or several as one request; prepared
statements and their cursors; and what a statement's completion tells.
Their values are in [Values](23-values.md).

## Running statements

### NAME

`connection::query`, `query_with`, `query_text`, `query_batches`,
`query_many`: a statement run by one call.

### SYNOPSIS

```cpp
#include <turborel/connection.hpp>

namespace turborel {
class connection {
public:
  result query(const std::string& sql);
  template <typename First, typename... Rest>
  result query(const std::string& sql, const First& first, const Rest&... values);
  result query_with(const std::string& sql, const params& values);
  result query_text(const std::string& sql, const std::vector<std::optional<std::string>>& values);
  result query_batches(const std::string& sql, const std::function<void(const result&)>& handler);
  ran query_many(const std::string& sql);
  void set_autocommit(bool on);
  bool autocommit() const noexcept;
  bool in_transaction() const noexcept;
};

class ran {
public:
  std::size_t size() const noexcept;
  result at(std::size_t index) const;
  std::optional<failure> stopped_by() const;
};
}
```

### DESCRIPTION

`query(sql)` runs one statement by Query: a query's Description, its
Rows and its Complete; another statement's Complete; a failure's Error.
`query(sql, values...)` and `query_with(sql, params)` prepare the
statement, execute it with the values, and deallocate it, so the
completion the connection tells after them is the deallocation's.
`query_text` does the same with each value given as text, which the
server assigns to its parameter's type, as SQL's CAST of the text does,
an empty optional the null value; the completion the connection tells
after it is the statement's own. `query_batches` gives a query's rows to
the handler a batch at a time, on the calling thread, as they come;
what the handler throws is thrown once the statement's replies are
read, and it is given no more batches after. `query_many` runs a text of
several statements as one request, with `multi-statement`, each in
order until one fails; `ran` holds each outcome, and the failure that
stopped the rest.

`set_autocommit(true)` makes each statement its own transaction, sent
with `ends` set: committed as it succeeds, rolled back as it fails, the
reply saying no transaction is active, with no COMMIT sent; off at
first. `in_transaction()` says whether the session's transaction is
active, as the server said after the last statement. A result that holds
a locator keeps its transaction, under autocommit too.

### ERRORS

A statement's failure is thrown, the server's own ([Errors](30-errors.md));
a commit that fails under autocommit gives the commit's. `query_many`
without `multi-statement` fails with SQLCODE -919, SQLSTATE 08006,
before sending. A value the client cannot take, in a reply whose frame
is whole, fails the statement with SQLCODE -916, SQLSTATE TB916, the rest
of its replies read past and the connection kept.

### EXAMPLES

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

## Results

### NAME

`turborel::result`, `turborel::row`: a statement's outcome, a range of
rows.

### SYNOPSIS

```cpp
#include <turborel/result.hpp>

namespace turborel {
class row {
public:
  std::size_t size() const noexcept;
  value at(std::size_t column) const;
  template <typename T> T get(std::size_t column) const;
};

class result {
public:
  class iterator;  // random access, of rows
  bool has_rows() const noexcept;
  std::vector<column> columns() const;
  std::size_t size() const noexcept;
  bool empty() const noexcept;
  row at(std::size_t index) const;
  iterator begin() const noexcept;
  iterator end() const noexcept;
  std::int32_t sqlcode() const noexcept;
  std::string_view sqlstate() const noexcept;
  bool more() const noexcept;
};
}
```

### DESCRIPTION

A `result` owns what a statement gave: a query's columns and rows,
`has_rows()` true, or another statement's completion. Its rows are a
`std::ranges::random_access_range`, each a `row`, a view valid while the
result is, whose values are read by `at(i)` or `get<T>(i)`. `sqlcode()`
and `sqlstate()` are its completion's: 0 and 00000, a warning's, or 100
and 02000 of a query that returned no row. `more()` says whether a
cursor's batch has rows after it. A result is moved, not copied.

### ERRORS

`at(i)` past the last row fails with SQLCODE -910, SQLSTATE TB910.

## Prepared statements

### NAME

`connection::prepare`, `execute`, `describe`, `deallocate`;
`turborel::statement`.

### SYNOPSIS

```cpp
namespace turborel {
class connection {
public:
  statement prepare(const std::string& sql);
  result execute(const statement& prepared, const params& values = params{});
  statement describe(const statement& prepared);
  void deallocate(const statement& prepared);
};

class statement {
public:
  std::size_t parameter_count() const noexcept;
  data_type parameter_type(std::size_t index) const;
  std::vector<column> columns() const;
};
}
```

### DESCRIPTION

`prepare` prepares a statement by Prepare, its parameters written `?`,
each statement prepared taking a number of its own, so none whose cursor
is open is replaced. A `statement` tells how many parameters it takes,
its inputs alone, the type of each, and its columns: a query's, or of a
CALL one for each `?` of an OUT or INOUT parameter, in the procedure's
order; none of another statement. `execute` runs it by Execute with its
values: a query's rows, a CALL's output parameters as one row, another
statement's completion. A statement is valid across transactions, until
it is deallocated; `describe` describes it again, with `describe`;
`deallocate` destroys it in the session.

### ERRORS

A statement deallocated, executed or deallocated again fails with
SQLCODE -921, SQLSTATE 26000; one whose cursor is open is not
deallocated, -501, 24000. `parameter_type` past the last fails with -910,
TB910.

### EXAMPLES

Made beforehand, as the owner:

```sql
CREATE TABLE STAFF (ID INTEGER PRIMARY KEY, NAME VARCHAR(20));
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>
#include <string>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  const turborel::statement insert = conn.prepare("INSERT INTO STAFF VALUES (?, ?)");
  for (std::size_t i = 0; i < insert.parameter_count(); ++i) {
    std::cout << "parameter " << i << ": " << insert.parameter_type(i).to_string() << '\n';
  }
  int id = 0;
  for (const char* name : {"Ada", "Grace", "Edsger"}) {
    static_cast<void>(conn.execute(insert, turborel::params::of(++id, name)));
  }
  static_cast<void>(conn.query("COMMIT"));
  const turborel::statement select = conn.prepare("SELECT NAME FROM STAFF WHERE ID = ?");
  std::cout << select.columns().at(0).name.value_or("?") << ": "
            << conn.execute(select, turborel::params::of(2)).at(0).get<std::string>(0) << '\n';
  conn.deallocate(select);
  try {
    static_cast<void>(conn.execute(select, turborel::params::of(2)));
  } catch (const turborel::error& e) {
    std::cout << e.what() << '\n';
  }
}
```

```
parameter 0: NUMERIC(10,0)
parameter 1: CHARACTER VARYING(20)
NAME: Grace
SQLCODE -921 (SQLSTATE 26000): there is no prepared statement 1 [ISO/IEC 9075:1992 §17.10 GR1]
```

## Cursors

### NAME

`turborel::cursor`: a query's rows read a batch at a time.

### SYNOPSIS

```cpp
#include <turborel/cursor.hpp>

namespace turborel {
inline constexpr std::uint32_t default_batch_rows = 1000;

class cursor {
public:
  static cursor open(connection& conn, const statement& prepared, const params& values = params{},
                     std::uint32_t rows = default_batch_rows, std::uint32_t number = 0);
  const result& batch() const noexcept;
  bool more() const noexcept;
  const result& fetch(std::uint32_t rows = default_batch_rows);
  void close();
  result describe();
  iterator begin() noexcept;          // an input iterator of rows
  static std::default_sentinel_t end() noexcept;
};
}
```

### DESCRIPTION

`open` executes a query's statement and keeps its cursor open, its first
batch at most `rows` rows, Suspended while the cursor has more, else
Complete, the cursor closed. `fetch` reads the next batch by Fetch;
`more()` says whether the cursor has rows it has not fetched; `close`
closes it by Close, as its destructor does. Cursor 0 is a statement's
own; with `cursors`, `number` opens another of the same statement, so
several are open at once. `describe` gives an open cursor's columns, with
`describe`. A cursor is a `std::ranges::input_range` of its rows, each
batch fetched as the last row of the one before is passed; a row is
valid until the iterator moves past its batch. Under autocommit with
`fetch-ends`, the fetch of the last row commits, close commits, and rows
that fail roll the transaction back.

### ERRORS

A cursor already open, of the statement and number, fails with SQLCODE
-501, SQLSTATE 24000; one not open fails `fetch` and `close` likewise. A
number other than 0 without `cursors` fails with -919, 08006, before
sending.

### EXAMPLES

```cpp
#include <turborel/turborel.hpp>

#include <cstdint>
#include <iostream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  const turborel::statement numbers = conn.prepare(
      "WITH RECURSIVE T(N) AS (VALUES CAST(1 AS INTEGER) UNION ALL SELECT N + 1 FROM T WHERE N < 10) SELECT N FROM T");
  turborel::cursor read = turborel::cursor::open(conn, numbers, turborel::params{}, 4);
  std::cout << "first " << read.batch().size() << ", more " << std::boolalpha << read.more() << '\n';
  while (read.more()) {
    std::cout << "fetched " << read.fetch(4).size() << '\n';
  }
  std::int64_t sum = 0;
  for (const turborel::row row : turborel::cursor::open(conn, numbers, turborel::params{}, 3)) {
    sum += row.get<std::int64_t>(0);
  }
  std::cout << "sum " << sum << '\n';
  turborel::cursor one = turborel::cursor::open(conn, numbers, turborel::params{}, 2, 1);
  turborel::cursor two = turborel::cursor::open(conn, numbers, turborel::params{}, 5, 2);
  std::cout << "cursor 1 holds " << one.batch().size() << ", cursor 2 " << two.batch().size() << '\n';
  std::cout << two.describe().columns().at(0).name.value_or("?") << '\n';
  one.close();
  two.close();
}
```

```
first 4, more true
fetched 4
fetched 2
sum 55
cursor 1 holds 2, cursor 2 5
N
```

## What a statement tells

### NAME

`connection::rows_affected`, `generated`, `server_time`,
`commit_position`, `warning`, `conditions`: what the last statement's
completion told.

### SYNOPSIS

```cpp
namespace turborel {
class connection {
public:
  std::optional<std::uint64_t> rows_affected() const noexcept;
  std::vector<std::string_view> generated() const;
  std::optional<timing> server_time() const noexcept;
  std::optional<std::uint64_t> commit_position() const noexcept;
  std::optional<std::string_view> warning() const noexcept;
  std::vector<failure> conditions() const;
  void clear_conditions() noexcept;
};
}
```

### DESCRIPTION

Each is of the last statement the connection completed, with the
capability that tells it: the rows it changed, or a query returned in
all, with `row-counts`; each identity value an INSERT generated, as SQL
writes it, in the order of its rows, with `generated-values`; the
server's time for it, total, queued and waiting for locks, with
`row-counts`; the log position of the commit it made, with
`commit-position`, of a database kept on disk. `warning()` is the
SQLSTATE of its completion if a warning; `conditions()` each condition
it raised, with `conditions`, which `clear_conditions` drops so each is
told once. Without `conditions`, the warnings come as notices
([Connections](21-connections.md)).

### EXAMPLES

Made beforehand, as the owner:

```sql
CREATE TABLE ORDERS (ID INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY, ITEM VARCHAR(5));
CREATE PROCEDURE WARNS () MODIFIES SQL DATA SIGNAL SQLSTATE '01ABC';
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  static_cast<void>(conn.query("INSERT INTO ORDERS (ITEM) VALUES ('tea'), ('jam')"));
  std::cout << conn.rows_affected().value_or(0) << " rows, ids";
  for (const std::string_view id : conn.generated()) {
    std::cout << ' ' << id;
  }
  std::cout << '\n' << "timed " << std::boolalpha << conn.server_time().has_value() << '\n';
  const turborel::statement insert = conn.prepare("INSERT INTO ORDERS (ITEM) VALUES (?)");
  static_cast<void>(conn.execute(insert, turborel::params::of("marmalade")));
  std::cout << "warning " << conn.warning().value_or("none") << ": " << conn.conditions().at(0).message << '\n';
  static_cast<void>(conn.query("CALL WARNS()"));
  for (const turborel::failure& condition : conn.conditions()) {
    std::cout << "condition " << condition.sqlstate << '\n';
  }
  turborel::connect_options no_conditions;
  no_conditions.ask_none().ask(turborel::capability::notices).ask(turborel::capability::diagnostics);
  turborel::connection noticed =
      turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust, no_conditions);
  noticed.on_notice([](const turborel::failure& notice) {
    std::cout << "notice " << notice.sqlstate << ' '
              << notice.diagnostics.value_or(turborel::diagnostics{}).severity.value_or("?") << '\n';
  });
  static_cast<void>(noticed.query("CALL WARNS()"));
  static_cast<void>(conn.query("ROLLBACK"));
}
```

```
2 rows, ids 1 2
timed true
warning 01004: the statement completed with warning: string data, right truncation
condition 01ABC
notice 01ABC WARNING
```

## STANDARD

The Native Protocol, version 1, its sections Query 'Q', Prepare 'P',
Execute 'E', Fetch 'F', Close 'C', Deallocate 'D', Describe 'Y',
Prepared 'P', Description 'D', Rows 'B', Suspended 'S', Complete 'K' and
Conversations; The Client Library, its sections Running statements,
Prepared statements and What the server tells.

## SEE ALSO

[Running Queries](../user/03-queries.md),
[Prepared Statements](../user/04-prepared-statements.md),
[Transactions](../user/05-transactions.md), [Values](23-values.md)
