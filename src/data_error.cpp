/// @file
/// @brief The data exceptions the driver raises itself (RULES.md D10).

#include "data_error.hpp"

#include <turborel/error.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace turborel::detail {

namespace {

/// turborel's SQLCODE of a data exception, a General Rule's "shall" failed
/// at run time (turborel's `Code::Data`).
constexpr std::int32_t data_sqlcode = -301;

} // namespace

void throw_data_error(std::string_view sqlstate, std::string message) {
  throw error{failure{.sqlcode = data_sqlcode, .sqlstate = std::string{sqlstate}, .message = std::move(message)}};
}

} // namespace turborel::detail
