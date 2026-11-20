/// @file
/// @brief An exact number's text (RULES.md D16), in the form
/// turborel-client's `Decimal` writes, so a value reads the same from
/// every client.

#include <turborel/decimal.hpp>
#include <turborel/int128.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace turborel {

namespace {

/// What separates the units' digits from the scale in a number written
/// with its power of ten.
constexpr std::string_view exponent_mark = "E-";

/// Whether `text` is one or more decimal digits.
bool digits(std::string_view text) noexcept {
  return !text.empty() && text.find_first_not_of("0123456789") == std::string_view::npos;
}

/// The scale `text` writes, as decimal digits; none if it writes none, or
/// one past a `u32`.
std::optional<std::uint32_t> scale_of(std::string_view text) noexcept {
  const std::optional<int128> scale = int128::parse(text);
  if (!digits(text) || !scale || *scale > int128{std::numeric_limits<std::uint32_t>::max()}) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(scale->low());
}

} // namespace

std::string decimal::to_string() const {
  std::string text = units.to_string();
  const std::size_t sign = units.negative() ? 1 : 0;
  if (scale == 0) {
    return text;
  }
  if (scale > max_written_scale) {
    return text + std::string{exponent_mark} + std::to_string(scale);
  }
  const std::size_t places = scale;
  const std::size_t digit_count = text.size() - sign;
  if (digit_count <= places) {
    text.insert(sign, places + 1 - digit_count, '0');
  }
  text.insert(text.size() - places, 1, '.');
  return text;
}

std::optional<decimal> decimal::parse(std::string_view text) {
  const std::string_view body = text.starts_with('-') || text.starts_with('+') ? text.substr(1) : text;
  const std::size_t mark = body.find(exponent_mark);
  if (mark != std::string_view::npos) {
    const std::optional<std::uint32_t> scale = scale_of(body.substr(mark + exponent_mark.size()));
    const std::optional<int128> units = int128::parse(text.substr(0, text.size() - body.size() + mark));
    if (!digits(body.substr(0, mark)) || !scale || !units) {
      return std::nullopt;
    }
    return decimal{.units = *units, .scale = *scale};
  }
  const std::size_t point = body.find('.');
  const std::string_view whole = body.substr(0, point);
  const std::string_view fraction = point == std::string_view::npos ? std::string_view{} : body.substr(point + 1);
  const bool pointed = point != std::string_view::npos;
  if (!digits(whole) || (pointed && !digits(fraction)) || fraction.size() > std::numeric_limits<std::uint32_t>::max()) {
    return std::nullopt;
  }
  std::string joined{text.substr(0, text.size() - body.size())};
  joined.append(whole).append(fraction);
  const std::optional<int128> units = int128::parse(joined);
  if (!units) {
    return std::nullopt;
  }
  return decimal{.units = *units, .scale = static_cast<std::uint32_t>(fraction.size())};
}

} // namespace turborel
