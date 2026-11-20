/// @file
/// @brief The SQL-agent through turborel-c [CLIENT The SQL-agent].

#include <turborel/agent.hpp>

#include "batches.hpp"
#include "capi.hpp"
#include "connection_state.hpp"

#include <turborel/connection.hpp>
#include <turborel/error.hpp>
#include <turborel/result.hpp>
#include <turborel/turborel.h>

#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace turborel {

namespace detail {

/// What an agent holds: its connector, before the agent that calls it,
/// and what the connector threw.
struct agent_state {
  /// The program's connector.
  agent::connector make;
  /// What it threw, thrown again once the agent's call returns.
  std::exception_ptr thrown;
  /// Whether the connector made a connection over the program's own
  /// stream, which the agent cannot keep.
  bool over_stream{};
  /// The agent.
  owner<trl_agent, trl_agent_free> handle;
};

} // namespace detail

namespace {

/// An optional copy of a string turborel-c gives, null as none.
std::optional<std::string> maybe_text(const char* text) {
  return text == nullptr ? std::nullopt : std::optional<std::string>{text};
}

/// Makes a connection by the program's connector, as turborel-c calls it:
/// the driver's error it throws is told to turborel-c, which refuses
/// CONNECT of it as of a connection's failure, 08001 of a server not
/// reached, else 08004; anything else it throws, or an error turborel-c
/// cannot be told, is thrown once the agent's call returns (R-RAII-2).
trl_status make_one(void* context, const char* server, const char* user, trl_conn** out, trl_failure* why) noexcept {
  auto& held = *static_cast<detail::agent_state*>(context);
  *out = nullptr;
  try {
    connection made = held.make(maybe_text(server), maybe_text(user));
    held.over_stream = detail::conn_access::over_stream(made);
    if (held.over_stream) {
      return TRL_FAILED;
    }
    *out = detail::conn_access::release(made);
    return TRL_OK;
  } catch (const error& e) {
    const failure& told = e.details();
    if (trl_failure_tell(why, told.sqlcode, told.sqlstate.c_str(), told.message.c_str()) != TRL_OK) {
      held.thrown = std::current_exception();
    }
    return TRL_FAILED;
  } catch (...) {
    held.thrown = std::current_exception();
    return TRL_FAILED;
  }
}

} // namespace

agent::agent(connector make) : state_{std::make_unique<detail::agent_state>()} {
  state_->make = std::move(make);
  trl_agent* made = nullptr;
  detail::check(trl_agent_new(make_one, state_.get(), &made));
  state_->handle.reset(made);
}

agent::agent(connector make, connection first) : state_{std::make_unique<detail::agent_state>()} {
  state_->make = std::move(make);
  trl_agent* made = nullptr;
  const trl_status status = trl_agent_connected(make_one, state_.get(), detail::conn_access::release(first), &made);
  detail::check(status);
  state_->handle.reset(made);
}

agent::agent(agent&&) noexcept = default;

agent& agent::operator=(agent&&) noexcept = default;

agent::~agent() = default;

namespace {

/// What an agent's statement came to: what its connector threw that
/// turborel-c was not told, a connection the agent cannot keep, or the
/// agent's failure; else its outcome.
result finished(detail::agent_state& state, trl_status status, result outcome) {
  if (std::exchange(state.over_stream, false)) {
    detail::raise_misuse("the agent's connector made a connection over the program's own stream, which it cannot keep");
  }
  if (state.thrown) {
    std::rethrow_exception(std::exchange(state.thrown, nullptr));
  }
  detail::check(status, trl_agent_failure(state.handle.get()));
  return outcome;
}

} // namespace

result agent::query(const std::string& sql) {
  trl_result* made = nullptr;
  const trl_status status = trl_agent_query(state_->handle.get(), sql.c_str(), &made);
  return finished(*state_, status, result{made});
}

result agent::query_batches(const std::string& sql, const std::function<void(const result&)>& handler) {
  detail::batches context{.handler = &handler, .thrown = nullptr};
  trl_result* made = nullptr;
  const trl_status status =
      trl_agent_query_batches(state_->handle.get(), sql.c_str(), detail::batch_to_handler, &context, &made);
  result outcome{made};
  if (context.thrown) {
    std::rethrow_exception(context.thrown);
  }
  return finished(*state_, status, std::move(outcome));
}

std::optional<std::string_view> agent::current_name() const noexcept {
  return detail::maybe(trl_agent_current_name(state_->handle.get()));
}

bool agent::has_current() const noexcept { return trl_agent_current(state_->handle.get()) != nullptr; }

std::size_t agent::size() const noexcept { return trl_agent_count(state_->handle.get()); }

void agent::terminate() {
  detail::check(trl_agent_terminate(state_->handle.get()), trl_agent_failure(state_->handle.get()));
}

} // namespace turborel
