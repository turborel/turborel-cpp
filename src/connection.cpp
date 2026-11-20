/// @file
/// @brief Connections through turborel-c [CLIENT Connecting]: opened by a
/// target, a stream or a URI; what the server granted and told; its
/// timeouts, Ping and Terminate; its canceller.

#include <turborel/connection.hpp>

#include "capi.hpp"
#include "connection_state.hpp"

#include <turborel/connect.hpp>
#include <turborel/error.hpp>
#include <turborel/turborel.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace turborel {

namespace {

/// The program's stream of a trl_stream's context.
byte_stream& stream_of(void* context) noexcept { return *static_cast<byte_stream*>(context); }

/// A read of the program's stream, as turborel-c calls it: a byte count, 0
/// at the end, or -1 if it failed or threw (R-RAII-2).
std::int64_t stream_read(void* context, std::uint8_t* into, std::size_t capacity) noexcept {
  try {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): std::byte and uint8_t are one byte alike, at C's boundary (R-CPP-4).
    const std::optional<std::size_t> read = stream_of(context).read({reinterpret_cast<std::byte*>(into), capacity});
    return read ? static_cast<std::int64_t>(*read) : -1;
  } catch (...) {
    return -1;
  }
}

/// A write of the program's stream, as turborel-c calls it.
std::int64_t stream_write(void* context, const std::uint8_t* bytes, std::size_t length) noexcept {
  try {
    const std::optional<std::size_t> written = stream_of(context).write(detail::byte_span(bytes, length));
    return written ? static_cast<std::int64_t>(*written) : -1;
  } catch (...) {
    return -1;
  }
}

/// The close of the program's stream, as turborel-c calls it.
void stream_close(void* context) noexcept { stream_of(context).close(); }

/// The bound of the program's stream, as turborel-c calls it.
bool stream_bound(void* context, std::uint32_t ms) noexcept {
  return stream_of(context).bound(std::chrono::milliseconds{ms});
}

/// The milliseconds of a wait, as turborel-c counts them.
std::uint32_t milliseconds(std::chrono::milliseconds wait) {
  if (wait.count() < 0 || wait.count() > std::numeric_limits<std::uint32_t>::max()) {
    detail::raise_misuse("a wait of " + std::to_string(wait.count()) + " ms is not from 0 to 2^32 - 1");
  }
  return static_cast<std::uint32_t>(wait.count());
}

} // namespace

void detail::conn_access::notice(void* context, const trl_failure* notice) noexcept {
  auto* held = static_cast<connection::state*>(context);
  try {
    held->handler(detail::failure_of(notice));
  } catch (...) {
    if (!held->pending) {
      held->pending = std::current_exception();
    }
  }
}

connection::connection(trl_conn* made) : state_{std::make_unique<state>()} { state_->conn.reset(made); }

connection::connection(connection&&) noexcept = default;

connection& connection::operator=(connection&&) noexcept = default;

connection::~connection() = default;

connection connection::connect(const target& where, const std::string& user, mechanism& proof,
                               const connect_options& options) {
  trl_conn* made = nullptr;
  const trl_status status = trl_connect(where.get(), user.c_str(), proof.get(), options.get(), &made);
  connection opened{made};
  detail::check_on(opened, status);
  return opened;
}

connection connection::open(std::unique_ptr<byte_stream> stream, const std::string& user, mechanism& proof,
                            const connect_options& options) {
  connection opened{nullptr};
  byte_stream* held = stream.get();
  opened.state_->stream = std::move(stream);
  const trl_stream functions{
      .context = held,
      .read = stream_read,
      .write = stream_write,
      .close = stream_close,
      .bound = held->bounds() ? stream_bound : nullptr,
  };
  trl_conn* made = nullptr;
  const trl_status status = trl_connect_stream(&functions, user.c_str(), proof.get(), options.get(), &made);
  opened.state_->conn.reset(made);
  detail::check_on(opened, status);
  return opened;
}

connection connection::open(const std::string& uri) {
  trl_conn* made = nullptr;
  const trl_status status = trl_connect_uri(uri.c_str(), &made);
  connection opened{made};
  detail::check_on(opened, status);
  return opened;
}

trl_conn* connection::get() const noexcept { return state_->conn.get(); }

bool connection::lost() const noexcept { return trl_conn_is_lost(get()); }

std::string_view connection::server() const noexcept { return detail::view(trl_conn_server(get())); }

std::string_view connection::authorization() const noexcept { return detail::view(trl_conn_authorization(get())); }

bool connection::granted(std::string_view name) const { return trl_conn_granted(get(), std::string{name}.c_str()); }

std::vector<std::string_view> connection::granted() const {
  std::vector<std::string_view> names;
  names.reserve(trl_conn_granted_count(get()));
  for (std::size_t i = 0; i < trl_conn_granted_count(get()); ++i) {
    names.emplace_back(trl_conn_granted_at(get(), i));
  }
  return names;
}

std::optional<std::string_view> connection::setting(std::string_view name) const {
  return detail::maybe(trl_conn_setting(get(), std::string{name}.c_str()));
}

std::vector<std::pair<std::string_view, std::string_view>> connection::settings() const {
  std::vector<std::pair<std::string_view, std::string_view>> told;
  const char* name = nullptr;
  const char* value = nullptr;
  for (std::size_t i = 0; trl_conn_setting_at(get(), i, &name, &value); ++i) {
    told.emplace_back(detail::view(name), detail::view(value));
  }
  return told;
}

void connection::set_read_timeout(std::chrono::milliseconds wait) {
  detail::check_on(*this, trl_conn_set_read_timeout(get(), milliseconds(wait)));
}

std::chrono::milliseconds connection::read_timeout() const noexcept {
  return std::chrono::milliseconds{trl_conn_read_timeout(get())};
}

void connection::set_max_body(std::uint32_t bytes) { detail::check_on(*this, trl_conn_set_max_body(get(), bytes)); }

void connection::ping() { detail::check_on(*this, trl_conn_ping(get())); }

void connection::terminate() { detail::check_on(*this, trl_conn_terminate(get())); }

canceller connection::make_canceller() {
  trl_canceller* made = nullptr;
  detail::check_on(*this, trl_canceller_new(get(), &made));
  return canceller{made};
}

void connection::set_autocommit(bool on) { detail::check_on(*this, trl_conn_set_autocommit(get(), on)); }

bool connection::autocommit() const noexcept { return trl_conn_autocommit(get()); }

bool connection::in_transaction() const noexcept { return trl_conn_in_transaction(get()); }

std::optional<std::string_view> connection::warning() const noexcept { return detail::maybe(trl_conn_warning(get())); }

std::optional<std::uint64_t> connection::rows_affected() const noexcept {
  std::uint64_t rows = 0;
  return trl_conn_rows_affected(get(), &rows) ? std::optional<std::uint64_t>{rows} : std::nullopt;
}

std::vector<std::string_view> connection::generated() const {
  std::vector<std::string_view> values;
  values.reserve(trl_conn_generated_count(get()));
  for (std::size_t i = 0; i < trl_conn_generated_count(get()); ++i) {
    values.emplace_back(trl_conn_generated_at(get(), i));
  }
  return values;
}

std::optional<timing> connection::server_time() const noexcept {
  trl_timing told{};
  if (!trl_conn_server_time(get(), &told)) {
    return std::nullopt;
  }
  const auto micros = [](std::uint64_t count) {
    return std::chrono::microseconds{static_cast<std::chrono::microseconds::rep>(count)};
  };
  return timing{.total = micros(told.total), .queued = micros(told.queued), .locks = micros(told.locks)};
}

std::optional<std::uint64_t> connection::commit_position() const noexcept {
  std::uint64_t position = 0;
  return trl_conn_commit_position(get(), &position) ? std::optional<std::uint64_t>{position} : std::nullopt;
}

std::vector<failure> connection::conditions() const {
  std::vector<failure> raised;
  raised.reserve(trl_conn_condition_count(get()));
  for (std::size_t i = 0; i < trl_conn_condition_count(get()); ++i) {
    raised.push_back(detail::failure_of(trl_conn_condition_at(get(), i)));
  }
  return raised;
}

void connection::clear_conditions() noexcept { trl_conn_clear_conditions(get()); }

void connection::on_notice(notice_handler handler) {
  state_->handler = std::move(handler);
  const trl_notice_handler c_handler = state_->handler ? detail::conn_access::notice : nullptr;
  detail::check_on(*this, trl_conn_set_notice_handler(get(), c_handler, state_.get()));
}

std::vector<failure> connection::notices() const {
  std::vector<failure> kept;
  kept.reserve(trl_conn_notice_count(get()));
  for (std::size_t i = 0; i < trl_conn_notice_count(get()); ++i) {
    kept.push_back(detail::failure_of(trl_conn_notice_at(get(), i)));
  }
  return kept;
}

void connection::clear_notices() noexcept { trl_conn_clear_notices(get()); }

void connection::reset() { detail::check_on(*this, trl_conn_reset(get())); }

void connection::change_user(const std::string& user, mechanism& proof) {
  detail::check_on(*this, trl_conn_change_user(get(), user.c_str(), proof.get()));
}

void connection::set_query_attributes(const std::vector<std::pair<std::string, std::string>>& attributes) {
  std::vector<const char*> names;
  std::vector<const char*> values;
  names.reserve(attributes.size());
  values.reserve(attributes.size());
  for (const auto& [name, value] : attributes) {
    names.push_back(name.c_str());
    values.push_back(value.c_str());
  }
  detail::check_on(*this, trl_conn_set_query_attributes(get(), names.data(), values.data(), attributes.size()));
}

server_role connection::role() {
  trl_server_role told = TRL_SERVER_UNKNOWN;
  detail::check_on(*this, trl_conn_role(get(), &told));
  switch (told) {
  case TRL_SERVER_READ_WRITE:
    return server_role::read_write;
  case TRL_SERVER_READ_ONLY:
    return server_role::read_only;
  case TRL_SERVER_UNKNOWN:
    break;
  }
  return server_role::unknown;
}

void canceller::freer::operator()(trl_canceller* made) const noexcept { trl_canceller_free(made); }

canceller::canceller(trl_canceller* made) noexcept : canceller_{made} {}

void canceller::cancel() { detail::check(trl_cancel(canceller_.get()), trl_canceller_failure(canceller_.get())); }

void canceller::cancel(std::chrono::milliseconds within) {
  detail::check(trl_cancel_within(canceller_.get(), milliseconds(within)), trl_canceller_failure(canceller_.get()));
}

} // namespace turborel
