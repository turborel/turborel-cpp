# turborel-cpp rules

How turborel-cpp, the C++ driver for turborel, is designed, written and
verified. Every change follows these rules; `cmake --workflow --preset
verify` checks those a tool can check, and review checks the rest.

Rules (R-…) say how code and tests are written. **shall** is mandatory,
and a departure needs a recorded deviation (§6). **should** is expected, and
a reviewer may accept a reasoned exception.

Decisions (D1…) record how the driver is built and why. Change one only by
changing this file in a reviewed commit. A decision says what was chosen and
why, and the file it names holds the mechanism.

Cite both by ID in code, tests, reviews and commit messages, as in
`RULES.md D5` or `R-ERR-2`.

Before changing code, read §3 (code rules), §4 (test rules) and the
decisions covering your area. What the driver implements of the protocol
and of turborel-client is in [CONFORMANCE.md](CONFORMANCE.md), and open
work is in [TODO.md](TODO.md).

[1 Decisions](#1-decisions) · [2 Layout](#2-project-layout) ·
[3 Code](#3-code-rules) · [4 Tests](#4-test-rules) ·
[5 Coverage](#5-coverage) · [6 Deviations](#6-deviations) ·
[7 Process](#7-process-and-tools) · [8 References](#8-references)

---

## 1. Decisions

| # | Decision |
|---|---|
| **The driver** | |
| D1 | A C++20 wrapper over turborel-c, with no protocol code of its own |
| D2 | The standard API is RAII C++ over turborel-c's handles |
| D3 | C++20, on GCC 13, Clang 18, Apple Clang and MSVC 19.40 or later |
| D4 | Built by CMake 3.25 or later, and installed as the CMake package `turborel-cpp` |
| D5 | Every failure is a `turborel::error`, a `std::runtime_error` |
| D6 | turborel-c is found installed, or fetched at a pinned commit, and is off until its M3 |
| **Tooling and process** | |
| D7 | Tests in Catch2 3, fetched at a pinned release |
| D8 | clang-format and clang-tidy of LLVM 23, and Doxygen, check what the compiler does not |
| D9 | The tests run under AddressSanitizer and UndefinedBehaviorSanitizer |
| D10 | Coverage is Clang's source-based coverage, and its floors only rise |
| D11 | Automation in CMake, never shell scripts |
| D12 | No codec test here: the codec and its tests are turborel-c's |
| D13 | Verified on Linux, macOS and Windows, in CI and before each commit |
| D14 | The version is the turborel release's, and nothing is published until every driver is done |
| D15 | Where these rules depart from C++'s conventions |

### The driver

#### D1. A C++20 wrapper over turborel-c, with no protocol code of its own

turborel-cpp speaks turborel's native protocol, version 1
(`doc/client/05-protocol.md` in turborel), only through turborel-c's C API.
It holds no codec, transport, authentication or TLS of its own: turborel-c
has them, and one implementation in C serves both drivers, so a defect is
fixed once and the two never disagree on the wire. There is no native core
beyond turborel-c, and turborel-c brings its own dependencies (OpenSSL or
AWS-LC, zlib) by its own decisions.

What this driver adds is the C++ on top: owners of turborel-c's handles,
exceptions, ranges of rows, `std::chrono` datetimes and a decimal type. It
matches everything turborel-client does (CONFORMANCE.md), feature by
feature, as turborel-c exposes it.

#### D2. The standard API is RAII C++ over turborel-c's handles

C++ has no standard database interface, so the standard API is the
language's own idioms, as its standard library writes them:

- each turborel-c handle is owned by a C++ object that frees it in its
  destructor, movable and not copyable;
- every failure is thrown as `turborel::error` (D5), so a call either
  succeeds or throws;
- a query's rows are a range, `std::ranges::input_range`, read as they
  come;
- datetimes are `std::chrono` types, and exact numbers, an `i128` of units
  and a `u32` scale on the wire, a decimal type of the driver's own, since
  the standard has none that holds them all;
- the API takes and gives `std::span` and `std::string_view` at its
  boundaries, and owns what it keeps.

Everything is in the namespace `turborel`, with headers under
`include/turborel/`, `turborel/turborel.hpp` including the rest, and the
CMake target `turborel::turborel-cpp`. During the alpha no ABI is kept
between releases.

#### D3. C++20, on GCC 13, Clang 18, Apple Clang and MSVC 19.40 or later

The language level is C++20, not C++23, because MSVC's C++23 is not
complete as of November 2026, and the driver builds with GCC, Clang and
MSVC alike. C++20 has what D2 needs: concepts, ranges, `std::span` and the
calendar types of `<chrono>`.

The compilers are those CI runs (D13): GCC 13 and 14 and Clang 18, as
Ubuntu 24.04 ships them, and the Apple Clang and MSVC of GitHub's current
macOS and Windows images. GCC 13 is the oldest GCC release series still
maintained upstream, and Visual Studio 2022 17.10 (MSVC 19.40) the oldest
MSVC supported. A compiler older than these may work, and is not tested.

#### D4. Built by CMake 3.25 or later, and installed as the CMake package `turborel-cpp`

CMake is what C++ libraries are built and found by. 3.25 is the first with
workflow presets and `SYSTEM` for fetched dependencies, both used here.
`CMakePresets.json` names the builds: `debug`, `release`, `asan-ubsan`,
`coverage` and `verify`, each a workflow of configure, build and test.

`cmake --install` installs the library, its headers, the generated
`turborel/export.hpp`, the license, and the CMake package
`turborel-cpp`, whose `find_package(turborel-cpp CONFIG)` gives
`turborel::turborel-cpp`; `add_subdirectory` and FetchContent give the
same target by an alias. The library is static by default, or shared with
`BUILD_SHARED_LIBS`, its symbols hidden but those marked
`TURBOREL_CPP_EXPORT`. The test `package.consume` builds a program against
the installed package each time the tests run (`test/package/`).

The ports for vcpkg and Conan come with M8 (TODO.md).

#### D5. Every failure is a `turborel::error`, a `std::runtime_error`

A failure is thrown as `turborel::error` (`include/turborel/error.hpp`).
It derives from `std::runtime_error`, so code that catches those catches
it, and carries the whole failure as the protocol writes it, a
`turborel::failure`: SQLCODE, SQLSTATE, message, the rule it cites, its
position, whether the transaction is still active, and the diagnostics of
tags 7 to 15. Its `what()` reads as turborel-client's display of a failure,
`SQLCODE -103 (SQLSTATE 2A000): table ANN.NOPE is not defined [X3.135-1986
§5.4 SR5]`, so a message looks the same from every client.

Copying it never throws, as the standard asks of exceptions: the failure
is held by a `std::shared_ptr<const failure>` its copies share. A failure
the driver finds itself, such as a closed handle used, is a `turborel::error`
too, with the SQLSTATE turborel-client gives the same case.

#### D6. turborel-c is found installed, or fetched at a pinned commit, and is off until its M3

turborel-c is consumed by CMake (`cmake/TurborelDependencies.cmake`), under
the option `TURBOREL_CPP_WITH_C`, off until turborel-c reaches its
milestone M3 and has an API to wrap:

- by default, `find_package(turborel-c <version> EXACT CONFIG)` finds an
  installed package, as `cmake --install`, vcpkg or Conan leave it, at the
  same turborel release (D14);
- where `TURBOREL_CPP_C_COMMIT` names a full commit, FetchContent fetches
  turborel-c at that commit, still trying the installed package first.

Both, rather than one: package managers and distributions give an
installed package, and a pinned commit lets the driver build from a clean
checkout with nothing installed, as CI does. A commit, not a tag, because
turborel-c has no tags during the alpha and a tag can be moved. Its target
is taken to be `turborel::turborel-c`, to be confirmed against turborel-c
when the option is first turned on.

### Tooling and process

#### D7. Tests in Catch2 3, fetched at a pinned release

The tests are Catch2 3.16.1 (BSL-1.0), fetched by FetchContent as the
release's source archive, its SHA-256 checked
(`cmake/TurborelDependencies.cmake`), built with the tests' C++20 and only
when tests are built. Catch2 is what C++ projects commonly test with, its
v3 releases are its stable line, and it needs nothing else. It is a test
dependency alone: nothing installed links it. Dependabot cannot follow a
FetchContent pin, so it is raised by hand, in a commit of its own.

`catch_discover_tests` makes each `TEST_CASE` a CTest test of its own.

#### D8. clang-format and clang-tidy of LLVM 23, and Doxygen, check what the compiler does not

- **clang-format 23.1.4** formats every C++ source by `.clang-format`
  (R-STYLE-1).
- **clang-tidy 23.1.4** runs as each of the project's targets builds, by
  `.clang-tidy`: `bugprone`, `cert`, `clang-analyzer`, `concurrency`,
  `cppcoreguidelines`, `misc`, `modernize`, `performance`, `portability`
  and `readability`, every finding an error (R-LINT-2). `test/.clang-tidy`
  turns off what Catch2's macros cannot meet, each with its reason.
- **Doxygen** builds the API reference, failing on any undocumented public
  item or malformed doc comment (R-DOC-1).

Both LLVM tools are pinned to one release, from PyPI's wheels, in
`tools/requirements-lint.txt`, which Dependabot follows; another version
formats and lints differently. The build refuses a clang-format or
clang-tidy of another major version. Doxygen is checked wherever it is
found, and CI requires it (`TURBOREL_CPP_REQUIRE_DOXYGEN`).

#### D9. The tests run under AddressSanitizer and UndefinedBehaviorSanitizer

The preset `asan-ubsan` builds the library and the tests with both, every
finding fatal; CI runs it with GCC and Clang on Linux and with Apple Clang
on macOS. MSVC has AddressSanitizer alone, which the option gives it.

#### D10. Coverage is Clang's source-based coverage, and its floors only rise

The preset `coverage` builds with `-fprofile-instr-generate
-fcoverage-mapping`, runs the tests, and `cmake/Coverage.cmake` merges
their profiles with `llvm-profdata` and reads the library's own coverage
with `llvm-cov`, its lines, functions and regions, against the floors in
`cmake/CoverageFloor.cmake` (R-COV). Clang's coverage, rather than gcov,
because its regions are finer than lines and one script reads it on every
platform Clang runs on.

#### D11. Automation in CMake, never shell scripts

Every check is CMake: presets, targets and `cmake -P` scripts in `cmake/`,
which run the same on Linux, macOS and Windows. The one shell file,
`.githooks/pre-commit`, only hands over to `cmake --workflow --preset
verify`, because git runs hooks as scripts.

#### D12. No codec test here: the codec and its tests are turborel-c's

Every turborel driver's first test encodes and decodes a frame header per
`[PROTOCOL Frames]`. This driver has none, since it has no codec (D1):
turborel-c holds the frames, fields and values, and tests them. The tests
here start at what the driver adds, its error type first, and test the
protocol through turborel-c's API once it exists.

#### D13. Verified on Linux, macOS and Windows, in CI and before each commit

CI (`.github/workflows/verify.yml`) builds and tests on Linux with GCC 13,
GCC 14 and Clang 18, on macOS with Apple Clang, and on Windows with MSVC,
in Debug and Release, warnings as errors. The checks of D8 run on Linux and
macOS, where clang-tidy reads the compiler's commands; MSVC's are checked
by its own warnings. The sanitizers run on Linux and macOS (D9) and
coverage on Linux (D10). The pre-commit hook runs `verify` on the
committer's own platform, because a hook can be skipped and only checks
one platform, and CI checks all three.

#### D14. The version is the turborel release's, and nothing is published until every driver is done

The driver's version is the turborel release it is made for, the same
number as the server's: the alpha is 0.0.0, as turborel's is, and speaks
protocol version 1. There is no version scheme of the driver's own
(VERSIONING.md). C and C++ have no central registry: a release is an
archive on GitHub, and vcpkg and Conan Center ports follow by pull request,
once every turborel driver is done and the owner publishes them.
`.github/workflows/release.yml` builds and checks a release by hand, and
its publishing step stays off until the owner turns it on.

#### D15. Where these rules depart from C++'s conventions

- The Core Guidelines allow `assert` and contracts that end the program on
  a broken precondition. Library code here never ends the process (R-ERR-3):
  a broken precondition the driver can see is a `turborel::error`.
- C++ libraries often keep code in headers. Here, what need not be inline
  is in `src/`, so the 600-line limit (R-SIZE-2) counts real code and the
  ABI hides what it can.
- Catch2 can tag a test with what it verifies; the tests here cite in a
  `/// Verifies:` paragraph above each `TEST_CASE` instead, as every
  turborel repository does, and `check.sources` holds each citation to a
  heading or decision that exists (R-TRACE-1).
- Every turborel driver begins with a frame header test; this one does not
  (D12).

---

## 2. Project layout

```
CMakeLists.txt          the library's target, options and checks (D4)
CMakePresets.json       debug, release, asan-ubsan, coverage, verify (D4)
cmake/                  CMake modules and `cmake -P` checks (D11):
  TurborelWarnings.cmake    warnings, sanitizers, coverage flags
  TurborelTools.cmake       clang-tidy, clang-format, Doxygen (D8)
  TurborelDependencies.cmake turborel-c and Catch2 (D6, D7)
  TurborelInstall.cmake     install and export rules (D4)
  CheckSources.cmake        the source rules a tool can check
  CheckFormat.cmake         R-STYLE-1
  Coverage.cmake            coverage against its floors (D10)
  CoverageFloor.cmake       the floors (R-COV-2)
.clang-format           formatting (D8)
.clang-tidy             lints (D8); test/.clang-tidy relaxes them for tests
tools/requirements-lint.txt  the pinned clang-format and clang-tidy (D8)
include/turborel/       the public headers (D2)
src/                    the library's sources
test/                   the tests, one Catch2 binary (D7)
  CMakeLists.txt        the test binary, coverage target, package test
  <area>.cpp            one source per area
  package/              a program built against the installed package
doc/                    the manual (R-DOC-7)
README.md               what the driver is; how to build it
RULES.md                this file
CONFORMANCE.md          the parity matrix: what is implemented of the
                        protocol and of turborel-client
TODO.md                 open work, by milestone
VERSIONING.md           the version, and how a release is published
CONTRIBUTING.md         changes, commit messages, pull requests, issues
SECURITY.md             how to report a vulnerability
AGENTS.md               the guide for coding agents
CLAUDE.md               points Claude Code to AGENTS.md
.github/                CI (workflows/), issue forms, pull request
                        template, labels, Dependabot
.githooks/pre-commit    runs verify before each commit (D11)
```

---

## 3. Code rules

### R-STYLE: formatting and idiom

| ID | Rule | Checked by |
|---|---|---|
| R-STYLE-1 | Code **shall** be formatted by clang-format, as `.clang-format` says. | `check.format` |
| R-STYLE-2 | Code **shall** be standard C++20, with no compiler extension (`CXX_EXTENSIONS OFF`). | compilers |
| R-STYLE-3 | Names **shall** be `lower_case`, as the standard library's, private members ending in `_`, template parameters `CamelCase`, macros `UPPER_CASE`. | `readability-identifier-naming` |
| R-STYLE-4 | A header **shall** have an include guard `TURBOREL_<NAME>_HPP` and include what it uses. | `portability-avoid-pragma-once`, `misc-include-cleaner` |
| R-STYLE-5 | No `using namespace` directive. | `google-build-using-namespace` |
| R-STYLE-6 | Library code **shall not** print, or read the environment except where a decision names it. | review |

### R-SIZE: short functions and files

| ID | Rule | Checked by |
|---|---|---|
| R-SIZE-1 | A function **shall** have at most 50 lines. | `readability-function-size` |
| R-SIZE-2 | A source file **shall** have at most 600 lines. | `check.sources` |
| R-SIZE-3 | A function **shall** take at most 5 parameters; else a struct, with designated initializers at the call. | `readability-function-size` |
| R-SIZE-4 | A header **should** hold one concept, stated in its `@file` doc. | review |

### R-CF: control flow

| ID | Rule | Checked by |
|---|---|---|
| R-CF-1 | A boolean decision **shall** have at most 4 conditions; else a named predicate. | review |
| R-CF-2 | Cognitive complexity **shall** be at most 15. | `readability-function-cognitive-complexity` |
| R-CF-3 | A `switch` on an enum **shall** name every enumerator, with no `default`. | `-Wswitch` in `-Wall`; review |
| R-CF-4 | Conditions **shall** have no side effects. | review |

### R-DOC: comments and documentation

| ID | Rule | Checked by |
|---|---|---|
| R-DOC-1 | Every public item **shall** have a Doxygen `///` comment, with `@param` for each parameter and `@throws` for what it throws. | `check.docs` (Doxygen), `-Wdocumentation` |
| R-DOC-2 | Comments **shall** say *why*: the rule implemented, the choice made, the invariant relied on. | review |
| R-DOC-3 | The API reference **shall** build without warnings. | `check.docs` |
| R-DOC-4 | Behaviour the protocol governs **shall** cite its heading, as `[PROTOCOL Frames]`; turborel-client's, `[CLIENT Pipelines]`; the driver's own choices, `[DESIGN]` and the decision. | review |
| R-DOC-5 | No commented-out code, and no open-work comments: open work goes in TODO.md, which code never cites. | `check.sources`; review |
| R-DOC-6 | Every C++ file **shall** start with a `/// @file` doc giving its responsibility. | `check.sources` |
| R-DOC-7 | A change visible to a program **shall** update the manual, `doc/`, in the same commit; claims are checked against the code, examples are run, and every relative link names a file that exists. | `check.sources` (links); review |

### R-CONST: no magic numbers or strings

| ID | Rule | Checked by |
|---|---|---|
| R-CONST-1 | Numeric literals other than `0` or `1` **shall** be named `constexpr` constants, documenting the value's source. | `readability-magic-numbers` |
| R-CONST-2 | Protocol tags, kinds, capability and status names, and setting names **shall** be named constants, defined once, beside the heading they come from. | review |
| R-CONST-3 | Messages for people are not constants, but cite their rule. | review |
| R-CONST-4 | Tests may use literal fixtures. | `test/.clang-tidy` |

### R-ERR: errors

| ID | Rule | Checked by |
|---|---|---|
| R-ERR-1 | Every failure **shall** be thrown as `turborel::error`, carrying SQLCODE, SQLSTATE, message and the rest of the failure (D5); allocation failure alone is the standard library's `std::bad_alloc`. | review; tests |
| R-ERR-2 | A turborel-c failure **shall** be turned into a `turborel::error` at the call that returned it, never dropped. | `[[nodiscard]]`, `bugprone-unused-return-value`; review |
| R-ERR-3 | Library code **shall not** end the process: no `abort`, `exit`, `terminate`, `assert` or `std::unreachable`. | review |
| R-ERR-4 | No exception **shall** leave a destructor, a `noexcept` function, or a callback turborel-c calls; such a callback catches everything and reports it through turborel-c. | `bugprone-exception-escape`; review |
| R-ERR-5 | Exceptions **shall** be thrown by value, caught by reference, and not used for control flow. | `misc-throw-by-value-catch-by-reference`; review |

### R-NUM: arithmetic and conversions

| ID | Rule | Checked by |
|---|---|---|
| R-NUM-1 | No implicit narrowing or sign conversion. | `-Wconversion`, `-Wsign-conversion`, `/W4` |
| R-NUM-2 | No C-style casts; `reinterpret_cast` only at turborel-c's boundary, with a comment why. | `-Wold-style-cast`, `cppcoreguidelines-pro-type-*` |
| R-NUM-3 | Integer arithmetic on values from the server **shall** be shown not to overflow, or checked. | review; UBSan |
| R-NUM-4 | Floats **shall not** be compared for equality, except as the value's own comparison defines, its `NOLINT` saying so. | `-Wfloat-equal` |

### R-SAFE: language subset and resources

| ID | Rule | Checked by |
|---|---|---|
| R-SAFE-1 | Every resource **shall** be owned by an object that releases it (RAII): no owning raw pointer, `new`, `delete` or `malloc`; a turborel-c handle is freed by its owner's destructor. | `cppcoreguidelines-owning-memory`, `cppcoreguidelines-no-malloc` |
| R-SAFE-2 | No global or static mutable state. | `cppcoreguidelines-avoid-non-const-global-variables` |
| R-SAFE-3 | No pointer arithmetic or array decay outside `std::span`. | `cppcoreguidelines-pro-bounds-*` |
| R-SAFE-4 | The library **shall not** start a thread, except where a decision says. | review |
| R-SAFE-5 | Every dependency **shall** have a §1 decision covering why, its version, license and readiness, and the standard library comes first. A dependency **shall** be production-grade by its own claims. | review |

### R-LINT: warnings and linters

| ID | Rule | Checked by |
|---|---|---|
| R-LINT-1 | Code **shall** compile without warnings at the project's warning levels (`cmake/TurborelWarnings.cmake`); warnings are errors (`CMAKE_COMPILE_WARNING_AS_ERROR`). | GCC, Clang, MSVC |
| R-LINT-2 | Code **shall** pass clang-tidy by `.clang-tidy`, every finding an error. | `verify` |

---

## 4. Test rules

### R-TEST: levels and layout

| ID | Rule | Checked by |
|---|---|---|
| R-TEST-1 | Tests **shall** be Catch2 `TEST_CASE`s in `test/`, one source per area, through the public API. | review |
| R-TEST-2 | Tests **shall** derive from requirements, not code: normal cases, boundaries, and failures with their exact SQLCODE and SQLSTATE. | review |
| R-TEST-3 | A test's name **shall** say the behaviour checked. | review |
| R-TEST-4 | Tests **shall** be deterministic and independent: no clock, randomness, or network beyond loopback; a live test runs its own server on its own port. | review |
| R-TEST-5 | Every fixed defect **shall** get a regression test that fails without the fix, its doc saying so. | review |
| R-TEST-6 | A fixture or helper used by more than one source **shall** be written once, in a shared test header. | review |

### R-TRACE: traceability

| ID | Rule | Checked by |
|---|---|---|
| R-TRACE-1 | Every `TEST_CASE` **shall** have a `/// Verifies:` paragraph right above it, starting with a tag: `[PROTOCOL <heading>]`, a heading of `doc/client/05-protocol.md`; `[CLIENT <heading>]`, a heading of `doc/client/02-library.md` or `02-library-connections.md`; or `[DESIGN] D<n>`, a decision of this file. | `check.sources` |
| R-TRACE-2 | CONFORMANCE.md **shall** change with the change implementing a row of it. | review |

---

## 5. Coverage

| ID | Rule | Checked by |
|---|---|---|
| R-COV-1 | Coverage **shall** be measured by the preset `coverage` over the tests, for `include/` and `src/` (D10). | `coverage-report` |
| R-COV-2 | Coverage **shall not** fall below the floors in `cmake/CoverageFloor.cmake`, of lines, functions and regions; a change raising it **should** raise them. | `coverage-report` |
| R-COV-3 | Gaps are closed by requirements-based tests or by removing dead code, never by tests that verify nothing. | review |

---

## 6. Deviations

| ID | Rule | Checked by |
|---|---|---|
| R-DEV-1 | A finding is silenced only by `NOLINTNEXTLINE(check)` or `NOLINT(check)` naming the check, then `:` and its reason, on the same comment. A `NOLINT` naming no check is refused. | `check.sources` |
| R-DEV-2 | A warning is never turned off for the project; one turned off for one line says why. | review |

---

## 7. Process and tools

| ID | Rule | Checked by |
|---|---|---|
| R-PROC-1 | `cmake --workflow --preset verify` **shall** pass before every commit: the build with clang-tidy and warnings as errors, the format, the source rules (file length, file docs, citations, `NOLINT` reasons, no open-work comment, no merge conflict marker), the docs where Doxygen is found, the package test and every test. | pre-commit hook; CI |
| R-PROC-2 | Every change **shall** be reviewed against the checklist below. | review |
| R-PROC-3 | Commits **should** be small and single-purpose, in CONTRIBUTING.md's form. | review |
| R-PROC-4 | A change of a pinned tool or dependency **shall** be its own commit, changing its decision. | review |

The review checklist:

- requirement cited in code (R-DOC-4) and tests (R-TRACE-1);
- normal and failure tests (R-TEST-2);
- no magic numbers or strings (R-CONST);
- every failure a `turborel::error`, none escaping a destructor (R-ERR);
- every resource owned (R-SAFE-1);
- comments explain why (R-DOC-2);
- CONFORMANCE.md, TODO.md and `doc/` updated where they change.

---

## 8. References

- turborel's `doc/client/05-protocol.md`, the native protocol, version 1;
  `doc/client/02-library.md` and `02-library-connections.md`, the client
  library turborel-client, which this driver matches.
- ISO/IEC 14882:2020, *Programming languages — C++*.
- B. Stroustrup and H. Sutter, eds., *C++ Core Guidelines*.
- *CMake Reference Documentation*: presets, FetchContent, packages.
- The Catch2 documentation, v3.
- The Clang documentation: clang-tidy, clang-format, sanitizers and
  source-based code coverage.
