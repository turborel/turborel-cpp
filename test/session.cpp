/// @file
/// @brief Tests of a connection's session: what it asks for and is told,
/// its timeouts, cancel and Terminate, live against the run's server
/// (R-TEST-8) and over scripted streams.

#include "support/frames.hpp"
#include "support/server.hpp"
#include "support/sockets.hpp"

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/error.hpp>
#include <turborel/result.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals; // NOLINT(google-build-using-namespace): chrono's literals read as the waits do.

/// A query that runs for minutes: a cross join of 100,000 rows by 100,000.
constexpr const char* slow_query = "WITH RECURSIVE T(N) AS (SELECT CAST(1 AS BIGINT) FROM (VALUES (1)) AS V(X) "
                                   "UNION ALL SELECT N + 1 FROM T WHERE N < 100000) SELECT COUNT(*) FROM T A, T B";

/// A query of 2,000 rows, more bytes than a frame sent compressed.
constexpr const char* long_query = "WITH RECURSIVE T(N) AS (SELECT CAST(1 AS BIGINT) FROM (VALUES (1)) AS V(X) "
                                   "UNION ALL SELECT N + 1 FROM T WHERE N < 2000) SELECT N FROM T";

/// The SQLCODE and SQLSTATE of what `call` throws as the driver's error.
template <typename Call>
std::string failure_of(const Call& call) {
  try {
    call();
  } catch (const turborel::error& e) {
    return std::to_string(e.sqlcode()) + " " + std::string{e.sqlstate()};
  }
  return "none";
}

/// Options asking for nothing but `capabilities`.
turborel::connect_options asking(std::initializer_list<std::string_view> capabilities) {
  turborel::connect_options options;
  options.ask_none();
  for (const std::string_view name : capabilities) {
    options.ask(name);
  }
  return options;
}

} // namespace

/// Verifies: [CLIENT What a connection asks for] A connection asks for every
/// capability but compression, and the server grants those it has, in the
/// protocol's order; an offer of none is the protocol's first form, and
/// still runs statements [PROTOCOL The Native Protocol]; a call that needs
/// a capability not granted fails before it sends anything, the connection
/// kept [PROTOCOL Capabilities].
TEST_CASE("a connection asks for every capability but compression") {
  LIVE_OR_SKIP();
  turborel::connection const every = test::live::trusted();
  const std::vector<std::string_view> granted = every.granted();
  CHECK(granted.front() == "notices");
  CHECK(every.granted(turborel::capability::declared_types));
  CHECK(every.granted(turborel::capability::parameter_modes));
  CHECK(every.granted(turborel::capability::debugger));
  CHECK_FALSE(every.granted(turborel::capability::compression));
  CHECK(granted.size() == 20);
  turborel::connection none = test::live::trusted(asking({}));
  CHECK(none.granted().empty());
  CHECK(none.query("VALUES 1").at(0).get<std::int64_t>(0) == 1);
  CHECK(failure_of([&none] { none.ping(); }) == "-919 08006");
  CHECK_FALSE(none.lost());
  CHECK(failure_of([&none] { none.reset(); }) == "-919 08006");
  CHECK(failure_of([&none] { static_cast<void>(none.query_many("VALUES 1; VALUES 2")); }) == "-919 08006");
  CHECK(none.setting("AUTHORIZATION") == std::nullopt);
}

/// Verifies: [CLIENT What a connection asks for] An offer of compression and
/// of connection attributes: the attributes are read by the server granted
/// or not, and shown in INFORMATION_SCHEMA.CONNECTION_ATTRIBUTES
/// [PROTOCOL Hello 'H']; compressed frames carry a long result both ways
/// [PROTOCOL Capabilities].
TEST_CASE("an offer of the program's own: compression and attributes") {
  LIVE_OR_SKIP();
  ZLIB_OR_SKIP();
  turborel::connect_options offer = asking({"compression"});
  offer.attribute("application", "cpp-live").compress_from(64);
  turborel::connection conn = test::live::trusted(offer);
  CHECK(conn.granted(turborel::capability::compression));
  CHECK(conn.query(long_query).size() == 2000);
  const turborel::result shown = conn.query("SELECT ATTRIBUTE_VALUE FROM INFORMATION_SCHEMA.CONNECTION_ATTRIBUTES "
                                            "WHERE ATTRIBUTE_NAME = 'application' AND ATTRIBUTE_VALUE = 'cpp-live'");
  CHECK(shown.size() == 1);
  turborel::connect_options too_many;
  CHECK(failure_of([&too_many] { too_many.attribute(std::string(65, 'n'), "v"); }) == "-919 08006");
  CHECK(failure_of([&too_many] { too_many.attribute("name", std::string(1025, 'v')); }) == "-919 08006");
  CHECK_THROWS_AS(too_many.ask(""), turborel::error);
}

/// Verifies: [PROTOCOL Status 'T'] At login the server tells each name of
/// the page's table, in its order, then the session's own settings; after
/// a statement only what it changed, which the program reads with no round
/// trip [CLIENT What the server tells].
TEST_CASE("the session's status is told at login and as it changes") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  const std::vector<std::pair<std::string_view, std::string_view>> told = conn.settings();
  const std::vector<std::string_view> table{
      "SERVER_VERSION", "AUTHORIZATION", "SCHEMA", "READ_ONLY", "SERVER_READ_ONLY", "CATALOG", "OWNER",
  };
  REQUIRE(told.size() > table.size());
  for (std::size_t i = 0; i < table.size(); ++i) {
    CHECK(told.at(i).first == table.at(i));
  }
  CHECK(conn.setting("SERVER_VERSION") == conn.server());
  CHECK(conn.setting("AUTHORIZATION") == "ANN");
  CHECK(conn.setting("READ_ONLY") == "NO");
  CHECK(conn.setting("SERVER_READ_ONLY") == "NO");
  CHECK(conn.setting("OWNER") == "YES");
  CHECK_FALSE(conn.setting("CATALOG").value_or("").empty());
  CHECK(conn.setting("SESSION.TIME_ZONE").has_value());
  CHECK(conn.setting("TRANSACTION.ISOLATION").has_value());
  static_cast<void>(conn.query("SET TIME ZONE INTERVAL '+02:00' HOUR TO MINUTE"));
  CHECK(conn.setting("SESSION.TIME_ZONE") == "+02:00");
  CHECK(conn.settings().size() == told.size());
}

/// Verifies: [CLIENT Timeouts] Connecting waits 30 seconds by default, or as
/// the program says, and a wait that runs out loses the connection (SQLSTATE
/// 08006); the read timeout is none by default, set as the program says,
/// and a read that waits past it loses the connection too.
TEST_CASE("a wait that runs out loses the connection") {
  REQUIRE(turborel::connect_within == 30s);
  if (!test::sockets::available()) {
    SKIP("no sockets for a server of the test's own on this platform");
  }
  turborel::mechanism trust = turborel::mechanism::trust();
  const test::sockets::silent_server quiet{{}};
  turborel::connect_options soon;
  soon.timeouts(200ms, 0ms);
  CHECK(failure_of([&] {
          static_cast<void>(
              turborel::connection::connect(turborel::target::tcp("127.0.0.1", quiet.port()), "ann", trust, soon));
        }) == "-919 08006");
  const test::sockets::silent_server ready_then_quiet{test::ready({})};
  turborel::connection conn =
      turborel::connection::connect(turborel::target::tcp("127.0.0.1", ready_then_quiet.port()), "ann", trust);
  CHECK(conn.read_timeout() == 0ms);
  conn.set_read_timeout(200ms);
  CHECK(conn.read_timeout() == 200ms);
  CHECK(failure_of([&conn] { static_cast<void>(conn.query("VALUES 1")); }) == "-919 08006");
  CHECK(conn.lost());
  CHECK_THROWS_AS(soon.timeouts(-1ms, 0ms), turborel::error);
}

/// Verifies: [CLIENT Timeouts] A stream of the program's own that bounds
/// itself is bounded by the connect timeout as it logs in, then by the
/// read timeout, which is set as a connection's; one that does not bound
/// itself waits as its maker set it, its read timeout refused.
TEST_CASE("a stream of the program's own is bounded as it says") {
  turborel::connect_options waits;
  waits.ask_none().timeouts(1500ms, 2500ms);
  auto bounding = std::make_unique<test::scripted>(test::ready({}), true);
  test::scripted const* kept = bounding.get();
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::open(std::move(bounding), "ANN", trust, waits);
  CHECK(kept->last_bound() == 2500ms);
  conn.set_read_timeout(700ms);
  CHECK(kept->last_bound() == 700ms);
  auto [unbound, stream] = test::open_scripted(test::ready({}), asking({}));
  CHECK(failure_of([&unbound] { unbound.set_read_timeout(700ms); }) == "-919 08006");
  CHECK_THROWS_AS(unbound.set_max_body(0), turborel::error);
}

/// Verifies: [PROTOCOL Cancel 'Z'] A statement cancelled from another
/// thread, by the key Ready gave over a connection of its own, fails with
/// SQLCODE -920, SQLSTATE TB920, and the connection goes on [CLIENT Running
/// statements] [PROTOCOL Ready 'R'].
TEST_CASE("a statement is cancelled from another thread") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  turborel::canceller canceller = conn.make_canceller();
  std::thread cancelling{[&canceller] {
    std::this_thread::sleep_for(300ms);
    canceller.cancel();
  }};
  const std::string failed = failure_of([&conn] { static_cast<void>(conn.query(slow_query)); });
  cancelling.join();
  CHECK(failed == "-920 TB920");
  CHECK(conn.query("VALUES 1").size() == 1);
  std::thread again{[&canceller] {
    std::this_thread::sleep_for(300ms);
    canceller.cancel(5s);
  }};
  const std::string failed_again = failure_of([&conn] { static_cast<void>(conn.query(slow_query)); });
  again.join();
  CHECK(failed_again == "-920 TB920");
}

/// Verifies: [PROTOCOL Terminate 'X'] Terminate ends the session, its
/// transaction committed as SESSION.COMMIT_AT_END says; a commit that fails
/// gives its failure (40002), a Notice before it read past [CLIENT
/// Connecting].
TEST_CASE("terminate commits as the session says, and tells a failing commit") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  static_cast<void>(conn.query("CREATE TABLE TD (ID INTEGER, CONSTRAINT TD_POS CHECK (ID > 0) INITIALLY DEFERRED)"));
  static_cast<void>(conn.query("COMMIT"));
  static_cast<void>(conn.query("SET SESSION.COMMIT_AT_END = 'ON'"));
  static_cast<void>(conn.query("INSERT INTO TD VALUES (-1)"));
  CHECK(failure_of([&conn] { conn.terminate(); }).ends_with("40002"));
  CHECK(conn.lost());
  const test::bytes failed_commit = test::writer{}
                                        .u32(static_cast<std::uint32_t>(-913))
                                        .text("40002")
                                        .text("a deferred check failed")
                                        .u8(0)
                                        .u8(0)
                                        .u8(0)
                                        .frame('E');
  auto [scripted, stream] = test::open_scripted(test::joined({
                                                    test::ready({"notices", "status"}),
                                                    test::frame("notice"),
                                                    test::frame("status-schema"),
                                                    failed_commit,
                                                }),
                                                asking({"notices", "status"}));
  CHECK(failure_of([&scripted] { scripted.terminate(); }).ends_with("40002"));
  CHECK(stream->written().back() == std::byte{0});
}

namespace {

/// A stream that says it bounds its waits, yet cannot: the base's bound.
class unbounded final : public turborel::byte_stream {
public:
  std::optional<std::size_t> read(std::span<std::byte> into) override {
    const std::size_t count = std::min(into.size(), login_.size() - at_);
    std::copy_n(login_.begin() + static_cast<std::ptrdiff_t>(at_), count, into.begin());
    at_ += count;
    return count;
  }
  std::optional<std::size_t> write(std::span<const std::byte> octets) override { return octets.size(); }
  void close() noexcept override {}
  [[nodiscard]] bool bounds() const noexcept override { return true; }

private:
  test::bytes login_ = test::ready({});
  std::size_t at_{};
};

/// A stream whose reads fail after the login's, or whose writes fail
/// after the Hello's, or that throws as it does.
class failing final : public turborel::byte_stream {
public:
  /// What fails, and how.
  enum class fails : std::uint8_t { read, write, read_throws, write_throws };

  explicit failing(fails how) : how_{how} {}

  std::optional<std::size_t> read(std::span<std::byte> into) override {
    if (at_ == login_.size()) {
      if (how_ == fails::read_throws) {
        throw std::runtime_error{"the read broke"};
      }
      if (how_ == fails::read) {
        return std::nullopt;
      }
    }
    const std::size_t count = std::min(into.size(), login_.size() - at_);
    std::copy_n(login_.begin() + static_cast<std::ptrdiff_t>(at_), count, into.begin());
    at_ += count;
    return count;
  }

  std::optional<std::size_t> write(std::span<const std::byte> octets) override {
    if (writes_++ > 0) {
      if (how_ == fails::write_throws) {
        throw std::runtime_error{"the write broke"};
      }
      if (how_ == fails::write) {
        return std::nullopt;
      }
    }
    return octets.size();
  }

  void close() noexcept override {}

private:
  fails how_;
  test::bytes login_ = test::ready({});
  std::size_t at_{};
  int writes_{};
};

/// A connection by trust over `stream`, asking nothing.
turborel::connection over(std::unique_ptr<turborel::byte_stream> stream) {
  turborel::mechanism trust = turborel::mechanism::trust();
  return turborel::connection::open(std::move(stream), "ANN", trust, asking({}));
}

} // namespace

/// Verifies: [CLIENT Timeouts] A stream of the program's own that says it
/// bounds its waits, yet cannot, fails the connection as it logs in
/// (08006); a wait past 2^32 - 1 ms is refused as a misuse.
TEST_CASE("a stream that cannot bound its waits fails the connection") {
  CHECK(failure_of([] { static_cast<void>(over(std::make_unique<unbounded>())); }) == "-919 08006");
  auto [conn, stream] = test::open_scripted(test::ready({}), asking({}));
  CHECK(failure_of([&conn] { conn.set_read_timeout(std::chrono::hours{24 * 50}); }) == "-910 TB910");
}

/// Verifies: [CLIENT Connecting] A stream of the program's own whose read or
/// write fails, by saying so or by throwing, loses the connection
/// (08006), and what it threw is not thrown (R-RAII-2).
TEST_CASE("a stream whose read or write fails loses the connection") {
  for (const failing::fails how :
       {failing::fails::read, failing::fails::write, failing::fails::read_throws, failing::fails::write_throws}) {
    turborel::connection conn = over(std::make_unique<failing>(how));
    CHECK(failure_of([&conn] { static_cast<void>(conn.query("VALUES 1")); }) == "-919 08006");
    CHECK(conn.lost());
  }
}

/// Verifies: [CLIENT Several servers] A connection's server is read-only as
/// SERVER_READ_ONLY last told it; one that tells it not is asked by
/// INFORMATION_SCHEMA.REPLICAS, and one that lists not itself is of a role
/// the session cannot see.
TEST_CASE("a server's role is told or asked") {
  auto [replica, replica_stream] = test::open_scripted(
      test::joined({test::frame("status-server-read-only"), test::ready({"status"})}), asking({"status"}));
  CHECK(replica.role() == turborel::server_role::read_only);
  auto [unseen, unseen_stream] = test::open_scripted(test::joined({
                                                         test::ready({}),
                                                         test::frame("description-character"),
                                                         test::frame("rows-none"),
                                                         test::frame("complete"),
                                                     }),
                                                     asking({}));
  CHECK(unseen.role() == turborel::server_role::unknown);
  CHECK_FALSE(unseen.autocommit());
}

/// Verifies: [PROTOCOL Cancel 'Z'] A cancel whose server cannot be reached
/// fails with why (08006), the connection it would cancel kept.
TEST_CASE("a cancel that cannot reach its server fails") {
  if (!test::sockets::available()) {
    SKIP("no sockets for a server of the test's own on this platform");
  }
  std::optional<test::sockets::silent_server> quiet{std::in_place, test::ready({})};
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn =
      turborel::connection::connect(turborel::target::tcp("127.0.0.1", quiet->port()), "ann", trust, asking({}));
  turborel::canceller canceller = conn.make_canceller();
  quiet.reset();
  CHECK(failure_of([&canceller] { canceller.cancel(); }).ends_with("08006"));
  CHECK(failure_of([&canceller] { canceller.cancel(200ms); }).ends_with("08006"));
}

/// Verifies: [CLIENT Connecting] A connection is opened by a URI, as
/// settings of it connect.
TEST_CASE("a connection is opened by a URI") {
  LIVE_OR_SKIP();
  turborel::connection const conn =
      turborel::connection::open("turborel://ann@127.0.0.1:" + std::to_string(test::live::port) + "/?tls_mode=disable");
  CHECK(conn.authorization() == "ANN");
  CHECK(failure_of([] { static_cast<void>(turborel::connection::open("other://ann@127.0.0.1")); }) == "-919 08001");
}
