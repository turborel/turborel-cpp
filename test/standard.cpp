/// @file
/// @brief Tests of the standard API (RULES.md D2): results as ranges of
/// the standard library, handles moved as values, parameters of every C++
/// type, and XML values' nodes walked.

#include "support/frames.hpp"

#include <turborel/connection.hpp>
#include <turborel/datetime.hpp>
#include <turborel/decimal.hpp>
#include <turborel/error.hpp>
#include <turborel/interval.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/settings.hpp>
#include <turborel/uuid.hpp>
#include <turborel/value.hpp>
#include <turborel/xml.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals; // NOLINT(google-build-using-namespace): chrono's literals read as the values do.

/// A result of three rows of one character string each: abc, the empty
/// string, and é€.
turborel::result three() {
  return test::native(
      test::frame("description-character"),
      {test::frame("value-character"), test::frame("value-character-empty"), test::frame("value-character-utf8")});
}

/// A result of one row of one XML value, its serialization `text`.
turborel::result xml_of(std::string_view text) {
  return test::native(test::frame("description-xml-content"),
                      {test::writer{}.u32(1).u32(1).u8(26).text(text).frame('B')});
}

/// The SQLSTATE of what `call` throws as the driver's error.
template <typename Call>
std::string sqlstate_of(const Call& call) {
  try {
    call();
  } catch (const turborel::error& e) {
    return std::string{e.sqlstate()};
  }
  return "none";
}

} // namespace

static_assert(std::ranges::random_access_range<turborel::result>);
static_assert(std::ranges::sized_range<turborel::result>);
static_assert(std::random_access_iterator<turborel::result::iterator>);

/// Verifies: [DESIGN] D2 A result is a random-access range of rows, read by
/// the standard library's algorithms and ranges, its iterator moved,
/// compared and subtracted as a pointer is.
TEST_CASE("a result is a random-access range of its rows") {
  const turborel::result rows = three();
  std::vector<std::string> texts;
  for (const turborel::row row : rows) {
    texts.push_back(row.get<std::string>(0));
  }
  CHECK(texts == std::vector<std::string>{"abc", "", "\xC3\xA9\xE2\x82\xAC"});
  CHECK(std::ranges::count_if(rows, [](const turborel::row& row) { return row.get<std::string>(0).empty(); }) == 1);
  auto at = rows.begin();
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): the iterator's [] is the case.
  CHECK(at[2].get<std::string>(0) == texts.at(2));
  CHECK((*at++).get<std::string>(0) == "abc");
  CHECK((*at).get<std::string>(0).empty());
  CHECK((*at--).get<std::string>(0).empty());
  CHECK(at == rows.begin());
  ++at;
  --at;
  at += 2;
  at -= 1;
  CHECK(at - rows.begin() == 1);
  CHECK(rows.end() - rows.begin() == 3);
  CHECK((rows.begin() + 3) == rows.end());
  CHECK((3 + rows.begin()) == rows.end());
  CHECK((rows.end() - 3) == rows.begin());
  CHECK((rows.begin() <=> rows.end()) == std::strong_ordering::less);
  CHECK(std::ranges::distance(rows | std::views::drop(1)) == 2);
}

/// Verifies: [DESIGN] D2 A result, a connection, parameters and settings
/// are moved as values, the one moved from left empty, and freed once;
/// settings moved from name no user.
TEST_CASE("handles are moved as values") {
  turborel::result first = three();
  turborel::result const second = std::move(first);
  CHECK(second.size() == 3);
  turborel::params values = turborel::params::of(1, "a");
  turborel::params taken{std::move(values)};
  CHECK(taken.size() == 2);
  turborel::params again;
  again = std::move(taken);
  CHECK(again.size() == 2);
  auto [conn, stream] = test::open_scripted(test::ready({}), turborel::connect_options{}.ask_none());
  auto [other, other_stream] = test::open_scripted(test::ready({}), turborel::connect_options{}.ask_none());
  conn = std::move(other);
  CHECK_FALSE(conn.lost());
  CHECK_FALSE(other_stream->closed());
  turborel::settings laid = turborel::settings::from_uri("turborel://ann@h/");
  const turborel::settings kept = std::move(laid);
  CHECK(kept.user() == "ann");
  // NOLINTNEXTLINE(bugprone-use-after-move,hicpp-invalid-access-moved): what settings moved from read is the case.
  CHECK(laid.user().empty());
}

/// Verifies: [DESIGN] D2 Parameters are added of each C++ type the driver
/// maps a value of, the null value of std::nullopt, nullptr or an empty
/// std::optional, a value of a result copied; JSON text no JSON value is
/// fails (22032); the list cleared is empty.
TEST_CASE("parameters are added of each C++ type") {
  const turborel::result rows = three();
  turborel::params values;
  values.add(std::nullopt).add(nullptr).add(std::optional<int>{}).add(std::optional<int>{7});
  values.add("text").add(std::string_view{"view"}).add(std::int16_t{-3}).add(2.5).add(true);
  values.add(turborel::decimal{.units = turborel::int128{150}, .scale = 2});
  values.add(turborel::date{.day = std::chrono::year{2026} / 11 / 20});
  values.add(turborel::time{.utc = 13h + 5min, .precision = 0});
  values.add(turborel::time{.utc = 13h, .displacement = 60min, .zoned = true});
  values.add(turborel::timestamp{.instant = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1}, .precision = 6});
  values.add(turborel::timestamp{.instant = {}, .displacement = -30min, .zoned = true});
  values.add(turborel::interval{
      .qualifier =
          {
              .start = turborel::interval_field::year,
              .end = turborel::interval_field::month,
              .leading_precision = 2,
          },
      .units = turborel::int128{14},
  });
  values.add(turborel::decfloat{.coefficient = turborel::int128{15}, .exponent = -1});
  values.add(turborel::uuid{}).add(turborel::json_text{R"({"a":[1,"x"]})"}).add(turborel::xml_text{"<a/>"});
  values.add(rows.at(0).at(0));
  CHECK(values.size() == 21);
  CHECK(sqlstate_of([&values] { values.add(turborel::json_text{"{"}); }) == "22032");
  CHECK(values.size() == 21);
  values.clear();
  CHECK(values.size() == 0);
}

/// Verifies: [CLIENT XML values] An XML value's tree is walked node by node:
/// the document, its elements, their attributes, text, comments and
/// processing instructions, each with its name, namespace, prefix and
/// value; a child or attribute past the last fails as a misuse; a value
/// not XML, or text not well formed, fails.
TEST_CASE("an XML value's nodes are walked") {
  const turborel::xml_tree tree = turborel::xml_tree::parse(R"(<p:a xmlns:p="urn:p" k="v">t<!--c--><?go now?></p:a>)");
  const turborel::xml_node document = tree.root();
  CHECK(document.kind() == turborel::xml_kind::document);
  REQUIRE(document.child_count() == 1);
  const turborel::xml_node element = document.child(0);
  CHECK(element.kind() == turborel::xml_kind::element);
  CHECK(element.name() == "a");
  CHECK(element.prefix() == "p");
  CHECK(element.namespace_uri() == "urn:p");
  REQUIRE(element.attribute_count() >= 1);
  const turborel::xml_node attribute = element.attribute(element.attribute_count() - 1);
  CHECK(attribute.kind() == turborel::xml_kind::attribute);
  CHECK(attribute.value() == "v");
  REQUIRE(element.child_count() == 3);
  CHECK(element.child(0).kind() == turborel::xml_kind::text);
  CHECK(element.child(1).kind() == turborel::xml_kind::comment);
  CHECK(element.child(2).kind() == turborel::xml_kind::processing_instruction);
  CHECK(element.child(2).name() == "go");
  CHECK(sqlstate_of([&element] { static_cast<void>(element.child(3)); }) == "TB910");
  CHECK(sqlstate_of([&element] { static_cast<void>(element.attribute(element.attribute_count())); }) == "TB910");
  CHECK(turborel::xml_tree::parse("").root().child_count() == 0);
  CHECK(turborel::xml_tree::of(xml_of("<a>&lt;</a>").at(0).at(0)).root().child(0).name() == "a");
  CHECK(sqlstate_of([] { static_cast<void>(turborel::xml_tree::of(xml_of("<a>").at(0).at(0))); }) == "2200N");
  CHECK(sqlstate_of([] { static_cast<void>(turborel::xml_tree::of(three().at(0).at(0))); }) == "TB910");
}
