/// @file
/// @brief A signed 128-bit integer, as the protocol writes exact numbers'
/// and intervals' units [PROTOCOL Values and fields]: standard C++ has no
/// such type, and MSVC no extension of one (RULES.md D16).

#ifndef TURBOREL_INT128_HPP
#define TURBOREL_INT128_HPP

#include <turborel/export.hpp>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace turborel {

/// A signed 128-bit integer in two's complement, held as its high and low
/// 64 bits, from -2^127 to 2^127 - 1.
///
/// It carries a value between the protocol and a program, so it holds,
/// orders, prints and parses; it does no arithmetic.
class TURBOREL_CPP_EXPORT int128 {
public:
  /// The bytes of an `i128` on the wire [PROTOCOL Values and fields].
  static constexpr std::size_t wire_bytes = 16;

  /// Zero.
  constexpr int128() noexcept = default;

  /// The value of `value`.
  /// @param value Any 64-bit integer.
  constexpr explicit int128(std::int64_t value) noexcept
      : high_{value < 0 ? -1 : 0}, low_{static_cast<std::uint64_t>(value)} {}

  /// The integer whose two's complement bits are `high` then `low`.
  /// @param high The high 64 bits, the sign's among them.
  /// @param low The low 64 bits.
  /// @return The integer.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the halves are named, in the order they are written.
  [[nodiscard]] static constexpr int128 from_halves(std::int64_t high, std::uint64_t low) noexcept {
    int128 value;
    value.high_ = high;
    value.low_ = low;
    return value;
  }

  /// The least value, -2^127.
  /// @return -2^127.
  [[nodiscard]] static constexpr int128 min() noexcept {
    return from_halves(std::numeric_limits<std::int64_t>::min(), 0);
  }

  /// The greatest value, 2^127 - 1.
  /// @return 2^127 - 1.
  [[nodiscard]] static constexpr int128 max() noexcept {
    return from_halves(std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::uint64_t>::max());
  }

  /// The high 64 bits, the sign's among them.
  /// @return Its high half.
  [[nodiscard]] constexpr std::int64_t high() const noexcept { return high_; }

  /// The low 64 bits.
  /// @return Its low half.
  [[nodiscard]] constexpr std::uint64_t low() const noexcept { return low_; }

  /// Whether it is less than zero.
  /// @return True if it is negative.
  [[nodiscard]] constexpr bool negative() const noexcept { return high_ < 0; }

  /// Its 16 bytes as the protocol writes an `i128`: two's complement,
  /// least significant first [PROTOCOL Values and fields].
  /// @return Its 16 bytes.
  [[nodiscard]] std::array<std::byte, wire_bytes> to_bytes() const noexcept;

  /// The integer of 16 bytes as the protocol writes an `i128`.
  /// @param bytes Two's complement, least significant first.
  /// @return The integer they write.
  [[nodiscard]] static int128 from_bytes(std::span<const std::byte, wire_bytes> bytes) noexcept;

  /// Its decimal digits, with a leading `-` if it is negative.
  /// @return Its text.
  [[nodiscard]] std::string to_string() const;

  /// The integer `text` writes: an optional `+` or `-`, then decimal
  /// digits, and nothing else; none if it writes none, or one out of range.
  /// @param text The text to read.
  /// @return The integer, or none.
  [[nodiscard]] static std::optional<int128> parse(std::string_view text) noexcept;

  /// Whether two are the same integer.
  friend constexpr bool operator==(const int128&, const int128&) noexcept = default;

  /// Their order as integers: the signed high halves first, then the
  /// unsigned low ones.
  /// @param left The one on the left.
  /// @param right The one on the right.
  /// @return How `left` orders against `right`.
  friend constexpr std::strong_ordering operator<=>(const int128& left, const int128& right) noexcept {
    if (left.high_ != right.high_) {
      return left.high_ <=> right.high_;
    }
    return left.low_ <=> right.low_;
  }

private:
  std::int64_t high_{};
  std::uint64_t low_{};
};

} // namespace turborel

#endif // TURBOREL_INT128_HPP
