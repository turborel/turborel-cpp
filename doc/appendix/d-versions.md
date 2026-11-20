# Appendix D: Versions

[Contents](README.md)

turborel-cpp's version is the turborel release it is made for, the same
number as the server's. A turborel release and each of its drivers'
releases share the number, so turborel-cpp 1.2.0 is the driver of
turborel 1.2.0. The driver has no version scheme of its own.

| turborel-cpp | turborel | Protocol |
|---|---|---|
| 0.0.0 (alpha) | 0.0.0 (alpha) | 1 |

During the alpha, version 0.0.0, anything may change from one build to
the next, the API and the build alike, and the CMake package matches only
its exact version. The alpha speaks protocol version 1, the only version;
the protocol grows by capabilities, never by a new version.

The version is set in `CMakeLists.txt`, by `project(turborel-cpp VERSION
...)`, and the CMake package's version file carries it.
