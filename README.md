<p align="center">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="artwork/turborel-logo-white.svg">
      <img src="artwork/turborel-logo.svg" alt="turborel" width="360">
    </picture>
</p>

> turborel-cpp is in alpha, meaning, version 0.0.0. Anything may change
> from one build to the next, API, behaviour and build alike. Its version
> is the turborel release it is made for, the same number as the
> server's, and the alpha speaks protocol version 1. See
> [VERSIONING.md](VERSIONING.md). The latest turborel alpha is built for
> Linux, macOS and Windows as the pre-release
> [nightly](https://github.com/turborel/turborel/releases/tag/nightly),
> and as the image `ghcr.io/turborel/turborel:nightly`.

turborel-cpp is the C++ driver for [turborel](https://github.com/turborel/turborel),
a relational database system. It connects a C++ program to a turborel
server, `turboreld`, over turborel's native protocol, through
[turborel-c](https://github.com/turborel/turborel-c), the C driver it
wraps, and gives it in the language's own idioms: owners of connections
and statements that close them as they go, failures thrown as
exceptions carrying their SQLCODE and SQLSTATE, a query's rows as a
range, datetimes in `std::chrono`, and exact numbers in a decimal type of
its own (RULES.md D1, D2).

It is being built to do everything turborel-client, turborel's own
client library, does: TLS, every mechanism of authentication, prepared
statements and cursors, large objects, bulk copy, settings from a URI or
the environment, several servers, pipelines and pools.
[CONFORMANCE.md](CONFORMANCE.md) shows how far it has come: so far it
holds its error type, and the repository's build, checks and CI.

## What it needs

- A C++20 compiler: GCC 13, Clang 18, Apple Clang or MSVC 19.40
  (Visual Studio 2022 17.10) or later (RULES.md D3).
- CMake 3.25 or later (D4).
- turborel-c, of the same turborel release, once the driver wraps it
  (D6). It brings the TLS library and zlib it needs.
- A turborel server to connect to.

It builds and is tested on Linux, macOS and Windows.

## Installing

No package is published yet, to vcpkg, Conan or anywhere else; build it
from source.

```
git clone https://github.com/turborel/turborel-cpp.git
cd turborel-cpp
cmake --workflow --preset release                 # build and test
cmake --install build/release --prefix /opt/turborel
```

A CMake project then finds it and links its target:

```cmake
find_package(turborel-cpp 0.0.0 EXACT CONFIG REQUIRED)
target_link_libraries(app PRIVATE turborel::turborel-cpp)
```

FetchContent or `add_subdirectory` of the source gives the same target.

## A first query

This is what the API will be. Only `turborel::error` exists today; the
rest is planned, and may change until M8 (TODO.md).

```cpp
#include <turborel/turborel.hpp>

#include <cstdint>
#include <iostream>
#include <string_view>

int main() {
  try {
    auto conn = turborel::connection::open("turborel://ann@localhost/?tls_mode=verify-full");
    for (const auto& row : conn.query("SELECT ID, NAME FROM STAFF ORDER BY ID")) {
      std::cout << row.get<std::int64_t>(0) << ' ' << row.get<std::string_view>(1) << '\n';
    }
  } catch (const turborel::error& e) {
    std::cerr << e.what() << '\n';  // SQLCODE -103 (SQLSTATE 2A000): table ANN.STAFF is not defined ...
    return 1;
  }
}
```

## Documents

- [doc/](doc/README.md): the manual, to be written by M8.
- [VERSIONING.md](VERSIONING.md): the version, and how a release is
  published.
- [CONFORMANCE.md](CONFORMANCE.md): what is implemented of the protocol
  and of turborel-client.
- [RULES.md](RULES.md): how the driver is designed, written and verified.
- [TODO.md](TODO.md): open work, by milestone.
- [CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md):
  contributing, and reporting an issue or a vulnerability.
- [LICENSE](LICENSE) and [TRADEMARK.md](TRADEMARK.md): the code is under
  the BSD-2-Clause Plus Patent License; the name Turborel and its logos
  are not, and TRADEMARK.md says how they may be used.
