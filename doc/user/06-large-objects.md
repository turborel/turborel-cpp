# Large Objects

[Contents](README.md)

A CHARACTER LARGE OBJECT or a BINARY LARGE OBJECT comes whole in its row
when it is short, and by its *locator* when it is long, which reads it a
range or a chunk at a time while the transaction lasts. This chapter
shows both, a large object read as a stream, a locator held past its
commit or freed, and a new one written a piece at a time.
[Large Objects](../client/24-large-objects.md) gives each call in full.

## Whole, or by its locator

A large object of at most SERVER.LOB_INLINE_BYTES, 64 KiB by default,
comes whole: its `text()` or `bytes()` read it at once. A longer one
comes by its locator, and `lob()` says what it is: character or binary,
its octets and its characters. `read_lob` reads a range, of octets or of
a CLOB's characters, at most 256 KiB a call; a `turborel::lob_reader` is
a `std::streambuf` of the whole value, read a chunk at a time, so a
`std::istream` reads it. A locator is valid until its transaction ends:

Made beforehand, as the owner:

```sql
CREATE TABLE NOTES (ID INTEGER, BODY CLOB(10M));
INSERT INTO NOTES VALUES (1, 'short');
```

```cpp
#include <turborel/turborel.hpp>

#include <istream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  turborel::lob_writer writer{conn, true};
  for (int i = 0; i < 1000; ++i) {
    writer.write(std::string(99, 'x') + "\n");
  }
  turborel::params values;
  values.add(2);
  writer.finish(values);
  static_cast<void>(conn.query_with("INSERT INTO NOTES VALUES (?, ?)", values));
  static_cast<void>(conn.query("COMMIT"));
  const turborel::result notes = conn.query("SELECT BODY FROM NOTES ORDER BY ID");
  const turborel::value whole = notes.at(0).at(0);
  std::cout << "whole: " << std::boolalpha << !whole.lob().located << ", " << whole.text() << '\n';
  const turborel::value located = notes.at(1).at(0);
  const turborel::lob_info info = located.lob();
  std::cout << "by locator: " << info.located << ", " << info.octets << " octets, " << info.characters
            << " characters\n";
  std::vector<std::byte> first(10);
  const std::size_t got = turborel::read_lob(conn, located, {.from = 95, .count = 10}, first);
  std::cout << "read " << got << " octets from 95\n";
  turborel::lob_reader reader{conn, located};
  std::istream in{&reader};
  std::size_t lines = 0;
  for (std::string line; std::getline(in, line);) {
    ++lines;
  }
  std::cout << lines << " lines\n";
  static_cast<void>(conn.query("COMMIT"));
  try {
    static_cast<void>(turborel::read_lob(conn, located, {.from = 0, .count = 10}, first));
  } catch (const turborel::error& e) {
    std::cout << e.sqlstate() << ": " << e.message() << '\n';
  }
}
```

```
whole: true, short
by locator: true, 100000 octets, 100000 characters
read 10 octets from 95
1000 lines
0F001: 2 is not a valid locator
```

A result that holds a locator keeps its transaction, under autocommit
too, so the locator stays valid until COMMIT or ROLLBACK ends it.
`hold_lob` keeps it past the commit, until it is freed or a transaction
is rolled back, and `free_lob` frees it:

Made beforehand, as the owner:

```sql
CREATE TABLE IMAGES (ID INTEGER, PICTURE BLOB(10M));
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>
#include <vector>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  turborel::lob_writer picture{conn, false};
  const std::vector<std::byte> chunk(65536, std::byte{0x2A});
  for (int i = 0; i < 4; ++i) {
    picture.write(chunk);
  }
  turborel::params values;
  values.add(1);
  picture.finish(values);
  static_cast<void>(conn.query_with("INSERT INTO IMAGES VALUES (?, ?)", values));
  static_cast<void>(conn.query("COMMIT"));
  conn.set_autocommit(true);
  const turborel::result read = conn.query("SELECT PICTURE FROM IMAGES");
  std::cout << "active " << std::boolalpha << conn.in_transaction() << '\n';
  const turborel::value lob = read.at(0).at(0);
  turborel::hold_lob(conn, lob);
  static_cast<void>(conn.query("COMMIT"));
  turborel::lob_reader reader{conn, lob};
  std::cout << reader.read_all().size() << " octets after the commit\n";
  turborel::free_lob(conn, lob);
}
```

```
active true
262144 octets after the commit
```

## Writing one

A `turborel::lob_writer` creates an empty large object on the server and
sends what is written to it a chunk at a time; `finish(values)` adds it
to a statement's parameters by its locator, valid until its transaction
ends. A CLOB's octets are UTF-8, a character free to be split between
writes; octets that are not are refused by the server as they are sent,
SQLSTATE 22021, and a CLOB that ends within a character where it is
used. The examples above write theirs so.
