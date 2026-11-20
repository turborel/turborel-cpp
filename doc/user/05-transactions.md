# Transactions

[Contents](README.md)

A session's statements run in transactions. A transaction begins with
the first statement after the last ended, and lasts until COMMIT or
ROLLBACK; under autocommit each statement is its own. This chapter shows
both, how a session tells whether a transaction is active, and how the
session itself is put back as at login, given another user, or given
attributes every statement carries.

## Transactions and autocommit

`in_transaction()` says whether the session's transaction is active, as
the server said after the last statement. `set_autocommit(true)` makes
each statement its own transaction, committed as it succeeds and rolled
back as it fails, with no COMMIT sent; a statement whose commit fails,
as a deferred constraint broken, gives the commit's failure, and no
transaction is active after it:

Made beforehand, as the owner:

```sql
CREATE TABLE ACCOUNTS (ID INTEGER PRIMARY KEY, BALANCE DECIMAL(9,2) CONSTRAINT POSITIVE CHECK (BALANCE >= 0) INITIALLY DEFERRED);
INSERT INTO ACCOUNTS VALUES (1, 100.00), (2, 50.00);
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  std::cout << std::boolalpha;
  static_cast<void>(conn.query("UPDATE ACCOUNTS SET BALANCE = BALANCE - 30 WHERE ID = 1"));
  static_cast<void>(conn.query("UPDATE ACCOUNTS SET BALANCE = BALANCE + 30 WHERE ID = 2"));
  std::cout << "active " << conn.in_transaction() << '\n';
  static_cast<void>(conn.query("COMMIT"));
  std::cout << "active " << conn.in_transaction() << '\n';
  conn.set_autocommit(true);
  static_cast<void>(conn.query("UPDATE ACCOUNTS SET BALANCE = BALANCE + 1 WHERE ID = 1"));
  std::cout << "autocommit, active " << conn.in_transaction() << '\n';
  try {
    static_cast<void>(conn.query("UPDATE ACCOUNTS SET BALANCE = BALANCE - 500 WHERE ID = 2"));
  } catch (const turborel::error& e) {
    std::cout << e.sqlstate() << ": " << e.message() << '\n';
  }
  for (const turborel::row row : conn.query("SELECT ID, BALANCE FROM ACCOUNTS ORDER BY ID")) {
    std::cout << row.at(0).to_string() << ' ' << row.at(1).to_string() << '\n';
  }
}
```

```
active true
active false
autocommit, active false
40002: the transaction is rolled back: CHECK ANN.POSITIVE of ANN.ACCOUNTS is false for a row [ISO/IEC 9075:1992 §11.6 GR3]
1 71.00
2 80.00
```

A result that holds a large object's locator keeps its transaction, so
the locator stays valid, under autocommit too; a cursor open keeps it,
until its last row's fetch or its close ends it, with `fetch-ends`.
[Large Objects](06-large-objects.md) says more of locators.

As a session ends by `terminate()`, its transaction is committed or
rolled back as the session setting SESSION.COMMIT_AT_END says, ON by
default; a commit that fails gives its failure.

## The session

`reset()` puts the session back as it was at login, with `reset`: its
transaction rolled back, told by the notice 01T01, its prepared
statements, cursors, temporary tables and settings gone, its user kept.
`change_user(user, mechanism)` authenticates the connection again as
another user, whose new session it then is; a refusal ends the
connection. `set_query_attributes` sets attributes every later statement
carries, which SQL reads by QUERY_ATTRIBUTE(name), until set again:

Made beforehand, as the owner:

```sql
CREATE USER BOB PASSWORD 'bob''s secret';
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::local("turborel.sock"), "ann", trust);
  conn.set_query_attributes({{"report", "monthly"}});
  std::cout << conn.query("VALUES (QUERY_ATTRIBUTE('report'))").at(0).at(0).to_string() << '\n';
  static_cast<void>(conn.query("SET TIME ZONE INTERVAL '+02:00' HOUR TO MINUTE"));
  static_cast<void>(conn.query("VALUES 1"));
  std::cout << conn.setting("SESSION.TIME_ZONE").value_or("?") << ", active " << std::boolalpha
            << conn.in_transaction() << '\n';
  conn.reset();
  std::cout << conn.setting("SESSION.TIME_ZONE").value_or("?") << ", active " << conn.in_transaction() << '\n';
  for (const turborel::failure& notice : conn.notices()) {
    std::cout << "notice " << notice.sqlstate << ": " << notice.message << '\n';
  }
  turborel::mechanism bob = turborel::mechanism::scram("bob's secret");
  conn.change_user("bob", bob);
  std::cout << conn.authorization() << '\n';
}
```

```
monthly
+02:00, active true
+00:00, active false
notice 01T01: [DESIGN] Reset rolled back the transaction that was active
BOB
```
