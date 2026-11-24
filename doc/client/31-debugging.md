# Debugging Routines

[Contents](README.md)

## NAME

`turborel::debugger`, `connection::debug_id` *(extension)*: one
connection driving the routines another connection's session runs.

## SYNOPSIS

```cpp
#include <turborel/debugger.hpp>

namespace turborel {
enum class routine_kind : std::uint8_t { procedure, function, trigger };
enum class debug_action : std::uint8_t { step_into, step_over, step_out, to_breakpoint };
enum class debug_said : std::uint8_t { paused, timed_out, cancelled, ended };

struct breakpoint_at {
  routine_kind kind{routine_kind::procedure};
  std::string schema;
  std::string name;
  std::uint32_t line{};
  std::optional<std::string> condition;
};

struct breakpoint {
  std::uint32_t number{};
  std::string schema;
  std::string name;
  std::uint32_t line{};
  std::optional<std::string> condition;
};

struct frame_value { std::string_view name; std::string_view type; value data{nullptr}; };
struct frame_cursor { std::string_view name; bool open{}; std::string_view query; };
struct frame_origin {
  routine_kind kind{routine_kind::procedure};
  std::string_view specific_schema, specific_name, schema, name;
};

struct debug_frame {
  std::optional<frame_origin> origin;
  bool visible{};
  std::optional<std::uint64_t> place;
  std::optional<std::uint32_t> line;
  std::optional<std::uint32_t> column;
  std::vector<frame_value> parameters;
  std::vector<frame_value> variables;
  std::vector<frame_cursor> cursors;
};

class debug_pause {
public:
  bool at_breakpoint() const noexcept;
  std::optional<std::uint32_t> breakpoint() const noexcept;
  std::vector<debug_frame> frames() const;
};

struct debug_event { debug_said said{debug_said::ended}; std::optional<debug_pause> pause; };

class debugger {
public:
  debugger(connection& conn, std::uint64_t target);
  debugger(debugger&& other) noexcept;
  debugger& operator=(debugger&& other) noexcept;
  ~debugger();
  breakpoint set_breakpoint(const breakpoint_at& at);
  void clear(std::uint32_t number);
  std::vector<breakpoint> breakpoints();
  std::optional<debug_event> wait(std::chrono::milliseconds within);
  debug_event wait();
  std::optional<debug_pause> stack();
  void resume(debug_action how);
  void interrupt();
  void detach();
  bool attached() const noexcept;
};

class connection {
public:
  std::uint64_t debug_id();
};
}
```

## DESCRIPTION

With `debugger` granted, which a connection asks for, `debug_id` gives a
connection's ID, as INFORMATION_SCHEMA.CONNECTIONS shows it. A
`debugger` made of another connection and that ID attaches the other
connection to the session the ID names, the target, and detaches as it
is destroyed, or by `detach`; detached, the target's breakpoints go, a
paused target goes on, and the events kept are dropped. The target is a
session of the same user, unless the user is the database's owner or
holds the role MONITOR. A connection debugs one session at a time, and
a session has one debugger. A debugger refers to its connection, which
outlives it; it is moved, not copied.

`set_breakpoint` sets a breakpoint at a line of a procedure's, a
function's or a trigger's text, of the routine's schema, or of the
session's default schema where none is given, paused at only where its
`condition`, a `<search condition>` over the frame's parameters and
variables, is true; it gives the breakpoint as the server set it, its
number, the routine's specific name and its schema. `clear` clears one
by its number, and `breakpoints` gives each set, in the order they were
set.

`wait` gives the target's next event: kept, as an event is kept
wherever a reply is read, or the next the server sends, waited for at
most the time given, a time below a millisecond waiting one; none if
none came in time. `wait()` with no time waits as long as it takes. An
event says the target `paused`, with its `pause`; `timed_out`, paused
past DEBUGGER.PAUSE_TIMEOUT_MS, so it went on by itself; `cancelled`,
its statement cancelled while it was paused; or `ended`, its session
ended and the debugger detached.

A `debug_pause` says whether the target paused `at_breakpoint`, and
which `breakpoint`, or at a step's end, and gives its call stack,
`frames`, the outermost first: of each its `origin`, the routine or
trigger it runs, none of a statement run directly; whether it is
`visible`, a frame the debugger's user may not see into having its
origin alone; its `place` in characters from the beginning of the
routine's text, its `line` and `column`; its `parameters`, a trigger's
transition variables, and its `variables`, each a name, a type as SQL
writes it, and a value; and its `cursors`, each a name, whether it is
open, and its query. A frame's views, and its values, are valid while
its pause is. `stack` gives the pause again, none while the target
runs.

`resume` lets a paused target go on: a step into its next statement in
whatever frame, a step over to the next statement of this frame, a step
out to the next of the frame this one returns to, or on to a breakpoint.
`interrupt` pauses a running target at its next statement, a pause of a
step.

The target runs its statements as ever, often on a thread of its own;
the debugger waits beside it, on its own connection.

## ERRORS

Each refusal keeps the session. An ID of no connection the user may
see fails with SQLCODE -919, SQLSTATE 08003. A breakpoint in a routine
the user neither owns nor holds DEBUG on fails with -104, 42000. An
action the target's state does not allow, as a resume of a target not
paused, an attach of a connection attached already or to a session that
has a debugger, or any of a debugger detached, fails with -910, TBD01.
Without `debugger` granted, `debug_id` fails before anything is sent,
-919, 08006, the connection kept. A `wait` with a time, over a stream
the program made, which its maker bounds, fails with -919, 08006.

## EXAMPLES

A debugger stops the target's CALL at line 4 of RAISE_PAY, reads its
parameter, steps over to line 5, and lets it finish:

Made beforehand, as the owner:

```sql
CREATE TABLE STAFF (ID INTEGER PRIMARY KEY, SALARY INTEGER);
INSERT INTO STAFF VALUES (1, 50000), (2, 60000);
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
  static_cast<void>(target.query("CREATE PROCEDURE RAISE_PAY(IN PCT INTEGER) MODIFIES SQL DATA\n"
                                 "BEGIN\n"
                                 "  DECLARE N INTEGER DEFAULT 0;\n"
                                 "  UPDATE STAFF SET SALARY = SALARY * (100 + PCT) / 100;\n"
                                 "  SET N = (SELECT COUNT(*) FROM STAFF);\n"
                                 "END"));
  static_cast<void>(target.query("COMMIT"));

  turborel::debugger debugging{conn, target.debug_id()};
  const turborel::breakpoint set = debugging.set_breakpoint({.name = "RAISE_PAY", .line = 4});
  std::cout << "breakpoint " << set.number << " at " << set.schema << '.' << set.name << " line " << set.line << '\n';

  std::jthread call{[&target] { static_cast<void>(target.query("CALL RAISE_PAY(10)")); }};
  if (std::optional<turborel::debug_event> event = debugging.wait(10s); event && event->pause) {
    const turborel::debug_frame top = event->pause->frames().back();
    std::cout << "paused at line " << top.line.value_or(0) << ", " << top.parameters.at(0).name << " = "
              << top.parameters.at(0).data.to_string() << '\n';
  }
  debugging.resume(turborel::debug_action::step_over);
  if (std::optional<turborel::debug_event> event = debugging.wait(10s); event && event->pause) {
    std::cout << "stepped to line " << event->pause->frames().back().line.value_or(0) << '\n';
  }
  debugging.resume(turborel::debug_action::to_breakpoint);
  call.join();
  std::cout << "the CALL ended\n";
}
```

```
breakpoint 1 at ANN.RAISE_PAY_1 line 4
paused at line 4, PCT = 10
stepped to line 5
the CALL ended
```

## SEE ALSO

[Debugging Routines](../user/11-debugging.md),
[Connections](21-connections.md)
