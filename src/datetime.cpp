/// @file
/// @brief Datetimes between std::chrono and the protocol's fields
/// (RULES.md D16), in turborel's range of years, 0001 to 9999, and of
/// displacements, -12:59 to +14:00.

#include <turborel/datetime.hpp>

#include "data_error.hpp"

#include <chrono>
#include <cstdint>

namespace turborel {

namespace {

using std::chrono::days;
using std::chrono::microseconds;
using std::chrono::sys_days;

/// The first year turborel keeps.
constexpr std::chrono::year first_year{1};
/// The last.
constexpr std::chrono::year last_year{9999};
/// The day the protocol counts from, 0001-01-01.
constexpr sys_days epoch{first_year / std::chrono::January / 1};
/// The last instant turborel keeps, the last microsecond of 9999-12-31.
constexpr std::chrono::sys_time<microseconds> last_instant{
    sys_days{last_year / std::chrono::December / 31} + days{1} - microseconds{1},
};
/// Microseconds in a day.
constexpr microseconds day = days{1};

/// Fails unless `displacement` is in turborel's range.
void check_displacement(std::chrono::minutes displacement) {
  if (displacement < min_displacement || displacement > max_displacement) {
    detail::throw_data_error(detail::invalid_time_zone_displacement,
                             "a time zone displacement is not from -12:59 to +14:00");
  }
}

/// Fails unless `instant` is in turborel's range.
void check_instant(std::chrono::sys_time<microseconds> instant) {
  if (instant < epoch || instant > last_instant) {
    detail::throw_data_error(detail::datetime_field_overflow, "a datetime is not from 0001-01-01 to 9999-12-31");
  }
}

/// The instant `utc` microseconds after the epoch, failing if it is out of
/// range, before it is added up, so no count overflows.
std::chrono::sys_time<microseconds> instant_of(std::int64_t utc) {
  if (utc < 0 || utc > (last_instant - epoch).count()) {
    detail::throw_data_error(detail::datetime_field_overflow, "a datetime is not from 0001-01-01 to 9999-12-31");
  }
  return epoch + microseconds{utc};
}

/// The displacement `fields` writes, in range.
std::chrono::minutes displacement_of(const datetime_fields& fields) {
  const std::chrono::minutes displacement{fields.displacement};
  check_displacement(displacement);
  return displacement;
}

} // namespace

datetime_fields date::fields() const {
  if (!day.ok() || day.year() < first_year || day.year() > last_year) {
    detail::throw_data_error(detail::datetime_field_overflow, "a date is not a day from 0001-01-01 to 9999-12-31");
  }
  return datetime_fields{.utc = std::chrono::duration_cast<microseconds>(sys_days{day} - epoch).count()};
}

date date::from_fields(const datetime_fields& fields) {
  return date{.day = std::chrono::floor<days>(instant_of(fields.utc))};
}

std::chrono::hh_mm_ss<microseconds> time::local() const {
  if (utc < microseconds{0} || utc >= day) {
    detail::throw_data_error(detail::datetime_field_overflow, "a time is not within a day");
  }
  check_displacement(displacement);
  microseconds shown = (utc + displacement) % day;
  if (shown < microseconds{0}) {
    shown += day;
  }
  return std::chrono::hh_mm_ss{shown};
}

datetime_fields time::fields() const {
  static_cast<void>(local());
  return datetime_fields{
      .zoned = zoned,
      .precision = precision,
      .displacement = static_cast<std::int16_t>(displacement.count()),
      .utc = utc.count(),
  };
}

time time::from_fields(const datetime_fields& fields) {
  const time read{
      .utc = microseconds{fields.utc},
      .displacement = std::chrono::minutes{fields.displacement},
      .zoned = fields.zoned,
      .precision = fields.precision,
  };
  static_cast<void>(read.local());
  return read;
}

std::chrono::local_time<microseconds> timestamp::local() const {
  check_instant(instant);
  check_displacement(displacement);
  return std::chrono::local_time<microseconds>{instant.time_since_epoch() + displacement};
}

datetime_fields timestamp::fields() const {
  static_cast<void>(local());
  return datetime_fields{
      .zoned = zoned,
      .precision = precision,
      .displacement = static_cast<std::int16_t>(displacement.count()),
      .utc = (instant - epoch).count(),
  };
}

timestamp timestamp::from_fields(const datetime_fields& fields) {
  return timestamp{
      .instant = instant_of(fields.utc),
      .displacement = displacement_of(fields),
      .zoned = fields.zoned,
      .precision = fields.precision,
  };
}

} // namespace turborel
