/// @file
/// @brief Pools of connections through turborel-c [CLIENT Pools].

#include <turborel/pool.hpp>

#include "capi.hpp"
#include "connection_state.hpp"

#include <turborel/connection.hpp>
#include <turborel/settings.hpp>
#include <turborel/turborel.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace turborel {

static_assert(pool_reset_most_statements == TRL_POOL_RESET_MOST_STATEMENTS);

namespace detail {

/// What a pool and its handles share. The settings and the connector are
/// declared before the pool, which uses them, so they outlive it.
struct pool_shared {
  /// The settings a pool of settings makes its connections of.
  std::optional<settings> how;
  /// The connector a pool of the program's own makes them by.
  pool::connector make;
  /// The statements run as each comes back.
  std::vector<std::string> reset;
  /// Their addresses, as turborel-c takes them.
  std::vector<const char*> reset_texts;
  /// The pool.
  owner<trl_pool, trl_pool_free> handle;
};

} // namespace detail

namespace {

/// A wait in turborel-c's milliseconds.
std::uint32_t milliseconds(std::chrono::milliseconds wait) {
  if (wait.count() < 0 || wait.count() > std::numeric_limits<std::uint32_t>::max()) {
    detail::raise_misuse("a pool's wait of " + std::to_string(wait.count()) + " ms is not from 0 to 2^32 - 1");
  }
  return static_cast<std::uint32_t>(wait.count());
}

/// turborel-c's configuration of the driver's, its statements kept by `held`.
trl_pool_config c_config(const pool_config& config, detail::pool_shared& held) {
  held.reset = config.reset;
  for (const std::string& statement : held.reset) {
    held.reset_texts.push_back(statement.c_str());
  }
  return trl_pool_config{
      .max = config.max,
      .max_idle = config.max_idle,
      .idle_timeout_ms = milliseconds(config.idle_timeout),
      .max_lifetime_ms = milliseconds(config.max_lifetime),
      .wait_ms = milliseconds(config.wait),
      .check = config.check,
      .reset = held.reset_texts.empty() ? nullptr : held.reset_texts.data(),
      .reset_count = held.reset_texts.size(),
  };
}

/// What a connector threw, kept for the get that called it. turborel-c
/// calls a pool's connector on the thread that gets (RULES.md D2), which
/// reads this as its get returns, so one for each thread holds it.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): one for each thread, set and read by its own get alone.
thread_local std::exception_ptr connector_threw;

/// Makes a connection by the program's connector, as turborel-c calls it,
/// keeping what it throws for the thread that got (R-RAII-2).
trl_status make_one(void* context, trl_conn** out) noexcept {
  auto& held = *static_cast<detail::pool_shared*>(context);
  *out = nullptr;
  try {
    connection made = held.make();
    if (detail::conn_access::over_stream(made)) {
      detail::raise_misuse("a pool's connector made a connection over the program's own stream, which it cannot keep");
    }
    *out = detail::conn_access::release(made);
    return TRL_OK;
  } catch (...) {
    connector_threw = std::current_exception();
    return TRL_FAILED;
  }
}

/// A pool of `held`, made by `make` once `held` is filled.
template <typename Make>
std::shared_ptr<detail::pool_shared> made_pool(std::shared_ptr<detail::pool_shared> held, const Make& make) {
  trl_pool* made = nullptr;
  detail::check(make(&made));
  held->handle.reset(made);
  return held;
}

} // namespace

pooled::pooled(std::shared_ptr<detail::pool_shared> from, connection conn) noexcept
    : from_{std::move(from)}, conn_{std::move(conn)} {}

pooled::pooled(pooled&& other) noexcept
    : from_{std::move(other.from_)}, conn_{std::move(other.conn_)}, held_{std::exchange(other.held_, false)} {}

pooled& pooled::operator=(pooled&& other) noexcept {
  if (this != &other) {
    give_back();
    from_ = std::move(other.from_);
    conn_ = std::move(other.conn_);
    held_ = std::exchange(other.held_, false);
  }
  return *this;
}

pooled::~pooled() { give_back(); }

void pooled::give_back() noexcept {
  if (held_) {
    trl_pool_release(from_->handle.get(), detail::conn_access::release(conn_));
    held_ = false;
  }
}

pool pool::of(settings how, const pool_config& config) {
  auto held = std::make_shared<detail::pool_shared>();
  held->how.emplace(std::move(how));
  const trl_pool_config c = c_config(config, *held);
  return pool{made_pool(held, [&held, &c](trl_pool** out) { return trl_pool_of_settings(held->how->get(), &c, out); })};
}

pool pool::of(connector make, const pool_config& config) {
  auto held = std::make_shared<detail::pool_shared>();
  held->make = std::move(make);
  const trl_pool_config c = c_config(config, *held);
  return pool{made_pool(held, [&held, &c](trl_pool** out) { return trl_pool_new(make_one, held.get(), &c, out); })};
}

pooled pool::get() {
  trl_conn* made = nullptr;
  connector_threw = nullptr;
  const trl_status status = trl_pool_get(held_->handle.get(), &made);
  if (connector_threw) {
    std::rethrow_exception(std::exchange(connector_threw, nullptr));
  }
  connection got{made};
  detail::check(status, trl_conn_failure(got.get()));
  return pooled{held_, std::move(got)};
}

std::size_t pool::made() const {
  std::size_t count = 0;
  trl_pool_counts(held_->handle.get(), &count, nullptr);
  return count;
}

std::size_t pool::idle() const {
  std::size_t count = 0;
  trl_pool_counts(held_->handle.get(), nullptr, &count);
  return count;
}

} // namespace turborel
