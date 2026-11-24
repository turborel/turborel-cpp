# Prepared Statements

[Contents](README.md)

A statement prepared once runs as many times as a program wants, with
new values each time, across transactions, until it is deallocated. A
query's rows may be read through a cursor, a batch at a time. This
chapter shows preparing and executing, cursors, numbered cursors, a
CALL's output parameters, and the types a statement's columns were
declared with. [Statements and Cursors](../client/22-statements.md)
gives each call in full.

## Prepare, execute, deallocate

`prepare(sql)` prepares a statement, its dynamic parameters written `?`,
and gives a `turborel::statement`: the type each parameter takes, and a
query's columns. `execute(statement, values)` runs it with a
`turborel::params` of its values, made by `params::of(values...)` or by
`add` a value at a time. `deallocate` destroys it in the session; one
deallocated is no more, SQLSTATE 26000:

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

`cursor::open(conn, statement, values, rows)` executes a query and keeps
its cursor open, its first `rows` rows its first batch. `fetch(rows)`
reads the next batch, `more()` says whether the cursor has more, and
`close()` closes it, as the cursor's destructor does. A cursor is an
input range of its rows, each batch fetched as the last is read.

By default a statement has one cursor, number 0. With `cursors`, several
of one statement are open at once, each by a number of the program's,
and `describe()` gives an open cursor's columns:

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

A cursor open keeps its transaction: under autocommit, the last row's
fetch commits, and closing it commits, as [Transactions](05-transactions.md)
says. A statement whose cursor is open is not deallocated, SQLSTATE
24000.

## A CALL's output parameters

A CALL's statement takes its input parameters alone; each `?` of an OUT
or INOUT parameter is a column of its, in the procedure's order, and its
execution gives one row of their values:

Made beforehand, as the owner:

```sql
CREATE PROCEDURE TWICE (IN X INTEGER, OUT Y INTEGER) CONTAINS SQL BEGIN SET Y = X * 2; END;
```

```cpp
#include <turborel/turborel.hpp>

#include <cstdint>
#include <iostream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  const turborel::statement call = conn.prepare("CALL TWICE(?, ?)");
  std::cout << call.parameter_count() << " input, " << call.columns().size() << " output\n";
  const turborel::result out = conn.execute(call, turborel::params::of(21));
  std::cout << out.at(0).get<std::int64_t>(0) << '\n';
}
```

```
1 input, 1 output
42
```

Granted `parameter-modes`, which a connection asks for, `markers` gives
every `?` of a CALL, its OUT ones too, each with its parameter's mode
and ordinal position; an IN or INOUT one has its place among the values
`execute` takes, an OUT or INOUT one the column of the row that gives
its value back:

Made beforehand, as the owner:

```sql
CREATE PROCEDURE MIXED (OUT TOTAL INTEGER, IN TIMES INTEGER, INOUT TAG VARCHAR(10)) CONTAINS SQL BEGIN SET TOTAL = TIMES * 2; SET TAG = TAG || 'B'; END;
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  const turborel::statement mixed = conn.prepare("CALL MIXED(?, ?, ?)");
  for (const turborel::marker& marker : mixed.markers()) {
    const bool out = marker.mode == turborel::parameter_mode::out;
    const bool inout = marker.mode == turborel::parameter_mode::inout;
    std::cout << (out ? "OUT" : inout ? "INOUT" : "IN") << " of parameter " << marker.position.value_or(0) << '\n';
  }
  const turborel::result back = conn.execute(mixed, turborel::params::of(2, "A"));
  for (const turborel::marker& marker : mixed.markers()) {
    if (marker.column) {
      std::cout << back.at(0).at(*marker.column).to_string() << '\n';
    }
  }
}
```

```
OUT of parameter 1
IN of parameter 2
INOUT of parameter 3
4
AB
```

## Declared types

A column's type, as its values' tag says, writes an INTEGER as
NUMERIC(10,0). Granted `declared-types`, which a connection asks for, a
column also has the type it was declared with: `declared_name`, the
Information Schema's DATA_TYPE, and `type_written`, the type as SQL
writes it:

Made beforehand, as the owner:

```sql
CREATE TABLE PRICES (ID INTEGER, QTY SMALLINT, PRICE DECIMAL(9,2), RATE DOUBLE PRECISION);
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  for (const turborel::column& column : conn.prepare("SELECT ID, QTY, PRICE, RATE FROM PRICES").columns()) {
    std::cout << column.name.value_or("?") << ": " << column.declared_name.value_or("?") << ", "
              << column.type_written << ", as its tag says " << column.type.to_string() << '\n';
  }
}
```

```
ID: INTEGER, INTEGER, as its tag says NUMERIC(10,0)
QTY: SMALLINT, SMALLINT, as its tag says NUMERIC(5,0)
PRICE: DECIMAL, DECIMAL(9,2), as its tag says NUMERIC(9,2)
RATE: DOUBLE PRECISION, DOUBLE PRECISION, as its tag says FLOAT(53)
```
