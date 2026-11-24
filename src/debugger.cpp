/// @file
/// @brief The routine debugger through turborel-c *(extension)* [CLIENT
/// Debugging routines]: attached and detached, breakpoints, events waited
/// for, the call stack and its frames.

#include <turborel/debugger.hpp>

#include "capi.hpp"
#include "connection_state.hpp"

#include <turborel/connection.hpp>
#include <turborel/turborel.h>
#include <turborel/value.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace turborel {

namespace {

/// An owned list of breakpoints.
using breakpoint_list = detail::owner<trl_debug_breakpoint_list, trl_debug_breakpoint_list_free>;

/// Attaches `conn` to the session `target` names.
/// @return true, once attached.
bool attach(connection& conn, std::uint64_t target) {
  detail::check_on(conn, trl_debug_attach(conn.get(), target));
  return true;
}

/// A copy of a string a list gives, none of null.
std::optional<std::string> maybe_copy(const char* text) {
  return text == nullptr ? std::nullopt : std::optional<std::string>{text};
}

/// The breakpoints a list holds, copied.
std::vector<breakpoint> breakpoints_of(const trl_debug_breakpoint_list* list) {
  std::vector<breakpoint> all;
  all.reserve(trl_debug_breakpoint_count(list));
  for (std::size_t i = 0; i < trl_debug_breakpoint_count(list); ++i) {
    all.push_back(breakpoint{
        .number = trl_debug_breakpoint_number(list, i),
        .schema = maybe_copy(trl_debug_breakpoint_schema(list, i)).value_or(std::string{}),
        .name = maybe_copy(trl_debug_breakpoint_name(list, i)).value_or(std::string{}),
        .line = trl_debug_breakpoint_line(list, i),
        .condition = maybe_copy(trl_debug_breakpoint_condition(list, i)),
    });
  }
  return all;
}

/// A frame's parameters, or its variables.
std::vector<frame_value> values_of(const trl_debug_frame* frame, trl_debug_values which) {
  std::vector<frame_value> values;
  values.reserve(trl_debug_frame_value_count(frame, which));
  for (std::size_t i = 0; i < trl_debug_frame_value_count(frame, which); ++i) {
    values.push_back(frame_value{
        .name = detail::view(trl_debug_frame_value_name(frame, which, i)),
        .type = detail::view(trl_debug_frame_value_type(frame, which, i)),
        .data = value{trl_debug_frame_value(frame, which, i)},
    });
  }
  return values;
}

/// A frame, read whole.
debug_frame frame_of(const trl_debug_frame* frame) {
  debug_frame read{.visible = trl_debug_frame_visible(frame)};
  if (trl_debug_frame_has_origin(frame)) {
    read.origin = frame_origin{
        .kind = static_cast<routine_kind>(trl_debug_frame_kind(frame)),
        .specific_schema = detail::view(trl_debug_frame_specific_schema(frame)),
        .specific_name = detail::view(trl_debug_frame_specific_name(frame)),
        .schema = detail::view(trl_debug_frame_schema(frame)),
        .name = detail::view(trl_debug_frame_name(frame)),
    };
  }
  std::uint64_t place = 0;
  if (trl_debug_frame_place(frame, &place)) {
    read.place = place;
  }
  std::uint32_t line = 0;
  std::uint32_t column = 0;
  if (trl_debug_frame_line(frame, &line, &column)) {
    read.line = line;
    read.column = column;
  }
  read.parameters = values_of(frame, TRL_DEBUG_PARAMETERS);
  read.variables = values_of(frame, TRL_DEBUG_VARIABLES);
  for (std::size_t i = 0; i < trl_debug_frame_cursor_count(frame); ++i) {
    read.cursors.push_back(frame_cursor{
        .name = detail::view(trl_debug_frame_cursor_name(frame, i)),
        .open = trl_debug_frame_cursor_open(frame, i),
        .query = detail::view(trl_debug_frame_cursor_query(frame, i)),
    });
  }
  return read;
}

/// The milliseconds turborel-c waits for `within`: at least one, as 0 is
/// its no limit, and at most what it takes.
std::uint32_t wait_ms(std::chrono::milliseconds within) noexcept {
  constexpr auto most = std::chrono::milliseconds{std::numeric_limits<std::uint32_t>::max()};
  return static_cast<std::uint32_t>(std::clamp(within, std::chrono::milliseconds{1}, most).count());
}

} // namespace

std::uint64_t connection::debug_id() {
  std::uint64_t id = 0;
  detail::check_on(*this, trl_debug_id(get(), &id));
  return id;
}

debug_pause::debug_pause(std::shared_ptr<const trl_debug_pause> held) noexcept : pause_{std::move(held)} {}

bool debug_pause::at_breakpoint() const noexcept {
  return trl_debug_pause_reason(pause_.get()) == TRL_DEBUG_AT_BREAKPOINT;
}

std::optional<std::uint32_t> debug_pause::breakpoint() const noexcept {
  if (!at_breakpoint()) {
    return std::nullopt;
  }
  return trl_debug_pause_breakpoint(pause_.get());
}

std::vector<debug_frame> debug_pause::frames() const {
  std::vector<debug_frame> frames;
  frames.reserve(trl_debug_pause_frame_count(pause_.get()));
  for (std::size_t i = 0; i < trl_debug_pause_frame_count(pause_.get()); ++i) {
    frames.push_back(frame_of(trl_debug_pause_frame(pause_.get(), i)));
  }
  return frames;
}

debugger::debugger(connection& conn, std::uint64_t target) : conn_{&conn}, attached_{attach(conn, target)} {}

debugger::debugger(debugger&& other) noexcept : conn_{other.conn_}, attached_{std::exchange(other.attached_, false)} {}

debugger& debugger::operator=(debugger&& other) noexcept {
  if (this != &other) {
    detach_quietly();
    conn_ = other.conn_;
    attached_ = std::exchange(other.attached_, false);
  }
  return *this;
}

debugger::~debugger() { detach_quietly(); }

void debugger::detach_quietly() noexcept {
  if (attached_ && !conn_->lost()) {
    // Nothing is thrown here (R-RAII-2): a detach that fails leaves its
    // failure for the connection's next call to tell.
    static_cast<void>(trl_debug_detach(conn_->get()));
  }
  attached_ = false;
}

breakpoint debugger::set_breakpoint(const breakpoint_at& at) {
  const trl_debug_location location{
      .kind = static_cast<trl_debug_kind>(at.kind),
      .schema = at.schema.c_str(),
      .name = at.name.c_str(),
      .line = at.line,
      .condition = at.condition ? at.condition->c_str() : nullptr,
  };
  trl_debug_breakpoint_list* made = nullptr;
  const trl_status status = trl_debug_break(conn_->get(), &location, &made);
  const breakpoint_list set{made};
  detail::check_on(*conn_, status);
  return breakpoints_of(set.get()).at(0);
}

void debugger::clear(std::uint32_t number) { detail::check_on(*conn_, trl_debug_clear(conn_->get(), number)); }

std::vector<breakpoint> debugger::breakpoints() {
  trl_debug_breakpoint_list* made = nullptr;
  const trl_status status = trl_debug_breakpoints(conn_->get(), &made);
  const breakpoint_list all{made};
  detail::check_on(*conn_, status);
  return breakpoints_of(all.get());
}

namespace {

/// An event turborel-c gave, its pause kept alive by the event.
debug_event event_of(trl_debug_event* made) {
  const std::shared_ptr<trl_debug_event> held{made, trl_debug_event_free};
  debug_event event{.said = static_cast<debug_said>(trl_debug_event_said(held.get()))};
  const trl_debug_pause* pause = trl_debug_event_pause(held.get());
  if (pause != nullptr) {
    event.pause = debug_pause{std::shared_ptr<const trl_debug_pause>{held, pause}};
  }
  return event;
}

} // namespace

std::optional<debug_event> debugger::wait(std::chrono::milliseconds within) {
  trl_debug_event* made = nullptr;
  const trl_status status = trl_debug_wait(conn_->get(), wait_ms(within), &made);
  detail::check_on(*conn_, status);
  if (made == nullptr) {
    return std::nullopt;
  }
  return event_of(made);
}

debug_event debugger::wait() {
  trl_debug_event* made = nullptr;
  const trl_status status = trl_debug_wait(conn_->get(), TRL_NO_LIMIT, &made);
  detail::check_on(*conn_, status);
  return event_of(made);
}

std::optional<debug_pause> debugger::stack() {
  trl_debug_pause* made = nullptr;
  const trl_status status = trl_debug_stack(conn_->get(), &made);
  std::shared_ptr<trl_debug_pause> held{made, trl_debug_pause_free};
  detail::check_on(*conn_, status);
  if (made == nullptr) {
    return std::nullopt;
  }
  return debug_pause{std::move(held)};
}

void debugger::resume(debug_action how) {
  detail::check_on(*conn_, trl_debug_resume(conn_->get(), static_cast<trl_debug_action>(how)));
}

void debugger::interrupt() { detail::check_on(*conn_, trl_debug_interrupt(conn_->get())); }

void debugger::detach() {
  detail::check_on(*conn_, trl_debug_detach(conn_->get()));
  attached_ = false;
}

} // namespace turborel
