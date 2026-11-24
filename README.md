<p align="center">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="artwork/turborel-logo-white.svg">
      <img src="artwork/turborel-logo.svg" alt="turborel" width="360">
    </picture>
</p>

> turborel-cpp is in alpha, meaning, version 0.0.0. Its version is the
> number of the Turborel release it is made for, and the alpha is made
> for Turborel 0.0.0, which speaks protocol version 1. Anything may
> change from one build to the next, API and behaviour alike. See
> [VERSIONING.md](VERSIONING.md). The latest Turborel alpha is built for
> Linux, macOS and Windows as the pre-release
> [nightly](https://github.com/turborel/turborel/releases/tag/nightly),
> and as the image `ghcr.io/turborel/turborel:nightly`.

turborel-cpp is the C++ driver for
[Turborel](https://github.com/turborel/turborel), a relational database
system. It connects a C++ program to a Turborel server,
`turboreld`, over Turborel's [native
protocol](https://github.com/turborel/turborel/blob/main/doc/client/05-protocol.md),
version 1, which it speaks through turborel-c, the C driver it wraps, with no protocol code of its own.

Its standard interface is RAII C++20 over turborel-c: owners that close what they hold, failures thrown as exceptions, a query's rows as a range, datetimes in `std::chrono`. Beneath it is a low-level
client built to do all that Turborel's own client library,
turborel-client, does: TCP, Unix sockets and Windows named pipes, TLS,
SCRAM-SHA-256 and SCRAM-SHA-256-PLUS, prepared statements and cursors,
large objects, bulk copy, import and export, settings from a URI, the
environment and service and password files, several servers, pipelines
and pools, and the routine debugger.

[CONFORMANCE.md](CONFORMANCE.md) shows what it does, row by row, against
every Turborel driver's parity matrix, and [TODO.md](TODO.md) what is
open.

## Requirements

- a C++20 compiler, GCC 13, Clang 18, Apple Clang or MSVC 19.40 or later, and CMake 3.25 or later, on Linux, macOS or Windows ([RULES.md](RULES.md) D3).
- turborel-c, of the same Turborel release, which brings the TLS library and zlib it needs: found installed, or fetched at a pinned commit and built with the driver ([RULES.md](RULES.md) D11).
- A Turborel server of the same release, which speaks protocol version 1
  ([running the server](https://github.com/turborel/turborel/blob/main/doc/operator/02-turboreld.md)).

## Installing

No package is published yet. The driver's package, the CMake package `turborel-cpp`, goes to
the repository's GitHub releases, as archives only once every Turborel driver is done, with the others
([VERSIONING.md](VERSIONING.md#publishing)). Until then, build the
driver from source:

```
git clone https://github.com/turborel/turborel-cpp.git
cd turborel-cpp
cmake --workflow --preset release
cmake --install build/release --prefix /usr/local
```

A CMake project then finds the installed package and links its target:

```cmake
find_package(turborel-cpp 0.0.0 EXACT CONFIG REQUIRED)
target_link_libraries(app PRIVATE turborel::turborel-cpp)
```

FetchContent or `add_subdirectory` of the source gives the same target.

## A first query

A program connects as a URI says, here over TLS checked against the
authority of `ca.pem`, and reads a query's rows as a range, a parameter
bound from a C++ value:

```cpp
#include <turborel/turborel.hpp>

#include <cstdint>
#include <iostream>
#include <string_view>

int main() {
  try {
    turborel::connection conn =
        turborel::connection::open("turborel://ann@localhost:17439/?tls_mode=verify-full&tls_roots=ca.pem");
    for (const turborel::row row : conn.query("SELECT ID, NAME FROM STAFF WHERE ID < ? ORDER BY ID", 3)) {
      std::cout << row.get<std::int64_t>(0) << ' ' << row.get<std::string_view>(1) << '\n';
    }
  } catch (const turborel::error& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
```

Of a table STAFF of Ada, Grace and Edsger, it prints:

```
1 Ada
2 Grace
```

## The manual

[doc/](doc/README.md) is the manual: a User's Guide, an API Reference
and Appendices, published at
https://turborel.com/docs/drivers/cpp/. The SQL a
program sends, and the server, are described in Turborel's own manual,
[Turborel Documentation](https://github.com/turborel/turborel/blob/main/doc/README.md).
[VERSIONING.md](VERSIONING.md) tells how the driver's releases are
numbered and published.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before making a change: how a
change is made, its commit message, pull requests and issues. Every
change passes `cmake --workflow --preset verify`. [RULES.md](RULES.md) says how the
driver is designed, written and verified,
[CONFORMANCE.md](CONFORMANCE.md) is the parity matrix every Turborel
driver keeps, and [TODO.md](TODO.md) lists open work.

## Security

Report a vulnerability privately, never in a public issue, as
[SECURITY.md](SECURITY.md) says.

## License and trademark

The code is under the BSD-2-Clause Plus Patent License, in
[LICENSE](LICENSE). The name Turborel and its logos are not:
[TRADEMARK.md](TRADEMARK.md) says how they may be used.
