/// @file
/// @brief Values and types read through turborel-c (RULES.md D16): each
/// value's C form turned into the driver's C++ type.

#include <turborel/value.hpp>

#include "capi.hpp"

#include <turborel/datetime.hpp>
#include <turborel/decimal.hpp>
#include <turborel/error.hpp>
#include <turborel/int128.hpp>
#include <turborel/interval.hpp>
#include <turborel/turborel.h>
#include <turborel/uuid.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace turborel {

namespace {

/// The driver's 128-bit integer of turborel-c's.
int128 int128_of(trl_int128 number) noexcept { return int128::from_halves(number.high, number.low); }

/// Fails as a misuse: a value read as a kind it is not.
[[noreturn]] void wrong_kind(const char* what) {
  detail::raise_misuse(std::string{"the value is not "} + what + " and is not read as one");
}

/// Checks a reader's status: anything but TRL_OK is a value of another kind.
void read_as(trl_status status, const char* what) {
  if (status != TRL_OK) {
    wrong_kind(what);
  }
}

/// The datetime fields of turborel-c's datetime.
datetime_fields fields_of(const trl_datetime& datetime) noexcept {
  return datetime_fields{
      .zoned = datetime.zoned,
      .precision = datetime.precision,
      .displacement = datetime.zone,
      .utc = datetime.utc,
  };
}

} // namespace

turborel::kind data_type::kind() const noexcept { return static_cast<turborel::kind>(trl_type_kind(type_)); }

std::uint64_t data_type::length() const noexcept { return trl_type_length(type_); }

std::uint32_t data_type::precision() const noexcept { return trl_type_precision(type_); }

std::uint32_t data_type::scale() const noexcept { return trl_type_scale(type_); }

std::string data_type::to_string() const {
  return detail::written([this](char* out, std::size_t capacity, std::size_t* length) {
    detail::check(trl_type_to_text(type_, out, capacity, length));
  });
}

turborel::kind value::kind() const noexcept { return static_cast<turborel::kind>(trl_value_kind(value_)); }

bool value::is_null() const noexcept { return trl_value_is_null(value_); }

void value::not_null() const {
  if (is_null()) {
    wrong_kind("other than null, as the type asked for, which is not optional,");
  }
}

std::string_view value::text() const {
  std::size_t length = 0;
  const char* text = trl_value_text(value_, &length);
  if (text == nullptr) {
    wrong_kind("text");
  }
  return {text, length};
}

std::span<const std::byte> value::bytes() const {
  std::size_t length = 0;
  const std::uint8_t* bytes = trl_value_bytes(value_, &length);
  if (bytes == nullptr) {
    wrong_kind("octets");
  }
  return detail::byte_span(bytes, length);
}

decimal value::as_decimal() const {
  trl_decimal number{};
  read_as(trl_value_decimal(value_, &number), "an exact number");
  return decimal{.units = int128_of(number.units), .scale = number.scale};
}

std::int64_t value::as_int64() const {
  const decimal number = as_decimal();
  const int128 least{std::numeric_limits<std::int64_t>::min()};
  const int128 most{std::numeric_limits<std::int64_t>::max()};
  if (number.scale != 0 || number.units < least || number.units > most) {
    throw error{failure{
        .sqlcode = detail::data_sqlcode,
        .sqlstate = std::string{detail::out_of_range_sqlstate},
        .message = "the exact number " + number.to_string() + " is not a 64-bit integer",
    }};
  }
  return static_cast<std::int64_t>(number.units.low());
}

double value::as_double() const {
  double number = 0.0;
  read_as(trl_value_double(value_, &number), "an approximate number");
  return number;
}

bool value::as_bool() const {
  bool truth = false;
  read_as(trl_value_boolean(value_, &truth), "a truth value");
  return truth;
}

date value::as_date() const {
  trl_datetime datetime{};
  if (kind() != turborel::kind::date) {
    wrong_kind("a date");
  }
  read_as(trl_value_datetime(value_, &datetime), "a date");
  return date::from_fields(fields_of(datetime));
}

time value::as_time() const {
  trl_datetime datetime{};
  if (kind() != turborel::kind::time) {
    wrong_kind("a time");
  }
  read_as(trl_value_datetime(value_, &datetime), "a time");
  return time::from_fields(fields_of(datetime));
}

timestamp value::as_timestamp() const {
  trl_datetime datetime{};
  if (kind() != turborel::kind::timestamp) {
    wrong_kind("a timestamp");
  }
  read_as(trl_value_datetime(value_, &datetime), "a timestamp");
  return timestamp::from_fields(fields_of(datetime));
}

interval value::as_interval() const {
  trl_interval read{};
  read_as(trl_value_interval(value_, &read), "an interval");
  const auto field = [](std::uint32_t number) { return interval_field_of(number).value_or(interval_field::second); };
  return interval{
      .qualifier =
          interval_qualifier{
              .start = field(read.qualifier.start),
              .end = field(read.qualifier.end),
              .leading_precision = read.qualifier.leading,
              .fractional_precision = read.qualifier.fraction,
          },
      .units = int128_of(read.units),
  };
}

decfloat value::as_decfloat() const {
  trl_decfloat number{};
  read_as(trl_value_decfloat(value_, &number), "a decimal floating-point number");
  return decfloat{.coefficient = int128_of(number.coefficient), .exponent = number.exponent};
}

uuid value::as_uuid() const {
  std::array<std::uint8_t, TRL_UUID_BYTES> octets{};
  read_as(trl_value_uuid(value_, octets.data()), "a UUID");
  std::array<std::byte, uuid::octet_count> bytes{};
  const std::span<const std::byte> read = detail::byte_span(octets.data(), octets.size());
  std::ranges::copy(read, bytes.begin());
  return uuid{bytes};
}

std::string value::json() const {
  if (kind() != turborel::kind::json) {
    wrong_kind("a JSON value");
  }
  return detail::written([this](char* out, std::size_t capacity, std::size_t* length) {
    const trl_status status = trl_value_json_text(value_, out, capacity, length);
    if (status == TRL_INVALID_TEXT) {
      throw error{failure{
          .sqlcode = detail::data_sqlcode,
          .sqlstate = std::string{detail::invalid_json_sqlstate},
          .message = "the JSON value holds a datetime, or an approximate number not finite, which no JSON text writes",
      }};
    }
    detail::check(status);
  });
}

lob_info value::lob() const {
  trl_lob_info info{};
  read_as(trl_value_lob(value_, &info), "a large object");
  return lob_info{
      .character = info.character,
      .located = info.located,
      .octets = info.octets,
      .characters = info.characters,
  };
}

std::size_t value::size() const noexcept { return trl_value_count(value_); }

value value::at(std::size_t index) const {
  const trl_value* element = trl_value_at(value_, index);
  if (element == nullptr) {
    detail::raise_misuse("the value has no element, field or attribute " + std::to_string(index));
  }
  return value{element};
}

std::string value::to_string() const {
  return detail::written([this](char* out, std::size_t capacity, std::size_t* length) {
    detail::check(trl_value_to_text(value_, out, capacity, length));
  });
}

} // namespace turborel
