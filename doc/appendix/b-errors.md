# Appendix B: Errors

[Contents](README.md)

This appendix lists the SQLSTATEs the driver reports: those it raises
itself, those its calls meet from the server, and the classes the server
sends, with how each reaches a program and which end the connection.
Turborel's own
[Appendix A: SQLSTATE and SQLCODE](https://github.com/turborel/turborel/blob/main/doc/appendix/a-sqlstate.md)
gives every SQLSTATE and SQLCODE the server sends, each in full.

## How a failure reaches a program

A call that fails throws a `turborel::error`, holding the failure: its
SQLCODE and SQLSTATE, its message, the rule it cites, its position,
whether a transaction is still active, and its diagnostics
([Errors](../client/30-errors.md)). A failure of SQLSTATE class 08, but
08003, ends the connection, as does one of class 28 as a connection is
made; every call on a lost connection fails at once, SQLCODE -919,
SQLSTATE 08006. Every other failure leaves the connection ready for the
next statement. A warning, of class 01, does not fail the call: it is
the completion's SQLSTATE, a condition, or a notice
([Errors and Diagnostics](../user/10-errors.md)). Memory not had is
thrown as `std::bad_alloc`.

## The driver's own

Each carries the SQLCODE and SQLSTATE turborel-client gives the same
failure, and cites the rule `DESIGN`, as turborel-c, which the driver
wraps, raises it:

| SQLSTATE | SQLCODE | Raised of | The connection |
|---|---|---|---|
| 08006 | -919 | a connection lost, timed out, or whose server sent a message malformed or out of turn | lost |
| 08006 | -919 | a call needing a capability the server did not grant; connection or query attributes past their bounds | kept |
| 08001 | -919 | settings that cannot be read, no user named, no server kept | not made |
| 28000 | -919 | a password or SCRAM over plain TCP; a SCRAM exchange the server did not prove, or one not bound where binding is required | not made |
| TB916 | -916 | a reply's value the driver cannot take, the rest of its statement's replies read past | kept |
| 0A000 | -900 | past Turborel's limits, as JSON text nested past 128 deep; TLS, SCRAM or compression where turborel-c was built without them | kept |
| 22000 | -301 | a value the protocol cannot carry; input an import cannot read; a large object misused | kept |
| 22003 | -301 | a number out of range, as an exact number read as `std::int64_t` | kept |
| 22008, 22009 | -301 | a datetime out of 0001 to 9999; a displacement out of -12:59 to +14:00 | kept |
| 22032 | -301 | JSON text that is not valid; a JSON value no JSON text writes | kept |
| 2200N | -301 | XML text that is not well formed | kept |
| TB910 | -910 | a misuse turborel-client has no case for, as a value read as another kind | kept |

## What the driver's calls meet

| SQLSTATE | SQLCODE | Of |
|---|---|---|
| 00000 | 0 | success |
| 01004 and the rest of class 01 | 0 | a warning: the completion's SQLSTATE, a condition, or a notice |
| 01T01 | 0 | a notice: Reset or ChangeUser rolled back the transaction that was active |
| 02000 | 100 | a query that returned no row |
| 08001 | -919 | the SQL-agent's CONNECT of a server not reached |
| 08003 | -919 | the debugger's attach to an ID of no connection the user may see |
| 08004 | -919 | a server that refused the connection, as of another protocol version; the SQL-agent's CONNECT a connector failed |
| 08006 | -919 | the server ending the connection |
| 0F001 | -301 | a locator no longer valid, freed or past its transaction |
| 22021 | -301 | a character large object's octets that are not UTF-8, refused by the Append that sends them, or ending within a character where it is used |
| 2200W | -301 | an XML value with no serialization, as an attribute standing alone, fetched |
| 24000 | -501 | a cursor opened that is open, or fetched, closed or deallocated that is not, or a statement deallocated whose cursor is open |
| 26000 | -921 | a statement executed or deallocated that is deallocated, or unknown to the session, as after a reset |
| 28000 | -919 | a login or a ChangeUser refused, which ends the connection |
| 40002 | -913 | a commit that failed a deferred constraint, the transaction rolled back |
| 42000 | -104 | a breakpoint in a routine the user neither owns nor holds DEBUG on |
| TB920 | -920 | a statement cancelled |
| TBD01 | -910 | a debugger's action the target's state does not allow, as a resume of a target not paused, or a second attach |
| TBR01 | -920 | a replica that had not applied the log position REPLICATION.READ_AFTER names in time |

## The classes the server sends

| Class | Condition | Ends the connection |
|---|---|---|
| 00 | successful completion | |
| 01 | warning | |
| 02 | no data | |
| 07 | dynamic SQL error | |
| 08 | connection exception | yes, but 08003 |
| 09 | triggered action exception | |
| 0A | feature not supported | |
| 0D, 0F, 0N, 0W | invalid target type; locator exception; SQL/XML mapping error; prohibited statement during a trigger | |
| 10 | XQuery error | |
| 21 | cardinality violation | |
| 22 | data exception | |
| 23 | integrity constraint violation | |
| 24 | invalid cursor state | |
| 25 | invalid transaction state | |
| 26 | invalid SQL statement name | |
| 27 | triggered data change violation | |
| 28 | invalid authorization specification | as a connection is made |
| 2B, 2E, 2F, 2H | dependent privilege descriptors still exist; invalid connection name; SQL routine exception, and 2FF of SQL/MM's parts; invalid collation name | |
| 33 to 3F | invalid descriptor, cursor or condition names; cursor sensitivity, external routine and savepoint exceptions; ambiguous cursor name; invalid catalog or schema name | |
| 40 | transaction rollback | |
| 42 | syntax error or access rule violation, of a statement run directly or prepared alike | |
| 44 | WITH CHECK OPTION violation | |
| 54 | program limit exceeded | |
| TB | Turborel's own | |

A failure's SQLCODE says the kind of exception, -103 a Syntax Rule
broken, -201 a constraint, -301 a data exception, -919 a connection
exception, as Turborel's Appendix A lists them.
