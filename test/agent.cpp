/// @file
/// @brief Live tests of the SQL-agent [CLIENT The SQL-agent], as
/// turborel's command-line client's tests run it (R-TEST-8): connections
/// made, chosen and ended by CONNECT, SET CONNECTION and DISCONNECT, and
/// each refusal by its SQLSTATE.

#include "support/frames.hpp"
#include "support/server.hpp"

#include <turborel/agent.hpp>
#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/error.hpp>
#include <turborel/result.hpp>
#include <turborel/settings.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

/// The server's address, as CONNECT TO names it.
std::string address() { return "127.0.0.1:" + std::to_string(test::live::port); }

/// Connects to the server `named`, a host and a port, or for none the
/// test's server, as `user`, or ann, by trust.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the agent's connector's, in its order.
turborel::connection dial(const std::optional<std::string>& named, const std::optional<std::string>& user) {
  std::string host = "127.0.0.1";
  std::uint16_t port = test::live::port;
  if (named) {
    const std::size_t colon = named->rfind(':');
    host = named->substr(0, colon);
    port = colon == std::string::npos ? test::live::port
                                      : static_cast<std::uint16_t>(std::stoul(named->substr(colon + 1)));
  }
  turborel::mechanism trust = turborel::mechanism::trust();
  return turborel::connection::connect(turborel::target::tcp(host, port), user.value_or("ann"), trust);
}

/// The one value of the one row `sql` gives, as text.
std::string value_of(turborel::agent& agent, const std::string& sql) {
  return agent.query(sql).at(0).at(0).to_string();
}

/// The SQLSTATE `sql` fails with, or of its completion.
std::string state_of(turborel::agent& agent, const std::string& sql) {
  try {
    return std::string{agent.query(sql).sqlstate()};
  } catch (const turborel::error& e) {
    return std::string{e.sqlstate()};
  }
}

/// The table the agent's connections read, of one row, readable by all.
void table() {
  turborel::connection conn = test::live::trusted();
  if (conn.query("SELECT 1 FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 'AG'").empty()) {
    static_cast<void>(conn.query("CREATE TABLE AG (K INTEGER)"));
    static_cast<void>(conn.query("INSERT INTO AG VALUES (1)"));
    static_cast<void>(conn.query("GRANT SELECT ON AG TO PUBLIC"));
  }
  static_cast<void>(conn.query("COMMIT"));
}

} // namespace

/// Verifies: [CLIENT The SQL-agent] CONNECT TO an address AS a name makes a
/// connection to that server, as the user named, current; SET CONNECTION
/// makes another current, DEFAULT the one the agent began with; DISCONNECT
/// ends one, and with the current one ended, none is current until one is
/// made or chosen; CONNECT TO DEFAULT makes the default one again.
TEST_CASE("connections are made, chosen and ended") {
  LIVE_OR_SKIP();
  table();
  turborel::agent agent{dial, test::live::trusted()};
  CHECK(state_of(agent, "CONNECT TO '" + address() + "' AS 'other' USER 'y'") == "00000");
  CHECK(agent.current_name() == "OTHER");
  CHECK(value_of(agent, "VALUES CURRENT_USER") == "Y");
  CHECK(state_of(agent, "COMMIT") == "00000");
  CHECK(state_of(agent, "SET CONNECTION DEFAULT") == "00000");
  CHECK(value_of(agent, "VALUES CURRENT_USER") == "ANN");
  CHECK(value_of(agent, "SELECT COUNT(*) FROM AG") == "1");
  CHECK(state_of(agent, "COMMIT") == "00000");
  CHECK(state_of(agent, "SET CONNECTION ' OTHER '") == "00000");
  CHECK(state_of(agent, "DISCONNECT CURRENT") == "00000");
  CHECK_FALSE(agent.has_current());
  CHECK(agent.size() == 1);
  CHECK(state_of(agent, "SELECT COUNT(*) FROM AG") == "08003");
  CHECK(state_of(agent, "SET CONNECTION DEFAULT") == "00000");
  CHECK(value_of(agent, "SELECT COUNT(*) FROM AG") == "1");
  CHECK(state_of(agent, "COMMIT") == "00000");
  CHECK(state_of(agent, "DISCONNECT ALL") == "00000");
  CHECK(agent.size() == 0);
  CHECK(state_of(agent, "CONNECT TO DEFAULT") == "00000");
  CHECK(value_of(agent, "SELECT COUNT(*) FROM AG") == "1");
  agent.terminate();
  const turborel::agent moved{std::move(agent)};
  CHECK(moved.size() == 0);
}

/// Verifies: [CLIENT The SQL-agent] Each refusal by its SQLSTATE: none such
/// (08003), a name in use (08002), a name or user not an identifier (2E000,
/// 28000), a server not reached (08001); CONNECT or SET CONNECTION in a
/// transaction (0A001), DISCONNECT of a connection whose transaction is
/// active (25000); the connections held go on.
TEST_CASE("connection statements are refused, saying why") {
  LIVE_OR_SKIP();
  table();
  turborel::agent agent{dial, test::live::trusted()};
  const std::string to = "CONNECT TO '" + address() + "'";
  const std::vector<std::pair<std::string, std::string>> refusals{
      {"SET CONNECTION 'NOPE'", "08003"},
      {"DISCONNECT 'NOPE'", "08003"},
      {"CONNECT TO DEFAULT", "08002"},
      {to, "2E000"},
      {to + " AS '1x'", "2E000"},
      {to + " AS 'C' USER 'a b'", "28000"},
      {"CONNECT TO '127.0.0.1:1' AS 'C'", "08001"},
      {"CONNECT TO 'no-such-host.invalid' AS 'C'", "08001"},
      {"SELECT COUNT(*) FROM AG", "00000"},
      {to + " AS 'C'", "0A001"},
      {"SET CONNECTION DEFAULT", "0A001"},
      {"DISCONNECT DEFAULT", "25000"},
      {"DISCONNECT ALL", "25000"},
      {"COMMIT", "00000"},
      {to + " AS 'C'", "00000"},
      {to + " AS 'c'", "08002"},
      {"DISCONNECT 'C'", "00000"},
      {"DISCONNECT DEFAULT", "00000"},
      {"DISCONNECT CURRENT", "08003"},
  };
  for (const auto& [sql, state] : refusals) {
    CAPTURE(sql);
    CHECK(state_of(agent, sql) == state);
  }
  CHECK(agent.size() == 0);
}

/// Verifies: [CLIENT The SQL-agent] An agent begun with no connection makes
/// the default one by CONNECT TO DEFAULT; what its connector throws is
/// thrown by the statement that called it.
TEST_CASE("an agent begun with none connects by its connector") {
  LIVE_OR_SKIP();
  table();
  turborel::agent agent{dial};
  CHECK_FALSE(agent.has_current());
  CHECK(agent.current_name() == std::nullopt);
  CHECK(state_of(agent, "CONNECT TO DEFAULT") == "00000");
  CHECK(value_of(agent, "SELECT COUNT(*) FROM AG") == "1");
  turborel::agent failing{
      [](const std::optional<std::string>&, const std::optional<std::string>&) -> turborel::connection {
        throw std::runtime_error{"no"};
      }};
  CHECK_THROWS_AS(failing.query("CONNECT TO DEFAULT"), std::runtime_error);
  CHECK(failing.size() == 0);
}

/// Verifies: [CLIENT The SQL-agent] A connector's failure refuses CONNECT:
/// of a server not reached, SQLSTATE 08001; of a server that refused the
/// login, 08004; a connector that made a connection over the program's
/// own stream fails as a misuse, as the agent cannot keep it; an agent is
/// moved as a value.
TEST_CASE("a connector's failure refuses CONNECT, saying why") {
  LIVE_OR_SKIP();
  table();
  turborel::agent agent{[](const std::optional<std::string>& named, const std::optional<std::string>& user) {
    if (user == "BAD") {
      turborel::mechanism wrong = turborel::mechanism::scram("not the password");
      wrong.in_the_clear();
      return turborel::connection::connect(test::live::tcp(), "ann", wrong);
    }
    if (user == "NONE") {
      return turborel::settings::from_uri("turborel://ann@127.0.0.1:1,127.0.0.1:2/?tls_mode=disable").connect();
    }
    if (user == "STREAM") {
      return std::move(test::open_scripted(test::ready({}), {}).conn);
    }
    return dial(named, user);
  }};
  CHECK(state_of(agent, "CONNECT TO '" + address() + "' AS 'A' USER 'BAD'") == "08004");
  CHECK(state_of(agent, "CONNECT TO '" + address() + "' AS 'A' USER 'NONE'") == "08001");
  CHECK(state_of(agent, "CONNECT TO '" + address() + "' AS 'A' USER 'STREAM'") == "TB910");
  CHECK(state_of(agent, "CONNECT TO '" + address() + "' AS 'A'") == "00000");
  turborel::agent other{dial};
  other = std::move(agent);
  CHECK(other.size() == 1);
  CHECK(other.current_name() == "A");
}

/// Verifies: [CLIENT The SQL-agent] A statement the agent sends to its
/// current connection gives a query's rows a batch at a time, as they
/// come; what the handler throws is thrown, and a connector's failure on
/// CONNECT refuses it as query() does [CLIENT Running statements].
TEST_CASE("the agent gives a query's rows a batch at a time") {
  LIVE_OR_SKIP();
  table();
  turborel::agent agent{dial, test::live::trusted()};
  std::size_t rows = 0;
  static_cast<void>(agent.query_batches("WITH RECURSIVE T(N) AS (VALUES CAST(1 AS INTEGER) UNION ALL SELECT N + 1 FROM "
                                        "T WHERE N < 20000) SELECT N FROM T",
                                        [&rows](const turborel::result& batch) { rows += batch.size(); }));
  CHECK(rows == 20000);
  CHECK_THROWS_AS(agent.query_batches("VALUES 1", [](const turborel::result&) { throw std::runtime_error{"stop"}; }),
                  std::runtime_error);
  static_cast<void>(agent.query("COMMIT"));
  try {
    static_cast<void>(agent.query_batches("CONNECT TO '127.0.0.1:1' AS 'C'", [](const turborel::result&) {}));
    FAIL("CONNECT to no server succeeded");
  } catch (const turborel::error& e) {
    CHECK(e.sqlstate() == "08001");
  }
}
