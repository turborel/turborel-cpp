/// @file
/// @brief Import through turborel-c [CLIENT Importing]: the program's
/// source or delimited text, its rows inserted a batch at a time.

#include <turborel/import.hpp>

#include "capi.hpp"
#include "connection_state.hpp"
#include "params_access.hpp"

#include <turborel/connection.hpp>
#include <turborel/error.hpp>
#include <turborel/params.hpp>
#include <turborel/turborel.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <ios>
#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace turborel {

namespace {

/// The most bytes kept of the message of what a source threw, its null
/// among them.
constexpr std::size_t source_message_bytes = 1024;

/// SQLSTATE 22000, the input's failure (the drivers' shared table).
constexpr std::string_view input_sqlstate = "22000";

/// A string turborel-c gives, copied; null as empty.
std::string text_of(const char* text) { return std::string{detail::view(text)}; }

/// The driver's position of turborel-c's.
record_position position_of(const trl_position& at) noexcept {
  return record_position{.record = at.record, .line = at.line};
}

/// The driver's cause of turborel-c's.
import_cause cause_of(const trl_cause& cause) {
  return import_cause{
      .sql = cause.sql,
      .message = text_of(cause.message),
      .condition = cause.failure == nullptr ? std::nullopt : std::optional<failure>{detail::failure_of(cause.failure)},
  };
}

/// What an import shares with its callbacks: the source, what they keep
/// alive between calls, and what the program's code threw.
struct importing {
  import_source* source{};
  const import_options* how{};
  std::vector<std::string> names;
  std::vector<const char*> pointers;
  /// The message of what the source threw of std::exception, cut to fit,
  /// kept with no allocation, which could fail.
  std::array<char, source_message_bytes> message{};
  std::exception_ptr thrown;
};

/// Runs a call of the program's source: what it throws of
/// std::exception is the input's failure, its message kept; anything else
/// is thrown once the import returns (R-RAII-2).
template <typename Call>
trl_source_read guarded(importing& held, trl_source_error* error, const Call& call) noexcept {
  try {
    return call();
  } catch (const std::exception& why) {
    const std::string_view said{why.what()};
    const std::size_t kept = std::min(said.size(), held.message.size() - 1);
    *std::copy_n(said.begin(), kept, held.message.begin()) = '\0';
    error->message = held.message.data();
    return TRL_READ_FAILED;
  } catch (...) {
    held.thrown = std::current_exception();
    return TRL_READ_FAILED;
  }
}

/// The program's source's headings, as turborel-c asks for them.
trl_source_read headings_of(void* context, trl_headings* out, trl_source_error* error) noexcept {
  auto& held = *static_cast<importing*>(context);
  return guarded(held, error, [&held, out] {
    std::optional<std::vector<std::string>> names = held.source->headings();
    *out = trl_headings{.names = nullptr, .count = 0};
    if (names) {
      held.names = std::move(*names);
      held.pointers.reserve(held.names.size());
      for (const std::string& name : held.names) {
        held.pointers.push_back(name.c_str());
      }
      *out = trl_headings{.names = held.pointers.data(), .count = held.pointers.size()};
    }
    return TRL_READ_OK;
  });
}

/// The program's source's next record, as turborel-c asks for it; where
/// the source said it was, read or failing.
trl_source_read record_of(void* context, trl_params* values, trl_position* at, trl_source_error* error) noexcept {
  auto& held = *static_cast<importing*>(context);
  record_position where;
  const trl_source_read read = guarded(held, error, [&held, values, &where] {
    params lent = detail::params_access::lent(values);
    return held.source->record(lent, where) ? TRL_READ_OK : TRL_READ_END;
  });
  *at = trl_position{.record = where.record, .line = where.line};
  error->position = *at;
  return read;
}

/// Gives a row skipped to the program's handler, keeping what it throws.
void rejected_to(void* context, const trl_rejected* row) noexcept {
  auto& held = *static_cast<importing*>(context);
  if (held.thrown || !held.how->rejected) {
    return;
  }
  try {
    held.how->rejected(rejected_row{.at = position_of(row->position), .cause = cause_of(row->cause)});
  } catch (...) {
    held.thrown = std::current_exception();
  }
}

/// Runs an import of `source`, throwing what it came to if it failed.
imported run(connection& conn, const trl_source& source, importing& held) {
  const trl_import c_how{
      .table = held.how->table.c_str(),
      .batch = held.how->batch,
      .skip = held.how->skip,
      .rejected = rejected_to,
      .context = &held,
  };
  trl_imported* made = nullptr;
  const trl_status status = trl_import_rows(conn.get(), &source, &c_how, &made);
  const detail::owner<trl_imported, trl_imported_free> done{made};
  if (held.thrown) {
    std::rethrow_exception(held.thrown);
  }
  trl_cause cause{};
  if (status == TRL_FAILED && made != nullptr && trl_imported_cause(made, &cause)) {
    trl_position at{};
    const bool placed = trl_imported_position(made, &at);
    const std::string text = detail::written([&made](char* out, std::size_t capacity, std::size_t* length) {
      *length = trl_imported_text(made, out, capacity);
    });
    throw import_error{cause_of(cause), placed ? std::optional{position_of(at)} : std::nullopt, trl_imported_rows(made),
                       trl_imported_rolled_back(made), text};
  }
  detail::check_on(conn, status);
  return imported{.rows = trl_imported_rows(made), .rejected = trl_imported_rejected(made)};
}

/// The failure of an import, of its cause.
failure failure_of(const import_cause& why, const std::string& text) {
  if (why.condition) {
    failure condition = *why.condition;
    return condition;
  }
  return failure{.sqlcode = detail::data_sqlcode, .sqlstate = std::string{input_sqlstate}, .message = text};
}

/// Reads delimited text's bytes from its stream.
bool read_text(void* context, std::uint8_t* into, std::size_t capacity, std::size_t* length) noexcept {
  auto* in = static_cast<std::istream*>(context);
  try {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the octets read as the stream's chars.
    in->read(reinterpret_cast<char*>(into), static_cast<std::streamsize>(capacity));
    *length = static_cast<std::size_t>(in->gcount());
    return !in->bad();
  } catch (...) {
    return false;
  }
}

} // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): named for what they count, in the order turborel-client's ImportError gives them.
import_error::import_error(import_cause why, std::optional<record_position> at, std::uint64_t committed,
                           std::uint64_t rolled_back, const std::string& text)
    : error{failure_of(why, text)}, cause_{std::make_shared<const import_cause>(std::move(why))}, at_{at},
      committed_{committed}, rolled_back_{rolled_back} {}

dialect dialect::csv() {
  const trl_dialect c = trl_dialect_csv();
  return dialect{
      .delimiter = c.delimiter,
      .quoted = c.quoted,
      .header = c.header,
      .null = text_of(c.null),
      .max_record = c.max_record,
  };
}

dialect dialect::tsv() {
  const trl_dialect c = trl_dialect_tsv();
  return dialect{
      .delimiter = c.delimiter,
      .quoted = c.quoted,
      .header = c.header,
      .null = text_of(c.null),
      .max_record = c.max_record,
  };
}

void delimited_text::freer::operator()(trl_delimited* made) const noexcept { trl_delimited_free(made); }

delimited_text::delimited_text(std::istream& in, const dialect& how) : null_{how.null} {
  const trl_dialect c_how{
      .delimiter = how.delimiter,
      .quoted = how.quoted,
      .header = how.header,
      .null = null_.c_str(),
      .max_record = how.max_record,
  };
  trl_delimited* made = nullptr;
  detail::check(trl_delimited_new(read_text, &in, &c_how, &made));
  text_.reset(made);
}

imported import_rows(connection& conn, import_source& source, const import_options& how) {
  importing held{.source = &source, .how = &how};
  const trl_source c_source{.context = &held, .headings = headings_of, .record = record_of};
  return run(conn, c_source, held);
}

imported import_rows(connection& conn, delimited_text& source, const import_options& how) {
  importing held{.source = nullptr, .how = &how};
  return run(conn, trl_delimited_source(source.get()), held);
}

} // namespace turborel
