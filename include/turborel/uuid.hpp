/// @file
/// @brief A UUID, as the protocol carries one: value tag 27, its 16 octets
/// (RULES.md D16).

#ifndef TURBOREL_UUID_HPP
#define TURBOREL_UUID_HPP

#include <turborel/export.hpp>

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace turborel {

/// A UUID: 16 octets, as the protocol writes type and value tag 27, the
/// value's body `bytes` of count 16 (turborel's RULES.md D184). Its text and
/// what it reads are turborel's own [CLIENT UUIDs].
class TURBOREL_CPP_EXPORT uuid {
public:
  /// The octets of a UUID.
  static constexpr std::size_t octet_count = 16;

  /// The nil UUID, every octet zero.
  constexpr uuid() noexcept = default;

  /// The UUID of `octets`.
  /// @param octets Its 16 octets, in the order they are written.
  constexpr explicit uuid(std::span<const std::byte, octet_count> octets) noexcept {
    std::ranges::copy(octets, octets_.begin());
  }

  /// Its 16 octets, in the order they are written.
  /// @return Its octets.
  [[nodiscard]] constexpr const std::array<std::byte, octet_count>& octets() const noexcept { return octets_; }

  /// Its text as turborel writes it: 32 lower-case hexadecimal digits in
  /// groups of 8, 4, 4, 4 and 12, joined by hyphens.
  /// @return Its text.
  [[nodiscard]] std::string to_string() const;

  /// The UUID `text` writes, as turborel's CAST reads it: 32 hexadecimal
  /// digits in either case, ASCII white space around them trimmed, braced
  /// or not, a hyphen allowed after each group of four digits but the
  /// last; none if it writes none.
  /// @param text The text to read.
  /// @return The UUID, or none.
  [[nodiscard]] static std::optional<uuid> parse(std::string_view text);

  /// Whether two have the same octets.
  friend constexpr bool operator==(const uuid&, const uuid&) noexcept = default;

  /// Their order, octet by octet.
  /// @param left The one on the left.
  /// @param right The one on the right.
  /// @return How `left` orders against `right`.
  friend constexpr std::strong_ordering operator<=>(const uuid& left, const uuid& right) noexcept {
    return left.octets_ <=> right.octets_;
  }

private:
  std::array<std::byte, octet_count> octets_{};
};

} // namespace turborel

#endif // TURBOREL_UUID_HPP
