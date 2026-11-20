/// @file
/// @brief Tests of turborel::interval, the protocol's interval (RULES.md
/// D16).

#include <turborel/interval.hpp>

#include <turborel/int128.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <optional>

/// Verifies: [PROTOCOL Data types] An interval's fields are numbered 0 YEAR,
/// 1 MONTH, 2 DAY, 3 HOUR, 4 MINUTE and 5 SECOND, and no other number is
/// one; a qualifier ending at MONTH is a year-month one.
TEST_CASE("interval fields are numbered as the protocol numbers them") {
  CHECK(turborel::interval_field_of(0) == turborel::interval_field::year);
  CHECK(turborel::interval_field_of(1) == turborel::interval_field::month);
  CHECK(turborel::interval_field_of(2) == turborel::interval_field::day);
  CHECK(turborel::interval_field_of(3) == turborel::interval_field::hour);
  CHECK(turborel::interval_field_of(4) == turborel::interval_field::minute);
  CHECK(turborel::interval_field_of(5) == turborel::interval_field::second);
  CHECK(turborel::interval_field_of(6) == std::nullopt);
  CHECK(turborel::interval_qualifier{.start = turborel::interval_field::year, .end = turborel::interval_field::month}
            .year_month());
  CHECK_FALSE(
      turborel::interval_qualifier{.start = turborel::interval_field::day, .end = turborel::interval_field::second}
          .year_month());
}

/// Verifies: [PROTOCOL Values] An interval's units are months of a
/// year-month interval and microseconds of a day-time one, given as the
/// chrono duration of its kind alone, and only where they fit it.
TEST_CASE("an interval's units are months or microseconds") {
  const turborel::interval_qualifier year_to_month{
      .start = turborel::interval_field::year,
      .end = turborel::interval_field::month,
      .leading_precision = 2,
  };
  const turborel::interval_qualifier day_to_second{
      .start = turborel::interval_field::day,
      .end = turborel::interval_field::second,
      .leading_precision = 2,
      .fractional_precision = 2,
  };
  const turborel::interval months{.qualifier = year_to_month, .units = turborel::int128{-14}};
  const turborel::interval micros{.qualifier = day_to_second, .units = turborel::int128{-1'500'000}};
  CHECK(months.months() == std::chrono::months{-14});
  CHECK(months.microseconds() == std::nullopt);
  CHECK(micros.microseconds() == std::chrono::microseconds{-1'500'000});
  CHECK(micros.months() == std::nullopt);
  CHECK(turborel::interval{.qualifier = day_to_second, .units = turborel::int128::max()}.microseconds() ==
        std::nullopt);
  CHECK(turborel::interval{.qualifier = year_to_month, .units = turborel::int128::min()}.months() == std::nullopt);
  CHECK(turborel::interval{.qualifier = year_to_month, .units = turborel::int128::from_halves(1, 0)}.months() ==
        std::nullopt);
}
