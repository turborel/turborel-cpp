# The CMake Package

[Contents](README.md)

## NAME

`turborel-cpp`: the CMake package and target a program builds against.

## SYNOPSIS

```cmake
find_package(turborel-cpp 0.0.0 EXACT CONFIG REQUIRED)   # an installed package
add_subdirectory(turborel-cpp)                            # the source in the tree
FetchContent_MakeAvailable(turborel-cpp)                  # the source fetched
target_link_libraries(app PRIVATE turborel::turborel-cpp)
```

## DESCRIPTION

`turborel::turborel-cpp` is the library, its headers and its C++20
requirement, got in any of the three ways above. The installed package
matches only its exact version, the Turborel release it is made for
([Appendix D: Versions](../appendix/d-versions.md)). The library is
static, or shared where `BUILD_SHARED_LIBS` is on.

| Option | What it does | Default |
|---|---|---|
| `TURBOREL_CPP_BUILD_TESTS` | Builds the tests and the checks that run as tests | on when turborel-cpp is the top project |
| `TURBOREL_CPP_INSTALL` | Generates the install rules and the CMake package | on |
| `TURBOREL_CPP_WITH_C` | Builds against turborel-c | off |
| `TURBOREL_CPP_C_COMMIT` | The turborel-c commit fetched where no installed package is wanted | none |
| `TURBOREL_CPP_LINT` | Runs clang-tidy as it builds, and the format and doc checks | off |
| `TURBOREL_CPP_REQUIRE_DOXYGEN` | Fails to configure without Doxygen when linting | off |
| `TURBOREL_CPP_SANITIZE` | Builds with AddressSanitizer and UndefinedBehaviorSanitizer | off |
| `TURBOREL_CPP_COVERAGE` | Builds with Clang's source-based coverage | off |

`CMakePresets.json` names five builds, each in `build/<preset>/`, and a
workflow of each that configures, builds and tests: `debug`, `release`,
`asan-ubsan`, `coverage` and `verify`, as `cmake --workflow --preset
release`.

## ERRORS

`find_package` fails, as CMake fails a required package, where no
installed package of the exact version is found.

## EXAMPLES

```
cmake --workflow --preset release
cmake --install build/release --prefix /usr/local
```

## STANDARD

The driver's own (RULES.md D9).

## SEE ALSO

[Installing](../user/01-installing.md),
[The turborel Namespace](01-api-namespace.md)
