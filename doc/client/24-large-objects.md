# Large Objects

[Contents](README.md)

CLOBs and BLOBs read whole from their row or by their locator, read as a
stream, freed and held, and written a piece at a time.

## Reading

### NAME

`turborel::read_lob`, `turborel::lob_reader`, `turborel::lob_range`,
`turborel::lob_info`.

### SYNOPSIS

```cpp
#include <turborel/lob.hpp>

namespace turborel {
inline constexpr std::size_t lob_chunk_bytes = 262144;
inline constexpr std::size_t lob_inline_bytes = 65536;

struct lob_info { bool character{}; bool located{}; std::uint64_t octets{}; std::uint64_t characters{}; };
struct lob_range { std::uint64_t from{}; std::uint64_t count{}; bool characters{}; };

std::size_t read_lob(connection& conn, const value& lob, lob_range range, std::span<std::byte> out);

class lob_reader : public std::streambuf {
public:
  lob_reader(connection& conn, const value& lob);
  std::size_t read(std::span<std::byte> out);
  std::vector<std::byte> read_all();
};
}
```

### DESCRIPTION

A large object of at most `lob_inline_bytes`, SERVER.LOB_INLINE_BYTES's
default, comes whole in its row; a longer one by its locator, its
`lob_info` saying whether it is a character one, whether it came by its
locator, and its octets and characters. `read_lob` reads a range of it
into `out`: of one whole, from the value; of one by its locator, from the
server by Lob read, at most `lob_chunk_bytes`; the range counts octets,
or a CLOB's whole characters where `characters` is set. It gives the
octets read, 0 past the end. A `lob_reader` reads it from its beginning
to its end, a chunk at a time, `read` into a buffer, `read_all` every
octet left, or as a `std::streambuf` a `std::istream` reads. A locator is
valid until its transaction ends; a result that sent one keeps its
transaction, under autocommit too.

### ERRORS

A locator freed, or past its transaction, fails with SQLCODE -301,
SQLSTATE 0F001. A value that is no large object, or of one whole freed
or held, fails with -301, 22000, as turborel-client's misuse of a large
object.

### EXAMPLES

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

## Locators

### NAME

`turborel::free_lob`, `turborel::hold_lob`.

### SYNOPSIS

```cpp
namespace turborel {
void free_lob(connection& conn, const value& lob);
void hold_lob(connection& conn, const value& lob);
}
```

### DESCRIPTION

`free_lob` frees a locator by Lob free, before its transaction ends.
`hold_lob` holds it past its transaction's commit by Lob hold, until it
is freed or a transaction is rolled back.

### ERRORS

As reading's.

### EXAMPLES

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

## Writing

### NAME

`turborel::lob_writer`: a new large object, written a piece at a time.

### SYNOPSIS

```cpp
namespace turborel {
class lob_writer {
public:
  lob_writer(connection& conn, bool character);
  lob_writer& write(std::span<const std::byte> octets);
  lob_writer& write(std::string_view text);
  void finish(params& values);
};
}
```

### DESCRIPTION

A writer creates an empty CLOB, or BLOB, on the server by Lob create,
and sends what is written to it by Lob append, a chunk at a time;
`finish` sends what is left, and adds the large object to `values` as
its next value, by its locator, valid until its transaction ends. A
CLOB's octets are UTF-8, a character free to be split between writes.

### ERRORS

A CLOB's octets that are not UTF-8 are refused by the Append that sends
them, SQLCODE -301, SQLSTATE 22021, the connection kept, and `values` is
as it was; a CLOB that ends within a character is refused where it is
used, 22021.

## STANDARD

The Native Protocol, version 1, its sections Lob 'L', the request and the
reply, and Values; The Client Library, its section Large objects.

## SEE ALSO

[Large Objects](../user/06-large-objects.md), [Values](23-values.md)
