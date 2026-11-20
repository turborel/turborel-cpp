/// @file
/// @brief Tests of turborel's dates, times and timestamps, the protocol's
/// datetime values (RULES.md D16).

#include <turborel/datetime.hpp>

#include <turborel/error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

namespace {

using namespace std::chrono_literals; // NOLINT(google-build-using-namespace): chrono's literals read as the values do.

/// Microseconds from 0001-01-01 to 1970-01-01, 719,162 days.
constexpr std::int64_t unix_epoch_utc = 62'135'596'800'000'000;
/// Microseconds from 0001-01-01 to the last of 9999-12-31.
constexpr std::int64_t last_utc = 315'537'897'599'999'999;

/// The SQLSTATE `call` throws as a turborel::error, with a data
/// exception's SQLCODE; empty if it throws none.
std::string sqlstate_of(const std::function<void()>& call) {
  try {
    call();
  } catch (const turborel::error& e) {
    CHECK(e.sqlcode() == -301);
    return std::string{e.sqlstate()};
  }
  return {};
}

} // namespace

/// Verifies: [PROTOCOL Values] A timestamp's fields are its zoned byte, its
/// precision, its displacement in minutes and its microseconds in UTC from
/// 0001-01-01, as turborel counts them, at both ends of its range.
TEST_CASE("a timestamp maps to the protocol's fields") {
  const turborel::timestamp noon{
      .instant = std::chrono::sys_days{1970y / 1 / 1} + 12h,
      .displacement = 120min,
      .zoned = true,
      .precision = 6,
  };
  const turborel::datetime_fields fields{
      .zoned = true,
      .precision = 6,
      .displacement = 120,
      .utc = unix_epoch_utc + 43'200'000'000,
  };
  CHECK(noon.fields() == fields);
  CHECK(turborel::timestamp::from_fields(fields) == noon);
  CHECK(noon.local() == std::chrono::local_days{1970y / 1 / 1} + 14h);
  CHECK(turborel::timestamp::from_fields({}).instant == std::chrono::sys_days{1y / 1 / 1});
  CHECK(turborel::timestamp::from_fields({.utc = last_utc}).instant ==
        std::chrono::sys_days{9999y / 12 / 31} + 24h - 1us);
}

/// Verifies: [DESIGN] D16 A timestamp out of 0001 to 9999 fails with
/// SQLSTATE 22008, and a displacement out of -12:59 to +14:00 with 22009,
/// whichever way it is mapped.
TEST_CASE("a timestamp out of range fails as a data exception") {
  const auto before = std::chrono::sys_days{1y / 1 / 1} - 1us;
  CHECK(sqlstate_of([] { static_cast<void>(turborel::timestamp::from_fields({.utc = -1})); }) == "22008");
  CHECK(sqlstate_of([] { static_cast<void>(turborel::timestamp::from_fields({.utc = last_utc + 1})); }) == "22008");
  CHECK(sqlstate_of([&] { static_cast<void>(turborel::timestamp{.instant = before}.fields()); }) == "22008");
  CHECK(sqlstate_of([] { static_cast<void>(turborel::timestamp::from_fields({.displacement = 841})); }) == "22009");
  CHECK(sqlstate_of([] { static_cast<void>(turborel::timestamp::from_fields({.displacement = -780})); }) == "22009");
  CHECK(sqlstate_of([] { static_cast<void>(turborel::timestamp::from_fields({.displacement = -779})); }).empty());
  CHECK(sqlstate_of([] { static_cast<void>(turborel::timestamp::from_fields({.displacement = 840})); }).empty());
}

/// Verifies: [PROTOCOL Values] A date is written as a timestamp, its
/// microseconds those of its midnight from 0001-01-01; a day that is none,
/// or out of 0001 to 9999, fails with SQLSTATE 22008.
TEST_CASE("a date maps to the protocol's fields") {
  CHECK(turborel::date{.day = 1970y / 1 / 1}.fields() == turborel::datetime_fields{.utc = unix_epoch_utc});
  CHECK(turborel::date::from_fields({.utc = unix_epoch_utc}) == turborel::date{.day = 1970y / 1 / 1});
  CHECK(turborel::date::from_fields({.utc = last_utc}) == turborel::date{.day = 9999y / 12 / 31});
  CHECK(turborel::date{.day = 1y / 1 / 1}.fields().utc == 0);
  CHECK(sqlstate_of([] { static_cast<void>(turborel::date{.day = 2026y / 2 / 30}.fields()); }) == "22008");
  CHECK(sqlstate_of([] { static_cast<void>(turborel::date{.day = 0y / 12 / 31}.fields()); }) == "22008");
  CHECK(sqlstate_of([] { static_cast<void>(turborel::date{.day = 10000y / 1 / 1}.fields()); }) == "22008");
}

/// Verifies: [PROTOCOL Values] A time is written as a timestamp, its
/// microseconds from midnight UTC, within a day, shown at its displacement
/// across midnight either way; one not within a day fails with 22008.
TEST_CASE("a time maps to the protocol's fields") {
  const turborel::time late{.utc = 23h + 30min, .displacement = 60min, .zoned = true, .precision = 3};
  const turborel::datetime_fields fields{.zoned = true, .precision = 3, .displacement = 60, .utc = 84'600'000'000};
  CHECK(late.fields() == fields);
  CHECK(turborel::time::from_fields(fields) == late);
  CHECK(late.local().hours() == 0h);
  CHECK(late.local().minutes() == 30min);
  CHECK(turborel::time{.utc = 30min, .displacement = -779min}.local().hours() == 11h);
  CHECK(sqlstate_of([] { static_cast<void>(turborel::time::from_fields({.utc = 86'400'000'000})); }) == "22008");
  CHECK(sqlstate_of([] { static_cast<void>(turborel::time::from_fields({.utc = -1})); }) == "22008");
  CHECK(sqlstate_of([] { static_cast<void>(turborel::time{.displacement = 900min}.fields()); }) == "22009");
}
