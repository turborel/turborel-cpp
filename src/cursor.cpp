/// @file
/// @brief Cursors through turborel-c [CLIENT Prepared statements]: opened,
/// fetched, closed and described, their rows read as an input range.

#include <turborel/cursor.hpp>

#include "connection_state.hpp"

#include <turborel/connection.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/statement.hpp>
#include <turborel/turborel.h>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace turborel {

namespace {

/// turborel-c's cursor of a statement and a number.
trl_cursor c_cursor(const statement& prepared, std::uint32_t number) noexcept {
  return trl_cursor{.statement = prepared.get(), .number = number};
}

} // namespace

row cursor::iterator::operator*() const { return cursor_->batch_.at(index_); }

cursor::iterator& cursor::iterator::operator++() {
  ++index_;
  if (index_ >= cursor_->batch_.size() && cursor_->more()) {
    cursor_->fetch(cursor_->rows_);
    index_ = 0;
  }
  return *this;
}

bool cursor::iterator::at_end() const noexcept {
  return cursor_ == nullptr || (index_ >= cursor_->batch_.size() && !cursor_->more());
}

cursor::cursor(connection& conn, const statement& prepared, std::uint32_t number, result first)
    : conn_{&conn}, statement_{&prepared}, number_{number}, batch_{std::move(first)}, open_{batch_.more()} {}

cursor cursor::open(connection& conn, const statement& prepared, const params& values, std::uint32_t rows,
                    std::uint32_t number) {
  trl_result* made = nullptr;
  const trl_status status = trl_cursor_open(conn.get(), c_cursor(prepared, number), values.get(), rows, &made);
  result first{made};
  detail::check_on(conn, status);
  cursor opened{conn, prepared, number, std::move(first)};
  opened.rows_ = rows;
  return opened;
}

cursor::cursor(cursor&& other) noexcept
    : conn_{other.conn_}, statement_{other.statement_}, number_{other.number_}, rows_{other.rows_},
      batch_{std::move(other.batch_)}, open_{std::exchange(other.open_, false)} {}

cursor& cursor::operator=(cursor&& other) noexcept {
  if (this != &other) {
    close_quietly();
    conn_ = other.conn_;
    statement_ = other.statement_;
    number_ = other.number_;
    rows_ = other.rows_;
    batch_ = std::move(other.batch_);
    open_ = std::exchange(other.open_, false);
  }
  return *this;
}

cursor::~cursor() { close_quietly(); }

void cursor::close_quietly() noexcept {
  if (open_ && !conn_->lost()) {
    // Nothing is thrown here (R-RAII-2): a close that fails leaves its
    // failure for the connection's next call to tell.
    static_cast<void>(trl_cursor_close(conn_->get(), c_cursor(*statement_, number_)));
  }
  open_ = false;
}

const result& cursor::fetch(std::uint32_t rows) {
  trl_result* made = nullptr;
  const trl_status status = trl_cursor_fetch(conn_->get(), c_cursor(*statement_, number_), rows, &made);
  result next{made};
  detail::check_on(*conn_, status);
  batch_ = std::move(next);
  open_ = batch_.more();
  return batch_;
}

void cursor::close() {
  open_ = false;
  detail::check_on(*conn_, trl_cursor_close(conn_->get(), c_cursor(*statement_, number_)));
}

result cursor::describe() {
  trl_result* made = nullptr;
  const trl_status status = trl_describe_cursor(conn_->get(), c_cursor(*statement_, number_), &made);
  result described{made};
  detail::check_on(*conn_, status);
  return described;
}

} // namespace turborel
