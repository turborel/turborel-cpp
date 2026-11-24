# The turborel Namespace

[Contents](README.md)

## NAME

`turborel`: the driver's namespace, its headers, and how its standard
interface is built.

## SYNOPSIS

```cpp
#include <turborel/turborel.hpp>      // every header below

#include <turborel/connect.hpp>       // targets, TLS, mechanisms, the key cache, options
#include <turborel/connection.hpp>    // connections and cancellers
#include <turborel/statement.hpp>     // prepared statements, several statements run
#include <turborel/result.hpp>        // results, rows, columns
#include <turborel/cursor.hpp>        // cursors
#include <turborel/value.hpp>         // values, types, kinds
#include <turborel/params.hpp>        // parameters
#include <turborel/decimal.hpp>       // exact numbers, with int128.hpp
#include <turborel/datetime.hpp>      // datetimes, with interval.hpp
#include <turborel/uuid.hpp>          // UUIDs
#include <turborel/xml.hpp>           // XML nodes
#include <turborel/lob.hpp>           // large objects
#include <turborel/copy.hpp>          // copy and export
#include <turborel/import.hpp>        // import
#include <turborel/settings.hpp>      // options and settings
#include <turborel/pipeline.hpp>      // pipelines
#include <turborel/pool.hpp>          // pools
#include <turborel/agent.hpp>         // the SQL-agent
#include <turborel/debugger.hpp>      // the routine debugger
#include <turborel/error.hpp>         // errors and failures
```

## DESCRIPTION

C++ has no standard interface to a database, so the driver's standard
interface is the language's own idioms, as its standard library writes
them, over turborel-c, the C driver it wraps:

- Each of turborel-c's handles is owned by a C++ object that frees it in
  its destructor, moved and not copied: a connection closes, a cursor
  closes, a copy not finished is abandoned, a mechanism's password and a
  canceller's key are cleared.
- Every failure is thrown as `turborel::error`, a `std::runtime_error`
  holding the whole failure, so a call either does what it says or
  throws; memory not had is `std::bad_alloc` ([Errors](30-errors.md)).
- A result is a `std::ranges::random_access_range` of its rows, and a
  cursor a `std::ranges::input_range`, read as its batches come; each
  value is read as its C++ type by `get<T>()`.
- Datetimes are `std::chrono` types, and an exact number, units of 128
  bits and a scale, a decimal type of the driver's own, since the
  standard has none that holds them all ([Values](23-values.md)).
- The interface takes and gives `std::span` and `std::string_view`, and
  owns what it keeps; a view it gives, as a row, a value or a column's
  name, is valid while what it came from is.
- What a program's own code throws in a call the driver makes of it, a
  handler, a stream, a source or a connector, is thrown as it was once
  the driver's call returns.

The low-level client beneath it is the rest of this volume, a page for
each part of Turborel's client library. During the alpha no ABI is kept
between releases.

## EXAMPLES

Made beforehand, as the owner:

```sql
CREATE TABLE STAFF (ID INTEGER PRIMARY KEY, NAME VARCHAR(20));
INSERT INTO STAFF VALUES (1, 'Ada'), (2, 'Grace'), (3, 'Edsger');
```

```cpp
#include <turborel/turborel.hpp>

#include <cstdint>
#include <iostream>
#include <string_view>

int main() {
  try {
    turborel::connection conn = turborel::connection::open("turborel://ann@127.0.0.1:17439/?tls_mode=disable");
    for (const turborel::row row : conn.query("SELECT ID, NAME FROM STAFF WHERE ID < ? ORDER BY ID", 3)) {
      std::cout << row.get<std::int64_t>(0) << ' ' << row.get<std::string_view>(1) << '\n';
    }
  } catch (const turborel::error& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
```

```
1 Ada
2 Grace
```

## STANDARD

The driver's own (RULES.md D2), over The Native Protocol, version 1,
and The Client Library.

## SEE ALSO

[The CMake Package](02-api-cmake.md), [Installing](../user/01-installing.md)
