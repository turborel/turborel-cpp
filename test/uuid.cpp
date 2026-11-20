/// @file
/// @brief Tests of turborel::uuid, the protocol's UUID (RULES.md D16).

#include <turborel/uuid.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <compare>
#include <cstddef>
#include <optional>

namespace {

/// The text of the client library's own example.
constexpr auto example = "919108f7-52d1-4320-9bac-f847db4148a8";

} // namespace

/// Verifies: [CLIENT UUIDs] A UUID is its 16 octets, in the order its text
/// writes them, and its text is turborel's: lower case, hyphens after the
/// groups of 8, 4, 4 and 4 digits.
TEST_CASE("a uuid is its octets, written as turborel writes them") {
  const std::optional<turborel::uuid> id = turborel::uuid::parse(example);
  REQUIRE(id.has_value());
  CHECK(id.value_or(turborel::uuid{}).to_string() == example);
  CHECK(id.value_or(turborel::uuid{}).octets().front() == std::byte{0x91});
  CHECK(id.value_or(turborel::uuid{}).octets().back() == std::byte{0xa8});
  CHECK(turborel::uuid{}.to_string() == "00000000-0000-0000-0000-000000000000");
  const std::array<std::byte, turborel::uuid::octet_count> octets{std::byte{1}};
  CHECK(turborel::uuid{octets}.octets() == octets);
  CHECK(turborel::uuid{} < turborel::uuid{octets});
  CHECK((turborel::uuid{octets} <=> turborel::uuid{octets}) == std::strong_ordering::equal);
}

/// Verifies: [CLIENT UUIDs] A UUID is read as turborel's CAST reads one: in
/// either case, trimmed, braced or not, with hyphens or without them, or
/// after any group of four digits; anything else reads as none.
TEST_CASE("a uuid is read as turborel reads one") {
  const std::optional<turborel::uuid> id = turborel::uuid::parse(example);
  for (const auto* same : {
           "919108F7-52D1-4320-9BAC-F847DB4148A8",
           " \t{919108f7-52d1-4320-9bac-f847db4148a8}\n",
           "919108f752d143209bacf847db4148a8",
           "9191-08f7-52d1-4320-9bac-f847-db41-48a8",
       }) {
    CHECK(turborel::uuid::parse(same) == id);
  }
  for (const auto* bad : {
           "",
           "   ",
           "{}",
           "919108f7-52d1-4320-9bac-f847db4148a",
           "919108f7-52d1-4320-9bac-f847db4148a80",
           "919108f7-52d1-4320-9bac-f847db4148a8-",
           "91910-8f7-52d1-4320-9bac-f847db4148a8",
           "919108f7--52d1-4320-9bac-f847db4148a8",
           "{919108f7-52d1-4320-9bac-f847db4148a8",
           "g19108f7-52d1-4320-9bac-f847db4148a8",
       }) {
    CHECK(turborel::uuid::parse(bad) == std::nullopt);
  }
}
