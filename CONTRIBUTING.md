# Contributing to turborel-cpp

turborel-cpp carries programs' data to and from a turborel server, and a
driver that gets a value wrong, leaks a secret, or falls over on a reply
fails every program built on it. So a change is judged first on whether it
keeps values exact, secrets private and the client sound, and then on what
it adds. The process follows the Linux kernel's, adapted to pull requests
and to a driver.

[Before you start](#before-you-start) · [Setting up](#setting-up) ·
[Making a change](#making-a-change) ·
[Changes that touch the wire](#changes-that-touch-the-wire) ·
[Commit messages](#commit-messages) · [Pull requests](#pull-requests) ·
[Review](#review) · [Reporting issues](#reporting-issues) ·
[Security](#security) · [License](#license)

---

## Before you start

Read [RULES.md](RULES.md) §3 (code), §4 (tests), §7 (process) and the
decisions for your area. Every rule applies to every change, and
`cmake --workflow --preset verify` checks those a tool can.
[CONFORMANCE.md](CONFORMANCE.md) says what is implemented of the protocol
and of turborel-client, and [TODO.md](TODO.md) lists open work.

Open an issue first for anything larger than a fix. That covers new
features, changes to a decision in RULES.md, new dependencies (R-SAFE-5)
and changes to the public API. Agreeing on the design first saves
everyone a rewrite.

The driver matches turborel-client and the protocol spec, turborel's
`doc/client/05-protocol.md`. Read the spec's section before writing code,
and cite it by its heading, as `[PROTOCOL Composite fields]`. Where the
spec and turborel-client differ, turborel-client is right. The protocol
itself is turborel-c's (RULES.md D1): a change to how bytes are read or
written belongs there.

## Setting up

```
python -m pip install -r tools/requirements-lint.txt   # the pinned clang-format and clang-tidy
git config core.hooksPath .githooks                    # run verify before each commit (once per clone)
cmake --workflow --preset verify                       # every check a change must pass
cmake --workflow --preset asan-ubsan                   # the tests under the sanitizers
cmake --workflow --preset coverage                     # coverage against its floors (Clang)
cmake --workflow --preset debug                        # build and test alone
```

Doxygen, from your platform's packages, adds the check that every public
item is documented; without it, `verify` leaves that check to CI. The
`build` target `format` formats the sources in place.

CI runs `verify` on Linux and macOS, builds and tests on Linux, macOS and
Windows with each supported compiler, and runs the sanitizers and coverage
(D13). A change that passes on one platform and fails on another isn't
done.

## Making a change

Keep one logical change per commit, and make each commit pass
`cmake --workflow --preset verify` on its own, because anyone bisecting may
land on it. Move code in one commit and change it in another. Don't mix a
reformat or rename with a change of behaviour.

Tests come with the change (R-TEST-1). Cover normal cases, boundaries, and
failures with the exact SQLCODE and SQLSTATE expected (R-TEST-2). Each
`TEST_CASE` has a `/// Verifies:` paragraph above it citing the spec's
heading, turborel-client's, or a decision (R-TRACE-1). A fixed defect gets
a test that fails without the fix (R-TEST-5).

Documents change in the same commit. Update CONFORMANCE.md for a row
implemented (R-TRACE-2), RULES.md for a decision made or changed, and
TODO.md for open work done or found. Code never cites TODO.md.

The manual changes in the same commit too (R-DOC-7). A change to what a
program sees updates each page of `doc/` that states it, with its examples
run again.

Some rules deserve a reminder.

- Every failure is a `turborel::error` (R-ERR-1), no exception leaves a
  destructor (R-ERR-4), and library code never ends the process (R-ERR-3).
- Every resource is owned (R-SAFE-1), and no dependency comes without a
  decision in RULES.md (R-SAFE-5).
- Performance claims cite measurements: what was measured, how, and the
  numbers, before and after.
- Coverage floors only rise (R-COV-2).

## Changes that touch the wire

These need more than passing tests. Say in the commit message what was done
for each that applies.

- A value's conversion, to or from C++, keeps every value of its type
  exact, at its edges too: the limits of an `i128`, a scale's extremes,
  NaN and -0.0, each interval qualifier, a zone's displacement.
- A change of what is sent says what an older server sees, and of what is
  read, what a reply the driver does not expect does: fails the
  connection, never guesses.
- A change to TLS, authentication or settings says which secret it
  touches, and shows it is never sent in the clear or kept where another
  can read it.
- A change of turborel-c's version says what changed in its API, and runs
  the live tests.

## Commit messages

The form is Linux's.

```
area: summary in the imperative, at most 75 characters

The problem: what goes wrong, or is missing, and who it affects. A
wrong value, a secret disclosed, a crash or a refusal is said first,
with the program that shows it.

Why this change, and how it solves the problem. Mention the choices
made and the alternatives rejected, the rules and sections applied
(RULES.md D5, R-ERR-4, [PROTOCOL Composite fields]), and what was
measured, with numbers.

Fixes: 54a4f0239f2e ("session: keep a failure's diagnostics")
Closes: <link to the issue>
Reported-by: A Reporter <reporter@example.org>
```

The subject is `area: summary`, at most 75 characters, in the imperative
("refuse a password over plain TCP", not "refused"), with no full stop. It
says what changes, isn't a file name, and isn't reused within a series.

The body is wrapped at 75 columns and written for someone reading `git log`
years later with no context. Say what the change does in plain prose, since
the diff shows how. Don't write "as discussed" or "v2".

Pick the area where most of the change is.

| Area | Covers |
|---|---|
| `codec` | values over turborel-c's codec: the decimal type, datetimes, intervals, JSON, XML, UUID |
| `transport` | connecting over TCP, a Unix socket or a named pipe; timeouts |
| `auth` | mechanisms of authentication, SCRAM and its key cache |
| `tls` | TLS by mode, certificates, channel binding |
| `session` | the session: Ready, status, notices, conditions, errors, cancel, terminate |
| `statements` | queries, prepared statements, cursors, rows as a range, describe |
| `lobs` | large objects by locator, their readers and writers |
| `copy` | copy, import and export |
| `settings` | the URI, options, variables, the service and password files, several servers |
| `pipeline` | pipelines of requests |
| `pool` | pools of connections, the SQL-agent |
| `api` | the public headers as a whole |
| `test` | the tests and their fixtures |
| `doc` | `doc/`, README.md, RULES.md, CONFORMANCE.md, TODO.md, AGENTS.md, CLAUDE.md, this file |
| `ci` | the workflows, issue forms, labels and Dependabot in `.github/`, the hook |
| `build` | CMake, the presets, the lint and format configs, the pinned tools |
| `treewide` | a mechanical change across many areas |

Trailers go at the end, one per line, never wrapped.

| Trailer | Meaning |
|---|---|
| `Fixes: <12+ hex digits> ("<subject>")` | the commit that introduced the defect fixed |
| `Closes: <link>` | the issue this commit resolves |
| `Link: <link>` | background, such as a discussion or an issue fixed only in part |
| `Reported-by:` | who reported the defect fixed, with `Closes:` |
| `Suggested-by:` | who suggested the idea |
| `Co-authored-by:` | a person who co-wrote it, as GitHub credits one |
| `Reviewed-by:` | a reviewer who checked the change and is satisfied with it |
| `Acked-by:` | a maintainer of the area who approves it without a full review |
| `Tested-by:` | someone who tested it, saying where if that matters |

`Reported-by:` and `Suggested-by:` may be added without asking. The others
name someone who said so on the record. `git config core.abbrev 12` gives
12-digit hashes, and `git log -1 --pretty='Fixes: %h ("%s")' <commit>` writes
the `Fixes:` line.

No commit or pull request carries AI or agent attribution: no
`Co-authored-by:` for an agent, no "Generated with" line, and no
`Signed-off-by:`. Contributors own their changes.

Stage a commit with an explicit list of paths, `git add -- <path> ...`,
never `git add -A` or `git add .` on the whole tree, so nothing stray goes
in.

## Pull requests

A pull request is a patch series, and its description is the cover letter.
The template asks for what and why, the series commit by commit, testing,
compatibility and performance. Each commit message still stands alone.

Base it on `main` and keep it rebased, without merging `main` into it. When
revising, rewrite the commits so the series is again what should be merged,
with no "fix review comments" commits. Force-push, and say in a comment what
changed since the last version.

Trailers given in review go into the commits they cover. Drop them from a
commit that later changes substantially, and say so.

Keep a series to about 15 commits. Split a larger one into series that each
stand alone.

## Review

Reviewers check each commit against RULES.md §7's checklist, and that the
manual is updated (R-DOC-7). For
[changes that touch the wire](#changes-that-touch-the-wire), they also check
that the value, compatibility and secrecy checks were done. Review comments
are about the change, never the person. A reviewer who is satisfied gives
`Reviewed-by:`.

## Reporting issues

Use the issue forms, **Bug**, **Conformance**, **Performance** or
**Feature**. Try the latest `main` first, and add to an existing issue that
matches rather than opening another.

Reproduce with the smallest program that shows it, and the SQL that sets
up the database it connects to.

State the impact first. Put it in the title's first word when it is severe.

| Tag | Meaning |
|---|---|
| `[WRONG RESULT]` | a wrong value, or rows lost or doubled |
| `[CRASH]` | a crash, an abort, a hang or a leak |
| `[REGRESSION]` | worked in an earlier commit (give the last good commit and, if you can, the first bad one) |

Give the commit (`git rev-parse --short=12 HEAD`), the server's version,
the compiler and its version, and the platform and architecture.

## Security

Don't open a public issue for a vulnerability. Report it privately, as
[SECURITY.md](SECURITY.md) says.

## License

turborel-cpp is under the BSD-2-Clause Plus Patent License in
[LICENSE](LICENSE), and a contribution is made under that license.
Contributors keep their copyright and own their changes. Whoever
submits a change answers for all of it, however it was written.
