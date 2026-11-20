/// @file
/// @brief An exact number as the protocol carries one, units and a scale
/// [PROTOCOL Values] (RULES.md D16).

#ifndef TURBOREL_DECIMAL_HPP
#define TURBOREL_DECIMAL_HPP

#include <turborel/export.hpp>
#include <turborel/int128.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace turborel {

/// An exact number: `units` divided by ten to the power `scale`, as the
/// protocol writes value tag 2, units `i128` then scale `u32`
/// [PROTOCOL Values]. Every value the protocol can carry is one, with no
/// loss.
///
/// Two are equal when their units and scales are, so 1.0 and 1.00 differ:
/// it keeps a value as it came, and does no arithmetic.
struct TURBOREL_CPP_EXPORT decimal {
  /// The greatest scale written out with zeros; past it, the text gives
  /// the power of ten, as `5E-40`, so a scale of billions never makes
  /// gigabytes of zeros. turborel's greatest exact precision, 18, which
  /// no column's scale exceeds.
  static constexpr std::uint32_t max_written_scale = 18;

  /// The significant digits, as an integer.
  int128 units;
  /// How many of them are after the decimal point.
  std::uint32_t scale{};

  /// Its text, as turborel-client writes an exact number: `-12.50`,
  /// `0.005`, `42`, or past `max_written_scale`, `125E-40`.
  /// @return Its text.
  [[nodiscard]] std::string to_string() const;

  /// The number `text` writes, as to_string writes one: an optional sign,
  /// digits, then either a point and digits or `E-` and the scale; none if
  /// it writes none, or its units are out of range.
  /// @param text The text to read.
  /// @return The number, or none.
  /// @throws std::bad_alloc If its digits cannot be gathered.
  [[nodiscard]] static std::optional<decimal> parse(std::string_view text);

  /// Whether two have the same units and scale.
  friend bool operator==(const decimal&, const decimal&) noexcept = default;
};

} // namespace turborel

#endif // TURBOREL_DECIMAL_HPP
