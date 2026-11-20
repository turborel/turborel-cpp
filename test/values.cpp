/// @file
/// @brief Tests of values read through the driver, every value tag of the
/// protocol read from the sample frames by turborel-c's codec and given as
/// the driver's C++ types (RULES.md D1, D16).

#include "support/frames.hpp"

#include <turborel/datetime.hpp>
#include <turborel/decimal.hpp>
#include <turborel/error.hpp>
#include <turborel/int128.hpp>
#include <turborel/interval.hpp>
#include <turborel/result.hpp>
#include <turborel/uuid.hpp>
#include <turborel/value.hpp>

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace {

using namespace std::chrono_literals; // NOLINT(google-build-using-namespace): chrono's literals read as the values do.

/// The one value of the sample `name`'s one row.
turborel::result one(const std::string& name) {
  return test::native(test::frame("description-character"), {test::frame(name)});
}

/// The SQLSTATE of what `read` throws, as a misuse of the API; empty if
/// it throws none.
template <typename Read>
std::string misuse_of(const Read& read) {
  try {
    read();
  } catch (const turborel::error& e) {
    return std::string{e.sqlstate()};
  }
  return {};
}

} // namespace

/// Verifies: [PROTOCOL Values] Tags 0, 1, 2, 3 and 6 read as the null value,
/// text in UTF-8, exact numbers at the limit of their units with no loss,
/// a double's bits with -0.0 kept, and octets.
TEST_CASE("the scalar values read as their C++ types") {
  CHECK(one("value-null").at(0).at(0).is_null());
  CHECK(one("value-null").at(0).get<std::optional<std::string_view>>(0) == std::nullopt);
  CHECK(one("value-character").at(0).get<std::string>(0) == "abc");
  CHECK(one("value-character-empty").at(0).get<std::string_view>(0).empty());
  CHECK(one("value-character-utf8").at(0).get<std::string>(0) == "\xC3\xA9\xE2\x82\xAC");
  CHECK(one("value-exact").at(0).get<turborel::decimal>(0).to_string() == "123.45");
  CHECK(one("value-exact-negative").at(0).get<std::int64_t>(0) == -1);
  CHECK(one("value-exact-largest").at(0).get<turborel::decimal>(0).units == turborel::int128::max());
  CHECK(std::bit_cast<std::uint64_t>(one("value-approximate").at(0).get<double>(0)) ==
        std::bit_cast<std::uint64_t>(1.5));
  const auto zero = one("value-approximate-negative-zero").at(0).get<double>(0);
  CHECK(std::bit_cast<std::uint64_t>(zero) == std::bit_cast<std::uint64_t>(-0.0));
  CHECK(one("value-binary").at(0).get<std::span<const std::byte>>(0).size() == 4);
  CHECK(one("value-binary-empty").at(0).get<std::span<const std::byte>>(0).empty());
  CHECK(one("value-true").at(0).get<bool>(0));
  CHECK_FALSE(one("value-false").at(0).get<bool>(0));
}

/// Verifies: [PROTOCOL Values] Tags 8, 9 and 11 read as dates, times and
/// timestamps, zoned or not, their microseconds from 0001-01-01 UTC, the
/// earliest instant among them.
TEST_CASE("datetimes read as std::chrono types") {
  CHECK(one("value-date").at(0).get<turborel::date>(0).day == std::chrono::year{2024} / 2 / 29);
  const auto noon = one("value-time").at(0).get<turborel::time>(0);
  CHECK_FALSE(noon.zoned);
  CHECK(noon.local().hours() == 12h);
  const auto zoned = one("value-time-zoned").at(0).get<turborel::time>(0);
  CHECK(zoned.zoned);
  CHECK(zoned.displacement == 330min);
  CHECK(zoned.local().minutes() == 34min);
  const auto at = one("value-timestamp").at(0).get<turborel::timestamp>(0);
  CHECK(at.instant == std::chrono::sys_days{std::chrono::year{2024} / 2 / 29} + 12h + 34min + 56s + 123456us);
  CHECK(at.precision == 6);
  const auto west = one("value-timestamp-zoned").at(0).get<turborel::timestamp>(0);
  CHECK(west.displacement == -480min);
  CHECK(west.local() == std::chrono::local_days{std::chrono::year{2024} / 2 / 29} + 12h + 34min + 56s);
  CHECK(one("value-timestamp-earliest").at(0).get<turborel::timestamp>(0).instant ==
        std::chrono::sys_days{std::chrono::year{1} / 1 / 1});
}

/// Verifies: [PROTOCOL Values] Tags 13, 15 and 27 read as intervals of
/// months and of microseconds with their qualifiers, DECFLOAT's
/// coefficient and exponent, and a UUID's 16 octets.
TEST_CASE("intervals, decfloats and uuids read as the driver's types") {
  const auto months = one("value-interval-year-month").at(0).get<turborel::interval>(0);
  CHECK(months.qualifier.start == turborel::interval_field::year);
  CHECK(months.months() == std::chrono::months{-14});
  const auto micros = one("value-interval-day-second").at(0).get<turborel::interval>(0);
  CHECK(micros.qualifier.end == turborel::interval_field::second);
  CHECK(micros.microseconds() == std::chrono::microseconds{93'784'500'000});
  const auto number = one("value-decfloat").at(0).get<turborel::decfloat>(0);
  CHECK(number == turborel::decfloat{.coefficient = turborel::int128{-15}, .exponent = -1});
  CHECK(one("value-uuid").at(0).get<turborel::uuid>(0).to_string() == "00112233-4455-6677-8899-aabbccddeeff");
}

/// Verifies: [PROTOCOL Values] Tags 16, 17, 22 and 23 read element by
/// element, nested, a null among them; tag 24's octets; tag 4's text and
/// octets kept as they came.
TEST_CASE("composite values read element by element") {
  const turborel::result array = one("value-array");
  CHECK(array.at(0).at(0).kind() == turborel::kind::array);
  CHECK(array.at(0).at(0).size() == 2);
  CHECK(array.at(0).at(0).at(0).get<std::int64_t>() == 1);
  CHECK(array.at(0).at(0).at(1).is_null());
  CHECK(one("value-array-empty").at(0).at(0).size() == 0);
  const turborel::result row = one("value-row");
  CHECK(row.at(0).at(0).at(0).get<std::string>() == "x");
  CHECK(row.at(0).at(0).at(1).get<bool>());
  CHECK(one("value-multiset").at(0).at(0).kind() == turborel::kind::multiset);
  CHECK(one("value-structured").at(0).at(0).at(0).get<std::int64_t>() == 3);
  CHECK(one("value-reference").at(0).at(0).kind() == turborel::kind::reference);
  CHECK(one("value-reference").at(0).at(0).to_string().size() > 0);
  const turborel::result plugin = one("value-plugin");
  CHECK(plugin.at(0).at(0).text() == "POINT (1 2)");
  CHECK_FALSE(plugin.at(0).at(0).bytes().empty());
}

/// Verifies: [PROTOCOL Values] Tag 25 read from D89's tree as JSON text,
/// a decimal keeping its scale [PROTOCOL JSON values]; tag 26 as its
/// serialization; tags 20 and 21 as large objects whole and by locator.
TEST_CASE("json, xml and large objects read as the protocol carries them") {
  CHECK(one("value-json").at(0).at(0).json() == R"({"a":[1,"x"]})");
  CHECK(one("value-json-scalars").at(0).at(0).json() == R"([null,true,false,-2,1.50,"",{}])");
  CHECK(one("value-xml").at(0).at(0).text() == "<a>&lt;</a>");
  const turborel::lob_info whole = one("value-lob-character").at(0).at(0).lob();
  CHECK(whole.character);
  CHECK_FALSE(whole.located);
  CHECK(one("value-lob-character").at(0).at(0).text() == "a\xC3\xA9");
  const turborel::lob_info located = one("value-lob-locator").at(0).at(0).lob();
  CHECK(located == turborel::lob_info{.character = true, .located = true, .octets = 300000, .characters = 200000});
  CHECK(one("value-lob-binary").at(0).at(0).bytes().size() == 2);
}

/// Verifies: [DESIGN] D10 A value read as a kind it is not, or past its
/// elements, fails as a misuse, SQLSTATE TB910; an exact number with a
/// fraction read as an integer fails with 22003.
TEST_CASE("a value read as another kind fails as a misuse") {
  const turborel::result text = one("value-character");
  const turborel::value abc = text.at(0).at(0);
  for (const auto& read : {
           +[](const turborel::value& v) { static_cast<void>(v.as_decimal()); },
           +[](const turborel::value& v) { static_cast<void>(v.as_double()); },
           +[](const turborel::value& v) { static_cast<void>(v.as_bool()); },
           +[](const turborel::value& v) { static_cast<void>(v.as_date()); },
           +[](const turborel::value& v) { static_cast<void>(v.as_time()); },
           +[](const turborel::value& v) { static_cast<void>(v.as_timestamp()); },
           +[](const turborel::value& v) { static_cast<void>(v.as_interval()); },
           +[](const turborel::value& v) { static_cast<void>(v.as_decfloat()); },
           +[](const turborel::value& v) { static_cast<void>(v.as_uuid()); },
           +[](const turborel::value& v) { static_cast<void>(v.json()); },
           +[](const turborel::value& v) { static_cast<void>(v.lob()); },
           +[](const turborel::value& v) { static_cast<void>(v.bytes()); },
           +[](const turborel::value& v) { static_cast<void>(v.at(0)); },
       }) {
    CHECK(misuse_of([&] { read(abc); }) == "TB910");
  }
  CHECK(misuse_of([] { static_cast<void>(one("value-binary").at(0).at(0).text()); }) == "TB910");
  CHECK(misuse_of([] { static_cast<void>(one("value-null").at(0).get<std::string>(0)); }) == "TB910");
  CHECK(misuse_of([] { static_cast<void>(one("value-exact").at(0).get<std::int64_t>(0)); }) == "22003");
  CHECK(misuse_of([] { static_cast<void>(one("value-exact-largest").at(0).get<std::int64_t>(0)); }) == "22003");
  CHECK(misuse_of([&] { static_cast<void>(text.at(1)); }) == "TB910");
  CHECK(misuse_of([&] { static_cast<void>(text.at(0).at(1)); }) == "TB910");
  CHECK(misuse_of([] { static_cast<void>(turborel::value{nullptr}.to_string()); }) == "TB910");
}

/// Verifies: [PROTOCOL JSON values] A JSON value that holds a datetime, or
/// an approximate number that is not finite, has no JSON text: reading it
/// as text fails (SQLCODE -301, SQLSTATE 22032), its stored form read as
/// it is.
TEST_CASE("a JSON value no JSON text writes fails as text") {
  const test::bytes nan_document = test::writer{}.u8(1).u8(7).u64(0x7FF8000000000000ULL).body();
  const test::bytes date_document = test::writer{}.u8(1).u8(9).u8(0).u64(0).u8(0).u8(0).u8(0).u8(0).body();
  for (const test::bytes& document : {nan_document, date_document}) {
    const turborel::result read =
        test::native(test::frame("description-json"), {test::writer{}.u32(1).u32(1).u8(25).raw(document).frame('B')});
    CHECK(misuse_of([&read] { static_cast<void>(read.at(0).at(0).json()); }) == "22032");
    CHECK(read.at(0).at(0).bytes().size() == document.size());
  }
}
