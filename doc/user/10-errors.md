# Errors and Diagnostics

[Contents](README.md)

Every failure is thrown as a `turborel::error`, a `std::runtime_error`
that holds the whole failure: its SQLCODE and SQLSTATE, its message, the
rule it cites, where in the statement it was found, whether the
transaction is still active, and, granted `diagnostics`, more about it.
A statement's warnings come too, as conditions or notices, and its
completion tells what it did. This chapter shows each;
[Errors](../client/30-errors.md) gives the types, and [Appendix B:
Errors](../appendix/b-errors.md) every code the driver gives.

## A failure

`what()` reads as Turborel's own client writes a failure; `sqlcode()`,
`sqlstate()` and `message()` give its parts, and `details()` the whole
`turborel::failure`. With `diagnostics`, which a connection asks for,
its `diagnostics` say its severity, and the schema, table, column, data
type and constraint it is about, where known:

Made beforehand, as the owner:

```sql
CREATE TABLE STAFF (ID INTEGER CONSTRAINT STAFF_KEY PRIMARY KEY, NAME VARCHAR(20));
INSERT INTO STAFF VALUES (1, 'Ada');
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  try {
    static_cast<void>(conn.query("SELECT NAME FROM STAF"));
  } catch (const turborel::error& e) {
    std::cout << e.what() << '\n';
    std::cout << e.sqlcode() << ' ' << e.sqlstate() << ", rule " << e.details().rule.value_or("none")
              << ", at " << e.details().position.value_or(0) << '\n';
  }
  try {
    static_cast<void>(conn.query("INSERT INTO STAFF VALUES (1, 'Grace')"));
  } catch (const turborel::error& e) {
    const turborel::diagnostics told = e.details().diagnostics.value_or(turborel::diagnostics{});
    std::cout << e.sqlstate() << ' ' << told.severity.value_or("?") << ", table " << told.table.value_or("?")
              << ", constraint " << told.constraint.value_or("?") << ", active " << std::boolalpha
              << e.details().active << '\n';
  }
}
```

```
SQLCODE -103 (SQLSTATE 2A000): table ANN.STAF is not defined [X3.135-1986 §5.4 SR5] (at character 17)
-103 2A000, rule X3.135-1986 §5.4 SR5, at 17
23000 ERROR, table STAFF, constraint STAFF_KEY, active true
```

A failure of the session's own, as a server lost, a reply that cannot be
read or a wait that runs out, loses the connection, SQLCODE -919,
SQLSTATE 08006; every call on it after fails. A statement's failure
leaves the connection as it was, its transaction active or rolled back
as `details().active` says. Memory not had is thrown as `std::bad_alloc`,
as C++ throws it.

## What a statement tells

A statement's completion tells, granted the capabilities a connection
asks for, the rows it changed or a query returned, `rows_affected()`;
the identity values an INSERT generated, `generated()`; the server's
time for it, `server_time()`; and the log position of the commit it
made, `commit_position()`, of a database kept on disk. Its warnings come
one way: with `conditions`, in its completion, `conditions()`, the
completion's own SQLSTATE `warning()`; else as notices, to a handler of
`on_notice`, or the last 64 kept, `notices()`:

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

The server's own notices, as Reset's 01T01, always come as notices.
