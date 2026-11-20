/// @file
/// @brief Tests of turborel::int128, the protocol's `i128` (RULES.md D16).

#include <turborel/int128.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace {

/// 128 bits as their two halves.
struct halves {
  /// The low 64 bits.
  std::uint64_t low;
  /// The high 64 bits.
  std::uint64_t high;
};

/// The 16 bytes `bits` write, least significant first.
std::array<std::byte, turborel::int128::wire_bytes> bytes_of(halves bits) {
  std::array<std::byte, turborel::int128::wire_bytes> bytes{};
  for (std::size_t i = 0; i < 8; ++i) {
    bytes.at(i) = static_cast<std::byte>(bits.low >> (i * 8));
    bytes.at(i + 8) = static_cast<std::byte>(bits.high >> (i * 8));
  }
  return bytes;
}

} // namespace

/// Verifies: [PROTOCOL Values and fields] An `i128` is 16 bytes of two's
/// complement, least significant first, read back as the same integer at
/// both ends of its range and across zero.
TEST_CASE("an int128 is written and read as the protocol's i128") {
  CHECK(turborel::int128{1}.to_bytes() == bytes_of({.low = 1, .high = 0}));
  CHECK(turborel::int128{-1}.to_bytes() == bytes_of({.low = ~0ULL, .high = ~0ULL}));
  CHECK(turborel::int128::min().to_bytes() == bytes_of({.low = 0, .high = 0x8000'0000'0000'0000ULL}));
  CHECK(turborel::int128::max().to_bytes() == bytes_of({.low = ~0ULL, .high = 0x7FFF'FFFF'FFFF'FFFFULL}));
  for (const turborel::int128 value : {
           turborel::int128::min(),
           turborel::int128{-1},
           turborel::int128{},
           turborel::int128{1},
           turborel::int128::max(),
           turborel::int128::from_halves(-2, 0x0123'4567'89AB'CDEFULL),
       }) {
    CHECK(turborel::int128::from_bytes(value.to_bytes()) == value);
  }
}

/// Verifies: [DESIGN] D16 An int128's text is its decimal digits, and every
/// integer of the range, its least and greatest too, reads back from it;
/// text out of range or not an integer reads as none.
TEST_CASE("an int128 is written and read as decimal text across its range") {
  const auto* const min_text = "-170141183460469231731687303715884105728";
  const auto* const max_text = "170141183460469231731687303715884105727";
  CHECK(turborel::int128::min().to_string() == min_text);
  CHECK(turborel::int128::max().to_string() == max_text);
  CHECK(turborel::int128{}.to_string() == "0");
  CHECK(turborel::int128{-42}.to_string() == "-42");
  CHECK(turborel::int128::parse(min_text) == turborel::int128::min());
  CHECK(turborel::int128::parse(max_text) == turborel::int128::max());
  CHECK(turborel::int128::parse("+7") == turborel::int128{7});
  CHECK(turborel::int128::parse("-0") == turborel::int128{});
  for (const auto* bad : {
           "",
           "-",
           "+",
           "1a",
           " 1",
           "170141183460469231731687303715884105728",
           "-170141183460469231731687303715884105729",
           "999999999999999999999999999999999999999999",
       }) {
    CHECK(turborel::int128::parse(bad) == std::nullopt);
  }
}

/// Verifies: [DESIGN] D16 Int128s order as the integers they are.
TEST_CASE("int128s order as integers") {
  CHECK(turborel::int128::min() < turborel::int128{-1});
  CHECK(turborel::int128{-1} < turborel::int128{});
  CHECK(turborel::int128{} < turborel::int128{1});
  CHECK(turborel::int128{1} < turborel::int128::from_halves(1, 0));
  CHECK(turborel::int128::from_halves(1, 0) < turborel::int128::max());
  CHECK((turborel::int128{7} <=> turborel::int128{7}) == std::strong_ordering::equal);
  CHECK((turborel::int128{8} <=> turborel::int128{7}) == std::strong_ordering::greater);
  CHECK((turborel::int128::max() <=> turborel::int128::min()) == std::strong_ordering::greater);
  CHECK(turborel::int128{-1}.negative());
  CHECK(turborel::int128{-1}.high() == -1);
  CHECK(turborel::int128{-1}.low() == ~0ULL);
}
