# Installing

[Contents](README.md)

This chapter will tell what the driver needs, a C++20 compiler, GCC 13, Clang 18, Apple Clang or MSVC 19.40 or later, and CMake 3.25 or later on Linux,
macOS or Windows, and how a program takes the CMake package `turborel-cpp` in from
the repository's GitHub releases, as archives. No package is published until every Turborel driver is
done; until then the driver is built from its source, as the
repository's
[README](../../README.md)
says. The chapter is written with the standard API, at milestone M7, and
again when the packages are published.

## Building from source

Until the packages are published, the driver is built and installed
from its source:

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
