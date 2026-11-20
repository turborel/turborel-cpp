/// @file
/// @brief Live tests of import [CLIENT Importing]: delimited text, or a
/// source of the program's own, read a record at a time and inserted a
/// batch to each transaction; bad rows refused or skipped; what stopped an
/// import, where, and what of it was kept (R-TEST-8).

#include "support/server.hpp"

#include <turborel/connection.hpp>
#include <turborel/error.hpp>
#include <turborel/import.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/value.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <ios>
#include <istream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>

namespace {

/// Runs `sql`, its result dropped.
void run(turborel::connection& conn, const std::string& sql) { static_cast<void>(conn.query(sql)); }

/// The count of rows of `table`.
std::int64_t count_of(turborel::connection& conn, const std::string& table) {
  return conn.query("SELECT COUNT(*) FROM " + table).at(0).get<std::int64_t>(0);
}

/// Imports `text`, written as `how` says, into the table `options` names.
turborel::imported import_text(turborel::connection& conn, const std::string& text, const turborel::dialect& how,
                               const turborel::import_options& options) {
  std::istringstream in{text};
  turborel::delimited_text source{in, how};
  return turborel::import_rows(conn, source, options);
}

/// What stopped an import of `text`, thrown as its error.
std::optional<turborel::import_error> stopped_by(turborel::connection& conn, const std::string& text,
                                                 const turborel::dialect& how,
                                                 const turborel::import_options& options) {
  try {
    static_cast<void>(import_text(conn, text, how, options));
  } catch (const turborel::import_error& e) {
    return e;
  }
  return std::nullopt;
}

/// A source of the program's own: rows of an ID and a name, then failing or
/// throwing at the row it is told.
class people final : public turborel::import_source {
public:
  /// Rows up to `count`, failing at `fail_at` (0 for none) with a
  /// std::exception, or with something else if `odd`.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the rows, then the row that fails, as the test reads.
  people(std::uint64_t count, std::uint64_t fail_at, bool odd) : count_{count}, fail_at_{fail_at}, odd_{odd} {}

  std::optional<std::vector<std::string>> headings() override { return std::vector<std::string>{"ID", "NAME"}; }

  bool record(turborel::params& values, turborel::record_position& at) override {
    ++next_;
    at = turborel::record_position{.record = next_, .line = next_ + 1};
    if (next_ == fail_at_) {
      if (odd_) {
        throw 7; // NOLINT(bugprone-std-exception-baseclass,hicpp-exception-baseclass): what a program throws that is no std::exception.
      }
      throw std::runtime_error{"the source broke"};
    }
    if (next_ > count_) {
      return false;
    }
    values.add(static_cast<std::int64_t>(next_));
    values.add("p" + std::to_string(next_));
    return true;
  }

private:
  std::uint64_t count_;
  std::uint64_t fail_at_;
  bool odd_;
  std::uint64_t next_{};
};

} // namespace

/// Verifies: [CLIENT Importing] CSV with a header naming the columns is
/// imported by copy, the server granting it; a string too long for its
/// column refused, not cut, its record and line said, the batch before it
/// kept; spaces beyond the length cut [CLIENT Copying and exporting].
TEST_CASE("CSV is imported by copy") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  run(conn, "CREATE TABLE PEOPLE (ID INTEGER, NAME VARCHAR(5))");
  run(conn, "COMMIT");
  turborel::dialect csv = turborel::dialect::csv();
  csv.header = true;
  turborel::import_options how{.table = "people"};
  CHECK(import_text(conn, "id,name\r\n1,Ada\r\n2,\"Bri,n\"\r\n3,Dan   \n", csv, how).rows == 3);
  CHECK(count_of(conn, "PEOPLE") == 3);
  run(conn, "COMMIT");
  how.batch = 2;
  const std::optional<turborel::import_error> stopped =
      stopped_by(conn, "id,name\n4,Eve\n5,Fay\n6,Charlotte\n", csv, how);
  REQUIRE(stopped.has_value());
  CHECK(stopped->committed() == 2);
  CHECK(stopped->rolled_back() == 0);
  CHECK(stopped->cause().sql);
  CHECK(stopped->sqlstate() == "22001");
  CHECK(stopped->at().value_or(turborel::record_position{}).record == 3);
  CHECK(stopped->at().value_or(turborel::record_position{}).line == 4);
  CHECK(std::string_view{stopped->what()}.find("does not fit CHARACTER VARYING(5)") != std::string_view::npos);
  CHECK(count_of(conn, "PEOPLE") == 5);
}

/// Verifies: [CLIENT Importing] A row that fails is skipped and given to the
/// program, if asked, by INSERT; without skipping, a failing row stops the
/// import, the batch under way rolled back, the batches before kept; a
/// record of too many fields refused.
TEST_CASE("bad rows are skipped, or stop the import") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  run(conn, "CREATE TABLE KEYED (ID INTEGER PRIMARY KEY, NAME VARCHAR(5))");
  run(conn, "COMMIT");
  const turborel::dialect csv = turborel::dialect::csv();
  std::vector<std::uint64_t> skipped;
  turborel::import_options how{
      .table = "KEYED",
      .skip = true,
      .rejected = [&skipped](const turborel::rejected_row& row) { skipped.push_back(row.at.record); },
  };
  const turborel::imported kept = import_text(conn, "1,a\n1,b\n2,c,d\n3,e\n", csv, how);
  CHECK(kept.rows == 2);
  CHECK(kept.rejected == 2);
  CHECK(skipped == std::vector<std::uint64_t>{2, 3});
  how.skip = false;
  how.batch = 2;
  const std::optional<turborel::import_error> stopped = stopped_by(conn, "10,a\n11,b\n12,c\n10,d\n", csv, how);
  REQUIRE(stopped.has_value());
  CHECK(stopped->committed() == 2);
  CHECK(stopped->rolled_back() == 1);
  CHECK(stopped->at().value_or(turborel::record_position{}).record == 4);
  CHECK_FALSE(conn.in_transaction());
  CHECK(count_of(conn, "KEYED") == 4);
  run(conn, "COMMIT");
  const std::optional<turborel::import_error> wide = stopped_by(conn, "20,a,b\n", csv, how);
  REQUIRE(wide.has_value());
  CHECK(wide->message() == "record 1, line 1: [DESIGN] 3 fields, for 2 columns; 0 rows committed, 0 rolled back");
  CHECK(wide->sqlstate() == "22000");
  turborel::import_options const throwing{
      .table = "KEYED",
      .skip = true,
      .rejected = [](const turborel::rejected_row&) { throw std::logic_error{"x"}; },
  };
  CHECK_THROWS_AS(import_text(conn, "1,a\n", csv, throwing), std::logic_error);
}

/// Verifies: [CLIENT Importing] Tab-separated text with a text for null, its
/// fields the table's columns in order; a field longer than 64 KiB for a
/// large object column sent by creating a large object [CLIENT Large
/// objects].
TEST_CASE("TSV and long fields are imported") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  run(conn, "CREATE TABLE NOTES (ID INTEGER, BODY CLOB(1M))");
  run(conn, "COMMIT");
  turborel::dialect tsv = turborel::dialect::tsv();
  CHECK(tsv.delimiter == '\t');
  CHECK_FALSE(tsv.quoted);
  tsv.null = "\\N";
  const std::string text = "1\t\\N\n2\t" + std::string(70000, 'x') + "\n";
  CHECK(import_text(conn, text, tsv, {.table = "NOTES"}).rows == 2);
  const turborel::result read = conn.query("SELECT BODY FROM NOTES ORDER BY ID");
  CHECK(read.at(0).at(0).is_null());
  CHECK(read.at(1).at(0).lob().octets == 70000);
  run(conn, "COMMIT");
}

/// Verifies: [CLIENT Importing] An import begins with no transaction active;
/// a table not named right, input not well formed, said with where it
/// began.
TEST_CASE("an import is refused before its rows") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  const turborel::dialect csv = turborel::dialect::csv();
  const std::optional<turborel::import_error> named = stopped_by(conn, "1\n", csv, {.table = "1x"});
  REQUIRE(named.has_value());
  CHECK(named->message() == "[ISO/IEC 9075:1992 \xc2\xa7"
                            "5.4] 1x is not a <table name>; 0 rows committed, 0 rolled back");
  CHECK_FALSE(named->at().has_value());
  run(conn, "CREATE TABLE QUOTED (A VARCHAR(5))");
  const std::optional<turborel::import_error> active = stopped_by(conn, "a\n", csv, {.table = "QUOTED"});
  REQUIRE(active.has_value());
  CHECK_FALSE(active->cause().sql);
  CHECK(active->cause().message.find("no transaction active") != std::string::npos);
  run(conn, "COMMIT");
  const std::optional<turborel::import_error> quote = stopped_by(conn, "a\nb\"c\n", csv, {.table = "QUOTED"});
  REQUIRE(quote.has_value());
  CHECK(quote->message() == "record 2, line 2: [RFC 4180 \xc2\xa7"
                            "2 rule 5] field 1: a quote in a field not enclosed in quotes; 0 rows "
                            "committed, 1 rolled back");
  const std::optional<turborel::import_error> open = stopped_by(conn, "\"open\n", csv, {.table = "QUOTED"});
  REQUIRE(open.has_value());
  CHECK(open->cause().message.find("not closed") != std::string::npos);
  turborel::dialect wrong = csv;
  wrong.delimiter = '"';
  std::istringstream in{"a\n"};
  CHECK_THROWS_AS(turborel::delimited_text(in, wrong), turborel::error);
}

/// Verifies: [CLIENT Importing] A source of the program's own gives its
/// headings and records; what it throws of std::exception stops the import
/// as the input's failure, where it was said; anything else is thrown as it
/// was, once the import returns (R-RAII-2).
TEST_CASE("a source of the program's own is imported") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  run(conn, "CREATE TABLE OWN (ID INTEGER, NAME VARCHAR(10))");
  run(conn, "COMMIT");
  people five{5, 0, false};
  CHECK(turborel::import_rows(conn, five, {.table = "OWN"}).rows == 5);
  CHECK(count_of(conn, "OWN") == 5);
  run(conn, "COMMIT");
  people broken{5, 3, false};
  try {
    static_cast<void>(turborel::import_rows(conn, broken, {.table = "OWN"}));
    FAIL("the import did not fail");
  } catch (const turborel::import_error& e) {
    CHECK_FALSE(e.cause().sql);
    CHECK(e.cause().message == "the source broke");
    CHECK(e.at().value_or(turborel::record_position{}).record == 3);
    CHECK(e.rolled_back() == 2);
  }
  people odd{5, 2, true};
  CHECK_THROWS_AS(turborel::import_rows(conn, odd, {.table = "OWN"}), int);
  CHECK(count_of(conn, "OWN") == 5);
}

namespace {

/// A stream buffer whose reads throw.
class unreadable final : public std::streambuf {
protected:
  int_type underflow() override { throw std::runtime_error{"cannot read"}; }
};

} // namespace

/// Verifies: [CLIENT Importing] Rows skipped are counted with no handler to
/// give them to; text whose stream fails to be read stops the import; a
/// source of none imports none, and is destroyed as a source.
TEST_CASE("rows are skipped with no handler, and an unreadable stream stops") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  run(conn, "CREATE TABLE SKIPS (ID INTEGER PRIMARY KEY)");
  run(conn, "COMMIT");
  const turborel::imported kept =
      import_text(conn, "1\n1\n2\n", turborel::dialect::csv(), {.table = "SKIPS", .skip = true});
  CHECK(kept.rows == 2);
  CHECK(kept.rejected == 1);
  unreadable buffer;
  std::istream in{&buffer};
  in.exceptions(std::ios::badbit);
  turborel::delimited_text source{in, turborel::dialect::csv()};
  CHECK_THROWS_AS(turborel::import_rows(conn, source, {.table = "SKIPS"}), turborel::import_error);
  run(conn, "CREATE TABLE NONE_GIVEN (ID INTEGER, NAME VARCHAR(10))");
  run(conn, "COMMIT");
  const std::unique_ptr<turborel::import_source> owned = std::make_unique<people>(0, 0, false);
  CHECK(turborel::import_rows(conn, *owned, {.table = "NONE_GIVEN"}).rows == 0);
}

/// Verifies: [CLIENT Importing] A batch of 0 imports every row at once, in
/// one transaction: a failing row rolls back every row before it.
TEST_CASE("a batch of none imports every row at once") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  run(conn, "CREATE TABLE ONCE (ID INTEGER PRIMARY KEY)");
  run(conn, "COMMIT");
  const std::optional<turborel::import_error> stopped =
      stopped_by(conn, "1\n2\n3\n1\n", turborel::dialect::csv(), {.table = "ONCE", .batch = 0});
  REQUIRE(stopped.has_value());
  CHECK(stopped->committed() == 0);
  CHECK(stopped->rolled_back() == 3);
  CHECK(import_text(conn, "1\n2\n3\n", turborel::dialect::csv(), {.table = "ONCE", .batch = 0}).rows == 3);
  CHECK(count_of(conn, "ONCE") == 3);
}
