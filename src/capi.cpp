/// @file
/// @brief The boundary with turborel-c (RULES.md D1, D10): its failures
/// copied into the driver's, and each status other than TRL_OK thrown as
/// the driver's error.

#include "capi.hpp"

#include <turborel/error.hpp>
#include <turborel/turborel.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace turborel::detail {

namespace {

/// A string turborel-c gives that may be null, as an optional string.
std::optional<std::string> copied(const char* text) {
  return text == nullptr ? std::nullopt : std::optional<std::string>{text};
}

/// The diagnostics of `failure`, tags 7 to 15, or none if it tells none.
std::optional<turborel::diagnostics> diagnostics_of(const trl_failure* failure) {
  const auto at = [failure](trl_diagnostic which) { return copied(trl_failure_diagnostic(failure, which)); };
  turborel::diagnostics told{
      .severity = at(TRL_DIAGNOSTIC_SEVERITY),
      .detail = at(TRL_DIAGNOSTIC_DETAIL),
      .hint = at(TRL_DIAGNOSTIC_HINT),
      .context = at(TRL_DIAGNOSTIC_CONTEXT),
      .schema = at(TRL_DIAGNOSTIC_SCHEMA),
      .table = at(TRL_DIAGNOSTIC_TABLE),
      .column = at(TRL_DIAGNOSTIC_COLUMN),
      .data_type = at(TRL_DIAGNOSTIC_DATA_TYPE),
      .constraint = at(TRL_DIAGNOSTIC_CONSTRAINT),
  };
  if (told == turborel::diagnostics{}) {
    return std::nullopt;
  }
  return told;
}

/// The failure of a status turborel-c gives with no handle's failure
/// [DESIGN] D10.
struct by_status {
  /// The status.
  trl_status status;
  /// Its SQLCODE.
  std::int32_t sqlcode;
  /// Its SQLSTATE.
  std::string_view sqlstate;
  /// What it says.
  std::string_view message;
};

/// The failures of the statuses, by their values: TRL_FAILED's that of a
/// handle that keeps no failure, the misuse; TRL_OK's and TRL_NO_MEMORY's
/// never read.
constexpr std::array statuses{
    by_status{
        .status = TRL_OK,
        .sqlcode = misuse_sqlcode,
        .sqlstate = misuse_sqlstate,
        .message = "the call did what was asked",
    },
    by_status{
        .status = TRL_INVALID_ARGUMENT,
        .sqlcode = misuse_sqlcode,
        .sqlstate = misuse_sqlstate,
        .message = "an argument is not one the call takes",
    },
    by_status{
        .status = TRL_INVALID_TEXT,
        .sqlcode = data_sqlcode,
        .sqlstate = invalid_text_sqlstate,
        .message = "the text writes no value of the type asked for",
    },
    by_status{
        .status = TRL_OUT_OF_RANGE,
        .sqlcode = data_sqlcode,
        .sqlstate = out_of_range_sqlstate,
        .message = "the value is out of the range of the type asked for",
    },
    by_status{
        .status = TRL_FAILED,
        .sqlcode = misuse_sqlcode,
        .sqlstate = misuse_sqlstate,
        .message = "turborel-c kept no failure of a call that failed",
    },
    by_status{
        .status = TRL_NO_MEMORY,
        .sqlcode = misuse_sqlcode,
        .sqlstate = misuse_sqlstate,
        .message = "memory could not be had",
    },
};
static_assert(statuses.at(TRL_FAILED).status == TRL_FAILED && statuses.at(TRL_NO_MEMORY).status == TRL_NO_MEMORY);

/// The failure of a status turborel-c gives with no handle's failure.
failure plain(std::int32_t sqlcode, std::string_view sqlstate, std::string message) {
  return failure{.sqlcode = sqlcode, .sqlstate = std::string{sqlstate}, .message = std::move(message)};
}

} // namespace

failure failure_of(const trl_failure* failure) {
  std::uint64_t position = 0;
  const bool placed = trl_failure_position(failure, &position);
  return turborel::failure{
      .sqlcode = trl_failure_sqlcode(failure),
      .sqlstate = trl_failure_sqlstate(failure),
      .message = trl_failure_message(failure),
      .rule = copied(trl_failure_rule(failure)),
      .position = placed ? std::optional<std::uint64_t>{position} : std::nullopt,
      .active = trl_failure_active(failure),
      .diagnostics = diagnostics_of(failure),
  };
}

void raise(trl_status status, const trl_failure* kept) {
  if (status == TRL_NO_MEMORY) {
    throw std::bad_alloc{};
  }
  if (status == TRL_FAILED && kept != nullptr) {
    throw error{failure_of(kept)};
  }
  const by_status& told = statuses.at(static_cast<std::size_t>(status));
  throw error{plain(told.sqlcode, told.sqlstate, std::string{told.message})};
}

void raise_misuse(std::string message) { throw error{plain(misuse_sqlcode, misuse_sqlstate, std::move(message))}; }

} // namespace turborel::detail
