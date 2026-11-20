/// @file
/// @brief Live tests of pipelines [CLIENT Pipelines], as turborel-client's
/// own tests run them (R-TEST-8): requests written at once, answered in
/// order, a failure its request's own; or, skipping after an error, the
/// requests after it undone and those before kept; batches longer than a
/// window written a window at a time.

#include "support/frames.hpp"
#include "support/server.hpp"

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/error.hpp>
#include <turborel/params.hpp>
#include <turborel/pipeline.hpp>
#include <turborel/result.hpp>
#include <turborel/statement.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <string>

namespace {

/// Runs `sql`, its result dropped.
void run(turborel::connection& conn, const std::string& sql) { static_cast<void>(conn.query(sql)); }

/// How many rows PT holds, as a new connection reads them.
std::size_t keys() { return test::live::trusted().query("SELECT K FROM PT").size(); }

/// A connection to PT emptied, made by the first call.
turborel::connection emptied() {
  turborel::connection conn = test::live::trusted();
  static const bool made = [&conn] {
    run(conn, "CREATE TABLE PT (K INTEGER PRIMARY KEY, V VARCHAR(100))");
    return true;
  }();
  static_cast<void>(made);
  run(conn, "DELETE FROM PT");
  run(conn, "COMMIT");
  return conn;
}

/// Whether request `i` of `piped` failed with a SQLSTATE of class `of`.
bool failed_of(const turborel::pipeline& piped, std::size_t i, const std::string& of) {
  return piped.outcome(i) == turborel::piped::failed &&
         piped.failure_of(i).value_or(turborel::failure{}).sqlstate.starts_with(of);
}

} // namespace

/// Verifies: [CLIENT Pipelines] A pipeline's requests, queries and executions
/// of a prepared statement, are answered one outcome each, in the order queued;
/// a failure, a key met twice (class 23), is its request's alone, and the
/// requests after it run; the connection goes on after, its transaction active
/// as the last request left it [PROTOCOL Requests] [PROTOCOL Requests].
TEST_CASE("a pipeline answers each request in order") {
  LIVE_OR_SKIP();
  turborel::connection conn = emptied();
  const turborel::statement insert = conn.prepare("INSERT INTO PT VALUES (?, 'v')");
  turborel::pipeline piped{conn};
  piped.query("INSERT INTO PT VALUES (1, 'a')")
      .query("INSERT INTO PT VALUES (1, 'again')")
      .execute(insert, turborel::params::of(2))
      .query("SELECT K, V FROM PT ORDER BY K");
  CHECK(piped.size() == 4);
  piped.run();
  CHECK(piped.outcome(0) == turborel::piped::done);
  CHECK(failed_of(piped, 1, "23"));
  CHECK(piped.outcome(2) == turborel::piped::done);
  CHECK(piped.result_of(3).size() == 2);
  CHECK(piped.failure_of(0) == std::nullopt);
  CHECK(conn.in_transaction());
  run(conn, "COMMIT");
  CHECK(keys() == 2);
  CHECK_THROWS_AS(piped.run(), turborel::error);
  CHECK_THROWS_AS(piped.query("VALUES 1"), turborel::error);
  turborel::pipeline none{conn};
  none.run();
  CHECK(none.size() == 0);
}

/// Verifies: [CLIENT Pipelines] Under autocommit each request of a pipeline
/// is its own transaction, so a failed one undoes none other; skipping on
/// error, the requests after the first failure are undone and skipped,
/// those before it kept, and under autocommit the batch's work committed
/// as it ends.
TEST_CASE("a pipeline skips after an error when asked") {
  LIVE_OR_SKIP();
  turborel::connection conn = emptied();
  conn.set_autocommit(true);
  turborel::pipeline each{conn};
  each.query("INSERT INTO PT VALUES (1, 'a')").query("INSERT INTO PT VALUES (1, 'b')");
  each.query("INSERT INTO PT VALUES (2, 'c')").run();
  CHECK(failed_of(each, 1, "23"));
  CHECK(each.outcome(2) == turborel::piped::done);
  CHECK_FALSE(conn.in_transaction());
  CHECK(keys() == 2);
  turborel::pipeline skipping{conn};
  skipping.when_one_fails(turborel::on_error::skip);
  skipping.query("INSERT INTO PT VALUES (3, 'kept')").query("INSERT INTO PT VALUES (1, 'twice')");
  skipping.query("INSERT INTO PT VALUES (4, 'undone')").query("INSERT INTO PT VALUES (5, 'undone')");
  skipping.run();
  CHECK(skipping.size() == 4);
  CHECK(skipping.outcome(0) == turborel::piped::done);
  CHECK(failed_of(skipping, 1, "23"));
  CHECK(skipping.outcome(2) == turborel::piped::skipped);
  CHECK(skipping.outcome(3) == turborel::piped::skipped);
  CHECK_THROWS_AS(skipping.result_of(2), turborel::error);
  CHECK(skipping.failure_of(2) == std::nullopt);
  CHECK_FALSE(conn.in_transaction());
  CHECK(keys() == 3);
  turborel::pipeline keeps_going{conn};
  keeps_going.when_one_fails(turborel::on_error::keep_going);
  keeps_going.query("VALUES 1").run();
  CHECK(keeps_going.outcome(0) == turborel::piped::done);
}

/// Verifies: [CLIENT Pipelines] Within a transaction a skipping pipeline
/// undoes only what came after its failure, the transaction's earlier work
/// kept and still active; its first request failing skips all the rest.
TEST_CASE("a skipping pipeline keeps the transaction before it") {
  LIVE_OR_SKIP();
  turborel::connection conn = emptied();
  run(conn, "INSERT INTO PT VALUES (1, 'before')");
  turborel::pipeline skipping{conn};
  skipping.when_one_fails(turborel::on_error::skip);
  skipping.query("INSERT INTO PT VALUES (1, 'twice')").query("INSERT INTO PT VALUES (2, 'undone')").run();
  CHECK(failed_of(skipping, 0, "23"));
  CHECK(skipping.outcome(1) == turborel::piped::skipped);
  CHECK(conn.in_transaction());
  CHECK(conn.query("SELECT K FROM PT").size() == 1);
  run(conn, "COMMIT");
  CHECK(keys() == 1);
}

/// Verifies: [CLIENT Pipelines] A batch of many more bytes than a window is
/// written a window at a time, each reply read in order, and every request
/// answered: none waits on a server waiting on it.
TEST_CASE("a batch longer than a window is answered whole") {
  LIVE_OR_SKIP();
  turborel::connection conn = emptied();
  const std::string value(90, 'w');
  const std::size_t count = turborel::pipeline_window / 50;
  turborel::pipeline piped{conn};
  for (std::size_t k = 0; k < count; ++k) {
    piped.query("INSERT INTO PT VALUES (" + std::to_string(k) + ", '" + value + "')");
    piped.query("SELECT K, V FROM PT WHERE K = 0");
  }
  piped.run();
  CHECK(piped.size() == count * 2);
  bool every = true;
  for (std::size_t i = 0; i < count * 2; ++i) {
    every = every && piped.outcome(i) == turborel::piped::done;
  }
  CHECK(every);
  run(conn, "COMMIT");
  CHECK(keys() == count);
}

/// Verifies: [CLIENT Pipelines] A pipeline whose connection is lost as it
/// runs fails with SQLSTATE 08006.
TEST_CASE("a pipeline whose connection is lost fails") {
  auto [conn, stream] = test::open_scripted(test::ready({}), turborel::connect_options{}.ask_none());
  turborel::pipeline piped{conn};
  piped.query("VALUES 1").query("VALUES 2");
  try {
    piped.run();
    FAIL("the pipeline ran");
  } catch (const turborel::error& e) {
    CHECK(e.sqlstate() == "08006");
  }
  CHECK(conn.lost());
}
