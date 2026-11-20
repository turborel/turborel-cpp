/// @file
/// @brief The data exceptions the driver raises itself, of a value it
/// cannot represent (RULES.md D10).

#ifndef TURBOREL_SRC_DATA_ERROR_HPP
#define TURBOREL_SRC_DATA_ERROR_HPP

#include <string>
#include <string_view>

namespace turborel::detail {

/// SQLSTATE 22008, datetime field overflow.
inline constexpr std::string_view datetime_field_overflow = "22008";
/// SQLSTATE 22009, invalid time zone displacement value.
inline constexpr std::string_view invalid_time_zone_displacement = "22009";

/// Throws a data exception of `sqlstate`, with turborel's SQLCODE of one,
/// -301, and `message`.
/// @param sqlstate Its SQLSTATE, of class 22.
/// @param message What is wrong, in plain words.
/// @throws turborel::error Always.
[[noreturn]] void throw_data_error(std::string_view sqlstate, std::string message);

} // namespace turborel::detail

#endif // TURBOREL_SRC_DATA_ERROR_HPP
