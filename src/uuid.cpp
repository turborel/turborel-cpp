/// @file
/// @brief A UUID's text, written and read as turborel's
/// `value/types/uuid.rs` does (RULES.md D16), so a UUID reads the same from
/// every client.

#include <turborel/uuid.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace turborel {

namespace {

/// The octets of each group of the text, joined by hyphens.
constexpr std::array<std::size_t, 5> groups{4, 2, 2, 2, 6};
/// Hexadecimal digits of the text.
constexpr std::size_t digit_count = 32;
/// Digits after which a hyphen may come.
constexpr std::size_t hyphen_every = 4;
/// Bits of a hexadecimal digit.
constexpr unsigned nibble_bits = 4;
/// The low digit of an octet.
constexpr unsigned nibble_mask = 0x0FU;
/// The digits written, lower case.
constexpr std::string_view hexits = "0123456789abcdef";
/// The value of the digit `a` or `A`.
constexpr int ten = 10;
/// What is trimmed around the text: ASCII white space.
constexpr std::string_view white_space = " \t\n\v\f\r";

/// The value of the hexadecimal digit `c`, either case; none if it is not
/// one.
std::optional<unsigned> hexit(char c) noexcept {
  if (c >= '0' && c <= '9') {
    return static_cast<unsigned>(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return static_cast<unsigned>(c - 'a' + ten);
  }
  if (c >= 'A' && c <= 'F') {
    return static_cast<unsigned>(c - 'A' + ten);
  }
  return std::nullopt;
}

/// `text` without the white space around it, and without the braces
/// around that, if it has both.
std::string_view trimmed(std::string_view text) noexcept {
  const std::size_t first = text.find_first_not_of(white_space);
  if (first == std::string_view::npos) {
    return {};
  }
  text.remove_suffix(text.size() - text.find_last_not_of(white_space) - 1);
  text.remove_prefix(first);
  if (text.size() >= 2 && text.starts_with('{') && text.ends_with('}')) {
    text.remove_prefix(1);
    text.remove_suffix(1);
  }
  return text;
}

} // namespace

std::string uuid::to_string() const {
  std::string text;
  std::size_t at = 0;
  for (const std::size_t group : groups) {
    if (at != 0) {
      text.push_back('-');
    }
    for (std::size_t i = 0; i < group; ++i, ++at) {
      const auto octet = std::to_integer<unsigned>(octets_.at(at));
      text.push_back(hexits.at(octet >> nibble_bits));
      text.push_back(hexits.at(octet & nibble_mask));
    }
  }
  return text;
}

std::optional<uuid> uuid::parse(std::string_view text) {
  std::array<std::byte, octet_count> octets{};
  std::size_t digits = 0;
  bool hyphen_allowed = false;
  for (const char c : trimmed(text)) {
    if (c == '-' && hyphen_allowed) {
      hyphen_allowed = false;
      continue;
    }
    const std::optional<unsigned> value = hexit(c);
    if (!value || digits == digit_count) {
      return std::nullopt;
    }
    std::byte& octet = octets.at(digits / 2);
    octet = static_cast<std::byte>((std::to_integer<unsigned>(octet) << nibble_bits) | *value);
    ++digits;
    hyphen_allowed = digits % hyphen_every == 0 && digits < digit_count;
  }
  if (digits != digit_count) {
    return std::nullopt;
  }
  return uuid{octets};
}

} // namespace turborel
