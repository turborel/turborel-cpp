# The turborel-cpp Documentation Set

turborel-cpp is the C++20 driver for turborel, a relational database
system: a wrapper over turborel-c that connects a C++ program to a
turborel server over the native protocol. This set of manuals describes
it for those who write programs with it.

turborel-cpp is in alpha: what these manuals describe may change in any
release until 1.0.0 ([Versions](appendix/d-versions.md)). Each change
updates the pages it touches, so the manuals describe the release they
come with. Most chapters are still to be written, each as its milestone
lands.

## The volumes

| Volume | For | |
|---|---|---|
| [I. User's Guide](user/README.md) | Anyone writing a program with the driver | Installing, connecting, queries, transactions and the rest, chapter by chapter |
| [II. API Reference](client/README.md) | The same, looking something up | The API, a page per main class or header |
| [Appendices](appendix/README.md) | Everyone | Types, errors, settings, versions |

## Conventions

**Code** is set in a fixed-width font, and is C++20. **Results** follow
the examples that print them. **Citations** of the protocol name its
section's heading, as [PROTOCOL Composite fields]. **Errors** are given by
SQLSTATE and SQLCODE.
