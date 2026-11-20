/// @file
/// @brief Bulk copy and export through turborel-c [CLIENT Copying and
/// exporting].

#include <turborel/copy.hpp>

#include "capi.hpp"
#include "connection_state.hpp"
#include "params_access.hpp"

#include <turborel/connection.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/turborel.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <ios>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace turborel {

namespace {

/// The table and columns of a copy, as turborel-c takes them, kept while
/// the call lasts.
struct c_target {
  std::vector<const char*> names;
  trl_copy_target target;

  c_target(const std::string& table, const std::vector<std::string>& columns) : target{} {
    names.reserve(columns.size());
    for (const std::string& column : columns) {
      names.push_back(column.c_str());
    }
    target = trl_copy_target{
        .table = table.c_str(),
        .columns = names.empty() ? nullptr : names.data(),
        .column_count = names.size(),
    };
  }
};

/// A row source's context: the program's function, and what it threw.
struct rows_of {
  const std::function<bool(params&)>* next;
  std::exception_ptr thrown;
};

/// Gives the next row, as turborel-c calls a source (R-RAII-2).
trl_source_status next_row(void* context, trl_params* row) noexcept {
  auto* held = static_cast<rows_of*>(context);
  try {
    params lent = detail::params_access::lent(row);
    return (*held->next)(lent) ? TRL_SOURCE_ROW : TRL_SOURCE_END;
  } catch (...) {
    held->thrown = std::current_exception();
    return TRL_SOURCE_FAILED;
  }
}

/// An export's writer's context: the stream.
bool write_to(void* context, const std::uint8_t* bytes, std::size_t length) noexcept {
  auto* out = static_cast<std::ostream*>(context);
  try {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the octets written as the stream's chars.
    out->write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(length));
    return static_cast<bool>(*out);
  } catch (...) {
    return false;
  }
}

/// A native reader's read of a stream.
std::size_t read_from(void* context, std::uint8_t* into, std::size_t capacity) noexcept {
  auto* in = static_cast<std::istream*>(context);
  try {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the octets read as the stream's chars.
    in->read(reinterpret_cast<char*>(into), static_cast<std::streamsize>(capacity));
    return static_cast<std::size_t>(in->gcount());
  } catch (...) {
    return 0;
  }
}

} // namespace

void copy::freer::operator()(trl_copy* made) const noexcept { trl_copy_free(made); }

copy copy::into(connection& conn, const std::string& table, const std::vector<std::string>& columns) {
  const c_target where{table, columns};
  trl_copy* made = nullptr;
  const trl_status status = trl_copy_in(conn.get(), &where.target, &made);
  copy begun{conn, made};
  detail::check_on(conn, status);
  return begun;
}

copy& copy::send(const params& row) {
  detail::check_on(*conn_, trl_copy_send(copy_.get(), row.get()));
  return *this;
}

std::uint64_t copy::finish() {
  std::uint64_t rows = 0;
  detail::check_on(*conn_, trl_copy_finish(copy_.get(), &rows));
  return rows;
}

void copy::abandon() { detail::check_on(*conn_, trl_copy_abandon(copy_.get())); }

std::uint64_t copy_rows(connection& conn, const std::string& table, const std::vector<std::string>& columns,
                        const std::function<bool(params&)>& next) {
  const c_target where{table, columns};
  rows_of source{.next = &next, .thrown = nullptr};
  std::uint64_t rows = 0;
  const trl_status status = trl_copy_rows(conn.get(), &where.target, next_row, &source, &rows);
  if (source.thrown) {
    std::rethrow_exception(source.thrown);
  }
  detail::check_on(conn, status);
  return rows;
}

std::uint64_t export_rows(connection& conn, const std::string& sql, export_format format, std::ostream& out) {
  const trl_export_sink sink{
      .format = format == export_format::native ? TRL_EXPORT_NATIVE : TRL_EXPORT_CSV,
      .write = write_to,
      .context = &out,
  };
  std::uint64_t rows = 0;
  detail::check_on(conn, trl_export(conn.get(), sql.c_str(), &sink, &rows));
  return rows;
}

result read_native(std::istream& in) {
  trl_result* made = nullptr;
  const trl_status status = trl_read_native(read_from, &in, &made);
  result read{made};
  detail::check(status, trl_result_failure(made));
  return read;
}

} // namespace turborel
