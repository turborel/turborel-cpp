# Guide for coding agents

How to work on turborel-cpp as a coding agent, with any tool. If this file
disagrees with [RULES.md](RULES.md) or [CONTRIBUTING.md](CONTRIBUTING.md),
follow those.

turborel-cpp is the C++20 driver for turborel, a relational database
system: a wrapper over turborel-c's C API, with no protocol code of its own
(RULES.md D1). See [README.md](README.md).

## Before you change anything

Read RULES.md §3 (code), §4 (tests), §7 (process) and the §1 decisions for
your area. Read CONTRIBUTING.md for how a change is made and the extra
checks for changes that touch the wire.

## Build and check

Every change must pass `cmake --workflow --preset verify` before it is
handed back, and `asan-ubsan` and `coverage` where it changes code.

```
python -m pip install -r tools/requirements-lint.txt   # clang-format and clang-tidy 23.1.4 (D8)
cmake --workflow --preset verify       # clang-tidy, format, source rules, docs, every test
cmake --workflow --preset asan-ubsan   # the tests under ASan and UBSan (D9)
cmake --workflow --preset coverage     # coverage against cmake/CoverageFloor.cmake (D10)
cmake --workflow --preset debug        # build and test alone
cmake --workflow --preset release
cmake --build --preset verify --target format   # format the sources in place
ctest --preset debug -R check.sources  # the source rules alone
```

Each preset builds in `build/<preset>/`. clang-format and clang-tidy must be
LLVM 23; the configure step refuses another major version. Doxygen is
checked where it is found, and always in CI. Coverage needs Clang.

CI runs `verify` on Linux and macOS, the build and tests on Linux, macOS
and Windows with GCC, Clang and MSVC, the sanitizers, and coverage on
Linux (D13). Automation is CMake, never shell scripts (D11).

## Where things are

| Path | What |
|---|---|
| `include/turborel/` | the public headers; `turborel.hpp` includes the rest |
| `src/` | the library's sources |
| `test/` | the tests, one Catch2 binary, one source per area (D7) |
| `test/package/` | a program built against the installed package (D4) |
| `cmake/` | CMake modules, and the checks run by `cmake -P` |
| `CMakePresets.json` | the presets: debug, release, asan-ubsan, coverage, verify |
| `.clang-tidy`, `.clang-format` | the lints and format (D8) |
| `tools/requirements-lint.txt` | the pinned clang-format and clang-tidy |
| `doc/` | the manual |

The protocol, its spec and the reference client are turborel's:
`doc/client/05-protocol.md` (the spec), `doc/client/02-library.md` and
`02-library-connections.md` (turborel-client's features), and
`client/src/` and `protocol/src/` (the reference code). Where the spec and
the code differ, the code is right. The codec itself is turborel-c's.

## Rules that are broken most often

`verify` treats every warning and every clang-tidy finding as an error.
RULES.md §3 and §4 have the full list.

- Every failure is thrown as `turborel::error`, with its SQLCODE and
  SQLSTATE (R-ERR-1). No `abort`, `exit`, `terminate`, `assert` or
  `std::unreachable` in library code (R-ERR-3).
- No exception leaves a destructor, a `noexcept` function or a callback
  turborel-c calls (R-ERR-4).
- Every resource is owned: no owning raw pointer, `new`, `delete` or
  `malloc`; each turborel-c handle has its owner (R-SAFE-1).
- No implicit narrowing or sign conversion, no C-style casts (R-NUM).
- Functions of at most 50 lines and 5 parameters, files of at most 600
  lines (R-SIZE).
- No magic numbers or strings; protocol tags are named constants
  (R-CONST).
- Every public item has a `///` Doxygen comment, with `@param` and
  `@throws` (R-DOC-1). Every C++ file starts with `/// @file` (R-DOC-6).
- Comments say why (R-DOC-2). No open-work comments or commented-out code
  (R-DOC-5).
- Include guards, not `#pragma once`; include what you use (R-STYLE-4).
- A `NOLINT` names its check and says why (R-DEV-1).
- No new dependency without a decision in RULES.md (R-SAFE-5).
- No protocol code here: frames, fields and values are turborel-c's (D1).

## Tests

Every `TEST_CASE` has a `/// Verifies:` paragraph right above it, starting
with a tag `check.sources` accepts (R-TRACE-1): `[PROTOCOL <heading>]`, a
heading of `05-protocol.md` such as `[PROTOCOL Composite fields]`;
`[CLIENT <heading>]`, a heading of `02-library.md` or
`02-library-connections.md` such as `[CLIENT Pipelines]`; or
`[DESIGN] D<n>`, a decision of RULES.md. The check refuses a heading or a
decision that does not exist.

Tests are deterministic, with no clock, randomness or network beyond
loopback (R-TEST-4). A live test runs its own turboreld; an agent's runs
use port 17439 and a data directory under `.claude/` or a temporary
directory of its own, and stop the server before handing back. A fixed
defect gets a regression test (R-TEST-5).

## Citing the spec

Check every citation against the spec's headings before writing it. Never
cite from memory. Code never cites TODO.md, and `check.sources` refuses it
(R-DOC-5).

## Documents

A finished feature updates these in the same change:

- RULES.md, if it made a decision, including the row in §1's table
- CONFORMANCE.md, for a row of the parity matrix implemented (R-TRACE-2)
- TODO.md, which only holds open work
- [doc/](doc/README.md), for anything a program sees (R-DOC-7)

Every claim in a document is checked against the code, and every example
is run.

## Commits

- Commit only when the maintainer asks, and never push without approval.
- Follow CONTRIBUTING.md's form, with one logical change per commit.
- Add no AI or agent attribution to a commit, a pull request or a file.
  That means no `Co-Authored-By` for an agent, no "Generated with" and no
  `Signed-off-by`.
- Stage with an explicit path list, `git add -- <path> ...`. Never use
  `git add -A` or `git add .` on the whole tree.
- Never stage `LICENSE`, which the owner commits, or scratch files.
- Code built on one platform only can't be verified locally on another.
  Say so when handing it back.
