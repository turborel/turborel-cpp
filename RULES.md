# turborel-cpp rules

How turborel-cpp is designed, written and verified. Every change follows
these rules; `cmake --workflow --preset verify` checks those a tool can check, and
review checks the rest.

Rules (R-…) say how code and tests are written. **shall** is mandatory,
and a departure needs a decision recorded here (§7). **should** is
expected, and a reviewer may accept a reasoned exception.

Decisions (D1…) record how the driver is built and why. Change one only
by changing this file in a reviewed commit. A decision says what was
chosen and why, and the module it names holds the mechanism.

Cite both by ID in code, tests, reviews and commit messages, as in
`RULES.md D6` or `R-ERR-2`.

The rules are modelled on Turborel's own
[RULES.md](https://github.com/turborel/turborel/blob/main/RULES.md),
adapted to drivers. Every Turborel driver keeps the same requirement IDs
with the same meaning, and the same decisions D1 to D8, so a rule is
cited the same way in every repository; a driver's own decisions begin
at D9, and its own requirements are in families of their own. Where
C++'s library conventions and a rule here disagree, the rule is
kept, and D8 lists where.

Before changing code, read §3 (code rules), §4 (test rules) and the
decisions covering your area. What the driver implements is in
[CONFORMANCE.md](CONFORMANCE.md), and open work is in [TODO.md](TODO.md).

[1 Decisions](#1-decisions) · [2 Layout](#2-project-layout) ·
[3 Code](#3-code-rules) · [4 Tests](#4-test-rules) ·
[5 Coverage](#5-coverage) · [6 Process](#6-process-and-tools) ·
[7 The decisions](#7-the-decisions) · [8 References](#8-references)

---

## 1. Decisions

Each is told in full in [§7](#7-the-decisions).

| # | Decision |
|---|---|
| **Shared by every Turborel driver** | |
| D1 | The native protocol, version 1, in layers that follow turborel-client |
| D2 | The language's standard API, over a low-level client of turborel-client's features |
| D3 | The runtime minimum follows the ecosystem's own support |
| D4 | One command runs every check, before each commit and in CI on three platforms |
| D5 | The version is the Turborel release's: 0.0.0 in the alpha, speaking protocol 1 |
| D6 | Tests and code cite Turborel's documents by their headings |
| D7 | A coverage floor, enforced in CI, that only rises |
| D8 | Where these rules depart from C++'s conventions |
| **This driver's own** | |
| D9 | Built by CMake 3.25 or later, and installed as the CMake package `turborel-cpp` |
| D10 | Every failure is a `turborel::error`, a `std::runtime_error` |
| D11 | turborel-c is found installed, or fetched at a pinned commit, and is off until its M3 |
| D12 | Tests in Catch2 3, fetched at a pinned release |
| D13 | clang-format and clang-tidy of LLVM 23, and Doxygen, check what the compiler does not |
| D14 | The tests run under AddressSanitizer and UndefinedBehaviorSanitizer |
| D15 | No codec tests here: the codec and its tests are turborel-c's |
| D16 | Values in C++ types of the driver's own, each mapped to the protocol's fields |

---

## 2. Project layout

| Path | What |
|---|---|
| `include/turborel/` | the public headers, the `api` layer; `turborel.hpp` includes the rest |
| `src/` | the library's sources, one per header; every other layer of D1 is turborel-c's, wrapped here from its M3 on |
| `test/` | the tests, one Catch2 binary, a source per area (D12) |
| `test/package/` | a program built against the installed package (D9) |
| `cmake/` | the CMake modules, and the checks `cmake -P` runs: the source rules, the format, coverage and its floor |
| `CMakeLists.txt`, `CMakePresets.json` | the target, its options, and the presets debug, release, asan-ubsan, coverage and verify |
| `.clang-format`, `.clang-tidy`, `test/.clang-tidy` | the formatter and the linters (D13) |
| `tools/requirements-lint.txt` | the pinned clang-format and clang-tidy |
| `doc/` | the manual |

---

## 3. Code rules

The tools named by "the formatter", "the linters", "the type checker" and
"the checks tool" are D4's.

### R-STYLE: formatting and idiom

| ID | Rule | Checked by |
|---|---|---|
| R-STYLE-1 | Code **shall** be formatted by the formatter. | the formatter |
| R-STYLE-2 | Names **shall** follow the language's own conventions; code implementing a message, field or setting is named after it, as the spec names it. | the linters; review |
| R-STYLE-3 | Library code **shall not** print, or write to a stream it was not given. | the linters or the checks tool |
| R-STYLE-4 | What is not public **shall** be private, by the language's own means, so the public API is what is documented. | review |

### R-SIZE: short functions and files

| ID | Rule | Checked by |
|---|---|---|
| R-SIZE-1 | A function **shall** have at most 50 lines of code, blank lines and comments not counted. | the linters or the checks tool |
| R-SIZE-2 | A source file **shall** have at most 600 lines; split by the spec's sections or by layer. | the linters or the checks tool |
| R-SIZE-3 | A function **shall** take at most 5 parameters; else a structure or an options object. | the linters |
| R-SIZE-4 | A module **should** hold one concept, stated in its doc comment. | review |
| R-SIZE-5 | A function's complexity **shall** be at most 15, and its blocks nested at most 5 deep. | the linters |

### R-DOC: comments and documentation

| ID | Rule | Checked by |
|---|---|---|
| R-DOC-1 | Every public item **shall** have a doc comment in the language's documentation format, saying each parameter, what it returns, and each failure it gives. | the linters; review |
| R-DOC-2 | Comments **shall** say *why*: the section implemented, the choice made, the invariant relied on. | review |
| R-DOC-3 | The API reference, where the language's tools build one, **shall** build without warnings. | the checks tool |
| R-DOC-4 | Behaviour the protocol or turborel-client governs **shall** cite it by its heading, `[PROTOCOL Frames]`, `[CLIENT Pools]`; the driver's own choices cite their decision (D6). | review; the checks tool |
| R-DOC-5 | No commented-out code, and no `TODO`, `FIXME` or `XXX` comments: open work goes in TODO.md, and code never cites TODO.md. | the linters or the checks tool |
| R-DOC-6 | Every source file **shall** begin with a doc comment giving its responsibility and the sections it implements. | review |
| R-DOC-7 | A change a program can see **shall** update the manual, `doc/`, in the same commit; every claim is checked against the code, and every example is run. The manual keeps Turborel's layout, which turborel.com's documentation site reads: each page opens with its `# Title` and a line holding only `[Contents](README.md)`, each volume's README.md lists its pages, links within `doc/` and to other files of this repository are relative links that resolve (the site points the latter at GitHub, at the same tag), a link to Turborel's own manual is its GitHub address under `https://github.com/turborel/turborel/blob/main/doc/`, `user/01-installing.md` keeps its name, and `doc/` holds no images. | review; the checks tool |

### R-CONST: no magic numbers or strings

| ID | Rule | Checked by |
|---|---|---|
| R-CONST-1 | Numbers in library code other than `0` or `1` **shall** be named constants, documenting the value's source. | the linters; review |
| R-CONST-2 | Every protocol tag **shall** be a named constant, defined once: a message's kind, a data type's and a value's tag, a tail tag, a capability's name, a mechanism's name, a status name, and each SQLSTATE the driver raises. | review |
| R-CONST-3 | Strings matched or looked up, such as settings' names, URI parts and environment variables, **shall** be named constants. Messages for people are not. | review |
| R-CONST-4 | Related constants **shall** live together in their layer's module, under a comment naming the section they come from. | review |
| R-CONST-5 | Tests may use literal fixtures, as the spec prints its bytes; an expected value restating a constant **should** use it. | review |

### R-ERR: errors

| ID | Rule | Checked by |
|---|---|---|
| R-ERR-1 | Every failure the library gives **shall** be the driver's error (D2): no stray exception, panic, abort, assertion or other error of the language or its libraries reaches a program, and no library code ends the process. | the linters; tests; review |
| R-ERR-2 | Each failure **shall** carry its SQLCODE, SQLSTATE and message, and, once read from a server, the rest of it: the rule it cites, its position, whether a transaction is still active, and its diagnostics. A failure the driver raises itself carries the SQLCODE and SQLSTATE turborel-client gives the same failure. | tests |
| R-ERR-3 | No failure **shall** be silenced. One caught and given again as the driver's keeps its cause; a result left unread says why it cannot matter. | the linters; review |
| R-ERR-4 | A connection that fails **shall** be closed, and every request waiting on it given that failure; a failed call leaks nothing and leaves nothing waiting. | tests; review |

### R-NUM: integers and numbers

| ID | Rule | Checked by |
|---|---|---|
| R-NUM-1 | Every integer of the protocol **shall** be read and written at its width, little-endian, explicitly: never by the platform's byte order, by casting a buffer, or by bitwise operators on numbers wider than the language's operators keep exact. | review; tests |
| R-NUM-2 | A value out of range for its field, or for the type it is read into, **shall** be refused with the driver's error, never truncated, wrapped or rounded. | tests |
| R-NUM-3 | An exact number (units `i128`, scale `u32`) and a DECFLOAT **shall** never pass through a binary floating-point type. | review; tests |
| R-NUM-4 | An approximate number **shall** keep its bits: NaN and -0.0 are read and written as the double they are, and floats are compared bit for bit, never for equality. | tests; review |
| R-NUM-5 | In a language whose integers are bounded, arithmetic on sizes, counts and offsets **shall** be shown not to overflow before it is done. | the linters; review |

### R-LINT: warnings

| ID | Rule | Checked by |
|---|---|---|
| R-LINT-1 | A warning is an error: of the compiler, the linters and the type checker, and of the runtime itself while the tests run. | the checks tool |
| R-LINT-2 | The strictest standard linters and type checking the language has **shall** be on; a check left off says why in the configuration that leaves it off. | review |
| R-LINT-3 | A warning **shall** be silenced only on its line, naming its rule and saying why; a suppression no longer needed **shall** be removed. | the linters; review |

### R-DEP: dependencies

| ID | Rule | Checked by |
|---|---|---|
| R-DEP-1 | The library **shall** depend on nothing but the language's standard library and runtime, unless a decision here admits a dependency, with its reason, version, license and review. | review |
| R-DEP-2 | A dependency, of the library or of its tools, **shall** be production-grade by its makers' own claims: a stable release they support. | review |
| R-DEP-3 | Tools **shall** be pinned by version, with the ecosystem's lock file under version control where it has one; a change of a tool's version is a commit of its own. | review |

### R-SAFE: what a server sends, and secrets

| ID | Rule | Checked by |
|---|---|---|
| R-SAFE-1 | A length or count a server, or a file the driver reads, gives **shall** be checked against what follows and the limit taken before anything is held for it, so no reply takes the program's memory without bound. | tests; review |
| R-SAFE-2 | What a server sends **shall** never be run, loaded or deserialized as code. | the linters; review |
| R-SAFE-3 | TLS **shall** be used and verified as the settings ask, and a connection **shall not** fall back to less than its mode allows. | tests |
| R-SAFE-4 | A secret (a password, anything derived from one, a SCRAM key or proof, a cancel key) **shall** never be logged, put in a failure's message or an object's display form, or sent where it was refused; where the language allows, it is cleared from memory once used. | tests; review |
| R-SAFE-5 | No module-level mutable state: what lasts belongs to an object a program made, such as a connection, a pool or a key cache. | review |
| R-SAFE-6 | The library **shall** start no thread, timer or task but for an object a program made, and a closed connection, cursor or pool leaves none behind. | tests; review |

### R-PLAT: platforms

| ID | Rule | Checked by |
|---|---|---|
| R-PLAT-1 | The driver **shall** run on Linux, macOS and Windows, on every runtime D3 supports; CI checks each (D4). | CI |
| R-PLAT-2 | Code of one platform, such as a Unix socket or a named pipe, **shall** be chosen by the platform, and elsewhere refused with the driver's error, saying so; never silently something else. | tests; review |
| R-PLAT-3 | Paths **shall** be made by the language's path functions, and files kept with LF line endings (`.gitattributes`). | review |
| R-PLAT-4 | Code built for one platform only **shall** say, when handed back, that it was not run on the others. | review |

### R-CF: control flow

| ID | Rule | Checked by |
|---|---|---|
| R-CF-1 | A boolean decision **shall** have at most 4 conditions; else a named predicate. | review |
| R-CF-2 | A `switch` on an enumeration **shall** name every enumerator, with no `default`. | `-Wswitch`; review |
| R-CF-3 | Conditions **shall** have no side effects. | review |

### R-RAII: resources and exceptions

| ID | Rule | Checked by |
|---|---|---|
| R-RAII-1 | Every resource **shall** be owned by an object that releases it: no owning raw pointer, `new`, `delete` or `malloc`; a turborel-c handle is freed by its owner's destructor. | `cppcoreguidelines-owning-memory`, `cppcoreguidelines-no-malloc` |
| R-RAII-2 | No exception **shall** leave a destructor, a `noexcept` function, or a callback turborel-c calls; such a callback catches everything and reports it through turborel-c. | `bugprone-exception-escape`; review |
| R-RAII-3 | Exceptions **shall** be thrown by value and caught by reference, and copying the driver's error **shall** never throw. | `misc-throw-by-value-catch-by-reference`; tests |

### R-CPP: the language

| ID | Rule | Checked by |
|---|---|---|
| R-CPP-1 | Code **shall** be standard C++20, with no compiler extension (`CXX_EXTENSIONS OFF`). | the compilers |
| R-CPP-2 | A header **shall** have an include guard, `TURBOREL_<NAME>_HPP`, and every file **shall** include what it uses. | `portability-avoid-pragma-once`, `misc-include-cleaner` |
| R-CPP-3 | No `using namespace` directive, but of a literals namespace in a test. | `google-build-using-namespace` |
| R-CPP-4 | No implicit narrowing or sign conversion, no C-style cast, and `reinterpret_cast` only at turborel-c's boundary, saying why. | `-Wconversion`, `-Wsign-conversion`, `-Wold-style-cast`, `cppcoreguidelines-pro-type-*` |
| R-CPP-5 | The API **shall** take and give `std::span` and `std::string_view` at its boundaries, and own what it keeps. | review |

---

## 4. Test rules

### R-TEST: levels and layout

| ID | Rule | Checked by |
|---|---|---|
| R-TEST-1 | A test **shall** sit at the lowest level that shows its requirement: a codec test of bytes, a test of a layer over a stream the test controls, a test of the API, or a live test against a server. | review |
| R-TEST-2 | Tests **shall** derive from the spec and turborel-client, not from the code: normal cases, boundaries included, and robustness cases with the exact SQLCODE and SQLSTATE expected. | review |
| R-TEST-3 | A test's name **shall** say the behaviour checked. | review |
| R-TEST-4 | Test code may assert, print and use literal fixtures; every other rule applies. | the linters' configuration |
| R-TEST-5 | Tests **shall** be deterministic and independent: no clock, randomness, network beyond loopback, or order between them. A case generator uses a fixed seed. A live test runs against a server on loopback that its own set-up starts, on its own port and data directory. | review |
| R-TEST-6 | Every fixed defect **shall** get a regression test that fails without the fix, its doc comment saying so. | review |
| R-TEST-7 | A fixture or helper shared by tests **shall** be written once, in one place. Tests use the one framework D9 or later names, no other. | review |
| R-TEST-8 | Live tests **shall** run against `ghcr.io/turborel/turborel:nightly` in CI, set up by the repository's own server set-up, with test certificates, SCRAM users and SERVER.TRUST_LOCAL. | CI |
| R-TEST-9 | SCRAM **shall** be tested by RFC 7677's test vector, and settings by the URI, service file, password file and environment cases of turborel-client's settings tests, so every driver reads settings as turborel-client does. | review |

### R-TRACE: traceability

| ID | Rule | Checked by |
|---|---|---|
| R-TRACE-1 | Every test **shall** cite what it verifies, by its doc comment, beginning `Verifies:`, beginning with a tag of D6: `[PROTOCOL <heading>]`, `[CLIENT <heading>]`, `[DESIGN] D<n>` or `[TOOL] R-<id>`. | the checks tool |
| R-TRACE-2 | A citation **shall** name a heading, decision or rule that exists. | the checks tool |
| R-TRACE-3 | Code implementing a requirement **shall** cite it (R-DOC-4). | review |
| R-TRACE-4 | CONFORMANCE.md **shall** change with the change that implements or changes a row. A row is marked done only where the driver's code for it exists and tests citing its section pass, or, of a driver built over another, as CONFORMANCE.md says. | review |

---

## 5. Coverage

| ID | Rule | Checked by |
|---|---|---|
| R-COV-1 | Coverage **shall** be measured by `cmake --workflow --preset coverage`, over the library and every test. | the checks tool |
| R-COV-2 | Coverage **shall not** fall below the floor (D7); a change raising it **should** raise the floor. | the checks tool, in CI |
| R-COV-3 | A gap is closed by a test derived from a requirement, or by removing dead code; never by a test of no requirement, and never by excluding the code. | review |

---

## 6. Process and tools

| ID | Rule | Checked by |
|---|---|---|
| R-PROC-1 | `cmake --workflow --preset verify` **shall** pass before every commit; CI adds every platform, every supported runtime and the coverage floor (D4). | pre-commit hook; CI |
| R-PROC-2 | Every change **shall** be reviewed against the checklist below. | review |
| R-PROC-3 | Commits **should** be small and single-purpose, in CONTRIBUTING.md's form (`area: summary`, then why), citing the rules applied or changed. | review |
| R-PROC-4 | A threshold change **shall** change RULES.md and the configuration that holds it together. | review |
| R-PROC-5 | Project automation **shall** be written in the language's own tooling, never shell scripts; the git hook only hands over to the checks tool. | review |
| R-PROC-6 | CI's actions **shall** be pinned by their full commit SHA, with the version in a comment. | review |
| R-PROC-7 | A release **shall** be published only as VERSIONING.md says, by the owner (D5). | review |

The review checklist:

- the spec or turborel-client cited in code (R-DOC-4) and tests
  (R-TRACE-1);
- normal and robustness tests (R-TEST-2), and a failing-first test for a
  fix (R-TEST-6);
- every failure the driver's, carrying its condition (R-ERR);
- no magic numbers or strings, protocol tags named (R-CONST);
- every length a server sends bounded, no secret disclosed (R-SAFE);
- comments explain why (R-DOC-2);
- CONFORMANCE.md (R-TRACE-4), the manual (R-DOC-7) and TODO.md updated
  where they change.

---

## 7. The decisions

### Shared by every Turborel driver

#### D1. The native protocol, version 1, in layers that follow turborel-client

The driver speaks Turborel's native protocol, version 1, as
`doc/client/05-protocol.md` in Turborel's repository lays it out,
through turborel-c, the C driver it wraps, with no protocol code of its own. Where that page and turborel-protocol's code differ,
the code is right, and the driver follows it; CONFORMANCE.md names each
such place.

The reference is turborel-client. The driver does all it does before it
is called done, row by row in CONFORMANCE.md, and is built in the same
layers as turborel-client's modules, so a reviewer checks one against
the other. Each layer is a commit area of CONTRIBUTING.md.

| Layer | What it covers | turborel-client |
|---|---|---|
| `codec` | the 5-byte frame header; fields; the tail and its tag order; the data types and values; JSON values; zlib `z` frames within the length bound; the 16 MiB body limit | `protocol/src` |
| `transport` | TCP on 7422; Unix sockets; the Windows named pipe `\\.\pipe\turborel`; TLS begun on the same port; the connect and read timeouts | `target.rs`, `timeouts.rs`, `pipe.rs` |
| `auth`, `tls` | trust, password, certificate, SCRAM-SHA-256 and -PLUS; secrets refused over plain TCP; the SCRAM key cache; TLS | `mechanism.rs`, `scram.rs`, `tls.rs` |
| `session` | Hello, Ready, Status, notices, conditions, diagnostics, autocommit, cancel, terminate, ping, reset, change-user, query attributes, multi-statement | `connection.rs`, `session.rs`, `cancel.rs`, `told.rs` |
| `statements` | queries and streamed queries; prepare, execute, fetch, close, deallocate; numbered cursors; describe; completions | `statements.rs`, `frames.rs` |
| `lobs`, `copy` | locators, their reads, writes and streams; copy, import and export | `lobs.rs`; `copy.rs`, `import/`, `export.rs` |
| `settings` | the URI, options, variables, service and password files, several hosts, roles, TLS modes, channel binding | `settings/`, `failover.rs`, `role.rs` |
| `pipeline`, `pool` | pipelines; pools; the SQL-agent | `pipeline.rs`; `pool/`, `agent.rs` |
| `api` | the standard API (D2) | the language's own |

turborel-cpp holds no layer of the protocol itself. Every layer from
`codec` to `pipeline` and `pool` is turborel-c's, in C17, and this
driver wraps turborel-c's C API in C++20, so one implementation of the
protocol serves both drivers, a defect is fixed once, and the two never
disagree on the wire. The TLS library, OpenSSL or AWS-LC, and zlib come
with turborel-c, by its own decisions. What this driver adds is the
`api` layer: owners of turborel-c's handles, exceptions, ranges of
rows, `std::chrono` datetimes and a decimal type. Until turborel-c's
API reaches its milestone M3, the driver holds its error type and value
types alone (D11, D15).

#### D2. The language's standard API, over a low-level client of turborel-client's features

The driver's standard interface is RAII C++20 over turborel-c: owners that close what they hold, failures thrown as exceptions, a query's rows as a range, datetimes in `std::chrono`, so a program, and the
tools built for C++'s database interface, use it as they use
any other driver. It sits over a low-level client that has every feature
of turborel-client. What the standard interface has no place for, such
as pipelines, pools, copy, large objects by locator, notices and the
session's status, is reached through the low-level client or the
driver's own extensions of the standard interface.

Every failure is the driver's error, carrying its SQLCODE and SQLSTATE
(R-ERR). The standard interface is built at milestone M7 and checked in
M8's audit; until then its names may change.

C++ has no standard database interface, so the standard API is the
language's own idioms, as its standard library writes them:

- each turborel-c handle is owned by a C++ object that frees it in its
  destructor, movable and not copyable;
- every failure is thrown as `turborel::error`, a `std::runtime_error`
  (D10), so a call either succeeds or throws;
- a query's rows are a range, a `std::ranges::input_range`, read as they
  come;
- datetimes are `std::chrono` types, and exact numbers, an `i128` of
  units and a `u32` scale on the wire, a decimal type of the driver's
  own, since the standard has none that holds them all;
- the API takes and gives `std::span` and `std::string_view` at its
  boundaries, and owns what it keeps.

Everything is in the namespace `turborel`, with headers under
`include/turborel/`, `turborel/turborel.hpp` including the rest, and
the CMake target `turborel::turborel-cpp`. During the alpha no ABI is
kept between releases.

#### D3. The runtime minimum follows the ecosystem's own support

The driver supports a C++20 compiler, GCC 13, Clang 18, Apple Clang or MSVC 19.40 or later, and CMake 3.25 or later. The rule is the ecosystem's own:
support every release its makers still support, and drop each at its
end of life. Where a major framework of the language sets a higher
floor, the driver matches it. The minimum changes in a commit of its
own, which changes this decision, the package's metadata, CI and
README.md together.

The language level is C++20, not C++23, because MSVC's C++23 is not
complete as of November 2026, and the driver builds with GCC, Clang and
MSVC alike. C++20 has what D2 needs: concepts, ranges, `std::span` and
the calendar types of `<chrono>`. The compilers are those CI runs: GCC
13 and 14 and Clang 18, as Ubuntu 24.04 ships them, and the Apple Clang
and MSVC of GitHub's current macOS and Windows images. GCC 13 is the
oldest GCC release series still maintained upstream, and Visual Studio
2022 17.10 (MSVC 19.40) the oldest MSVC supported. CMake 3.25 is the
first with workflow presets and `SYSTEM` for fetched dependencies, both
used here (D9).

#### D4. One command runs every check, before each commit and in CI on three platforms

`cmake --workflow --preset verify` runs every check a change must pass, stopping at the
first failure: the formatter, the linters, the type checker where the
language has one, the checks tool's own rules (lengths a linter does not
measure, citations and their targets, open-work markers, the manual's
titles, `[Contents](README.md)` lines and links), and every test.
`cmake --workflow --preset coverage` measures coverage and holds it to its floor (D7).

`.githooks/pre-commit` hands over to the same command. It is a script
only because git runs hooks as scripts. CI runs it on Linux, macOS and
Windows, over every runtime D3 supports, with the live tests against the
nightly image and coverage on Linux. Every action is pinned by its full
commit SHA, its version in a comment, and Dependabot proposes each bump.

The checks tool is CMake itself: the presets of `CMakePresets.json`,
and `cmake -P` scripts in `cmake/` that run the same on every platform
(R-PROC-5). The preset `verify` builds with clang-tidy as each of the
project's targets compiles, every warning an error
(`CMAKE_COMPILE_WARNING_AS_ERROR`), then runs as CTest tests the format
check (`cmake/CheckFormat.cmake`), the source rules
(`cmake/CheckSources.cmake`: file lengths, file docs, citations and
their targets, `NOLINT` reasons, open-work markers and conflict
markers), the manual's rules (`cmake/CheckManual.cmake`: its titles,
`[Contents](README.md)` lines, contents and links), the API reference by Doxygen where Doxygen is found, the
install package built against by a program, and every test. The tools
are D13's.

CI runs `verify` on Linux and macOS, where clang-tidy reads the
compiler's commands; on Windows it builds and tests with MSVC, its
warnings errors. It builds and tests with GCC 13 and 14, Clang 18,
Apple Clang and MSVC, in Debug and Release, runs the sanitizers (D14),
and measures coverage on Linux. Live tests come with M2, once the
driver connects through turborel-c.

#### D5. The version is the Turborel release's: 0.0.0 in the alpha, speaking protocol 1

The driver has no version scheme of its own. Its version is the number
of the Turborel release it is made for, the same as the server's: a
Turborel release and each driver's release for it share the number,
**MAJOR.MINOR.PATCH**, as Turborel's VERSIONING.md numbers it. The alpha
is 0.0.0, as Turborel's is, and speaks protocol version 1.

The number is written once, in `project(... VERSION ...)` in `CMakeLists.txt`. Nothing is
published, and no release is tagged, until every Turborel driver is
done; then the owner publishes them together (VERSIONING.md).

#### D6. Tests and code cite Turborel's documents by their headings

A test cites what it verifies, and code what it implements, by the
heading of the section that says it, as printed:

- `[PROTOCOL <heading>]`, a heading of The Native Protocol,
  `doc/client/05-protocol.md`, as `[PROTOCOL Frames]`;
- `[CLIENT <heading>]`, a heading of The Client Library,
  `doc/client/02-library.md`, or of Settings, Pipelines and Pools,
  `doc/client/02-library-connections.md`, which share no heading, as
  `[CLIENT Pipelines]`;
- `[DESIGN] D<n>`, a decision of this file;
- `[TOOL] R-<id>`, a rule the checks tool holds, for the tool's own
  tests.

What else a test relies on follows the tag, as an RFC by its section:
`[PROTOCOL Mechanisms] [RFC 7677 §3]`. The heading `Lob 'L'` is printed
twice in The Native Protocol, under Requests and under Replies, so a
citation of it says which it means.

The checks tool keeps the headings of those three documents, copied at
the Turborel commit CONFORMANCE.md names, and refuses a citation of a
heading, decision or rule that is not there (R-TRACE-2). In C++,
a test cites by its doc comment, beginning `Verifies:`.

#### D7. A coverage floor, enforced in CI, that only rises

`cmake --workflow --preset coverage` measures the library's coverage over every test,
and CI fails below the floor. In every Turborel driver the floor is at
least 90 % of lines, and of branches where the tool measures them. It
only rises: a change that raises coverage well above it raises it. A
floor below 100 % says why.

The floor is 100 % of the library's lines, functions and regions, in
`cmake/CoverageFloor.cmake`. Coverage is Clang's source-based coverage:
the preset `coverage` builds with `-fprofile-instr-generate
-fcoverage-mapping`, runs the tests, and `cmake/Coverage.cmake` merges
their profiles with `llvm-profdata` and reads the library's coverage of
`include/` and `src/` with `llvm-cov` against the floor. Clang's,
rather than gcov's, because its regions are finer than lines and one
script reads it on every platform Clang runs on.

#### D8. Where these rules depart from C++'s conventions

The driver follows its language's library conventions for layout,
naming, packaging, documentation and testing. Where one conflicts with a
rule here, the rule is kept:

- Libraries commonly leave the length of functions and files to taste.
  These are bounded (R-SIZE).
- Libraries commonly fail an argument out of range with the language's
  own error. Here every failure is the driver's, an argument's too, with
  the SQLCODE and SQLSTATE turborel-client gives it (R-ERR-1), so a
  program handles one kind of failure.

- The C++ Core Guidelines allow `assert` and contracts that end the
  program on a broken precondition. Library code here never ends the
  process (R-ERR-1): a broken precondition the driver can see is a
  `turborel::error`.
- C++ libraries often keep their code in headers. Here, what need not be
  inline is in `src/`, so the 600-line limit (R-SIZE-2) counts real code
  and the ABI hides what it can.
- Catch2 can tag a test with what it verifies; the tests here cite in a
  `/// Verifies:` paragraph above each `TEST_CASE` instead, as every
  Turborel driver does (D6).
- Every Turborel driver's first test encodes a frame header; this one
  has none, since its codec is turborel-c's (D15).

### This driver's own

#### D9. Built by CMake 3.25 or later, and installed as the CMake package `turborel-cpp`

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

Ports for vcpkg and Conan come once every Turborel driver is done (D5).

#### D10. Every failure is a `turborel::error`, a `std::runtime_error`

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

#### D11. turborel-c is found installed, or fetched at a pinned commit, and is off until its M3

turborel-c is consumed by CMake (`cmake/TurborelDependencies.cmake`), under
the option `TURBOREL_CPP_WITH_C`, off until turborel-c reaches its
milestone M3 and has an API to wrap:

- by default, `find_package(turborel-c <version> EXACT CONFIG)` finds an
  installed package, as `cmake --install`, vcpkg or Conan leave it, at the
  same turborel release (D5);
- where `TURBOREL_CPP_C_COMMIT` names a full commit, FetchContent fetches
  turborel-c at that commit, still trying the installed package first.

Both, rather than one: package managers and distributions give an
installed package, and a pinned commit lets the driver build from a clean
checkout with nothing installed, as CI does. A commit, not a tag, because
turborel-c has no tags during the alpha and a tag can be moved. Its target
is taken to be `turborel::turborel-c`, to be confirmed against turborel-c
when the option is first turned on.

#### D12. Tests in Catch2 3, fetched at a pinned release

The tests are Catch2 3.16.1 (BSL-1.0), fetched by FetchContent as the
release's source archive, its SHA-256 checked
(`cmake/TurborelDependencies.cmake`), built with the tests' C++20 and only
when tests are built. Catch2 is what C++ projects commonly test with, its
v3 releases are its stable line, and it needs nothing else. It is a test
dependency alone: nothing installed links it. Dependabot cannot follow a
FetchContent pin, so it is raised by hand, in a commit of its own.

`catch_discover_tests` makes each `TEST_CASE` a CTest test of its own.

#### D13. clang-format and clang-tidy of LLVM 23, and Doxygen, check what the compiler does not

- **clang-format 23.1.5** formats every C++ source by `.clang-format`
  (R-STYLE-1).
- **clang-tidy 23.1.4** runs as each of the project's targets builds, by
  `.clang-tidy`: `bugprone`, `cert`, `clang-analyzer`, `concurrency`,
  `cppcoreguidelines`, `misc`, `modernize`, `performance`, `portability`
  and `readability`, every finding an error (R-LINT-2). `test/.clang-tidy`
  turns off what Catch2's macros cannot meet, each with its reason.
- **Doxygen** builds the API reference, failing on any undocumented public
  item or malformed doc comment (R-DOC-1).

Both LLVM tools are pinned, from PyPI's wheels, in
`tools/requirements-lint.txt`, which Dependabot follows, each release
of LLVM 23; another major version formats and lints differently. The build refuses a clang-format or
clang-tidy of another major version. Doxygen is checked wherever it is
found, and CI requires it (`TURBOREL_CPP_REQUIRE_DOXYGEN`).

#### D14. The tests run under AddressSanitizer and UndefinedBehaviorSanitizer

The preset `asan-ubsan` builds the library and the tests with both, every
finding fatal; CI runs it with GCC and Clang on Linux and with Apple Clang
on macOS. MSVC has AddressSanitizer alone, which the option gives it.

#### D15. No codec test here: the codec and its tests are turborel-c's

Every turborel driver's first test encodes and decodes a frame header per
`[PROTOCOL Frames]`. This driver has none, since it has no codec (D1):
turborel-c holds the frames, fields and values, and tests them. The tests
here start at what the driver adds, its error type first, and test the
protocol through turborel-c's API once it exists.

#### D16. Values in C++ types of the driver's own, each mapped to the protocol's fields

Where the standard library has a type that holds every value of a
protocol type, the driver uses it; where it has none, the driver has its
own, which holds every value the protocol can carry, with no loss:

| Protocol | C++ | Why |
|---|---|---|
| `i128` | `turborel::int128` (`int128.hpp`) | C++20 has no 128-bit integer, and MSVC no extension of one |
| exact number, units `i128` and scale `u32` | `turborel::decimal` (`decimal.hpp`) | the standard has no decimal type |
| DATE | `turborel::date`, a `std::chrono::year_month_day` | |
| TIME, WITH TIME ZONE or not | `turborel::time`: UTC since midnight, displacement, zoned, precision | `std::chrono::hh_mm_ss` holds no displacement |
| TIMESTAMP, WITH TIME ZONE or not | `turborel::timestamp`: a `std::chrono::sys_time` of microseconds, displacement, zoned, precision | as TIME |
| INTERVAL | `turborel::interval`: its qualifier and `int128` units; `std::chrono::months` or `microseconds` where they fit | its units may exceed 64 bits |
| UUID | `turborel::uuid`, 16 `std::byte`s | |

A datetime's fields, `turborel::datetime_fields`, are the protocol's
body of tags 8, 9 and 11, its microseconds counted from 0001-01-01 UTC as
turborel's `value/datetime.rs` counts them, not from 1970. Each datetime
type maps to them and back, failing with SQLSTATE 22008 out of 0001 to
9999 and 22009 for a displacement out of -12:59 to +14:00, turborel's
own ranges, with turborel's SQLCODE of a data exception, -301 (D10). Text, of
a decimal or a UUID, is written and read as turborel's own code writes
and reads it, so a value reads the same from every client. These types
carry values and do no arithmetic. The bytes on the wire stay
turborel-c's (D1); `int128` alone writes its 16 bytes, as the type
turborel-c's halves become.

---

## 8. References

- Turborel, `doc/client/05-protocol.md`, *The Native Protocol*,
  version 1: the requirements of the codec and every message.
- Turborel, `doc/client/02-library.md`, *The Client Library*, and
  `doc/client/02-library-connections.md`, *Settings, Pipelines and
  Pools*, and the crates turborel-client and turborel-protocol: the
  features to match, and the reference where the spec is silent or
  differs.
- Turborel, `RULES.md` D89, the JSON value's binary tree, and D184, the
  UUID's tag.
- RFC 1950, *ZLIB Compressed Data Format Specification*; RFC 5802 and
  RFC 7677, SCRAM and SCRAM-SHA-256; RFC 5929 and RFC 9266, the
  `tls-server-end-point` and `tls-exporter` channel bindings.
- The standards of C++ and of its database interface, named in
  D2 and D3.
