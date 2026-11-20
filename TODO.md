# TODO

Open work, by milestone. How the driver is built is in
[RULES.md](RULES.md), and what is implemented in
[CONFORMANCE.md](CONFORMANCE.md). Finished work leaves this file. Code
never cites this file (R-DOC-5).

turborel-cpp wraps turborel-c (RULES.md D1), so each milestone starts once
turborel-c exposes what it wraps; the driver starts in earnest once
turborel-c reaches its M3.

## M1: Values

- Turn on `TURBOREL_CPP_WITH_C`, pin turborel-c's commit, and confirm its
  exported target's name (RULES.md D6).
- C++ types of every value over turborel-c's: the decimal type (units
  `i128`, scale `u32`), `std::chrono` datetimes with their displacement,
  intervals, DECFLOAT, arrays, rows, multisets, structured values,
  references, locators, JSON (D89's tree), XML and UUID; tests at each
  type's edges.
- The manual's [Values](doc/client/04-values.md) and
  [Appendix A](doc/appendix/a-types.md).

## M2: Connect

- RAII owners of turborel-c's connection handle; TCP, Unix socket, named
  pipe, TLS; every mechanism, SCRAM-SHA-256-PLUS and the key cache;
  capabilities, Ready and Status; timeouts, terminate, cancel.
- Live tests against turboreld on port 17439, set up in `test/server/`.
- The manual's [Connecting](doc/user/02-connecting.md) and
  [turborel::connection](doc/client/02-connection.md).

## M3: Statements

- Query and streamed query, rows as an input range; prepared statements,
  numbered cursors, describe; diagnostics, notices, conditions;
  autocommit; completion details; multi-statement, query attributes,
  ping, reset, change-user.
- The SQLSTATE classes mapped to the error (Appendix B), and whether an
  error ends the connection.

## M4: Large objects and bulk

- Locators, readers and writers; copy; import of CSV and TSV; export as
  CSV and native frames.

## M5: Settings

- The URI, options, variables, service and password files; several hosts,
  load balance, target role; TLS modes; channel binding.

## M6: Throughput

- Pipelines with skip-on-error; pools with PoolConfig's defaults; the
  SQL-agent.

## M7: Standard API

- The C++ API settled across the layers (RULES.md D2), and the manual
  written, every example run.

## M8: Parity audit and packaging

- Every row of CONFORMANCE.md checked against turborel-client and the
  spec; the coverage floor held; this file empty.
- Ports for vcpkg (`vcpkg.json`, with Dependabot's vcpkg ecosystem) and
  Conan (`conanfile.py`), carrying the license; the release archives.
