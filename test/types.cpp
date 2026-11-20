/// @file
/// @brief Tests of columns' types read through the driver: every data type
/// tag of the sample frames' Descriptions, and the declared types the
/// capability `declared-types` adds (RULES.md D1).

#include "support/frames.hpp"

#include <turborel/result.hpp>
#include <turborel/value.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

/// The one column of the sample Description `name`.
turborel::column column_of(const turborel::result& described) { return described.columns().at(0); }

/// The result of the sample Description `name`, with no row.
turborel::result described(const std::string& name) { return test::native(test::frame(name), {}); }

/// What a type is: its kind and the text SQL writes it as.
struct shown {
  turborel::kind kind;
  std::string written;
};

/// The kind and text of the sample `name`'s one column's type.
shown shown_of(const std::string& name) {
  const turborel::result read = described(name);
  const turborel::column column = column_of(read);
  return shown{.kind = column.type.kind(), .written = column.type.to_string()};
}

} // namespace

/// Verifies: [PROTOCOL Data types] Tags 1 to 15 are read with what each
/// writes after it: lengths and units, precisions and scales, a plug-in's
/// name, an interval's qualifier; a column's name and type as SQL writes
/// it; a plug-in's type the client does not have read by its name; a
/// column's named and not-null flags [PROTOCOL Composite fields].
TEST_CASE("the scalar types are read with their parameters") {
  const turborel::result character = described("description-character");
  const turborel::column c = column_of(character);
  CHECK(c.name == "C");
  CHECK(c.type.kind() == turborel::kind::character);
  CHECK(c.type.length() == 4);
  CHECK(c.type_written == "CHARACTER(4)");
  CHECK_FALSE(c.declared_name.has_value());
  CHECK(c.not_null == false);
  const std::vector<turborel::column> two = described("description").columns();
  REQUIRE(two.size() == 2);
  CHECK(two.at(0).name == "K");
  CHECK(two.at(0).not_null == true);
  CHECK(two.at(1).name == std::nullopt);
  CHECK(two.at(1).not_null == false);
  const turborel::result exact = described("description-exact");
  CHECK(column_of(exact).type.precision() == 9);
  CHECK(column_of(exact).type.scale() == 2);
  CHECK(shown_of("description-exact").written == "NUMERIC(9,2)");
  CHECK(shown_of("description-approximate").kind == turborel::kind::approximate);
  CHECK(shown_of("description-plugin-type").kind == turborel::kind::plugin);
  // A plug-in's type the client does not have is written as turborel's
  // own client writes it, UNREGISTERED (value/types/unregistered.rs).
  CHECK(shown_of("description-plugin-type").written == "UNREGISTERED");
  CHECK(shown_of("description-varying").kind == turborel::kind::character_varying);
  CHECK(shown_of("description-binary").kind == turborel::kind::binary);
  CHECK(shown_of("description-binary-varying").kind == turborel::kind::binary_varying);
  CHECK(shown_of("description-date").kind == turborel::kind::date);
  CHECK(shown_of("description-time").kind == turborel::kind::time);
  CHECK(shown_of("description-time-zoned").kind == turborel::kind::time_zoned);
  CHECK(shown_of("description-timestamp").kind == turborel::kind::timestamp);
  CHECK(shown_of("description-timestamp-zoned").kind == turborel::kind::timestamp_zoned);
  CHECK(shown_of("description-interval-year-month").written == "INTERVAL YEAR(4) TO MONTH");
  CHECK(shown_of("description-interval-day-second").written == "INTERVAL DAY(2) TO SECOND(6)");
  CHECK(shown_of("description-boolean").kind == turborel::kind::boolean);
  CHECK(shown_of("description-decfloat").kind == turborel::kind::decfloat);
}

/// Verifies: [PROTOCOL Data types] Tags 16 to 27 are read with what each
/// writes after it: an array's cardinality and element type, a row's
/// fields, large objects, multisets, structured and reference types, JSON,
/// XML's modifiers and UUID; a distinct type as its source type.
TEST_CASE("the composite types are read with their parameters") {
  CHECK(shown_of("description-array").kind == turborel::kind::array);
  CHECK(shown_of("description-row").kind == turborel::kind::row);
  CHECK(shown_of("description-clob").kind == turborel::kind::clob);
  CHECK(shown_of("description-blob").kind == turborel::kind::blob);
  CHECK(shown_of("description-multiset").kind == turborel::kind::multiset);
  CHECK(shown_of("description-structured").kind == turborel::kind::structured);
  CHECK(shown_of("description-reference").kind == turborel::kind::reference);
  CHECK(shown_of("description-reference-scoped").kind == turborel::kind::reference);
  CHECK(shown_of("description-json").kind == turborel::kind::json);
  CHECK(shown_of("description-xml-document").kind == turborel::kind::xml);
  CHECK(shown_of("description-xml-schema").kind == turborel::kind::xml);
  CHECK(shown_of("description-uuid").kind == turborel::kind::uuid);
  CHECK(shown_of("description-distinct").kind == turborel::kind::character_varying);
  for (const std::string name : {
           "description-array",
           "description-row",
           "description-multiset",
           "description-structured",
           "description-reference-scoped",
           "description-xml-content",
           "description-xml-sequence",
       }) {
    CHECK_FALSE(shown_of(name).written.empty());
  }
}

/// Verifies: [PROTOCOL Declared types] Tail 19 gives each column's declared
/// type, its DATA_TYPE name and as SQL writes it, so INTEGER is told from
/// NUMERIC(10,0); without it the type written is its tag's.
TEST_CASE("declared types are read where the server told them") {
  const turborel::result declared = described("description-declared");
  const std::vector<turborel::column> columns = declared.columns();
  REQUIRE(columns.size() == 7);
  CHECK(columns.at(0).declared_name == "INTEGER");
  CHECK(columns.at(0).type_written == "INTEGER");
  CHECK(columns.at(1).type_written == "SMALLINT");
  CHECK(columns.at(3).declared_name == "DECIMAL");
  CHECK(columns.at(3).type_written == "DECIMAL(9,2)");
  CHECK(columns.at(6).declared_name == "USER-DEFINED");
  CHECK(columns.at(6).type_written == "S.MONEY");
  const turborel::result array = described("description-declared-array");
  CHECK(column_of(array).declared_name == "ARRAY");
  CHECK(column_of(array).type_written == "INTEGER ARRAY[3]");
  const turborel::result plain = described("description");
  CHECK(column_of(plain).type_written == "NUMERIC(10,0)");
  CHECK_FALSE(plain.columns().at(1).name.has_value());
}

/// Verifies: [PROTOCOL Composite fields] A Description of no columns, and
/// rows of no values, are read as such.
TEST_CASE("empty descriptions and rows are read") {
  const turborel::result none = test::native(test::frame("description-empty"), {test::frame("rows-empty-row")});
  CHECK(none.columns().empty());
  CHECK(none.size() == 1);
  CHECK(none.at(0).size() == 0);
  CHECK(none.has_rows());
  const turborel::result two =
      test::native(test::frame("description"), {test::frame("rows"), test::frame("rows-none")});
  CHECK(two.size() == 2);
  CHECK(two.at(1).at(0).is_null());
  CHECK(two.at(1).get<std::string>(1).empty());
}
