/// @file
/// @brief The routine debugger *(extension)* [CLIENT Debugging routines]:
/// with `debugger` granted, one connection, the debugger, attaches to the
/// session of another, the target, by its ID; sets breakpoints in the
/// procedures, functions and triggers its user may debug; and drives the
/// target's routines as they pause, reading their call stack, parameters,
/// variables and cursors [PROTOCOL Debug 'V'] (RULES.md D2).

#ifndef TURBOREL_DEBUGGER_HPP
#define TURBOREL_DEBUGGER_HPP

#include <turborel/connection.hpp>
#include <turborel/export.hpp>
#include <turborel/value.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct trl_debug_pause;

namespace turborel {

/// The kind of routine a breakpoint, or a frame, is of.
enum class routine_kind : std::uint8_t {
  /// A procedure.
  procedure = 0,
  /// A function.
  function = 1,
  /// A trigger.
  trigger = 2,
};

/// How a paused target goes on.
enum class debug_action : std::uint8_t {
  /// To its next statement, in whatever frame.
  step_into = 0,
  /// To the next statement of this frame, or of one it returns to.
  step_over = 1,
  /// To the next statement of the frame this one returns to.
  step_out = 2,
  /// On to a breakpoint.
  to_breakpoint = 3,
};

/// Where a breakpoint goes: a line of a routine's or a trigger's text.
struct breakpoint_at {
  /// The kind of routine.
  routine_kind kind{routine_kind::procedure};
  /// Its schema; empty for the debugger's session's default schema.
  std::string schema;
  /// The routine's specific name, or its name if one routine of its kind
  /// has it, or the trigger's.
  std::string name;
  /// The line of its text, from 1.
  std::uint32_t line{};
  /// A `<search condition>` over the frame's parameters and variables,
  /// paused only where true; none for none.
  std::optional<std::string> condition;
};

/// A breakpoint, as the server gives it.
struct breakpoint {
  /// Its number.
  std::uint32_t number{};
  /// The schema of its routine.
  std::string schema;
  /// The specific name of its routine.
  std::string name;
  /// Its line, from 1.
  std::uint32_t line{};
  /// Its condition, if it has one.
  std::optional<std::string> condition;
};

/// A parameter or variable of a frame: views, valid while its pause is.
struct frame_value {
  /// Its name.
  std::string_view name;
  /// Its data type, as SQL writes it.
  std::string_view type;
  /// Its value.
  value data{nullptr};
};

/// A cursor of a frame: views, valid while its pause is.
struct frame_cursor {
  /// Its name.
  std::string_view name;
  /// Whether it is open.
  bool open{};
  /// Its query.
  std::string_view query;
};

/// The routine or trigger a frame runs: views, valid while its pause is.
struct frame_origin {
  /// Its kind.
  routine_kind kind{routine_kind::procedure};
  /// Its specific schema.
  std::string_view specific_schema;
  /// Its specific name.
  std::string_view specific_name;
  /// Its schema, as named.
  std::string_view schema;
  /// Its name, or the trigger's.
  std::string_view name;
};

/// A frame of a paused target's call stack: views, valid while its pause
/// is.
struct debug_frame {
  /// The routine or trigger it runs; none of a statement run directly.
  std::optional<frame_origin> origin;
  /// Whether the debugger may see into it; one it may not has its origin
  /// alone.
  bool visible{};
  /// Its place, in characters from the beginning of its routine's text.
  std::optional<std::uint64_t> place;
  /// Its line, from 1.
  std::optional<std::uint32_t> line;
  /// Its column, from 1.
  std::optional<std::uint32_t> column;
  /// Its parameters; a trigger's, its transition variables.
  std::vector<frame_value> parameters;
  /// Its variables.
  std::vector<frame_value> variables;
  /// Its cursors.
  std::vector<frame_cursor> cursors;
};

/// A paused target: why it paused, and its call stack.
class TURBOREL_CPP_EXPORT debug_pause {
public:
  /// A pause, kept alive by what holds it.
  /// @param held The pause.
  explicit debug_pause(std::shared_ptr<const trl_debug_pause> held) noexcept;

  /// Whether it paused at a breakpoint, rather than at a step's end.
  /// @return Whether it did.
  [[nodiscard]] bool at_breakpoint() const noexcept;

  /// The number of the breakpoint it paused at.
  /// @return The number; none of a pause at a step's end.
  [[nodiscard]] std::optional<std::uint32_t> breakpoint() const noexcept;

  /// Its call stack, the outermost frame first.
  /// @return The frames, views valid while the pause is.
  [[nodiscard]] std::vector<debug_frame> frames() const;

private:
  std::shared_ptr<const trl_debug_pause> pause_;
};

/// What an event says of the target.
enum class debug_said : std::uint8_t {
  /// It paused, at a breakpoint or a step's end.
  paused = 0,
  /// Paused past DEBUGGER.PAUSE_TIMEOUT_MS, it went on by itself.
  timed_out = 1,
  /// Its statement was cancelled while it was paused.
  cancelled = 2,
  /// Its session ended, and the debugger is detached.
  ended = 3,
};

/// An event of the target [PROTOCOL Event 'W'].
struct debug_event {
  /// What it says.
  debug_said said{debug_said::ended};
  /// Its pause, of one that says paused.
  std::optional<debug_pause> pause;
};

/// A connection's debugging of another's session: attached as it is made,
/// detached as it is destroyed. It refers to its connection, which
/// outlives it.
class TURBOREL_CPP_EXPORT debugger {
public:
  /// Attaches `conn` to the session of the connection `target` names, one
  /// of the same user unless the user is the database's owner or holds
  /// the role MONITOR.
  /// @param conn The debugger's connection, granted `debugger`.
  /// @param target The target's ID, as connection::debug_id() gives it.
  /// @throws turborel::error Of an ID of no connection the user may see
  ///   (SQLSTATE 08003), or of `conn` attached already (TBD01).
  debugger(connection& conn, std::uint64_t target);

  debugger(const debugger&) = delete;
  debugger& operator=(const debugger&) = delete;
  /// Moves the debugging.
  /// @param other The debugger moved from, left detached.
  debugger(debugger&& other) noexcept;
  /// Moves the debugging, detaching the one this held.
  /// @param other The debugger moved from, left detached.
  /// @return This.
  debugger& operator=(debugger&& other) noexcept;
  /// Detaches, if attached.
  ~debugger();

  /// Sets a breakpoint.
  /// @param at Where, and on what condition.
  /// @return The breakpoint set.
  /// @throws turborel::error Of a routine the user may not debug
  ///   (SQLCODE -104, SQLSTATE 42000).
  breakpoint set_breakpoint(const breakpoint_at& at);

  /// Clears a breakpoint.
  /// @param number Its number.
  /// @throws turborel::error The server's refusal.
  void clear(std::uint32_t number);

  /// Every breakpoint set, in the order they were set.
  /// @return The breakpoints.
  /// @throws turborel::error The server's refusal.
  [[nodiscard]] std::vector<breakpoint> breakpoints();

  /// The target's next event not yet given, waited for at most `within`,
  /// a time below a millisecond waiting one.
  /// @param within The longest wait.
  /// @return The event; none if none came in time.
  /// @throws turborel::error Of a reply where an event was waited for, or
  ///   of the server gone (SQLSTATE 08006).
  [[nodiscard]] std::optional<debug_event> wait(std::chrono::milliseconds within);

  /// The target's next event not yet given, waited for as long as it
  /// takes.
  /// @return The event.
  /// @throws turborel::error As wait(within) does.
  [[nodiscard]] debug_event wait();

  /// The target's pause again, with its call stack.
  /// @return The pause; none if the target runs.
  /// @throws turborel::error The server's refusal.
  [[nodiscard]] std::optional<debug_pause> stack();

  /// Tells the paused target how to go on.
  /// @param how A step into, over or out, or on to a breakpoint.
  /// @throws turborel::error Of a target not paused (SQLSTATE TBD01).
  void resume(debug_action how);

  /// Tells the running target to pause at its next statement.
  /// @throws turborel::error The server's refusal.
  void interrupt();

  /// Ends the debugging: the breakpoints go, the target goes on if
  /// paused, and the events kept are dropped.
  /// @throws turborel::error The server's refusal.
  void detach();

  /// Whether it is attached.
  /// @return Whether it is.
  [[nodiscard]] bool attached() const noexcept { return attached_; }

private:
  /// Detaches, if attached, throwing nothing.
  void detach_quietly() noexcept;

  connection* conn_;
  bool attached_{};
};

} // namespace turborel

#endif // TURBOREL_DEBUGGER_HPP
