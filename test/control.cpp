/// @file
/// @brief Live tests of the session's control and what the server tells
/// [CLIENT Controlling the session] [CLIENT What the server tells]: Ping,
/// Reset, ChangeUser, attributes, several statements as one, notices,
/// conditions, completions' details; XML and UUID values (R-TEST-8).

#include "support/frames.hpp"
#include "support/server.hpp"

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/cursor.hpp>
#include <turborel/error.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/statement.hpp>
#include <turborel/uuid.hpp>
#include <turborel/value.hpp>
#include <turborel/xml.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

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

/// Options asking for every capability but `left_out`.
turborel::connect_options all_but(std::string_view left_out) {
  turborel::connect_options options;
  options.ask_none();
  for (const std::string_view name : {
           turborel::capability::notices,
           turborel::capability::status,
           turborel::capability::diagnostics,
           turborel::capability::describe,
           turborel::capability::cursors,
           turborel::capability::ping,
           turborel::capability::reset,
           turborel::capability::change_user,
           turborel::capability::query_attributes,
           turborel::capability::multi_statement,
           turborel::capability::commit_position,
           turborel::capability::copy,
           turborel::capability::row_counts,
           turborel::capability::generated_values,
           turborel::capability::fetch_ends,
           turborel::capability::conditions,
           turborel::capability::declared_types,
       }) {
    if (name != left_out) {
      options.ask(name);
    }
  }
  return options;
}

} // namespace

/// Verifies: [CLIENT Controlling the session] Ping asks whether the server
/// answers; Reset puts the session back as at login, its transaction rolled
/// back and told by notice 01T01, its prepared statements and settings gone,
/// its user kept [PROTOCOL Reset 'W'] [PROTOCOL Errors and notices] [PROTOCOL
/// Ping 'G'] [PROTOCOL Pong 'G'].
TEST_CASE("ping and reset the session") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  conn.ping();
  static_cast<void>(conn.query("CREATE TABLE CT1 (ID INTEGER)"));
  static_cast<void>(conn.query("COMMIT"));
  static_cast<void>(conn.query("SET TIME ZONE INTERVAL '+05:00' HOUR TO MINUTE"));
  static_cast<void>(conn.query("INSERT INTO CT1 VALUES (1)"));
  const turborel::statement kept = conn.prepare("SELECT ID FROM CT1");
  conn.clear_notices();
  conn.reset();
  CHECK_FALSE(conn.in_transaction());
  CHECK(conn.setting("SESSION.TIME_ZONE") != "+05:00");
  REQUIRE_FALSE(conn.notices().empty());
  CHECK(conn.notices().back().sqlstate == "01T01");
  CHECK(conn.notices().back().diagnostics.value_or(turborel::diagnostics{}).severity == "NOTICE");
  CHECK(conn.authorization() == "ANN");
  CHECK(sqlstate_of([&] { static_cast<void>(conn.execute(kept)); }) == "26000");
  CHECK(conn.query("SELECT ID FROM CT1").empty());
}

/// Verifies: [CLIENT Controlling the session] Change user authenticates the
/// connection again as another user, whose session it then is, a
/// transaction rolled back told by 01T01; a refusal ends the connection
/// [PROTOCOL ChangeUser 'U'].
TEST_CASE("change the connection's user") {
  LIVE_OR_SKIP();
  TLS_OR_SKIP();
  test::live::user("BOB", "bob secret");
  turborel::connection conn = test::live::trusted();
  static_cast<void>(conn.query("VALUES 1"));
  turborel::mechanism bob = turborel::mechanism::scram("bob secret");
  bob.in_the_clear();
  conn.change_user("bob", bob);
  CHECK(conn.authorization() == "BOB");
  CHECK(conn.setting("AUTHORIZATION") == "BOB");
  CHECK_FALSE(conn.in_transaction());
  turborel::mechanism wrong = turborel::mechanism::scram("not bob's");
  wrong.in_the_clear();
  CHECK(sqlstate_of([&] { conn.change_user("bob", wrong); }) != "none");
  CHECK(conn.lost());
}

/// Verifies: [CLIENT Controlling the session] Query attributes are carried by
/// every later statement, read by QUERY_ATTRIBUTE, until set again; none
/// clears them; past the protocol's bounds they are refused before sending,
/// the connection kept [PROTOCOL The tail].
TEST_CASE("query attributes go with every statement until set again") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  conn.set_query_attributes({{"k", "v"}});
  CHECK(conn.query("VALUES (QUERY_ATTRIBUTE('k'))").at(0).get<std::string>(0) == "v");
  CHECK(conn.query("VALUES (QUERY_ATTRIBUTE('k'))").at(0).get<std::string>(0) == "v");
  conn.set_query_attributes({});
  CHECK(conn.query("VALUES (QUERY_ATTRIBUTE('k'))").at(0).at(0).is_null());
  std::vector<std::pair<std::string, std::string>> too_many;
  too_many.reserve(33);
  for (int i = 0; i < 33; ++i) {
    too_many.emplace_back("a" + std::to_string(i), "v");
  }
  CHECK(sqlstate_of([&] { conn.set_query_attributes(too_many); }) == "08006");
  CHECK_FALSE(conn.lost());
  turborel::connection without = test::live::trusted(all_but(turborel::capability::query_attributes));
  CHECK(sqlstate_of([&] { without.set_query_attributes({{"k", "v"}}); }) == "08006");
}

/// Verifies: [CLIENT Controlling the session] Several statements run as one
/// request, each in order, until one fails: each outcome, and the failure
/// that stopped the rest; text of no statement, one completion [PROTOCOL
/// Query 'Q'].
TEST_CASE("several statements run as one request") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  const turborel::ran stopped = conn.query_many("VALUES (1); CREATE TABLE MS (ID INTEGER); SELECT NOPE; VALUES (2)");
  CHECK(stopped.size() == 2);
  CHECK(stopped.at(0).at(0).get<std::int64_t>(0) == 1);
  REQUIRE(stopped.stopped_by().has_value());
  CHECK(stopped.stopped_by().value_or(turborel::failure{}).sqlcode < 0);
  CHECK_THROWS_AS(stopped.at(2), turborel::error);
  const turborel::ran both = conn.query_many("VALUES (1); VALUES (2)");
  CHECK(both.size() == 2);
  CHECK_FALSE(both.stopped_by().has_value());
  CHECK(conn.query_many("").size() == 1);
}

/// Verifies: [CLIENT What the server tells] A statement's warnings come as
/// notices to a handler, or kept, the last 64; with `conditions` they come
/// in its completion instead, each a failure; what a handler throws is
/// thrown once the statement returns [PROTOCOL Errors and notices].
TEST_CASE("warnings come as notices or as conditions") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  static_cast<void>(conn.query("CREATE PROCEDURE WARNS () MODIFIES SQL DATA SIGNAL SQLSTATE '01ABC'"));
  static_cast<void>(conn.query("COMMIT"));
  static_cast<void>(conn.query("CALL WARNS()"));
  REQUIRE(conn.conditions().size() == 1);
  CHECK(conn.conditions().front().sqlstate == "01ABC");
  conn.clear_conditions();
  CHECK(conn.conditions().empty());
  turborel::connection noticed = test::live::trusted(all_but(turborel::capability::conditions));
  std::vector<std::string> told;
  noticed.on_notice([&told](const turborel::failure& notice) { told.push_back(notice.sqlstate); });
  static_cast<void>(noticed.query("CALL WARNS()"));
  CHECK(told == std::vector<std::string>{"01ABC"});
  CHECK(noticed.notices().empty());
  noticed.on_notice([](const turborel::failure&) { throw std::runtime_error{"seen"}; });
  CHECK_THROWS_AS(noticed.query("CALL WARNS()"), std::runtime_error);
  noticed.on_notice(nullptr);
  static_cast<void>(noticed.query("CALL WARNS()"));
  REQUIRE(noticed.notices().size() == 1);
  CHECK(noticed.notices().front().diagnostics.value_or(turborel::diagnostics{}).severity == "WARNING");
}

/// Verifies: [CLIENT What the server tells] A completion tells the rows a
/// statement changed or a query returned, the identity values an INSERT
/// generated and the server's time; a failure's diagnostics name its
/// constraint and table [PROTOCOL Complete
/// 'K'] [PROTOCOL Errors and notices].
TEST_CASE("a completion tells what the statement did") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  conn.set_autocommit(true);
  static_cast<void>(conn.query("CREATE TABLE GV (ID INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY, N VARCHAR(5))"));
  static_cast<void>(conn.query("INSERT INTO GV (N) VALUES ('a'), ('b')"));
  CHECK(conn.rows_affected() == 2);
  CHECK(conn.generated() == std::vector<std::string_view>{"1", "2"});
  CHECK(conn.server_time().has_value());
  // A database kept in memory has no log, so tells no commit's position;
  // the frames' tests read the position a Complete tells.
  CHECK_FALSE(conn.commit_position().has_value());
  static_cast<void>(conn.query("SELECT N FROM GV"));
  CHECK(conn.rows_affected() == 2);
  try {
    static_cast<void>(conn.query("INSERT INTO GV (ID, N) OVERRIDING SYSTEM VALUE VALUES (1, 'c')"));
    FAIL("the INSERT did not fail");
  } catch (const turborel::error& e) {
    const turborel::diagnostics told = e.details().diagnostics.value_or(turborel::diagnostics{});
    CHECK(told.severity == "ERROR");
    CHECK(told.table == "GV");
    CHECK(told.constraint.has_value());
  }
}

/// Verifies: [CLIENT What the server tells] Under autocommit with
/// `fetch-ends`, a cursor's transaction ends as it ends: the last row's
/// fetch commits, and close commits [PROTOCOL Conversations].
TEST_CASE("a cursor under autocommit ends its transaction as it ends") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  conn.set_autocommit(true);
  const turborel::statement select = conn.prepare("SELECT X FROM (VALUES (1), (2), (3)) AS V(X)");
  turborel::cursor read = turborel::cursor::open(conn, select, turborel::params{}, 2);
  CHECK(conn.in_transaction());
  static_cast<void>(read.fetch(2));
  CHECK_FALSE(conn.in_transaction());
  turborel::cursor closed = turborel::cursor::open(conn, select, turborel::params{}, 1);
  CHECK(conn.in_transaction());
  closed.close();
  CHECK_FALSE(conn.in_transaction());
}

/// Verifies: [CLIENT XML values] An XML value is read as its serialization,
/// its nodes parsed from it, and sent as a parameter of its text; a value
/// the server cannot serialize fails the fetch (2200W).
TEST_CASE("XML values are read, parsed and sent") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  static_cast<void>(conn.query("CREATE TABLE XO (BODY XML)"));
  const std::string order = R"(<order no="18"><item>jam</item></order>)";
  static_cast<void>(conn.query("INSERT INTO XO VALUES (?)", turborel::xml_text{order}));
  const turborel::result read = conn.query("SELECT BODY FROM XO");
  CHECK(read.at(0).get<std::string>(0) == order);
  const turborel::xml_tree tree = turborel::xml_tree::of(read.at(0).at(0));
  const turborel::xml_node element = tree.root().child(0);
  CHECK(element.kind() == turborel::xml_kind::element);
  CHECK(element.name() == "order");
  CHECK(element.attribute(0).value() == "18");
  CHECK(element.child(0).child(0).value() == "jam");
  CHECK(sqlstate_of([] { static_cast<void>(turborel::xml_tree::parse("<a>")); }) == "2200N");
  CHECK(sqlstate_of([&conn] {
          static_cast<void>(
              conn.query("VALUES (XMLQUERY('attribute a {1}' RETURNING SEQUENCE BY REF EMPTY ON EMPTY))"));
        }) == "2200W");
}

/// Verifies: [CLIENT UUIDs] A UUID is read as its 16 octets and its text,
/// and sent as a parameter made of either.
TEST_CASE("UUIDs are read and sent") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  static_cast<void>(conn.query("CREATE TABLE TK (ID UUID)"));
  const std::optional<turborel::uuid> id = turborel::uuid::parse("919108f7-52d1-4320-9bac-f847db4148a8");
  static_cast<void>(conn.query("INSERT INTO TK VALUES (?)", id.value_or(turborel::uuid{})));
  static_cast<void>(conn.query("INSERT INTO TK VALUES (CAST(? AS UUID))", id.value_or(turborel::uuid{}).to_string()));
  const turborel::result read = conn.query("SELECT ID FROM TK");
  REQUIRE(read.size() == 2);
  CHECK(read.at(0).get<turborel::uuid>(0) == id);
  CHECK(read.at(1).get<turborel::uuid>(0).to_string() == "919108f7-52d1-4320-9bac-f847db4148a8");
  CHECK(read.at(0).at(0).bytes().size() == 16);
}

/// Verifies: [PROTOCOL JSON values] A JSON value is read as its JSON text
/// and sent as a parameter of it, every node kept: an exact number keeps
/// the scale it was written with, a wide one its digits, an approximate
/// number, a decimal floating point one, a datetime and a string theirs;
/// text nesting arrays and objects 128 deep is taken, 129 deep refused
/// before sending (SQLCODE -900, SQLSTATE 0A000), text no JSON value is
/// refused as such (22018) [PROTOCOL Values].
TEST_CASE("JSON values are read and sent, every node kept") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  const std::string exact = R"({"a":1.50,"w":123456789012345678901234567890.5,"i":-7,"s":"x\"[{"})";
  CHECK(conn.query("VALUES CAST(? AS JSON)", turborel::json_text{exact}).at(0).at(0).json() == exact);
  const turborel::result built = conn.query(
      "VALUES JSON_ARRAY(CAST(1.5 AS DOUBLE PRECISION), CAST(1.5 AS DECFLOAT), DATE '2026-11-20' RETURNING JSON)");
  CHECK(built.at(0).at(0).kind() == turborel::kind::json);
  CHECK(built.at(0).at(0).json() == R"([1.5E0,1.5,"2026-11-20"])");
  const std::string deepest = std::string(128, '[') + std::string(128, ']');
  CHECK(conn.query("VALUES CAST(? AS JSON)", turborel::json_text{deepest}).at(0).at(0).json() == deepest);
  turborel::params values;
  try {
    values.add(turborel::json_text{"[" + deepest + "]"});
    FAIL("JSON past its limit was taken");
  } catch (const turborel::error& e) {
    CHECK(e.sqlcode() == -900);
    CHECK(e.sqlstate() == "0A000");
  }
  CHECK(sqlstate_of([&values] { values.add(turborel::json_text{R"({"a":[}"})"}); }) == "22018");
  const std::string quoted_brackets = R"({"a\"[[[":[)" + std::string(126, '[') + "}";
  CHECK(sqlstate_of([&] { values.add(turborel::json_text{quoted_brackets}); }) == "22018");
  CHECK(values.size() == 0);
}
