/// @file
/// @brief Live tests of large objects [CLIENT Large objects]: whole and by
/// locator, read by range and by stream, locators freed and held, and
/// written a piece at a time (R-TEST-8).

#include "support/server.hpp"

#include <turborel/connection.hpp>
#include <turborel/error.hpp>
#include <turborel/lob.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/value.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <span>
#include <string>
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

/// A CLOB written a piece at a time, `pieces` of `piece`, and inserted.
void insert_clob(turborel::connection& conn, const std::string& table, int key, const std::string& piece, int pieces) {
  turborel::lob_writer writer{conn, true};
  for (int i = 0; i < pieces; ++i) {
    writer.write(piece);
  }
  turborel::params values;
  values.add(key);
  writer.finish(values);
  static_cast<void>(conn.query_with("INSERT INTO " + table + " VALUES (?, ?)", values));
}

} // namespace

/// Verifies: [CLIENT Large objects] A CLOB of at most 64 KiB comes whole in
/// its row; a longer one as its locator and lengths, read by a range of
/// octets or of characters, at most a chunk of 256 KiB, and whole through a
/// stream [PROTOCOL Values] [PROTOCOL Lob 'L'].
TEST_CASE("a large object comes whole or by its locator") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  static_cast<void>(conn.query("CREATE TABLE LB (ID INTEGER, BODY CLOB(10M))"));
  static_cast<void>(conn.query("COMMIT"));
  insert_clob(conn, "LB", 1, "short", 1);
  insert_clob(conn, "LB", 2, std::string(1000, 'x') + "\xC3\xA9", 300);
  static_cast<void>(conn.query("COMMIT"));
  const turborel::result read = conn.query("SELECT BODY FROM LB ORDER BY ID");
  const turborel::value whole = read.at(0).at(0);
  CHECK_FALSE(whole.lob().located);
  CHECK(whole.text() == "short");
  const turborel::value located = read.at(1).at(0);
  const turborel::lob_info info = located.lob();
  CHECK(info.located);
  CHECK(info.character);
  CHECK(info.octets == 1002 * 300);
  CHECK(info.characters == 1001 * 300);
  std::vector<std::byte> out(turborel::lob_chunk_bytes * 2);
  CHECK(turborel::read_lob(conn, located, {.from = 0, .count = std::uint64_t{1002} * 300}, out) ==
        turborel::lob_chunk_bytes);
  std::vector<std::byte> few(8);
  CHECK(turborel::read_lob(conn, located, {.from = 999, .count = 2, .characters = true}, few) == 3);
  turborel::lob_reader reader{conn, located};
  std::istream in{&reader};
  std::array<char, 4096> chunk{};
  std::size_t octets = 0;
  while (in.read(chunk.data(), chunk.size()) || in.gcount() > 0) {
    octets += static_cast<std::size_t>(in.gcount());
  }
  CHECK(octets == 1002 * 300);
  turborel::lob_reader again{conn, located};
  CHECK(again.read_all().size() == 1002 * 300);
  CHECK(again.read(out) == 0);
  CHECK(sqlstate_of([&] { static_cast<void>(turborel::read_lob(conn, read.at(0).at(0), {}, few)); }) != "22000");
}

/// Verifies: [CLIENT Large objects] A locator lasts until its transaction
/// ends, which a result sending one keeps open under autocommit too; one
/// freed, or past its commit, is no longer valid (0F001); one held lasts
/// past the commit [PROTOCOL Query 'Q'] [PROTOCOL Lob 'L'].
TEST_CASE("a locator lasts its transaction, or past it if held") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  static_cast<void>(conn.query("CREATE TABLE LH (ID INTEGER, BODY CLOB(10M))"));
  static_cast<void>(conn.query("COMMIT"));
  insert_clob(conn, "LH", 1, std::string(100000, 'y'), 1);
  static_cast<void>(conn.query("COMMIT"));
  conn.set_autocommit(true);
  static_cast<void>(conn.query("SELECT ID FROM LH"));
  CHECK_FALSE(conn.in_transaction());
  const turborel::result held = conn.query("SELECT BODY FROM LH");
  CHECK(conn.in_transaction());
  const turborel::value lob = held.at(0).at(0);
  turborel::hold_lob(conn, lob);
  static_cast<void>(conn.query("COMMIT"));
  std::vector<std::byte> out(16);
  CHECK(turborel::read_lob(conn, lob, {.from = 0, .count = 16}, out) == 16);
  turborel::free_lob(conn, lob);
  CHECK(sqlstate_of([&] { static_cast<void>(turborel::read_lob(conn, lob, {.from = 0, .count = 16}, out)); }) ==
        "0F001");
  static_cast<void>(conn.query("COMMIT"));
  const turborel::result gone = conn.query("SELECT BODY FROM LH");
  const turborel::value ended = gone.at(0).at(0);
  static_cast<void>(conn.query("COMMIT"));
  CHECK(sqlstate_of([&] { static_cast<void>(turborel::read_lob(conn, ended, {.from = 0, .count = 16}, out)); }) ==
        "0F001");
  const turborel::result small = conn.query("VALUES CAST('ab' AS CLOB(1K))");
  CHECK(sqlstate_of([&] { turborel::free_lob(conn, small.at(0).at(0)); }) == "22000");
}

/// Verifies: [CLIENT Large objects] A BLOB written a piece at a time is
/// passed by its locator as a parameter; a CLOB's octets are UTF-8, a
/// character free to be split between writes; octets that are not UTF-8
/// are refused by the Append that sends them, and a CLOB that ends within a
/// character where it is used (22021), the connection kept [PROTOCOL Lob
/// 'L'].
TEST_CASE("a large object is written a piece at a time") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  static_cast<void>(conn.query("CREATE TABLE LW (ID INTEGER, IMAGE BLOB(10M), BODY CLOB(1M))"));
  static_cast<void>(conn.query("COMMIT"));
  turborel::lob_writer image{conn, false};
  const std::vector<std::byte> chunk(4096, std::byte{7});
  for (int i = 0; i < 100; ++i) {
    image.write(chunk);
  }
  image.write(std::span<const std::byte>{});
  turborel::params values;
  values.add(1);
  image.finish(values);
  turborel::lob_writer split{conn, true};
  split.write(std::string_view{"a\xC3"});
  split.write(std::string_view{"\xA9z"});
  split.finish(values);
  static_cast<void>(conn.query_with("INSERT INTO LW VALUES (?, ?, ?)", values));
  static_cast<void>(conn.query("COMMIT"));
  const turborel::result read = conn.query("SELECT IMAGE, BODY FROM LW");
  CHECK(read.at(0).at(0).lob().octets == 4096 * 100);
  CHECK(read.at(0).get<std::string>(1) == "a\xC3\xA9z");
  static_cast<void>(conn.query("COMMIT"));
  turborel::lob_writer bad{conn, true};
  bad.write(std::string_view{"a\xFF"});
  turborel::params refused;
  CHECK(sqlstate_of([&] { bad.finish(refused); }) == "22021");
  CHECK(refused.size() == 0);
  CHECK_FALSE(conn.lost());
  turborel::lob_writer cut{conn, true};
  cut.write(std::string_view{"a\xC3"});
  turborel::params ending;
  ending.add(2);
  ending.add(std::nullopt);
  cut.finish(ending);
  CHECK(sqlstate_of([&] { static_cast<void>(conn.query_with("INSERT INTO LW VALUES (?, ?, ?)", ending)); }) == "22021");
  CHECK_FALSE(conn.lost());
}
