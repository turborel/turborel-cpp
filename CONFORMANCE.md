# Conformance

The parity matrix: every protocol message, capability, value tag and
turborel-client feature, from turborel's `doc/client/05-protocol.md`,
`02-library.md` and `02-library-connections.md`, in their order. A row
is claimed done only when the C++ API gives it through turborel-c
(RULES.md D1) and a test citing its section passes. M is the milestone
that fills it (TODO.md).

## The protocol

### Frames

| Row | Cited as | M | Status |
|---|---|---|---|
| The 5-byte header: kind u8, length u32 | `[PROTOCOL Frames]` | M1 | not yet |
| An unknown kind, a body cut short, or bytes left over fail the connection (08006) | `[PROTOCOL Frames]` | M1 | not yet |
| The 16 MiB body limit, and a smaller one where set | `[PROTOCOL Frames]` | M1 | not yet |

### Compressed frames

| Row | Cited as | M | Status |
|---|---|---|---|
| Frame `z`: inflated to at most its length; nested or unasked `z` fails | `[PROTOCOL Compressed frames]` | M1 | not yet |
| Compression threshold of the client's own | `[PROTOCOL Compressed frames]` | M1 | not yet |

### Values and fields

| Row | Cited as | M | Status |
|---|---|---|---|
| u8, u32, u64, i128, flag, string, bytes, list, strings, key | `[PROTOCOL Values and fields]` | M1 | not yet |
| A count larger than what follows fails without reserving memory | `[PROTOCOL Values and fields]` | M1 | not yet |

### Data types

| Row | Cited as | M | Status |
|---|---|---|---|
| Type tag 1 CHARACTER | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 2 exact numeric | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 3 approximate numeric | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 4 a plug-in's type | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 5 CHARACTER VARYING | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 6 BINARY | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 7 BINARY VARYING | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 8 DATE | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 9 TIME | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 10 TIME WITH TIME ZONE | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 11 TIMESTAMP | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 12 TIMESTAMP WITH TIME ZONE | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 13 INTERVAL | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 14 BOOLEAN | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 15 DECFLOAT | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 16 array | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 17 row | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 18 CHARACTER LARGE OBJECT | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 19 BINARY LARGE OBJECT | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 22 multiset | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 23 structured type | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 24 reference type | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 25 JSON | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 26 XML | `[PROTOCOL Data types]` | M1 | not yet |
| Type tag 27 UUID | `[PROTOCOL Data types]` | M1 | not yet |

### Values

| Row | Cited as | M | Status |
|---|---|---|---|
| Value tag 0 null | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 1 character string | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 2 exact number | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 3 approximate number | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 4 a plug-in's value | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 6 binary string | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 8 date | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 9 time | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 11 timestamp | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 13 interval | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 14 truth value | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 15 decimal floating point | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 16 array | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 17 row | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 20 large object, whole | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 21 large object, by locator | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 22 multiset | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 23 structured value | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 24 reference | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 25 JSON, D89's binary tree | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 26 XML | `[PROTOCOL Values]` | M1 | not yet |
| Value tag 27 UUID, 16 octets | `[PROTOCOL Values]` | M1 | not yet |

### Composite fields

| Row | Cited as | M | Status |
|---|---|---|---|
| row | `[PROTOCOL Composite fields]` | M1 | not yet |
| column | `[PROTOCOL Composite fields]` | M1 | not yet |
| parameter | `[PROTOCOL Composite fields]` | M1 | not yet |
| failure | `[PROTOCOL Composite fields]` | M1 | not yet |

### The tail

| Row | Cited as | M | Status |
|---|---|---|---|
| Tail tag 1 attributes | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 2 multi | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 3 cursor | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 4 position | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 5 more | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 6 rows | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 7 severity | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 8 detail | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 9 hint | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 10 context | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 11 schema | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 12 table | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 13 column | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 14 data type | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 15 constraint | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 16 generated | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 17 conditions | `[PROTOCOL The tail]` | M1 | not yet |
| Tail tag 18 time | `[PROTOCOL The tail]` | M1 | not yet |
| Tags in increasing order; an unknown or repeated tag fails | `[PROTOCOL The tail]` | M1 | not yet |
| At most 32 attributes, names 1 to 64 bytes, values at most 1024 | `[PROTOCOL The tail]` | M1 | not yet |

### Mechanisms

| Row | Cited as | M | Status |
|---|---|---|---|
| trust | `[PROTOCOL Mechanisms]` | M2 | not yet |
| password | `[PROTOCOL Mechanisms]` | M2 | not yet |
| certificate | `[PROTOCOL Mechanisms]` | M2 | not yet |
| scram-sha-256 | `[PROTOCOL Mechanisms]` | M2 | not yet |
| scram-sha-256-plus, by tls-exporter and tls-server-end-point | `[PROTOCOL Mechanisms]` | M2 | not yet |

### Capabilities

| Row | Cited as | M | Status |
|---|---|---|---|
| Capability `notices` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `status` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `diagnostics` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `describe` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `cursors` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `ping` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `reset` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `change-user` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `query-attributes` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `connection-attributes` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `multi-statement` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `compression` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `commit-position` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `copy` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `row-counts` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `generated-values` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `fetch-ends` | `[PROTOCOL Capabilities]` | M2 | not yet |
| Capability `conditions` | `[PROTOCOL Capabilities]` | M2 | not yet |

### Requests

| Row | Cited as | M | Status |
|---|---|---|---|
| Request Hello 'H' | `[PROTOCOL Requests]` | M2 | not yet |
| Request Answer 'A' | `[PROTOCOL Requests]` | M2 | not yet |
| Request Query 'Q' | `[PROTOCOL Requests]` | M2 | not yet |
| Request Prepare 'P' | `[PROTOCOL Requests]` | M2 | not yet |
| Request Execute 'E' | `[PROTOCOL Requests]` | M2 | not yet |
| Request Fetch 'F' | `[PROTOCOL Requests]` | M2 | not yet |
| Request Close 'C' | `[PROTOCOL Requests]` | M2 | not yet |
| Request Deallocate 'D' | `[PROTOCOL Requests]` | M2 | not yet |
| Request Lob 'L': read, create, append, free, hold | `[PROTOCOL Requests]` | M2 | not yet |
| Request Cancel 'Z' | `[PROTOCOL Requests]` | M2 | not yet |
| Request Terminate 'X' | `[PROTOCOL Requests]` | M2 | not yet |
| Request Describe 'Y' | `[PROTOCOL Requests]` | M2 | not yet |
| Request Ping 'G' | `[PROTOCOL Requests]` | M2 | not yet |
| Request Reset 'W' | `[PROTOCOL Requests]` | M2 | not yet |
| Request ChangeUser 'U' | `[PROTOCOL Requests]` | M2 | not yet |
| Request CopyIn 'I' | `[PROTOCOL Requests]` | M2 | not yet |
| Request CopyData 'J' | `[PROTOCOL Requests]` | M2 | not yet |
| Request CopyEnd 'N' | `[PROTOCOL Requests]` | M2 | not yet |

### Replies

| Row | Cited as | M | Status |
|---|---|---|---|
| Reply Challenge 'C' | `[PROTOCOL Replies]` | M2 | not yet |
| Reply Ready 'R' | `[PROTOCOL Replies]` | M2 | not yet |
| Reply Prepared 'P' | `[PROTOCOL Replies]` | M2 | not yet |
| Reply Description 'D' | `[PROTOCOL Replies]` | M2 | not yet |
| Reply Rows 'B' | `[PROTOCOL Replies]` | M2 | not yet |
| Reply Suspended 'S' | `[PROTOCOL Replies]` | M2 | not yet |
| Reply Complete 'K' | `[PROTOCOL Replies]` | M2 | not yet |
| Reply Error 'E' | `[PROTOCOL Replies]` | M2 | not yet |
| Reply Lob 'L' | `[PROTOCOL Replies]` | M2 | not yet |
| Reply Notice 'N' | `[PROTOCOL Replies]` | M2 | not yet |
| Reply Status 'T': AUTHORIZATION, SCHEMA, READ_ONLY, SERVER_VERSION, SERVER_READ_ONLY, CATALOG, OWNER and the session's settings | `[PROTOCOL Replies]` | M2 | not yet |
| Reply Pong 'G' | `[PROTOCOL Replies]` | M2 | not yet |

### Conversations

| Row | Cited as | M | Status |
|---|---|---|---|
| Replies in their fixed order: rows, notices, statuses, the last reply | `[PROTOCOL Conversations]` | M3 | not yet |

### Errors and notices

| Row | Cited as | M | Status |
|---|---|---|---|
| A failure's fields and its diagnostics | `[PROTOCOL Errors and notices]` | M3 | not yet |
| Conditions in Complete or as Notices | `[PROTOCOL Errors and notices]` | M3 | not yet |
| Malformed messages fail the connection (08006) | `[PROTOCOL Errors and notices]` | M3 | not yet |

### A worked example

| Row | Cited as | M | Status |
|---|---|---|---|
| The page's bytes, encoded and decoded | `[PROTOCOL A worked example]` | M1 | not yet |

## turborel-client: The Client Library

### Connecting

| Row | Cited as | M | Status |
|---|---|---|---|
| TCP, Unix socket and named pipe `\\.\pipe\turborel` | `[CLIENT Connecting]` | M2 | not yet |
| TLS on the same port, trusting files given or the platform's authorities | `[CLIENT Connecting]` | M2 | not yet |
| Mechanisms Scram, Password, Certificate and Trust | `[CLIENT Connecting]` | M2 | not yet |
| Secrets refused over plain TCP before connecting (28000) | `[CLIENT Connecting]` | M2 | not yet |
| Terminate, its commit's failure, within 60 s | `[CLIENT Connecting]` | M2 | not yet |

### Timeouts

| Row | Cited as | M | Status |
|---|---|---|---|
| Connect timeout, 30 s by default | `[CLIENT Timeouts]` | M2 | not yet |
| Read timeout, none by default | `[CLIENT Timeouts]` | M2 | not yet |
| The reply limit, 16 MiB, and a smaller one set | `[CLIENT Timeouts]` | M2 | not yet |

### Running statements

| Row | Cited as | M | Status |
|---|---|---|---|
| query: rows or a completion | `[CLIENT Running statements]` | M3 | not yet |
| query_into: rows as they come | `[CLIENT Running statements]` | M3 | not yet |
| Failure with SQLCODE, SQLSTATE, message, rule and position | `[CLIENT Running statements]` | M3 | not yet |
| in_transaction | `[CLIENT Running statements]` | M3 | not yet |
| Cancel from another thread, within 10 s | `[CLIENT Running statements]` | M3 | not yet |
| Autocommit | `[CLIENT Running statements]` | M3 | not yet |

### Prepared statements

| Row | Cited as | M | Status |
|---|---|---|---|
| prepare, execute, parameters, columns | `[CLIENT Prepared statements]` | M3 | not yet |
| open_cursor, fetch, close, deallocate | `[CLIENT Prepared statements]` | M3 | not yet |

### Large objects

| Row | Cited as | M | Status |
|---|---|---|---|
| Locators for values past 64 KiB | `[CLIENT Large objects]` | M4 | not yet |
| read_lob by octets or characters, 256 KiB at a time | `[CLIENT Large objects]` | M4 | not yet |
| A stream reader | `[CLIENT Large objects]` | M4 | not yet |
| free_lob and hold_lob | `[CLIENT Large objects]` | M4 | not yet |
| create_lob and its writer | `[CLIENT Large objects]` | M4 | not yet |

### XML values

| Row | Cited as | M | Status |
|---|---|---|---|
| XML values as text, and sent as parameters | `[CLIENT XML values]` | M1 | not yet |

### UUIDs

| Row | Cited as | M | Status |
|---|---|---|---|
| UUIDs as 16 octets or text, and sent as parameters | `[CLIENT UUIDs]` | M1 | not yet |

### Importing

| Row | Cited as | M | Status |
|---|---|---|---|
| Import of a source into a table, by batch, with skip | `[CLIENT Importing]` | M4 | not yet |
| CSV and TSV sources | `[CLIENT Importing]` | M4 | not yet |

### What a connection asks for

| Row | Cited as | M | Status |
|---|---|---|---|
| Every capability but compression, by default | `[CLIENT What a connection asks for]` | M2 | not yet |
| An offer of the program's own, attributes among it | `[CLIENT What a connection asks for]` | M2 | not yet |
| What was granted; a method not granted fails before sending | `[CLIENT What a connection asks for]` | M2 | not yet |

### What the server tells

| Row | Cited as | M | Status |
|---|---|---|---|
| setting and settings from Status | `[CLIENT What the server tells]` | M3 | not yet |
| Notices to a handler, or the last 64 kept | `[CLIENT What the server tells]` | M3 | not yet |
| conditions | `[CLIENT What the server tells]` | M3 | not yet |
| rows_affected, last_generated, server_time, commit_position | `[CLIENT What the server tells]` | M3 | not yet |
| Diagnostics of a failure | `[CLIENT What the server tells]` | M3 | not yet |

### Controlling the session

| Row | Cited as | M | Status |
|---|---|---|---|
| ping, reset, change_user | `[CLIENT Controlling the session]` | M3 | not yet |
| Query attributes | `[CLIENT Controlling the session]` | M3 | not yet |
| query_many | `[CLIENT Controlling the session]` | M3 | not yet |
| describe, numbered cursors, describe_cursor | `[CLIENT Controlling the session]` | M3 | not yet |

### Copying and exporting

| Row | Cited as | M | Status |
|---|---|---|---|
| copy_in, send, finish, abandon on drop | `[CLIENT Copying and exporting]` | M4 | not yet |
| copy_rows | `[CLIENT Copying and exporting]` | M4 | not yet |
| export as CSV and native frames, and read_native | `[CLIENT Copying and exporting]` | M4 | not yet |

### The SQL-agent

| Row | Cited as | M | Status |
|---|---|---|---|
| CONNECT, SET CONNECTION and DISCONNECT over named connections | `[CLIENT The SQL-agent]` | M6 | not yet |

## turborel-client: Settings, Pipelines and Pools

### Connecting by settings

| Row | Cited as | M | Status |
|---|---|---|---|
| Settings::connect, the mechanism chosen from the settings | `[CLIENT Connecting by settings]` | M5 | not yet |

### Where settings come from

| Row | Cited as | M | Status |
|---|---|---|---|
| The settings and their defaults | `[CLIENT Where settings come from]` | M5 | not yet |
| Layers: options or URI, service file, environment, defaults | `[CLIENT Where settings come from]` | M5 | not yet |
| TURBOREL_* variables | `[CLIENT Where settings come from]` | M5 | not yet |
| Refusals with 08001 | `[CLIENT Where settings come from]` | M5 | not yet |
| application_name as the attribute `application` | `[CLIENT Where settings come from]` | M5 | not yet |

### The connection URI

| Row | Cited as | M | Status |
|---|---|---|---|
| turborel:// with users, hosts, ports and settings, percent-encoded | `[CLIENT The connection URI]` | M5 | not yet |

### The service file

| Row | Cited as | M | Status |
|---|---|---|---|
| Sections of settings, comments, errors by line | `[CLIENT The service file]` | M5 | not yet |

### The password file

| Row | Cited as | M | Status |
|---|---|---|---|
| host:port:user:password lines, `*` and `\` escapes | `[CLIENT The password file]` | M5 | not yet |

### Several servers

| Row | Cited as | M | Status |
|---|---|---|---|
| Hosts tried in order, or load_balance=random | `[CLIENT Several servers]` | M5 | not yet |
| target any, read-write, read-only, by SERVER_READ_ONLY | `[CLIENT Several servers]` | M5 | not yet |
| The role of a connection | `[CLIENT Several servers]` | M5 | not yet |

### TLS by mode

| Row | Cited as | M | Status |
|---|---|---|---|
| tls_mode disable, prefer, require, verify-ca, verify-full | `[CLIENT TLS by mode]` | M5 | not yet |
| channel_binding disable, prefer, require | `[CLIENT TLS by mode]` | M5 | not yet |

### Pipelines

| Row | Cited as | M | Status |
|---|---|---|---|
| Requests queued and run in a 64 KiB window | `[CLIENT Pipelines]` | M6 | not yet |
| Skip on error by savepoints | `[CLIENT Pipelines]` | M6 | not yet |

### Pools

| Row | Cited as | M | Status |
|---|---|---|---|
| Pool with PoolConfig's defaults | `[CLIENT Pools]` | M6 | not yet |
| Ping check and Reset return, or ROLLBACK and a pipeline | `[CLIENT Pools]` | M6 | not yet |
| get waits, fails with 08001 | `[CLIENT Pools]` | M6 | not yet |

### The SCRAM key cache

| Row | Cited as | M | Status |
|---|---|---|---|
| 64 entries by salt, iterations and a keyed tag, dropped on a failed login | `[CLIENT The SCRAM key cache]` | M2 | not yet |

## The standard API

| Row | Cited as | M | Status |
|---|---|---|---|
| RAII owners of turborel-c's handles | `[DESIGN] D2` | M7 | not yet |
| Exceptions: `turborel::error` with SQLCODE and SQLSTATE | `[DESIGN] D2` | M7 | not yet |
| Rows as an input range | `[DESIGN] D2` | M7 | not yet |
| `std::chrono` datetimes | `[DESIGN] D2` | M7 | not yet |
| A decimal type of the driver's own | `[DESIGN] D2` | M7 | not yet |
| `std::span` and `std::string_view` at the boundaries | `[DESIGN] D2` | M7 | not yet |
