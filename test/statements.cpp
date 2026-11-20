/// @file
/// @brief Live tests of statements [CLIENT Running statements] [CLIENT
/// Prepared statements]: queries, prepared statements and their cursors,
/// transactions and autocommit, against the run's server (R-TEST-8).

#include "support/server.hpp"

#include <turborel/connection.hpp>
#include <turborel/cursor.hpp>
#include <turborel/datetime.hpp>
#include <turborel/decimal.hpp>
#include <turborel/error.hpp>
#include <turborel/interval.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/statement.hpp>
#include <turborel/uuid.hpp>
#include <turborel/value.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals; // NOLINT(google-build-using-namespace): chrono's literals read as the values do.

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

/// A table of the test's own, made afresh in ANN's schema.
void table(turborel::connection& conn, const std::string& name, const std::string& columns) {
  static_cast<void>(conn.query("CREATE TABLE " + name + " (" + columns + ")"));
  static_cast<void>(conn.query("COMMIT"));
}

} // namespace

/// Verifies: [CLIENT Running statements] A statement's outcome is a query's
/// columns and rows, or another's completion, its SQLCODE and SQLSTATE; a query
/// of no rows completes with 100, 02000 [PROTOCOL Query 'Q']; a failure gives
/// its SQLCODE, SQLSTATE, message, rule and position [PROTOCOL Description 'D']
/// [PROTOCOL Complete 'K'].
TEST_CASE("a statement gives its rows or its completion") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  table(conn, "ST1", "ID INTEGER, NAME VARCHAR(10)");
  const turborel::result inserted = conn.query("INSERT INTO ST1 VALUES (1, 'Ada'), (2, 'Grace')");
  CHECK_FALSE(inserted.has_rows());
  CHECK(inserted.sqlcode() == 0);
  CHECK(inserted.sqlstate() == "00000");
  const turborel::result read = conn.query("SELECT ID, NAME FROM ST1 ORDER BY ID");
  CHECK(read.has_rows());
  CHECK(read.columns().at(1).name == "NAME");
  CHECK(read.size() == 2);
  CHECK(read.at(1).get<std::string>(1) == "Grace");
  const turborel::result none = conn.query("SELECT ID FROM ST1 WHERE ID > 9");
  CHECK(none.empty());
  CHECK(none.sqlcode() == 100);
  CHECK(none.sqlstate() == "02000");
  try {
    static_cast<void>(conn.query("SELECT * FROM NOPE"));
    FAIL("the query did not fail");
  } catch (const turborel::error& e) {
    CHECK(e.sqlcode() < 0);
    CHECK(e.sqlstate().size() == 5);
    CHECK(e.details().rule.has_value());
    CHECK(e.details().position.has_value());
    CHECK(e.message().find("NOPE") != std::string_view::npos);
  }
}

/// Verifies: [CLIENT Running statements] A query's rows come to a handler a
/// batch at a time, as they come; what the handler throws is thrown once
/// the statement's replies are read [PROTOCOL Rows 'B'].
TEST_CASE("a query's rows come a batch at a time") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  std::size_t rows = 0;
  std::size_t batches = 0;
  const turborel::result columns = conn.query_batches("WITH RECURSIVE T(N) AS (VALUES CAST(1 AS INTEGER) UNION ALL "
                                                      "SELECT N + 1 FROM T WHERE N < 20000) SELECT N FROM T",
                                                      [&](const turborel::result& batch) {
                                                        rows += batch.size();
                                                        ++batches;
                                                      });
  CHECK(rows == 20000);
  CHECK(batches > 1);
  CHECK(columns.columns().size() == 1);
  CHECK(columns.empty());
  CHECK_THROWS_AS(conn.query_batches("VALUES 1", [](const turborel::result&) { throw std::runtime_error{"stop"}; }),
                  std::runtime_error);
  CHECK(conn.query("VALUES 2").size() == 1);
}

/// Verifies: [CLIENT Running statements] Every kind of value is read, and
/// sent as a parameter: exact and approximate numbers, characters, octets,
/// datetimes, intervals, truth values, UUIDs and the null value.
TEST_CASE("every kind of value goes to the server and back") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  const turborel::date day{.day = std::chrono::year{2026} / 11 / 20};
  const turborel::timestamp at{.instant = std::chrono::sys_days{day.day} + 13h + 5min, .precision = 6};
  const turborel::interval span{
      .qualifier =
          {
              .start = turborel::interval_field::day,
              .end = turborel::interval_field::second,
              .leading_precision = 2,
              .fractional_precision = 6,
          },
      .units = turborel::int128{90'000'000},
  };
  const std::optional<turborel::uuid> id = turborel::uuid::parse("919108f7-52d1-4320-9bac-f847db4148a8");
  const turborel::result back = conn.query(
      "VALUES (CAST(? AS DECIMAL(9,2)), CAST(? AS DOUBLE PRECISION), CAST(? AS VARCHAR(5)), CAST(? AS VARBINARY(4)), "
      "CAST(? AS DATE), CAST(? AS TIMESTAMP), CAST(? AS INTERVAL DAY TO SECOND), CAST(? AS BOOLEAN), "
      "CAST(? AS UUID), CAST(? AS INTEGER))",
      turborel::decimal{.units = turborel::int128{-1250}, .scale = 2}, 2.5, "Ada",
      std::span<const std::byte>{std::array{std::byte{1}, std::byte{2}}}, day, at, span, true,
      id.value_or(turborel::uuid{}), std::nullopt);
  const turborel::row row = back.at(0);
  CHECK(row.get<turborel::decimal>(0).to_string() == "-12.50");
  CHECK(std::bit_cast<std::uint64_t>(row.get<double>(1)) == std::bit_cast<std::uint64_t>(2.5));
  CHECK(row.get<std::string>(2) == "Ada");
  CHECK(row.get<std::span<const std::byte>>(3).size() == 2);
  CHECK(row.get<turborel::date>(4) == day);
  CHECK(row.get<turborel::timestamp>(5).instant == at.instant);
  CHECK(row.get<turborel::interval>(6).microseconds() == 90s);
  CHECK(row.get<bool>(7));
  CHECK(row.get<turborel::uuid>(8) == id);
  CHECK(row.get<std::optional<std::int64_t>>(9) == std::nullopt);
  CHECK(row.at(0).kind() == turborel::kind::exact);
  CHECK(conn.query("VALUES CAST(? AS VARCHAR(5))", "").at(0).get<std::string>(0).empty());
}

/// Verifies: [CLIENT Running statements] A transaction begins with the first
/// statement and lasts until COMMIT or ROLLBACK, the session telling
/// whether it is active; under autocommit each statement is its own
/// transaction, committed as it succeeds, rolled back as it fails, with no
/// COMMIT sent [PROTOCOL Query 'Q'] [PROTOCOL Conversations].
TEST_CASE("transactions last to COMMIT, or a statement under autocommit") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  table(conn, "ST2", "ID INTEGER PRIMARY KEY");
  CHECK_FALSE(conn.autocommit());
  static_cast<void>(conn.query("INSERT INTO ST2 VALUES (1)"));
  CHECK(conn.in_transaction());
  static_cast<void>(conn.query("ROLLBACK"));
  CHECK_FALSE(conn.in_transaction());
  conn.set_autocommit(true);
  CHECK(conn.autocommit());
  static_cast<void>(conn.query("INSERT INTO ST2 VALUES (2)"));
  CHECK_FALSE(conn.in_transaction());
  CHECK(sqlstate_of([&conn] { static_cast<void>(conn.query("INSERT INTO ST2 VALUES (2)")); }) == "23000");
  CHECK_FALSE(conn.in_transaction());
  conn.set_autocommit(false);
  CHECK(conn.query("SELECT ID FROM ST2").size() == 1);
}

/// Verifies: [CLIENT Prepared statements] A statement prepared once, its
/// parameters' types and its columns given, runs with values as many times
/// as wanted, across transactions, until deallocated, after which it is no
/// more (26000) [PROTOCOL Prepare 'P'] [PROTOCOL Deallocate 'D'].
TEST_CASE("a prepared statement runs as many times as wanted") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  table(conn, "ST3", "ID INTEGER, NAME VARCHAR(10)");
  const turborel::statement insert = conn.prepare("INSERT INTO ST3 VALUES (?, ?)");
  CHECK(insert.parameter_count() == 2);
  CHECK(insert.parameter_type(0).kind() == turborel::kind::exact);
  CHECK(insert.columns().empty());
  CHECK_THROWS_AS(insert.parameter_type(2), turborel::error);
  static_cast<void>(conn.execute(insert, turborel::params::of(1, "Ada")));
  static_cast<void>(conn.query("COMMIT"));
  static_cast<void>(conn.execute(insert, turborel::params::of(2, "Grace")));
  const turborel::statement select = conn.prepare("SELECT NAME FROM ST3 WHERE ID = ?");
  CHECK(select.columns().at(0).name == "NAME");
  CHECK(conn.execute(select, turborel::params::of(2)).at(0).get<std::string>(0) == "Grace");
  conn.deallocate(select);
  CHECK(sqlstate_of([&] { conn.deallocate(select); }) == "26000");
  CHECK(sqlstate_of([&] { static_cast<void>(conn.execute(select, turborel::params::of(2))); }) == "26000");
}

/// Verifies: [CLIENT Prepared statements] A cursor of a query is opened with
/// its first rows, fetched a batch at a time, Suspended while it has more,
/// and closed; a cursor not open fails (24000) [PROTOCOL Execute 'E']
/// [PROTOCOL Fetch 'F'] [PROTOCOL Close 'C'] [PROTOCOL Suspended 'S'].
TEST_CASE("a cursor is fetched a batch at a time") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  const turborel::statement select = conn.prepare(
      "WITH RECURSIVE T(N) AS (VALUES CAST(1 AS INTEGER) UNION ALL SELECT N + 1 FROM T WHERE N < 10) SELECT N FROM T");
  turborel::cursor cursor = turborel::cursor::open(conn, select, turborel::params{}, 4);
  CHECK(cursor.batch().size() == 4);
  CHECK(cursor.more());
  CHECK(cursor.fetch(4).size() == 4);
  CHECK(cursor.fetch(4).size() == 2);
  CHECK_FALSE(cursor.more());
  CHECK(sqlstate_of([&cursor] { static_cast<void>(cursor.fetch(4)); }) == "24000");
  turborel::cursor open = turborel::cursor::open(conn, select, turborel::params{}, 3);
  CHECK(sqlstate_of([&] { static_cast<void>(turborel::cursor::open(conn, select, turborel::params{}, 3)); }) ==
        "24000");
  CHECK(sqlstate_of([&] { conn.deallocate(select); }) == "24000");
  open.close();
  CHECK(sqlstate_of([&open] { open.close(); }).size() == 5);
  std::int64_t sum = 0;
  turborel::cursor all = turborel::cursor::open(conn, select, turborel::params{}, 3);
  for (const turborel::row row : all) {
    sum += row.get<std::int64_t>(0);
  }
  CHECK(sum == 55);
  turborel::cursor moved = turborel::cursor::open(conn, select, turborel::params{}, 3);
  turborel::cursor taken{std::move(moved)};
  turborel::cursor numbered = turborel::cursor::open(conn, select, turborel::params{}, 3, 1);
  taken = std::move(numbered);
  CHECK(taken.more());
  CHECK(turborel::cursor::open(conn, select, turborel::params{}, 3).more());
}

/// Verifies: [CLIENT Controlling the session] Several cursors of one
/// statement are open at once, each by a number of the program's, and an
/// open cursor's columns are described; a statement is described again
/// [PROTOCOL Describe 'Y'].
TEST_CASE("numbered cursors are open at once, and described") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  const turborel::statement select = conn.prepare("SELECT CAST(? AS INTEGER) AS N FROM (VALUES 1, 2, 3) AS V(X)");
  turborel::cursor const one = turborel::cursor::open(conn, select, turborel::params::of(10), 1, 1);
  turborel::cursor two = turborel::cursor::open(conn, select, turborel::params::of(20), 1, 2);
  CHECK(one.batch().at(0).get<std::int64_t>(0) == 10);
  CHECK(two.batch().at(0).get<std::int64_t>(0) == 20);
  CHECK(two.describe().columns().at(0).name == "N");
  const turborel::statement again = conn.describe(select);
  CHECK(again.columns().at(0).name == "N");
  CHECK(again.parameter_count() == 1);
}

/// Verifies: [PROTOCOL Execute 'E'] A CALL's Prepared names its inputs as
/// parameters, and one column for each `?` of an OUT or INOUT parameter;
/// its Execute sends one row of their values before its completion
/// [PROTOCOL Prepared 'P'].
TEST_CASE("a CALL gives its output parameters as one row") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  static_cast<void>(
      conn.query("CREATE PROCEDURE TWICE (IN X INTEGER, OUT Y INTEGER) CONTAINS SQL BEGIN SET Y = X * 2; END"));
  static_cast<void>(conn.query("COMMIT"));
  const turborel::statement call = conn.prepare("CALL TWICE(?, ?)");
  CHECK(call.parameter_count() == 1);
  REQUIRE(call.columns().size() == 1);
  const turborel::result out = conn.execute(call, turborel::params::of(21));
  CHECK(out.size() == 1);
  CHECK(out.at(0).get<std::int64_t>(0) == 42);
}

/// Verifies: [CLIENT Prepared statements] A column's declared type, its
/// DATA_TYPE name and as SQL writes it, comes with `declared-types`, which
/// connecting asks for, so INTEGER is told from NUMERIC(10,0)
/// [PROTOCOL Declared types]; and whether it is NOT NULL, a prepared
/// statement's column as a result's [PROTOCOL Composite fields].
TEST_CASE("a column's declared type is told") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  table(conn, "ST4", "I INTEGER, D DECIMAL(9,2), F DOUBLE PRECISION");
  const turborel::result read = conn.query("SELECT I, D, F FROM ST4");
  const std::vector<turborel::column> columns = read.columns();
  CHECK(columns.at(0).declared_name == "INTEGER");
  CHECK(columns.at(0).type_written == "INTEGER");
  CHECK(columns.at(1).type_written == "DECIMAL(9,2)");
  CHECK(columns.at(2).declared_name == "DOUBLE PRECISION");
  const turborel::statement select = conn.prepare("SELECT I FROM ST4");
  CHECK(select.columns().at(0).declared_name == "INTEGER");
  CHECK(!select.columns().at(0).not_null);
  CHECK(!read.columns().at(0).not_null);
  table(conn, "ST4N", "K INTEGER NOT NULL, V INTEGER");
  const std::vector<turborel::column> keyed = conn.prepare("SELECT K, V FROM ST4N").columns();
  CHECK(keyed.at(0).not_null);
  CHECK(!keyed.at(1).not_null);
}

/// Verifies: [CLIENT Prepared statements] Parameters given as text, as
/// turborel-c's trl_query_params takes them, are assigned to each
/// parameter's type by the server, as SQL's CAST of the text; a null among
/// them; what the connection tells after is the statement's own
/// completion, its rows and generated values.
TEST_CASE("parameters given as text are cast by the server") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  const turborel::result read =
      conn.query_text("VALUES (CAST(? AS INTEGER) + 1, CAST(? AS VARCHAR(3)))", {std::string{"41"}, std::nullopt});
  CHECK(read.at(0).get<std::int64_t>(0) == 42);
  CHECK(read.at(0).at(1).is_null());
  CHECK(sqlstate_of([&conn] { static_cast<void>(conn.query("VALUES CAST(? AS INTEGER)", "x")); }) != "none");
  table(conn, "ST4T", "ID INTEGER GENERATED ALWAYS AS IDENTITY, V INTEGER");
  static_cast<void>(conn.query_text("INSERT INTO ST4T (V) VALUES (CAST(? AS INTEGER)), (2)", {std::string{"1"}}));
  CHECK(conn.rows_affected() == 2);
  CHECK(conn.generated() == std::vector<std::string_view>{"1", "2"});
}

/// Verifies: [CLIENT Running statements] A handler that throws on a query's
/// first batch is given no more; what it threw is thrown once the rest are
/// read, and the connection goes on.
TEST_CASE("a handler that throws is given no more batches") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  std::size_t given = 0;
  CHECK_THROWS_AS(conn.query_batches("WITH RECURSIVE T(N) AS (VALUES CAST(1 AS INTEGER) UNION ALL SELECT N + 1 FROM T "
                                     "WHERE N < 20000) SELECT N FROM T",
                                     [&given](const turborel::result&) {
                                       ++given;
                                       throw std::runtime_error{"stop"};
                                     }),
                  std::runtime_error);
  CHECK(given == 1);
  CHECK(conn.query("VALUES 1").at(0).get<std::optional<std::int64_t>>(0) == 1);
  const turborel::statement select = conn.prepare("SELECT X FROM (VALUES (1), (2)) AS V(X)");
  turborel::cursor read = turborel::cursor::open(conn, select, turborel::params{}, 1);
  auto at = read.begin();
  at++;
  CHECK(at != read.end());
}

/// Verifies: [PROTOCOL Query 'Q'] Under autocommit a statement whose commit
/// fails, a deferred constraint broken, gives the commit's failure, and no
/// transaction is active after it.
TEST_CASE("a statement whose commit fails under autocommit gives the failure") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  table(conn, "ST5", "ID INTEGER, CONSTRAINT ST5_POS CHECK (ID > 0) INITIALLY DEFERRED");
  conn.set_autocommit(true);
  CHECK(sqlstate_of([&conn] { static_cast<void>(conn.query("INSERT INTO ST5 VALUES (-1)")); }) == "40002");
  CHECK_FALSE(conn.in_transaction());
  CHECK(conn.query("SELECT ID FROM ST5").empty());
}

/// Verifies: [PROTOCOL Prepare 'P'] Each statement prepared takes a number
/// of its own, so none whose cursor is open is replaced: one prepared
/// while a cursor is open leaves the cursor reading on.
TEST_CASE("a statement prepared while a cursor is open replaces none") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  const turborel::statement select = conn.prepare("SELECT X, X * 2 FROM (VALUES (1), (2), (3)) AS V(X)");
  turborel::cursor open = turborel::cursor::open(conn, select, turborel::params{}, 1);
  const turborel::statement other = conn.prepare("VALUES (1)");
  const turborel::result& next = open.fetch(1);
  CHECK(next.columns().size() == 2);
  CHECK(next.at(0).get<std::int64_t>(0) == 2);
  CHECK(conn.execute(other).size() == 1);
}

/// Verifies: [CLIENT What the server tells] Under autocommit with
/// `fetch-ends`, a cursor whose rows fail as they are read rolls its
/// transaction back, the connection kept [PROTOCOL Conversations].
TEST_CASE("a cursor's failing rows roll back under autocommit") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  conn.set_autocommit(true);
  const turborel::statement select = conn.prepare("SELECT 10 / (X - 3) FROM (VALUES (1), (2), (3)) AS V(X)");
  CHECK(sqlstate_of([&] { static_cast<void>(turborel::cursor::open(conn, select, turborel::params{}, 1)); }) ==
        "22012");
  CHECK_FALSE(conn.in_transaction());
  CHECK_FALSE(conn.lost());
}
