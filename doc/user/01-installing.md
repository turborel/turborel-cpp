# Installing

[Contents](README.md)

This chapter tells what the driver needs, how to build and install it,
and how a program finds it. It ends with a first program.

No package of the driver is published yet. Its package, the CMake
package `turborel-cpp`, goes to archives on the repository's GitHub
releases once every Turborel driver is done ([Appendix D:
Versions](../appendix/d-versions.md)). Until then the driver is built
from its source.

## What the driver needs

- A C++20 compiler, GCC 13, Clang 18, Apple Clang or MSVC 19.40 or later,
  and CMake 3.25 or later, on Linux, macOS or Windows.
- turborel-c, the C driver it wraps, of the same Turborel release:
  found installed, or fetched at the commit the driver pins and built
  into the library. turborel-c brings TLS and SCRAM by OpenSSL 3.5 or
  later, or AWS-LC, and compressed frames by zlib; built without them, it
  refuses TLS and SCRAM, or compression, saying which library it lacks.
- A Turborel server of the same release, which speaks protocol
  version 1
  ([Running the Server](https://github.com/turborel/turborel/blob/main/doc/operator/02-turboreld.md)).

## Building from source

```
git clone https://github.com/turborel/turborel-cpp.git
cd turborel-cpp
cmake --workflow --preset release          # configure, build and test
cmake --install build/release --prefix /usr/local
```

The release workflow configures, builds and runs the tests. Its tests
against a live server run only when `TURBOREL_CPP_TEST_SERVER` names a
`turboreld` to start; otherwise they are skipped. [The CMake
Package](../client/02-api-cmake.md) gives the options that change what
is built. The install holds the library, its headers, the CMake package
`turborel-cpp`, and the turborel-c it was built with, where it fetched
one.

## Using the library

A CMake project finds the installed package and links its target, which
brings its headers, its C++20 requirement and turborel-c:

```cmake
find_package(turborel-cpp 0.0.0 EXACT CONFIG REQUIRED)
target_link_libraries(app PRIVATE turborel::turborel-cpp)
```

FetchContent or `add_subdirectory` of the source gives the same target.
A program includes `<turborel/turborel.hpp>`, or the header of each part
it uses, and everything is in the namespace `turborel`.

## A first program

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
