/// @file
/// @brief An interval's field numbers and its std::chrono durations
/// (RULES.md D16).

#include <turborel/int128.hpp>
#include <turborel/interval.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>

namespace turborel {

namespace {

/// `units`, if it fits a 64-bit integer whose duration fits `Rep`.
template <typename Rep>
std::optional<Rep> narrowed(const int128& units) noexcept {
  const bool fits =
      units >= int128{std::numeric_limits<Rep>::min()} && units <= int128{std::numeric_limits<Rep>::max()};
  if (!fits) {
    return std::nullopt;
  }
  // In range, the low half's bits are the value's, modulo 2^64 as C++20 converts.
  return static_cast<Rep>(units.low());
}

} // namespace

std::optional<interval_field> interval_field_of(std::uint32_t number) noexcept {
  if (number > static_cast<std::uint32_t>(interval_field::second)) {
    return std::nullopt;
  }
  return static_cast<interval_field>(number);
}

std::optional<std::chrono::months> interval::months() const noexcept {
  const std::optional<std::chrono::months::rep> count = narrowed<std::chrono::months::rep>(units);
  if (!qualifier.year_month() || !count) {
    return std::nullopt;
  }
  return std::chrono::months{*count};
}

std::optional<std::chrono::microseconds> interval::microseconds() const noexcept {
  const std::optional<std::chrono::microseconds::rep> count = narrowed<std::chrono::microseconds::rep>(units);
  if (qualifier.year_month() || !count) {
    return std::nullopt;
  }
  return std::chrono::microseconds{*count};
}

} // namespace turborel
