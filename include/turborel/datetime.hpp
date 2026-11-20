/// @file
/// @brief Dates, times and timestamps in std::chrono, and their mapping to
/// the protocol's datetime values, tags 8, 9 and 11 [PROTOCOL Values]
/// (RULES.md D16).

#ifndef TURBOREL_DATETIME_HPP
#define TURBOREL_DATETIME_HPP

#include <turborel/export.hpp>

#include <chrono>
#include <cstdint>

namespace turborel {

/// The body of a datetime value as the protocol writes it, the same for a
/// date, a time and a timestamp [PROTOCOL Values]. `utc` counts
/// microseconds as turborel keeps them: of a timestamp, from 0001-01-01
/// 00:00:00 UTC; of a time, from midnight UTC, within a day; of a date,
/// from 0001-01-01 to its midnight.
struct datetime_fields {
  /// Whether its type is WITH TIME ZONE: the zoned byte.
  bool zoned{};
  /// The digits of its seconds' fraction shown, from 0 to 6.
  std::uint8_t precision{};
  /// Its time zone displacement in minutes: its own if zoned, else the one
  /// it is shown in.
  std::int16_t displacement{};
  /// Its microseconds, as above.
  std::int64_t utc{};

  /// Whether two hold the same fields.
  friend bool operator==(const datetime_fields&, const datetime_fields&) noexcept = default;
};

/// The least time zone displacement turborel takes, -12:59, in minutes.
inline constexpr std::chrono::minutes min_displacement{-((12 * 60) + 59)};
/// The greatest, +14:00.
inline constexpr std::chrono::minutes max_displacement{14 * 60};

/// A DATE value: a day of the Gregorian calendar, 0001-01-01 to 9999-12-31.
struct TURBOREL_CPP_EXPORT date {
  /// The day.
  std::chrono::year_month_day day;

  /// Its fields as the protocol writes a date, value tag 8.
  /// @return Its fields.
  /// @throws turborel::error SQLSTATE 22008 if the day is not one, or not
  ///   in 0001 to 9999.
  [[nodiscard]] datetime_fields fields() const;

  /// The date `fields` writes; its displacement, precision and zone are a
  /// date's none.
  /// @param fields A date's fields, as the protocol wrote them.
  /// @return The date.
  /// @throws turborel::error SQLSTATE 22008 if they write no day of 0001 to
  ///   9999.
  [[nodiscard]] static date from_fields(const datetime_fields& fields);

  /// Whether two are the same day.
  friend bool operator==(const date&, const date&) noexcept = default;
};

/// A TIME value, WITH TIME ZONE or not: a time of day in UTC, and the
/// displacement it is its own or is shown in.
struct TURBOREL_CPP_EXPORT time {
  /// The time since midnight UTC, under a day.
  std::chrono::microseconds utc{};
  /// The time zone displacement.
  std::chrono::minutes displacement{};
  /// Whether its type is WITH TIME ZONE.
  bool zoned{};
  /// The digits of its seconds' fraction shown.
  std::uint8_t precision{};

  /// The time of day at its displacement.
  /// @return Its local time of day.
  /// @throws turborel::error SQLSTATE 22008 if `utc` is not under a day,
  ///   or 22009 if the displacement is out of range.
  [[nodiscard]] std::chrono::hh_mm_ss<std::chrono::microseconds> local() const;

  /// Its fields as the protocol writes a time, value tag 9.
  /// @return Its fields.
  /// @throws turborel::error As local() does.
  [[nodiscard]] datetime_fields fields() const;

  /// The time `fields` writes.
  /// @param fields A time's fields, as the protocol wrote them.
  /// @return The time.
  /// @throws turborel::error As local() does, of the time read.
  [[nodiscard]] static time from_fields(const datetime_fields& fields);

  /// Whether two hold the same fields.
  friend bool operator==(const time&, const time&) noexcept = default;
};

/// A TIMESTAMP value, WITH TIME ZONE or not: an instant, and the
/// displacement it is its own or is shown in.
struct TURBOREL_CPP_EXPORT timestamp {
  /// The instant, 0001-01-01 00:00:00 to 9999-12-31 23:59:59.999999 UTC.
  std::chrono::sys_time<std::chrono::microseconds> instant;
  /// The time zone displacement.
  std::chrono::minutes displacement{};
  /// Whether its type is WITH TIME ZONE.
  bool zoned{};
  /// The digits of its seconds' fraction shown.
  std::uint8_t precision{};

  /// The date and time at its displacement.
  /// @return Its local date and time.
  /// @throws turborel::error SQLSTATE 22008 if the instant is out of range,
  ///   or 22009 if the displacement is.
  [[nodiscard]] std::chrono::local_time<std::chrono::microseconds> local() const;

  /// Its fields as the protocol writes a timestamp, value tag 11.
  /// @return Its fields.
  /// @throws turborel::error As local() does.
  [[nodiscard]] datetime_fields fields() const;

  /// The timestamp `fields` writes.
  /// @param fields A timestamp's fields, as the protocol wrote them.
  /// @return The timestamp.
  /// @throws turborel::error As local() does, of the timestamp read.
  [[nodiscard]] static timestamp from_fields(const datetime_fields& fields);

  /// Whether two hold the same fields.
  friend bool operator==(const timestamp&, const timestamp&) noexcept = default;
};

} // namespace turborel

#endif // TURBOREL_DATETIME_HPP
