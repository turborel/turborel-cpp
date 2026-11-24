/// @file
/// @brief The routine debugger *(extension)*: its requests written and its
/// replies and events read as the sample frames lay them out, and two
/// connections against a live server, a target whose CALL runs on a thread
/// of its own and a debugger that drives it [CLIENT Debugging routines]
/// [PROTOCOL Debug 'V'].

#include "support/frames.hpp"
#include "support/server.hpp"

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/debugger.hpp>
#include <turborel/error.hpp>
#include <turborel/value.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace {

/// Options asking for `capabilities` alone.
turborel::connect_options asking(std::initializer_list<std::string_view> capabilities) {
  turborel::connect_options options;
  options.ask_none();
  for (const std::string_view name : capabilities) {
    options.ask(name);
  }
  return options;
}

/// The SQLCODE and SQLSTATE of what `call` threw, or "none".
template <typename Call>
std::string failure_of(const Call& call) {
  try {
    call();
  } catch (const turborel::error& e) {
    return std::to_string(e.sqlcode()) + " " + std::string{e.sqlstate()};
  }
  return "none";
}

/// Whether the client wrote the sample `name` last, from `at`.
bool wrote(const test::bytes& written, std::size_t at, const std::string& name) {
  const test::bytes& sample = test::frame(name);
  return written.size() == at + sample.size() &&
         std::equal(sample.begin(), sample.end(), written.begin() + static_cast<std::ptrdiff_t>(at));
}

/// The exact number a value holds, as text.
std::string exact(const turborel::value& held) { return held.as_decimal().to_string(); }

/// The innermost frame of a pause.
turborel::debug_frame innermost(const turborel::debug_pause& pause) {
  std::vector<turborel::debug_frame> frames = pause.frames();
  REQUIRE_FALSE(frames.empty());
  return std::move(frames.back());
}

/// The pause the samples debugged-stack and event-paused hold: at
/// breakpoint 1, one frame of ANN.RAISE_PAY at line 4, column 3, PCT 10,
/// N null, a cursor C closed.
void check_sample_pause(const turborel::debug_pause& pause) {
  CHECK(pause.at_breakpoint());
  CHECK(pause.breakpoint() == 1U);
  const std::vector<turborel::debug_frame> frames = pause.frames();
  REQUIRE(frames.size() == 1);
  const turborel::debug_frame& frame = frames.front();
  REQUIRE(frame.origin.has_value());
  CHECK(frame.origin->kind == turborel::routine_kind::procedure);
  CHECK(frame.origin->schema == "ANN");
  CHECK(frame.origin->name == "RAISE_PAY");
  CHECK(frame.visible);
  CHECK(frame.line == 4U);
  CHECK(frame.column == 3U);
  REQUIRE(frame.parameters.size() == 1);
  CHECK(frame.parameters.front().name == "PCT");
  CHECK(exact(frame.parameters.front().data) == "10");
  REQUIRE(frame.variables.size() == 1);
  CHECK(frame.variables.front().name == "N");
  CHECK(frame.variables.front().data.is_null());
  REQUIRE(frame.cursors.size() == 1);
  CHECK(frame.cursors.front().name == "C");
  CHECK_FALSE(frame.cursors.front().open);
}

} // namespace

/// Verifies: [PROTOCOL Debug 'V'] Granted `debugger`, each Debug request is
/// written as its sample is, and each Debugged reply read as its sample
/// says: identify and a connection ID, attach, break with a condition and
/// a breakpoint, breakpoints, clear, a stack of none while the target runs
/// and a stack of a pause read whole, resume, interrupt, detach, the
/// replies of no data each a Complete [PROTOCOL Debugged 'V'] [PROTOCOL
/// Capabilities] [PROTOCOL Sample frames].
TEST_CASE("the debugger's requests and replies are the samples'") {
  auto [conn, stream] = test::open_scripted(test::joined({
                                                test::ready({"debugger"}),
                                                test::frame("debugged-connection"),
                                                test::complete(),
                                                test::frame("debugged-breakpoint"),
                                                test::frame("debugged-breakpoints"),
                                                test::complete(),
                                                test::frame("debugged-running"),
                                                test::frame("debugged-stack"),
                                                test::complete(),
                                                test::complete(),
                                                test::complete(),
                                            }),
                                            asking({"debugger"}));
  CHECK(conn.granted(turborel::capability::debugger));
  std::size_t at = stream->written().size();
  CHECK(conn.debug_id() == 2U);
  CHECK(wrote(stream->written(), at, "debug-identify"));
  at = stream->written().size();
  turborel::debugger debugging{conn, 2};
  CHECK(debugging.attached());
  CHECK(wrote(stream->written(), at, "debug-attach"));
  at = stream->written().size();
  const turborel::breakpoint set = debugging.set_breakpoint(turborel::breakpoint_at{
      .kind = turborel::routine_kind::procedure,
      .schema = "ANN",
      .name = "RAISE_PAY",
      .line = 4,
      .condition = "PCT > 5",
  });
  CHECK(wrote(stream->written(), at, "debug-break"));
  CHECK(set.number == 1U);
  CHECK(set.schema == "ANN");
  CHECK(set.name == "RAISE_PAY");
  CHECK(set.line == 4U);
  CHECK_FALSE(set.condition.has_value());
  at = stream->written().size();
  CHECK(debugging.breakpoints().size() == 1);
  CHECK(wrote(stream->written(), at, "debug-breakpoints"));
  at = stream->written().size();
  debugging.clear(1);
  CHECK(wrote(stream->written(), at, "debug-clear"));
  at = stream->written().size();
  CHECK_FALSE(debugging.stack().has_value());
  CHECK(wrote(stream->written(), at, "debug-stack"));
  const std::optional<turborel::debug_pause> pause = debugging.stack();
  REQUIRE(pause.has_value());
  check_sample_pause(*pause);
  at = stream->written().size();
  debugging.resume(turborel::debug_action::step_over);
  CHECK(wrote(stream->written(), at, "debug-resume"));
  at = stream->written().size();
  debugging.interrupt();
  CHECK(wrote(stream->written(), at, "debug-interrupt"));
  at = stream->written().size();
  debugging.detach();
  CHECK_FALSE(debugging.attached());
  CHECK(wrote(stream->written(), at, "debug-detach"));
}

/// Verifies: [PROTOCOL Event 'W'] An Event is read wherever a reply is read,
/// and never ends the request it came before: kept, it is given by the
/// next wait, as the samples say, paused with the pause read whole, timed
/// out, cancelled and ended; a wait with a time over the program's own
/// stream, which its maker bounds, fails (SQLSTATE 08006); without
/// `debugger`, a Debug is refused before it is sent, the connection kept
/// [PROTOCOL Debug 'V'] [PROTOCOL Sample frames].
TEST_CASE("the debugger's events are kept until waited for") {
  auto [conn, stream] = test::open_scripted(test::joined({
                                                test::ready({"debugger"}),
                                                test::complete(),
                                                test::frame("event-paused"),
                                                test::frame("event-timed-out"),
                                                test::frame("debugged-breakpoints"),
                                                test::frame("event-cancelled"),
                                                test::frame("event-ended"),
                                                test::complete(),
                                            }),
                                            asking({"debugger"}));
  turborel::debugger debugging{conn, 2};
  CHECK(debugging.breakpoints().size() == 1);
  const turborel::debug_event paused = debugging.wait();
  CHECK(paused.said == turborel::debug_said::paused);
  REQUIRE(paused.pause.has_value());
  check_sample_pause(*paused.pause);
  CHECK(debugging.wait().said == turborel::debug_said::timed_out);
  CHECK(debugging.wait().said == turborel::debug_said::cancelled);
  const turborel::debug_event ended = debugging.wait();
  CHECK(ended.said == turborel::debug_said::ended);
  CHECK_FALSE(ended.pause.has_value());
  CHECK(failure_of([&debugging] { static_cast<void>(debugging.wait(10ms)); }) == "-919 08006");
  auto [bare, written] = test::open_scripted(test::ready({}), asking({}));
  const std::size_t before = written->written().size();
  CHECK(failure_of([&bare] { static_cast<void>(bare.debug_id()); }) == "-919 08006");
  CHECK(written->written().size() == before);
  CHECK_FALSE(bare.lost());
}

namespace {

/// The procedure debugged: line 4 its UPDATE, line 5 its SET, a cursor C
/// declared on line 4's left.
constexpr std::string_view raise_pay = "CREATE PROCEDURE RAISE_PAY(IN PCT INTEGER) MODIFIES SQL DATA\n"
                                       "BEGIN\n"
                                       "  DECLARE N INTEGER DEFAULT 0;\n"
                                       "  UPDATE DSTAFF SET SALARY = SALARY * (100 + PCT) / 100;\n"
                                       "  SET N = (SELECT COUNT(*) FROM DSTAFF);\n"
                                       "END";

/// A procedure that runs long: a loop of a billion steps.
constexpr std::string_view loops = "CREATE PROCEDURE LOOPS() MODIFIES SQL DATA\n"
                                   "BEGIN\n"
                                   "  DECLARE I INTEGER DEFAULT 0;\n"
                                   "  WHILE I < 1000000000 DO SET I = I + 1; END WHILE;\n"
                                   "END";

/// The longest a debugger waits for an event in these tests.
constexpr std::chrono::milliseconds patience = 10s;

/// Makes the tables and procedures the live tests debug, once.
void routines(turborel::connection& conn) {
  if (conn.query("SELECT 1 FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 'DSTAFF'").empty()) {
    static_cast<void>(conn.query("CREATE TABLE DSTAFF (ID INTEGER PRIMARY KEY, SALARY INTEGER)"));
    static_cast<void>(conn.query("INSERT INTO DSTAFF VALUES (1, 50000), (2, 60000)"));
    static_cast<void>(conn.query(std::string{raise_pay}));
    static_cast<void>(conn.query(std::string{loops}));
    static_cast<void>(conn.query("COMMIT"));
  }
}

/// The next event, which must come in time and be a pause.
turborel::debug_pause paused(turborel::debugger& debugging) {
  std::optional<turborel::debug_event> event = debugging.wait(patience);
  REQUIRE(event.has_value());
  REQUIRE(event->said == turborel::debug_said::paused);
  REQUIRE(event->pause.has_value());
  return std::move(*event->pause);
}

} // namespace

/// Verifies: [CLIENT Debugging routines] A debugger attached to the target's
/// session by its ID sets a breakpoint at a line of a procedure; the
/// target's CALL pauses there, and the debugger, waiting, is given the
/// pause: the breakpoint, and the procedure's frame at its line and
/// column, its parameter
/// and its variable with their types and values; the stack again; a step
/// over to the next statement, a pause of a step; on to the CALL's end;
/// the breakpoints listed and cleared; detached as the debugger goes
/// [PROTOCOL Event 'W'] [PROTOCOL Debugged 'V'].
TEST_CASE("a debugger drives a paused procedure") {
  LIVE_OR_SKIP();
  turborel::connection target = test::live::trusted();
  turborel::connection conn = test::live::trusted();
  routines(target);
  CHECK(conn.granted(turborel::capability::debugger));
  std::uint32_t number = 0;
  {
    turborel::debugger debugging{conn, target.debug_id()};
    const turborel::breakpoint set = debugging.set_breakpoint(turborel::breakpoint_at{.name = "RAISE_PAY", .line = 4});
    number = set.number;
    CHECK(set.schema == "ANN");
    CHECK(set.line == 4U);
    CHECK_FALSE(set.condition.has_value());
    CHECK_FALSE(debugging.stack().has_value());
    std::jthread call{[&target] { static_cast<void>(target.query("CALL RAISE_PAY(10)")); }};
    const turborel::debug_pause at_break = paused(debugging);
    CHECK(at_break.breakpoint() == number);
    const turborel::debug_frame top = innermost(at_break);
    REQUIRE(top.origin.has_value());
    CHECK(top.origin->name == "RAISE_PAY");
    CHECK(top.origin->kind == turborel::routine_kind::procedure);
    CHECK(top.visible);
    CHECK(top.line == 4U);
    CHECK(top.column == 3U);
    CHECK(top.place.has_value());
    REQUIRE(top.parameters.size() == 1);
    CHECK(top.parameters.front().name == "PCT");
    CHECK(top.parameters.front().type == "INTEGER");
    CHECK(exact(top.parameters.front().data) == "10");
    REQUIRE(top.variables.size() == 1);
    CHECK(exact(top.variables.front().data) == "0");
    const std::optional<turborel::debug_pause> again = debugging.stack();
    REQUIRE(again.has_value());
    CHECK(innermost(*again).line == 4U);
    debugging.resume(turborel::debug_action::step_over);
    const turborel::debug_pause stepped = paused(debugging);
    CHECK_FALSE(stepped.at_breakpoint());
    CHECK_FALSE(stepped.breakpoint().has_value());
    CHECK(innermost(stepped).line == 5U);
    REQUIRE(debugging.breakpoints().size() == 1);
    debugging.resume(turborel::debug_action::to_breakpoint);
    call.join();
    debugging.clear(number);
    CHECK(debugging.breakpoints().empty());
    static_cast<void>(target.query("ROLLBACK"));
  }
}

/// Verifies: [CLIENT Debugging routines] A running target interrupted
/// pauses at its next statement, a pause of a step; its statement then
/// cancelled from another connection, the debugger is told it was
/// cancelled, and the target's CALL fails as cancelled (SQLCODE -920); a
/// debugger is moved as a value, the one moved from detached, the one
/// moved over detached from its own target [PROTOCOL Event 'W'].
TEST_CASE("a running target is interrupted and cancelled") {
  LIVE_OR_SKIP();
  turborel::connection target = test::live::trusted();
  turborel::connection conn = test::live::trusted();
  routines(target);
  turborel::debugger first{conn, target.debug_id()};
  turborel::debugger debugging{std::move(first)};
  // NOLINTNEXTLINE(bugprone-use-after-move): read on purpose, to show it detached.
  CHECK_FALSE(first.attached());
  CHECK(debugging.attached());
  turborel::canceller canceller = target.make_canceller();
  std::string failed;
  std::jthread call{
      [&target, &failed] { failed = failure_of([&target] { static_cast<void>(target.query("CALL LOOPS()")); }); }};
  std::this_thread::sleep_for(300ms);
  debugging.interrupt();
  const turborel::debug_pause interrupted = paused(debugging);
  CHECK_FALSE(interrupted.at_breakpoint());
  const turborel::debug_frame loop = innermost(interrupted);
  REQUIRE(loop.origin.has_value());
  CHECK(loop.origin->name == "LOOPS");
  REQUIRE_FALSE(loop.variables.empty());
  CHECK(loop.variables.front().name == "I");
  canceller.cancel();
  const std::optional<turborel::debug_event> cancelled = debugging.wait(patience);
  REQUIRE(cancelled.has_value());
  CHECK(cancelled->said == turborel::debug_said::cancelled);
  call.join();
  CHECK(failed.starts_with("-920"));
  static_cast<void>(target.query("ROLLBACK"));
  turborel::connection spare = test::live::trusted();
  turborel::connection spare_target = test::live::trusted();
  turborel::debugger other{spare, spare_target.debug_id()};
  other = std::move(debugging);
  CHECK(other.attached());
  const turborel::debugger after{spare, spare_target.debug_id()};
  CHECK(after.attached());
}

/// Verifies: [PROTOCOL Debug 'V'] The refusals keep the session: an action
/// of a debugger attached to none, or a second attach, SQLSTATE TBD01
/// (SQLCODE -910); a wait whose time runs out, no event, the connection
/// kept; an ID of no connection, or of another user's, 08003; a breakpoint
/// in a routine the user neither owns nor holds DEBUG on, 42000 (SQLCODE
/// -104), set once DEBUG is granted, its condition given back; a debugger
/// whose connection was lost goes quietly [CLIENT Debugging routines].
TEST_CASE("the debugger's refusals keep the session") {
  LIVE_OR_SKIP();
  turborel::connection owner = test::live::trusted();
  turborel::connection conn = test::live::trusted();
  routines(owner);
  test::live::user("BOB", "bob secret");
  CHECK(failure_of([&conn] { const turborel::debugger none{conn, 999999}; }) == "-919 08003");
  turborel::debugger debugging{conn, owner.debug_id()};
  CHECK(failure_of([&conn, &owner] { const turborel::debugger twice{conn, owner.debug_id()}; }) == "-910 TBD01");
  CHECK(failure_of([&debugging] { debugging.resume(turborel::debug_action::to_breakpoint); }) == "-910 TBD01");
  CHECK_FALSE(debugging.wait(100ms).has_value());
  CHECK_FALSE(conn.lost());
  conn.ping();
  turborel::connection bob = test::live::trusted();
  turborel::mechanism trust = turborel::mechanism::trust();
  bob.change_user("bob", trust);
  turborel::connection bob_target = test::live::trusted();
  bob_target.change_user("bob", trust);
  CHECK(failure_of([&bob, &owner] { const turborel::debugger others{bob, owner.debug_id()}; }) == "-919 08003");
  turborel::debugger bobs{bob, bob_target.debug_id()};
  const turborel::breakpoint_at guarded{.schema = "ANN", .name = "RAISE_PAY", .line = 4, .condition = "PCT > 5"};
  CHECK(failure_of([&bobs, &guarded] { static_cast<void>(bobs.set_breakpoint(guarded)); }) == "-104 42000");
  static_cast<void>(owner.query("GRANT DEBUG ON PROCEDURE ANN.RAISE_PAY TO BOB"));
  static_cast<void>(owner.query("COMMIT"));
  CHECK(bobs.set_breakpoint(guarded).condition == "PCT > 5");
  bobs.detach();
  CHECK(failure_of([&bobs] { bobs.interrupt(); }) == "-910 TBD01");
  const turborel::debugger lost{bob, bob_target.debug_id()};
  bob.terminate();
  CHECK(bob.lost());
}
