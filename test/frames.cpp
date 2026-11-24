/// @file
/// @brief Tests of frames read and written through the driver, over a
/// stream the test scripts (RULES.md D1): the frame header, compressed
/// frames, fields, the tail, failures, and the worked example, each read
/// by turborel-c's codec and seen through the C++ API.

#include "support/frames.hpp"

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/cursor.hpp>
#include <turborel/error.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/statement.hpp>
#include <turborel/value.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

/// Options asking for `capabilities` alone.
turborel::connect_options asking(std::initializer_list<std::string_view> capabilities) {
  turborel::connect_options options;
  options.ask_none();
  for (const std::string_view capability : capabilities) {
    options.ask(capability);
  }
  return options;
}

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

/// The Rows reply of the worked example: one row of the exact number 1.
test::bytes rows_of_one() { return test::writer{}.u32(1).u32(1).u8(2).u64(1).u64(0).u32(0).frame('B'); }

/// A frame of `kind`, a length that may not be its body's, and `body`.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): a frame's header, its kind then its length, in the protocol's order.
test::bytes raw_frame(char kind, std::uint32_t length, std::initializer_list<std::uint8_t> body) {
  test::bytes frame{static_cast<std::byte>(kind)};
  for (unsigned i = 0; i < 4; ++i) {
    frame.push_back(static_cast<std::byte>(length >> (8U * i)));
  }
  for (const std::uint8_t octet : body) {
    frame.push_back(std::byte{octet});
  }
  return frame;
}

} // namespace

/// Verifies: [PROTOCOL A worked example] The Hello of trust as ANN asking
/// for ping is its 41 bytes; Ping and Pong are a header alone; the Query
/// of VALUES 1 with ends set is its bytes; the Rows of the exact number 1
/// is read as 1; the Complete with an empty tail says no transaction is
/// active.
TEST_CASE("the worked example, byte for byte, through the driver") {
  CHECK(rows_of_one() ==
        test::writer{}
            .u32(1)
            .u32(1)
            .u8(2)
            .octets(test::bytes{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
                                std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
                                std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}})
            .u32(0)
            .frame('B'));
  const test::bytes description = test::writer{}.u32(1).u8(1).text("1").u8(2).u32(1).u32(0).u8(1).frame('D');
  auto [conn, stream] = test::open_scripted(
      test::joined({test::ready({"ping"}), test::frame("pong"), description, rows_of_one(), test::frame("complete")}),
      asking({"ping"}));
  CHECK(stream->written() == test::frame("hello-ping"));
  CHECK(stream->written().size() == 41);
  conn.ping();
  conn.set_autocommit(true);
  const turborel::result one = conn.query("VALUES 1");
  CHECK(one.at(0).get<std::int64_t>(0) == 1);
  CHECK_FALSE(conn.in_transaction());
  CHECK(stream->written() == test::joined({test::frame("hello-ping"), test::frame("ping"), test::frame("query")}));
}

/// Verifies: [PROTOCOL Frames] A frame is a kind, a u32 length and its
/// body; a kind the reader does not know, a body cut short, bytes left
/// after the last field, or a reply longer than the program takes fail
/// the connection with SQLCODE -919, SQLSTATE 08006, the connection lost.
TEST_CASE("a malformed frame fails the connection") {
  const auto failing = [](const test::bytes& reply, std::uint32_t max_body = 16777216) {
    auto [conn, stream] = test::open_scripted(test::joined({test::ready({}), reply}), asking({}));
    conn.set_max_body(max_body);
    const std::string failed = failure_of([&conn] { static_cast<void>(conn.query("VALUES 1")); });
    CHECK(conn.lost());
    return failed;
  };
  CHECK(failing(raw_frame('?', 0, {})) == "-919 08006");
  CHECK(failing(raw_frame('K', 30, {0, 0, 0, 0})) == "-919 08006");
  CHECK(failing(test::joined({test::writer{}.u32(0).text("00000").u8(0).u8(0).frame('K')})) == "-919 08006");
  CHECK(failing(test::frame("complete-every"), 16) == "-919 08006");
}

/// Verifies: [PROTOCOL Values and fields] A flag other than 0 or 1, and a
/// count larger than what follows, fail the connection; a value whose body
/// cannot be taken, in a frame read whole, fails its request alone with
/// that value's error, not of class 08, the rest of its replies read past,
/// and the connection goes on [CLIENT Running statements].
TEST_CASE("a malformed field fails, a value's failure is its request's") {
  for (const test::bytes& reply :
       {test::writer{}.u32(0).text("00000").u8(2).frame('K'), test::writer{}.u32(0xFFFF'FFFFU).frame('B')}) {
    auto [conn, stream] =
        test::open_scripted(test::joined({test::ready({}), test::frame("description"), reply}), asking({}));
    CHECK(failure_of([&conn] { static_cast<void>(conn.query("VALUES 1")); }) == "-919 08006");
    CHECK(conn.lost());
  }
  const test::bytes lob_type_as_value = test::writer{}.u32(1).u32(1).u8(18).frame('B');
  auto [conn, stream] = test::open_scripted(test::joined({
                                                test::ready({}),
                                                test::frame("description-character"),
                                                lob_type_as_value,
                                                test::frame("complete"),
                                                test::frame("complete"),
                                            }),
                                            asking({}));
  CHECK(failure_of([&conn] { static_cast<void>(conn.query("VALUES 1")); }) == "-916 TB916");
  CHECK_FALSE(conn.lost());
  CHECK(conn.query("COMMIT").sqlstate() == "00000");
}

/// Verifies: [PROTOCOL Compressed frames] Granted `compression`, a frame
/// of kind z is inflated to the frame it holds, 64 rows; one received when
/// compression was not granted, one holding another z, or one that
/// inflates to another length fail the connection; a frame longer than the
/// client's threshold is sent compressed, a shorter one as it is.
TEST_CASE("compressed frames are inflated and sent past the threshold") {
  ZLIB_OR_SKIP();
  turborel::connect_options compressed = asking({"compression"});
  compressed.compress_from(1024);
  auto [conn, stream] = test::open_scripted(test::joined({
                                                test::ready({"compression"}),
                                                test::frame("description"),
                                                test::frame("rows-compressed"),
                                                test::frame("complete"),
                                            }),
                                            compressed);
  const turborel::result rows = conn.query("SELECT K, V FROM T");
  CHECK(rows.size() == 64);
  CHECK(rows.at(63).get<std::int64_t>(0) == 63);
  const test::bytes& written = stream->written();
  const std::size_t hello = written.size();
  conn.set_autocommit(true);
  CHECK_THROWS(conn.query("SELECT '" + std::string(2000, 'x') + "' FROM T"));
  CHECK(written.at(hello) == std::byte{'z'});
  CHECK(written.at(hello + 5) == std::byte{'Q'});
  const test::bytes sample = test::frame("query-compressed");
  CHECK(std::equal(sample.begin() + 5, sample.begin() + 10, written.begin() + static_cast<std::ptrdiff_t>(hello) + 5));
  test::bytes inner_z = test::frame("rows-compressed");
  inner_z.at(5) = std::byte{'z'};
  test::bytes other_length = test::frame("rows-compressed");
  other_length.at(6) = std::byte{0x83};
  for (const test::bytes& reply : {inner_z, other_length}) {
    auto [failing, unused] = test::open_scripted(
        test::joined({test::ready({"compression"}), test::frame("description"), reply}), compressed);
    CHECK(failure_of([&failing] { static_cast<void>(failing.query("VALUES 1")); }) == "-919 08006");
  }
  auto [ungranted, unused] = test::open_scripted(
      test::joined({test::ready({}), test::frame("description"), test::frame("rows-compressed")}), asking({}));
  CHECK(failure_of([&ungranted] { static_cast<void>(ungranted.query("VALUES 1")); }) == "-919 08006");
}

/// Verifies: [PROTOCOL The tail] Complete's tags 4, 6, 16 and 18 are read
/// as the commit's position, the rows counted, the values generated and
/// the server's time; tags out of order fail the connection; a Complete of
/// an empty tail is its first form.
TEST_CASE("a completion's tail is read as the program sees it") {
  auto [conn, stream] =
      test::open_scripted(test::joined({
                              test::ready({"commit-position", "multi-statement", "row-counts", "generated-values"}),
                              test::frame("complete-every"),
                          }),
                          asking({"commit-position", "multi-statement", "row-counts", "generated-values"}));
  static_cast<void>(conn.query("INSERT INTO T VALUES (1)"));
  CHECK(conn.commit_position() == 4711);
  CHECK(conn.rows_affected() == 3);
  CHECK(conn.generated() == std::vector<std::string_view>{"7", "8"});
  const std::optional<turborel::timing> time = conn.server_time();
  REQUIRE(time.has_value());
  CHECK(time.value_or(turborel::timing{}).total == std::chrono::microseconds{1500});
  CHECK(time.value_or(turborel::timing{}).queued == std::chrono::microseconds{20});
  CHECK(time.value_or(turborel::timing{}).locks == std::chrono::microseconds{300});
  const test::bytes backwards = test::writer{}.u32(0).text("00000").u8(0).u8(6).u64(1).u8(4).u64(2).frame('K');
  auto [failing, unused] =
      test::open_scripted(test::joined({test::ready({"row-counts", "commit-position"}), backwards}),
                          asking({"row-counts", "commit-position"}));
  CHECK(failure_of([&failing] { static_cast<void>(failing.query("COMMIT")); }) == "-919 08006");
  auto [warned, warned_stream] = test::open_scripted(
      test::joined({test::ready({"conditions"}), test::frame("complete-conditions")}), asking({"conditions"}));
  static_cast<void>(warned.query("INSERT INTO T VALUES ('abcdef')"));
  CHECK(warned.warning() == "01004");
  REQUIRE(warned.conditions().size() == 1);
  CHECK(warned.conditions().front().sqlstate == "01004");
  CHECK(warned.in_transaction());
  auto [plain, plain_stream] =
      test::open_scripted(test::joined({test::ready({}), test::frame("complete")}), asking({}));
  static_cast<void>(plain.query("COMMIT"));
  CHECK(plain.rows_affected() == std::nullopt);
  CHECK(plain.server_time() == std::nullopt);
  CHECK(plain.commit_position() == std::nullopt);
  CHECK(plain.warning() == std::nullopt);
}

/// Verifies: [PROTOCOL Composite fields] An Error's failure is read whole:
/// SQLCODE, SQLSTATE, message, the rule it cites, its position, whether a
/// transaction is active, and with `diagnostics` its tail of tags 7 to 15
/// [PROTOCOL Errors and notices].
TEST_CASE("a failure is read with every field") {
  auto [conn, stream] = test::open_scripted(
      test::joined({test::ready({"diagnostics"}), test::frame("error-cited"), test::frame("error-diagnostics")}),
      asking({"diagnostics"}));
  try {
    static_cast<void>(conn.query("INSERT INTO T VALUES (1)"));
    FAIL("the statement did not fail");
  } catch (const turborel::error& e) {
    CHECK(e.sqlcode() == -803);
    CHECK(e.sqlstate() == "23505");
    CHECK(e.details().rule == "ISO/IEC 9075-2:2023 \xC2\xA7"
                              "11.7");
    CHECK(e.details().position == 12);
    CHECK(e.details().active);
    CHECK_FALSE(e.details().diagnostics.has_value());
  }
  try {
    static_cast<void>(conn.query("INSERT INTO T VALUES (1)"));
    FAIL("the statement did not fail");
  } catch (const turborel::error& e) {
    const turborel::diagnostics told = e.details().diagnostics.value_or(turborel::diagnostics{});
    CHECK(told.severity == "ERROR");
    CHECK(told.schema == "S");
    CHECK(told.table == "T");
    CHECK(told.column == "K");
    CHECK(told.data_type == "INTEGER");
    CHECK(told.constraint == "T_PK");
    CHECK(told.detail.has_value());
    CHECK(told.hint.has_value());
    CHECK(told.context.has_value());
  }
  CHECK_FALSE(conn.lost());
}

/// Verifies: [PROTOCOL Ready 'R'] Ready gives the authorization identifier,
/// the capabilities granted and the server's product; Statuses before it
/// are the session's settings [PROTOCOL Status 'T']; a Notice before a
/// completion is kept [PROTOCOL Notice 'N']; a connection's Error of class
/// 08 ends it [PROTOCOL Error 'E'].
TEST_CASE("the login's and a statement's replies are read") {
  auto [conn, stream] = test::open_scripted(test::joined({
                                                test::frame("status-server-version"),
                                                test::frame("status-null"),
                                                test::ready({"status", "notices"}),
                                                test::frame("notice"),
                                                test::frame("complete"),
                                                test::frame("error-connection"),
                                            }),
                                            asking({"status", "notices"}));
  CHECK(conn.authorization() == "ANN");
  CHECK(conn.server() == "turborel 0.0.0 (alpha)");
  CHECK(conn.granted(turborel::capability::status));
  CHECK_FALSE(conn.granted(turborel::capability::ping));
  CHECK(conn.granted() == std::vector<std::string_view>{"notices", "status"});
  CHECK(conn.setting("SERVER_VERSION") == "turborel 0.0.0 (alpha)");
  CHECK(conn.setting("STATEMENT.TIMEOUT_MS") == "");
  CHECK(conn.setting("NO_SUCH") == std::nullopt);
  CHECK(conn.settings().size() == 2);
  static_cast<void>(conn.query("COMMIT"));
  REQUIRE(conn.notices().size() == 1);
  CHECK(conn.notices().front().sqlstate == "01004");
  conn.clear_notices();
  CHECK(conn.notices().empty());
  CHECK(failure_of([&conn] { static_cast<void>(conn.query("COMMIT")); }) == "-919 08006");
  CHECK(conn.lost());
}

namespace {

/// Whether `written` holds the sample `name`'s bytes from `at` on.
bool wrote(const test::bytes& written, std::size_t at, const std::string& name) {
  const test::bytes sample = test::frame(name);
  return written.size() >= at + sample.size() &&
         std::equal(sample.begin(), sample.end(), written.begin() + static_cast<std::ptrdiff_t>(at));
}

} // namespace

/// Verifies: [PROTOCOL Sample frames] Every line of the sample frames' file,
/// copied at 5863fe65, is read as the page lays it out; each server
/// sample of a data type or a value is read through the driver by
/// turborel-c's codec, which turborel-c's own tests check frame by frame;
/// the client frames of Ping, a Query under autocommit and Terminate are
/// written byte for byte as the file holds them; the compressed Rows is
/// inflated to its 64 rows (the compressed frames' test).
TEST_CASE("the sample frames read and written through the driver") {
  const std::map<std::string, test::sample>& all = test::samples();
  CHECK(all.size() == 166);
  const std::map<std::string, std::size_t, std::less<>> columns_of{
      {"description-empty", 0},
      {"description-declared", 7},
  };
  std::size_t read = 0;
  for (const auto& [name, sample] : all) {
    CAPTURE(name);
    CHECK((sample.direction == "client" || sample.direction == "server"));
    CHECK_FALSE(sample.means.empty());
    CHECK(static_cast<char>(sample.frame.at(0)) != '\0');
    if (name.starts_with("description-")) {
      const std::size_t columns = test::native(sample.frame, {test::frame("rows-none")}).columns().size();
      const auto other = columns_of.find(name);
      CHECK(columns == (other == columns_of.end() ? 1 : other->second));
      ++read;
    } else if (name.starts_with("value-")) {
      CHECK(test::native(test::frame("description-character"), {sample.frame}).size() == 1);
      ++read;
    }
  }
  CHECK(read > 60);
  auto [conn, stream] = test::open_scripted(test::joined({
                                                test::ready({"ping"}),
                                                test::frame("pong"),
                                                test::frame("description"),
                                                test::frame("rows"),
                                                test::frame("complete"),
                                            }),
                                            asking({"ping"}));
  const std::size_t hello = stream->written().size();
  conn.ping();
  CHECK(wrote(stream->written(), hello, "ping"));
  conn.set_autocommit(true);
  static_cast<void>(conn.query("VALUES 1"));
  CHECK(wrote(stream->written(), hello + test::frame("ping").size(), "query"));
  const std::size_t before = stream->written().size();
  conn.terminate();
  CHECK(wrote(stream->written(), before, "terminate"));
}

/// Verifies: [PROTOCOL Errors and notices] A replica that had not applied
/// REPLICATION.READ_AFTER's position in time fails the statement with
/// SQLCODE -920, SQLSTATE TBR01, the connection kept.
TEST_CASE("a replica behind its read-after position fails the statement") {
  const test::bytes behind = test::writer{}
                                 .u32(static_cast<std::uint32_t>(-920))
                                 .text("TBR01")
                                 .text("the replica had not applied the position in time")
                                 .u8(0)
                                 .u8(0)
                                 .u8(0)
                                 .frame('E');
  auto [conn, stream] = test::open_scripted(test::joined({test::ready({}), behind}), asking({}));
  CHECK(failure_of([&conn] { static_cast<void>(conn.query("SELECT 1")); }) == "-920 TBR01");
  CHECK_FALSE(conn.lost());
}

/// Verifies: [PROTOCOL JSON values] A JSON document of a version other than
/// 1 is refused as its value's failure (SQLCODE -916), the request failed
/// and the connection kept [PROTOCOL Values and fields].
TEST_CASE("a JSON document of another version fails its request") {
  const test::bytes version_two =
      test::writer{}.u32(1).u32(1).u8(25).raw(std::array{std::byte{2}, std::byte{0}}).frame('B');
  auto [conn, stream] = test::open_scripted(test::joined({
                                                test::ready({}),
                                                test::frame("description-json"),
                                                version_two,
                                                test::frame("complete"),
                                                test::frame("complete"),
                                            }),
                                            asking({}));
  CHECK(failure_of([&conn] { static_cast<void>(conn.query("SELECT J FROM T")); }) == "-916 TB916");
  CHECK_FALSE(conn.lost());
  CHECK(conn.query("COMMIT").sqlstate() == "00000");
}

/// Verifies: [PROTOCOL Execute 'E'] A cursor of a number other than 0, which
/// needs `cursors`, is refused before anything is sent where it was not
/// granted (SQLSTATE 08006), the connection kept [PROTOCOL Capabilities].
TEST_CASE("a numbered cursor needs cursors granted") {
  auto [conn, stream] = test::open_scripted(test::joined({test::ready({}), test::frame("prepared")}), asking({}));
  const turborel::statement prepared = conn.prepare("SELECT K FROM T WHERE K > ?");
  const std::size_t before = stream->written().size();
  CHECK(failure_of([&] { static_cast<void>(turborel::cursor::open(conn, prepared, turborel::params::of(1), 1, 7)); }) ==
        "-919 08006");
  CHECK(stream->written().size() == before);
  CHECK_FALSE(conn.lost());
}

namespace {

/// A `?`'s mode and ordinal position, as tail 20 writes them.
struct coded {
  /// Its mode's code.
  std::uint8_t mode;
  /// Its ordinal position.
  std::uint32_t position;
};

/// The sample prepared-called with its first `?`'s mode and ordinal
/// position written as `first` says.
test::bytes called_with(coded first) {
  test::bytes frame = test::frame("prepared-called");
  // The procedure's name, P, then the count of 2 `?`s: the first's mode
  // and position follow.
  const std::array<std::byte, 5> named{std::byte{0x50}, std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0}};
  const auto at = std::ranges::search(frame, named).begin() + named.size();
  *at = std::byte{first.mode};
  for (std::size_t i = 0; i < 4; ++i) {
    *(at + 1 + static_cast<std::ptrdiff_t>(i)) = std::byte{static_cast<std::uint8_t>(first.position >> (8U * i))};
  }
  return frame;
}

} // namespace

/// Verifies: [PROTOCOL Parameter modes] Granted `parameter-modes`, a
/// Prepared of a CALL ends with tail 20, the procedure's specific schema
/// and name, then every `?` in written order, OUT ones too, each with its
/// mode, its ordinal position and its type: the sample's IN of parameter
/// 1, an input, and OUT of parameter 2, the CALL's column; a mode of
/// another code, or a position of 0, fails the connection (SQLCODE -919,
/// SQLSTATE 08006) [PROTOCOL Capabilities] [PROTOCOL Sample frames].
TEST_CASE("a CALL's Prepared tells each marker's mode and position") {
  auto [conn, stream] = test::open_scripted(
      test::joined({test::ready({"parameter-modes"}), test::frame("prepared-called")}), asking({"parameter-modes"}));
  CHECK(conn.granted(turborel::capability::parameter_modes));
  const turborel::statement call = conn.prepare("CALL S.P(?, ?)");
  CHECK(call.procedure_schema() == "S");
  CHECK(call.procedure_name() == "P");
  const std::vector<turborel::marker> markers = call.markers();
  REQUIRE(markers.size() == 2);
  CHECK(markers.at(0).mode == turborel::parameter_mode::in);
  CHECK(markers.at(0).position == 1U);
  CHECK(markers.at(0).parameter == 0U);
  CHECK_FALSE(markers.at(0).column.has_value());
  CHECK(markers.at(0).type.kind() == turborel::kind::exact);
  CHECK(markers.at(1).mode == turborel::parameter_mode::out);
  CHECK(markers.at(1).position == 2U);
  CHECK_FALSE(markers.at(1).parameter.has_value());
  CHECK(markers.at(1).column == 0U);
  CHECK(call.parameter_count() == 1);
  for (const test::bytes& wrong : {called_with({.mode = 3, .position = 1}), called_with({.mode = 1, .position = 0})}) {
    auto [broken, written] =
        test::open_scripted(test::joined({test::ready({"parameter-modes"}), wrong}), asking({"parameter-modes"}));
    CHECK(failure_of([&broken] { static_cast<void>(broken.prepare("CALL S.P(?, ?)")); }) == "-919 08006");
    CHECK(broken.lost());
  }
}

/// Verifies: [PROTOCOL Parameter modes] A Prepared with no tail 20, of
/// another statement or where `parameter-modes` was not granted, tells no
/// procedure, and each of its markers is an input, IN, of no position.
TEST_CASE("a Prepared without tail 20 has its inputs alone, each IN") {
  auto [conn, stream] = test::open_scripted(test::joined({test::ready({}), test::frame("prepared")}), asking({}));
  const turborel::statement prepared = conn.prepare("SELECT K FROM T WHERE K > ?");
  CHECK_FALSE(prepared.procedure_name().has_value());
  CHECK_FALSE(prepared.procedure_schema().has_value());
  const std::vector<turborel::marker> markers = prepared.markers();
  REQUIRE(markers.size() == prepared.parameter_count());
  for (std::size_t i = 0; i < markers.size(); ++i) {
    CHECK(markers.at(i).mode == turborel::parameter_mode::in);
    CHECK_FALSE(markers.at(i).position.has_value());
    CHECK(markers.at(i).parameter == i);
    CHECK_FALSE(markers.at(i).column.has_value());
  }
}
