/// @file
/// @brief Intervals, as the protocol writes type and value tag 13
/// [PROTOCOL Data types] [PROTOCOL Values], and their std::chrono
/// durations (RULES.md D16).

#ifndef TURBOREL_INTERVAL_HPP
#define TURBOREL_INTERVAL_HPP

#include <turborel/export.hpp>
#include <turborel/int128.hpp>

#include <chrono>
#include <cstdint>
#include <optional>

namespace turborel {

/// An interval's field, numbered as the protocol numbers it
/// [PROTOCOL Data types].
enum class interval_field : std::uint8_t {
  /// YEAR, 0.
  year = 0,
  /// MONTH, 1.
  month = 1,
  /// DAY, 2.
  day = 2,
  /// HOUR, 3.
  hour = 3,
  /// MINUTE, 4.
  minute = 4,
  /// SECOND, 5.
  second = 5,
};

/// The field the protocol numbers `number`; none if it numbers none.
/// @param number A field's number, as the protocol wrote it.
/// @return The field, or none.
[[nodiscard]] TURBOREL_CPP_EXPORT std::optional<interval_field> interval_field_of(std::uint32_t number) noexcept;

/// An interval's qualifier, as the INTERVAL type writes it: start field,
/// end field, leading precision and fractional precision, each `u32`
/// [PROTOCOL Data types].
struct interval_qualifier {
  /// Its first field.
  interval_field start{interval_field::day};
  /// Its last field, the same as the first of a single-field interval.
  interval_field end{interval_field::day};
  /// The digits of its leading field.
  std::uint32_t leading_precision{};
  /// The digits of its seconds' fraction.
  std::uint32_t fractional_precision{};

  /// Whether it is a year-month interval, of YEAR and MONTH alone, whose
  /// units are months; else it is a day-time one, whose units are
  /// microseconds.
  /// @return True if it is a year-month qualifier.
  [[nodiscard]] constexpr bool year_month() const noexcept { return end <= interval_field::month; }

  /// Whether two are the same qualifier.
  friend constexpr bool operator==(const interval_qualifier&, const interval_qualifier&) noexcept = default;
};

/// An interval value, value tag 13: its qualifier, then its units, months
/// of a year-month interval and microseconds of a day-time one
/// [PROTOCOL Values]. Every value the protocol can carry is one.
struct TURBOREL_CPP_EXPORT interval {
  /// Its qualifier.
  interval_qualifier qualifier;
  /// Its months or microseconds, as the qualifier says.
  int128 units;

  /// Its months, if it is a year-month interval and they fit
  /// std::chrono::months; else none.
  /// @return Its months, or none.
  [[nodiscard]] std::optional<std::chrono::months> months() const noexcept;

  /// Its microseconds, if it is a day-time interval and they fit
  /// std::chrono::microseconds; else none.
  /// @return Its microseconds, or none.
  [[nodiscard]] std::optional<std::chrono::microseconds> microseconds() const noexcept;

  /// Whether two have the same qualifier and units.
  friend bool operator==(const interval&, const interval&) noexcept = default;
};

} // namespace turborel

#endif // TURBOREL_INTERVAL_HPP
