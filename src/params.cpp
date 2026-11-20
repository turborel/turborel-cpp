/// @file
/// @brief Parameters sent through turborel-c (RULES.md D16): each C++
/// value turned into the C form turborel-c sends.

#include <turborel/params.hpp>

#include "capi.hpp"

#include <turborel/datetime.hpp>
#include <turborel/decimal.hpp>
#include <turborel/int128.hpp>
#include <turborel/interval.hpp>
#include <turborel/turborel.h>
#include <turborel/uuid.hpp>
#include <turborel/value.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace turborel {

namespace {

/// turborel-c's 128-bit integer of the driver's.
trl_int128 c_int128(const int128& number) noexcept { return trl_int128{.low = number.low(), .high = number.high()}; }

/// turborel-c's datetime of the driver's fields.
trl_datetime c_datetime(const datetime_fields& fields) noexcept {
  return trl_datetime{
      .zoned = fields.zoned,
      .precision = fields.precision,
      .zone = fields.displacement,
      .utc = fields.utc,
  };
}

/// The address of `text` as turborel-c takes it: never null, as an empty
/// view's may be.
const char* address(std::string_view text) noexcept { return text.empty() ? "" : text.data(); }

/// Checks an add: no handle keeps a failure of one.
void added(trl_status status) { detail::check(status); }

} // namespace

void params::freer::operator()(trl_params* list) const noexcept { trl_params_free(list); }

params::params() {
  trl_params* made = nullptr;
  added(trl_params_new(&made));
  owned_.reset(made);
  list_ = made;
}

params& params::add(std::nullopt_t /*null*/) {
  added(trl_params_add_null(get()));
  return *this;
}

params& params::add(std::nullptr_t /*null*/) { return add(std::nullopt); }

params& params::add(std::string_view text) {
  added(trl_params_add_text(get(), address(text), text.size()));
  return *this;
}

params& params::add(std::span<const std::byte> octets) {
  added(trl_params_add_bytes(get(), detail::octets(octets), octets.size()));
  return *this;
}

params& params::add(const decimal& number) {
  added(trl_params_add_decimal(get(), trl_decimal{.units = c_int128(number.units), .scale = number.scale}));
  return *this;
}

params& params::add(double number) {
  added(trl_params_add_double(get(), number));
  return *this;
}

params& params::add(bool truth) {
  added(trl_params_add_boolean(get(), truth));
  return *this;
}

params& params::add(const date& day) {
  added(trl_params_add_datetime(get(), TRL_KIND_DATE, c_datetime(day.fields())));
  return *this;
}

params& params::add(const time& moment) {
  const trl_kind kind = moment.zoned ? TRL_KIND_TIME_ZONED : TRL_KIND_TIME;
  added(trl_params_add_datetime(get(), kind, c_datetime(moment.fields())));
  return *this;
}

params& params::add(const timestamp& instant) {
  const trl_kind kind = instant.zoned ? TRL_KIND_TIMESTAMP_ZONED : TRL_KIND_TIMESTAMP;
  added(trl_params_add_datetime(get(), kind, c_datetime(instant.fields())));
  return *this;
}

params& params::add(const interval& span) {
  const trl_interval c_span{
      .qualifier =
          trl_interval_qualifier{
              .start = static_cast<std::uint32_t>(span.qualifier.start),
              .end = static_cast<std::uint32_t>(span.qualifier.end),
              .leading = span.qualifier.leading_precision,
              .fraction = span.qualifier.fractional_precision,
          },
      .units = c_int128(span.units),
  };
  added(trl_params_add_interval(get(), c_span));
  return *this;
}

params& params::add(const decfloat& number) {
  added(trl_params_add_decfloat(get(), trl_decfloat{
                                           .coefficient = c_int128(number.coefficient),
                                           .exponent = number.exponent,
                                       }));
  return *this;
}

params& params::add(const uuid& id) {
  added(trl_params_add_uuid(get(), detail::octets(id.octets())));
  return *this;
}

params& params::add(const json_text& json) {
  // turborel-c keeps why it refused the text: -301/22032 of text no JSON
  // value is, 22003 of a number no type holds, -900/0A000 past Turborel's
  // limits [PROTOCOL JSON values].
  detail::check(trl_params_add_json(get(), address(json.text), json.text.size()), trl_params_failure(get()));
  return *this;
}

params& params::add(const xml_text& xml) {
  added(trl_params_add_xml(get(), address(xml.text), xml.text.size()));
  return *this;
}

params& params::add(const value& copied) {
  added(trl_params_add_value(get(), copied.c_value()));
  return *this;
}

std::size_t params::size() const noexcept { return trl_params_count(get()); }

void params::clear() noexcept { trl_params_clear(get()); }

} // namespace turborel
