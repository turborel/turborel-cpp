# Versioning

turborel-cpp's version is the turborel release it is made for, the same
number as the server's. A turborel release and each driver's release
share the number; the driver has no version scheme of its own.

## Alpha

The alpha is version **0.0.0**, as turborel's is (turborel's
VERSIONING.md), and speaks protocol version 1. While the version is 0.y.z
nothing is promised: the API, its behaviour and the build may change in
any release, and no ABI is kept. The CMake package matches only its exact
version (`ExactVersion`).

## Where the version is

- `CMakeLists.txt`: `project(turborel-cpp VERSION 0.0.0)`.
- The installed CMake package's `turborel-cpp-config-version.cmake`.

## Publishing

C and C++ have no central registry. A release is an archive on GitHub,
and ports to vcpkg and Conan Center follow by pull request (TODO.md, M8).
Nothing is published until every turborel driver is done, and the owner
publishes them. `.github/workflows/release.yml`, run by hand, builds,
tests and packs a release; its publishing step is off until the owner
turns it on (RULES.md D14).
