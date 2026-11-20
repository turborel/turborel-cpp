/// @file
/// @brief The 128-bit integer's bytes and decimal text (RULES.md D16): a
/// magnitude is worked on as four 32-bit limbs, so every step fits in 64
/// bits on every compiler.

#include <turborel/int128.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

namespace turborel {

namespace {

/// Bits in a limb, half of a 64-bit half.
constexpr unsigned limb_bits = 32;
/// The low limb of a 64-bit half.
constexpr std::uint64_t limb_mask = 0xFFFF'FFFFU;
/// Bits in a byte.
constexpr unsigned byte_bits = 8;
/// Bytes in a 64-bit half.
constexpr std::size_t half_bytes = 8;
/// The base of decimal text.
constexpr std::uint64_t radix = 10;

/// A magnitude, most significant limb first.
using limbs = std::array<std::uint64_t, 4>;

/// The magnitude of the unsigned 128 bits `high` and `low`.
limbs limbs_of(std::uint64_t high, std::uint64_t low) noexcept {
  return {high >> limb_bits, high & limb_mask, low >> limb_bits, low & limb_mask};
}

/// Whether every limb of `value` is zero.
bool zero(const limbs& value) noexcept {
  return std::ranges::all_of(value, [](std::uint64_t limb) { return limb == 0; });
}

/// Divides `value` by ten in place, giving the remainder.
std::uint64_t divide_by_radix(limbs& value) noexcept {
  std::uint64_t remainder = 0;
  for (std::uint64_t& limb : value) {
    const std::uint64_t current = (remainder << limb_bits) | limb;
    limb = current / radix;
    remainder = current % radix;
  }
  return remainder;
}

/// Multiplies `value` by ten and adds `digit` in place; false if the
/// result needs more than 128 bits.
bool multiply_add(limbs& value, std::uint64_t digit) noexcept {
  std::uint64_t carry = digit;
  for (std::uint64_t& limb : std::views::reverse(value)) {
    const std::uint64_t current = (limb * radix) + carry;
    limb = current & limb_mask;
    carry = current >> limb_bits;
  }
  return carry == 0;
}

/// 128 bits as two unsigned halves.
struct halves {
  /// The high 64 bits.
  std::uint64_t high;
  /// The low 64 bits.
  std::uint64_t low;
};

/// The two's complement negation of `bits`.
halves negate(halves bits) noexcept {
  const std::uint64_t low = ~bits.low + 1;
  return {.high = ~bits.high + (low == 0 ? 1 : 0), .low = low};
}

/// Writes `half` into `out`, least significant byte first.
void write_half(std::uint64_t half, std::span<std::byte, half_bytes> out) noexcept {
  unsigned shift = 0;
  for (std::byte& byte : out) {
    byte = static_cast<std::byte>(half >> shift);
    shift += byte_bits;
  }
}

/// The 64-bit half `in` writes, least significant byte first.
std::uint64_t read_half(std::span<const std::byte, half_bytes> in) noexcept {
  std::uint64_t half = 0;
  unsigned shift = 0;
  for (const std::byte byte : in) {
    half |= std::to_integer<std::uint64_t>(byte) << shift;
    shift += byte_bits;
  }
  return half;
}

} // namespace

std::array<std::byte, int128::wire_bytes> int128::to_bytes() const noexcept {
  std::array<std::byte, wire_bytes> bytes{};
  write_half(low_, std::span{bytes}.first<half_bytes>());
  write_half(static_cast<std::uint64_t>(high_), std::span{bytes}.last<half_bytes>());
  return bytes;
}

int128 int128::from_bytes(std::span<const std::byte, wire_bytes> bytes) noexcept {
  return from_halves(static_cast<std::int64_t>(read_half(bytes.last<half_bytes>())),
                     read_half(bytes.first<half_bytes>()));
}

std::string int128::to_string() const {
  halves bits{.high = static_cast<std::uint64_t>(high_), .low = low_};
  if (negative()) {
    bits = negate(bits);
  }
  limbs magnitude = limbs_of(bits.high, bits.low);
  const auto next_digit = [&magnitude] { return static_cast<char>('0' + divide_by_radix(magnitude)); };
  std::string digits(1, next_digit());
  while (!zero(magnitude)) {
    digits.push_back(next_digit());
  }
  if (negative()) {
    digits.push_back('-');
  }
  std::ranges::reverse(digits);
  return digits;
}

std::optional<int128> int128::parse(std::string_view text) noexcept {
  const bool minus = text.starts_with('-');
  if (minus || text.starts_with('+')) {
    text.remove_prefix(1);
  }
  if (text.empty()) {
    return std::nullopt;
  }
  limbs magnitude{};
  for (const char c : text) {
    if (c < '0' || c > '9' || !multiply_add(magnitude, static_cast<std::uint64_t>(c - '0'))) {
      return std::nullopt;
    }
  }
  const auto [highest, high_low, low_high, lowest] = magnitude;
  const std::uint64_t high = (highest << limb_bits) | high_low;
  const std::uint64_t low = (low_high << limb_bits) | lowest;
  // The magnitude is at most 2^127 - 1, or 2^127 of a negative number.
  const auto top = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::min());
  if (high > top || (high == top && (low != 0 || !minus))) {
    return std::nullopt;
  }
  if (!minus) {
    return from_halves(static_cast<std::int64_t>(high), low);
  }
  const halves negated = negate({.high = high, .low = low});
  return from_halves(static_cast<std::int64_t>(negated.high), negated.low);
}

} // namespace turborel
