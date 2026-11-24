# Debugging Routines

[Contents](README.md)

*(extension)* A procedure, a function or a trigger can be debugged as it
runs: one connection, the debugger, attaches to the session of another,
the target, and pauses the target's routines at breakpoints, reading
their parameters, variables and cursors and stepping through their
statements. The target runs its statements as ever, often on a thread
of its own, while the debugger waits for it on its own connection.
[Debugging Routines](../client/31-debugging.md) gives each type and
call.

## Who may debug what

A connection is granted `debugger`, which it asks for, and gives its ID
by `debug_id`. A debugger attaches to a session of its own user, unless
its user is the database's owner or holds the role MONITOR, and sets
breakpoints in the routines its user owns, or holds DEBUG on:

```sql
GRANT DEBUG ON PROCEDURE ANN.BIG_ORDERS TO BOB;
```

## Breakpoints and pauses

A `turborel::debugger` is made of the debugger's connection and the
target's ID, and detaches as it is destroyed. A breakpoint names a
routine, a line of its text and, if it is to pause only sometimes, a
condition over the routine's parameters and variables. `wait` waits for
the target's next event, at most the time it is given, and a pause holds
the call stack, the innermost frame last. `resume` lets the target go
on, a step at a time or to the next breakpoint:

Made beforehand, as the owner:

```sql
CREATE TABLE ORDERS (ID INTEGER PRIMARY KEY, QTY INTEGER);
INSERT INTO ORDERS VALUES (1, 3), (2, 12), (3, 7);
```

```cpp
#include <turborel/turborel.hpp>

#include <chrono>
#include <iostream>
#include <thread>

int main() {
  using namespace std::chrono_literals;
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection target = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  static_cast<void>(target.query("CREATE PROCEDURE BIG_ORDERS(IN MOST INTEGER, OUT BIG INTEGER) READS SQL DATA\n"
                                 "BEGIN\n"
                                 "  DECLARE ROWS_SEEN INTEGER DEFAULT 0;\n"
                                 "  SET ROWS_SEEN = (SELECT COUNT(*) FROM ORDERS);\n"
                                 "  SET BIG = (SELECT COUNT(*) FROM ORDERS WHERE QTY > MOST);\n"
                                 "END"));
  static_cast<void>(target.query("COMMIT"));

  turborel::debugger debugging{conn, target.debug_id()};
  static_cast<void>(
      debugging.set_breakpoint({.name = "BIG_ORDERS", .line = 5, .condition = "MOST > 5"}));

  const turborel::statement big = target.prepare("CALL BIG_ORDERS(?, ?)");
  std::jthread call{[&target, &big] {
    const turborel::result out = target.execute(big, turborel::params::of(10));
    std::cout << "BIG = " << out.at(0).at(0).to_string() << '\n';
  }};
  if (std::optional<turborel::debug_event> event = debugging.wait(10s); event && event->pause) {
    const turborel::debug_frame top = event->pause->frames().back();
    std::cout << "paused in " << top.origin->name << " at line " << top.line.value_or(0) << '\n';
    for (const turborel::frame_value& variable : top.variables) {
      std::cout << "  " << variable.name << ' ' << variable.type << " = " << variable.data.to_string() << '\n';
    }
  }
  debugging.resume(turborel::debug_action::to_breakpoint);
  call.join();
  debugging.detach();
}
```

```
paused in BIG_ORDERS at line 5
  ROWS_SEEN INTEGER = 3
BIG = 1
```

A breakpoint whose condition is false lets the routine pass. A target
paused longer than DEBUGGER.PAUSE_TIMEOUT_MS goes on by itself, and the
debugger is told it timed out; one whose statement is cancelled while
paused, that it was cancelled; one whose session ends, that it ended.
`interrupt` pauses a target that runs long at its next statement.

## When it is refused

A debugger keeps its session through each refusal: an ID of no
connection its user may see, SQLSTATE 08003; a breakpoint in a routine
its user may not debug, 42000; a step of a target not paused, TBD01.
[Appendix B: Errors](../appendix/b-errors.md) lists them.
