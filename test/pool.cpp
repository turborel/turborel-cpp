/// @file
/// @brief Live tests of pools of connections [CLIENT Pools], as
/// turborel-client's own tests run them (R-TEST-8): bounded under many
/// threads, waiting at most their limit, each connection given back reset
/// to its login, one that no longer answers or has lived its time closed.

#include "support/frames.hpp"
#include "support/server.hpp"

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/error.hpp>
#include <turborel/pool.hpp>
#include <turborel/result.hpp>
#include <turborel/settings.hpp>
#include <turborel/statement.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals; // NOLINT(google-build-using-namespace): chrono's literals read as the waits do.

/// Runs `sql`, its result dropped.
void run(turborel::connection& conn, const std::string& sql) { static_cast<void>(conn.query(sql)); }

/// The settings of the pools, their connections named `name`.
turborel::settings settings_of(const std::string& name = "pooled") {
  return turborel::settings::from_uri("turborel://ann@127.0.0.1:" + std::to_string(test::live::port) +
                                      "/?tls_mode=disable&application_name=" + name);
}

/// A pool of at most `max` connections, waiting `wait`.
turborel::pool pool_of(std::size_t max, std::chrono::milliseconds wait, const std::string& name = "pooled") {
  turborel::pool_config config;
  config.max = max;
  config.max_idle = max;
  config.wait = wait;
  return turborel::pool::of(settings_of(name), config);
}

/// The table the pools' connections read, made once.
void table() {
  static const bool made = [] {
    turborel::connection conn = test::live::trusted();
    run(conn, "CREATE TABLE PL (K INTEGER)");
    run(conn, "COMMIT");
    return true;
  }();
  static_cast<void>(made);
}

/// The count a query of one count gives.
std::int64_t count_of(turborel::connection& conn, const std::string& sql) {
  return conn.query(sql).at(0).get<std::int64_t>(0);
}

/// Asserts a connection of `pool`, given back, is reset: its transaction
/// rolled back, its statements gone, its schema its user's, autocommit
/// off; and handed out again, none more made.
void given_back_reset(turborel::pool& pool, turborel::pooled conn) {
  run(*conn, "INSERT INTO PL VALUES (1)");
  static_cast<void>(conn->prepare("SELECT K FROM PL"));
  run(*conn, "SET SCHEMA 'ELSEWHERE'");
  conn->set_autocommit(true);
  CHECK(conn->in_transaction());
  {
    const turborel::pooled given_back = std::move(conn);
  }
  turborel::pooled again = pool.get();
  CHECK_FALSE(again->in_transaction());
  CHECK_FALSE(again->autocommit());
  CHECK(count_of(*again, "SELECT COUNT(*) FROM PL") == 0);
  const turborel::statement fresh = again->prepare("SELECT K FROM PL");
  CHECK(again->execute(fresh).empty());
  run(*again, "ROLLBACK");
  CHECK(pool.made() == 1);
  CHECK(pool.idle() == 0);
}

} // namespace

/// Verifies: [CLIENT Pools] Eight threads borrowing from a pool of three,
/// again and again, each wait for a connection to come back; never more
/// than three are made, each borrower's statements run, and as the threads
/// end every connection made is idle.
TEST_CASE("a pool bounds its connections under threads") {
  LIVE_OR_SKIP();
  table();
  turborel::pool pool = pool_of(3, 30s);
  std::atomic<std::size_t> peak{0};
  std::atomic<bool> fine{true};
  std::vector<std::thread> threads;
  threads.reserve(8);
  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([&] {
      for (int round = 0; round < 5; ++round) {
        try {
          turborel::pooled conn = pool.get();
          const std::size_t made = pool.made();
          std::size_t seen = peak.load();
          while (made > seen && !peak.compare_exchange_weak(seen, made)) {
          }
          fine = fine && count_of(*conn, "SELECT COUNT(*) FROM PL") == 0;
        } catch (const turborel::error&) {
          fine = false;
        }
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  CHECK(fine);
  CHECK(peak <= 3);
  CHECK(pool.made() <= 3);
  CHECK(pool.made() == pool.idle());
}

/// Verifies: [CLIENT Pools] A pool whose every connection is handed out
/// waits its limit, then fails (SQLSTATE 08001); the connection given back
/// is handed out again, none more made.
TEST_CASE("an exhausted pool waits its limit") {
  LIVE_OR_SKIP();
  turborel::pool pool = pool_of(1, 50ms);
  {
    turborel::pooled held = pool.get();
    CHECK(held->query("VALUES 1").size() == 1);
    try {
      static_cast<void>(pool.get());
      FAIL("the pool handed out a second connection");
    } catch (const turborel::error& e) {
      CHECK(e.sqlstate() == "08001");
      CHECK(e.message().find("no connection of the pool's 1 came back within 50ms") != std::string_view::npos);
    }
  }
  const turborel::pooled again = pool.get();
  CHECK(pool.made() == 1);
  CHECK(pool.idle() == 0);
}

/// Verifies: [CLIENT Pools] A connection given back is reset to its login,
/// by Reset of a server that grants it: its transaction rolled back, the
/// rows it wrote gone; its prepared statements destroyed, so the next
/// borrower prepares afresh, more of them than statements would destroy
/// kept the connection; its schema its user's again, autocommit off.
TEST_CASE("a connection given back is reset") {
  LIVE_OR_SKIP();
  table();
  turborel::pool pool = pool_of(1, 30s);
  turborel::pooled conn = pool.get();
  for (std::size_t i = 0; i <= turborel::pool_reset_most_statements; ++i) {
    static_cast<void>(conn->prepare("VALUES 1"));
  }
  given_back_reset(pool, std::move(conn));
}

/// Verifies: [CLIENT Pools] Of a server that grants neither Ping nor Reset,
/// a connection is checked by ROLLBACK and reset by statements, to the
/// same login, and the program's statements after; with too many
/// statements left prepared, it is closed rather than kept; a pool made of
/// a function connects by it, and what it throws is thrown by the get.
TEST_CASE("a connection granted nothing is reset by statements") {
  LIVE_OR_SKIP();
  table();
  turborel::pool_config config;
  config.max = 1;
  config.max_idle = 1;
  config.reset = {"SET TIME ZONE INTERVAL '+03:00' HOUR TO MINUTE"};
  turborel::pool pool = turborel::pool::of(
      [] {
        turborel::connect_options none;
        none.ask_none();
        return test::live::trusted(none);
      },
      config);
  turborel::pooled conn = pool.get();
  CHECK(conn->granted().empty());
  given_back_reset(pool, std::move(conn));
  turborel::pooled many = pool.get();
  CHECK(many->query("VALUES CURRENT_TIME").at(0).at(0).to_string().ends_with("+03:00"));
  for (std::size_t i = 0; i <= turborel::pool_reset_most_statements; ++i) {
    static_cast<void>(many->prepare("VALUES 1"));
  }
  {
    const turborel::pooled given_back = std::move(many);
  }
  CHECK(pool.made() == 0);
  CHECK(pool.idle() == 0);
  turborel::pool failing = turborel::pool::of([]() -> turborel::connection { throw std::runtime_error{"no"}; });
  CHECK_THROWS_AS(failing.get(), std::runtime_error);
  turborel::pool streamed = turborel::pool::of([] { return std::move(test::open_scripted(test::ready({}), {}).conn); });
  CHECK_THROWS_AS(streamed.get(), turborel::error);
}

/// Verifies: [CLIENT Pools] An idle connection its server ended fails its
/// check as it is handed out, and is closed; a new one is made in its
/// place.
TEST_CASE("a connection that no longer answers is replaced") {
  LIVE_OR_SKIP();
  turborel::pool pool = pool_of(2, 30s, "pooled-replaced");
  {
    const turborel::pooled conn = pool.get();
  }
  turborel::connection owner = test::live::trusted();
  const std::int64_t id =
      count_of(owner, "SELECT ID FROM INFORMATION_SCHEMA.CONNECTIONS WHERE APPLICATION = 'pooled-replaced'");
  REQUIRE(id > 0);
  run(owner, "END CONNECTION " + std::to_string(id));
  run(owner, "ROLLBACK");
  std::this_thread::sleep_for(100ms);
  turborel::pooled conn = pool.get();
  run(*conn, "ROLLBACK");
  CHECK(pool.made() == 1);
  CHECK(pool.idle() == 0);
}

/// Verifies: [CLIENT Pools] A connection that has lived its time is closed
/// as it comes back; one idle past its limit is closed rather than handed
/// out, another made.
TEST_CASE("connections live and idle within their limits") {
  LIVE_OR_SKIP();
  turborel::pool_config lived_config;
  lived_config.max_lifetime = 1ms;
  turborel::pool lived = turborel::pool::of(settings_of(), lived_config);
  {
    const turborel::pooled conn = lived.get();
    std::this_thread::sleep_for(5ms);
  }
  CHECK(lived.made() == 0);
  CHECK(lived.idle() == 0);
  turborel::pool_config resting_config;
  resting_config.idle_timeout = 1ms;
  resting_config.check = false;
  turborel::pool resting = turborel::pool::of(settings_of(), resting_config);
  {
    const turborel::pooled conn = resting.get();
  }
  CHECK(resting.made() == 1);
  CHECK(resting.idle() == 1);
  std::this_thread::sleep_for(5ms);
  {
    const turborel::pooled conn = resting.get();
  }
  CHECK(resting.made() == 1);
  CHECK(resting.idle() == 1);
}

/// Verifies: [CLIENT Pools] A wait past 2^32 - 1 ms, or below none, is
/// refused as a misuse as the pool is made; a connection handed out is
/// moved as a value, one moved onto given back first.
TEST_CASE("a pool's limits are checked, and its handles moved") {
  LIVE_OR_SKIP();
  turborel::pool_config wrong;
  wrong.wait = -1ms;
  CHECK_THROWS_AS(turborel::pool::of(settings_of(), wrong), turborel::error);
  turborel::pool pool = pool_of(2, 30s);
  turborel::pooled one = pool.get();
  turborel::pooled two = pool.get();
  CHECK(pool.idle() == 0);
  one = std::move(two);
  CHECK(pool.idle() == 1);
  CHECK(one->query("VALUES 1").size() == 1);
}

/// Verifies: [CLIENT Pools] A pool's defaults: at most 10 made and 10 idle,
/// idle 10 minutes, living an hour, a get waiting 30 seconds, each idle
/// connection checked, no statements of the program's at reset; a copy of
/// a pool is the same pool; one given back beyond `max_idle` is closed.
TEST_CASE("a pool's defaults, its copies, and its idle bound") {
  const turborel::pool_config defaults;
  CHECK(defaults.max == 10);
  CHECK(defaults.max_idle == 10);
  CHECK(defaults.idle_timeout == 10min);
  CHECK(defaults.max_lifetime == 1h);
  CHECK(defaults.wait == 30s);
  CHECK(defaults.check);
  CHECK(defaults.reset.empty());
  LIVE_OR_SKIP();
  turborel::pool_config none_idle;
  none_idle.max_idle = 0;
  turborel::pool pool = turborel::pool::of(settings_of(), none_idle);
  const turborel::pool same = pool;
  {
    const turborel::pooled conn = pool.get();
  }
  CHECK(same.made() == 0);
  CHECK(same.idle() == 0);
  const turborel::pool kept = pool_of(2, 30s);
  turborel::pool copy = kept;
  {
    const turborel::pooled conn = copy.get();
  }
  CHECK(kept.made() == 1);
  CHECK(kept.idle() == 1);
}
