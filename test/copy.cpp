/// @file
/// @brief Live tests of bulk copy and export [CLIENT Copying and exporting]
/// (R-TEST-8), and of native rows read back offline.

#include "support/server.hpp"

#include <turborel/connection.hpp>
#include <turborel/copy.hpp>
#include <turborel/error.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <ios>
#include <istream>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <vector>

namespace {

/// The failure `call` throws as the driver's error.
template <typename Call>
std::optional<turborel::failure> failure_of(const Call& call) {
  try {
    call();
  } catch (const turborel::error& e) {
    return e.details();
  }
  return std::nullopt;
}

} // namespace

/// Verifies: [CLIENT Copying and exporting] A copy into a table, of the
/// columns named or all, sends rows many to a message with no reply each,
/// Ping answered during it, and finishing gives the rows inserted
/// [PROTOCOL CopyIn 'I'] [PROTOCOL CopyData 'J'] [PROTOCOL CopyEnd 'N'].
TEST_CASE("a copy sends rows and gives the rows inserted") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  static_cast<void>(conn.query("CREATE TABLE CP (ID INTEGER, NAME VARCHAR(10))"));
  static_cast<void>(conn.query("COMMIT"));
  turborel::copy into = turborel::copy::into(conn, "CP", {"ID", "NAME"});
  for (int i = 0; i < 1000; ++i) {
    into.send(turborel::params::of(i, "n" + std::to_string(i)));
  }
  conn.ping();
  CHECK(into.finish() == 1000);
  turborel::copy every = turborel::copy::into(conn, "CP");
  every.send(turborel::params::of(-1, std::nullopt));
  CHECK(every.finish() == 1);
  static_cast<void>(conn.query("COMMIT"));
  CHECK(conn.query("SELECT COUNT(*) FROM CP").at(0).get<std::int64_t>(0) == 1001);
}

/// Verifies: [CLIENT Copying and exporting] A copy is all or nothing: a row
/// refused undoes every row, its failure's detail naming the row; one
/// abandoned, or dropped before it finishes, inserts none [PROTOCOL
/// CopyEnd 'N'] [PROTOCOL Conversations].
TEST_CASE("a copy is all or nothing") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  static_cast<void>(conn.query("CREATE TABLE CN (ID INTEGER CHECK (ID > 0))"));
  static_cast<void>(conn.query("COMMIT"));
  turborel::copy into = turborel::copy::into(conn, "CN", {"ID"});
  for (int i = 1; i <= 1000; ++i) {
    into.send(turborel::params::of(i == 712 ? -1 : i));
  }
  const std::optional<turborel::failure> refused = failure_of([&into] { static_cast<void>(into.finish()); });
  REQUIRE(refused.has_value());
  CHECK(refused->sqlstate.starts_with("23"));
  CHECK(refused->diagnostics.value_or(turborel::diagnostics{}).detail == "row 712 of the copy");
  turborel::copy abandoned = turborel::copy::into(conn, "CN", {"ID"});
  abandoned.send(turborel::params::of(1));
  abandoned.abandon();
  {
    turborel::copy dropped = turborel::copy::into(conn, "CN", {"ID"});
    dropped.send(turborel::params::of(2));
  }
  static_cast<void>(conn.query("COMMIT"));
  CHECK(conn.query("SELECT COUNT(*) FROM CN").at(0).get<std::int64_t>(0) == 0);
}

/// Verifies: [CLIENT Copying and exporting] The rows an iterator gives are
/// copied; what it throws abandons the copy and is thrown.
TEST_CASE("the rows a function gives are copied") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  static_cast<void>(conn.query("CREATE TABLE CI (ID INTEGER)"));
  static_cast<void>(conn.query("COMMIT"));
  int next = 0;
  CHECK(turborel::copy_rows(conn, "CI", {}, [&next](turborel::params& row) {
          if (next == 5) {
            return false;
          }
          row.add(next++);
          return true;
        }) == 5);
  CHECK_THROWS_AS(
      turborel::copy_rows(conn, "CI", {}, [](turborel::params&) -> bool { throw std::runtime_error{"no"}; }),
      std::runtime_error);
  static_cast<void>(conn.query("COMMIT"));
  CHECK(conn.query("SELECT COUNT(*) FROM CI").at(0).get<std::int64_t>(0) == 5);
  turborel::connection without = test::live::trusted(turborel::connect_options{}.ask_none());
  const std::optional<turborel::failure> refused =
      failure_of([&without] { static_cast<void>(turborel::copy::into(without, "CI")); });
  CHECK(refused.value_or(turborel::failure{}).sqlstate == "08006");
  CHECK_FALSE(without.lost());
}

/// Verifies: [CLIENT Copying and exporting] An export writes a query's rows
/// as they come, as CSV, a header of the columns' names then a record for
/// each row, or as the protocol's own frames, which read back with every
/// value as it was.
TEST_CASE("an export writes CSV or the protocol's frames") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  std::ostringstream csv;
  CHECK(turborel::export_rows(
            conn, "SELECT X AS A, Y AS B FROM (VALUES (1, 'a,b'), (2, CAST(NULL AS VARCHAR(3)))) AS V(X, Y)",
            turborel::export_format::csv, csv) == 2);
  CHECK(csv.str() == "A,B\r\n1,\"a,b\"\r\n2,\r\n");
  std::ostringstream native;
  CHECK(turborel::export_rows(conn, "SELECT X FROM (VALUES (1.50), (2.25)) AS V(X)", turborel::export_format::native,
                              native) == 2);
  std::istringstream in{native.str()};
  const turborel::result back = turborel::read_native(in);
  CHECK(back.size() == 2);
  CHECK(back.at(0).get<turborel::decimal>(0).to_string() == "1.50");
  std::istringstream cut{native.str().substr(0, 7)};
  CHECK(failure_of([&cut] { static_cast<void>(turborel::read_native(cut)); }).value_or(turborel::failure{}).sqlstate ==
        "08006");
  CHECK(failure_of([&conn, &csv] {
          static_cast<void>(
              turborel::export_rows(conn, "CREATE TABLE EX (ID INTEGER)", turborel::export_format::csv, csv));
        }).has_value());
}

namespace {

/// A stream buffer that fails every read and write by throwing, as a
/// stream whose exceptions are asked for throws.
class broken final : public std::streambuf {
protected:
  int_type overflow(int_type /*c*/) override { throw std::runtime_error{"cannot write"}; }
  int_type underflow() override { throw std::runtime_error{"cannot read"}; }
};

} // namespace

/// Verifies: [CLIENT Copying and exporting] An export whose stream fails,
/// by its state or by throwing, fails, the connection kept; native rows
/// read of a stream that throws fail as a reply cut short (08006).
TEST_CASE("an export or a read of a failing stream fails") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  broken buffer;
  std::ostream out{&buffer};
  out.exceptions(std::ios::badbit);
  CHECK(failure_of([&] {
          static_cast<void>(turborel::export_rows(conn, "VALUES 1", turborel::export_format::csv, out));
        }).has_value());
  CHECK_FALSE(conn.lost());
  std::istream in{&buffer};
  in.exceptions(std::ios::badbit);
  CHECK(failure_of([&in] { static_cast<void>(turborel::read_native(in)); }).value_or(turborel::failure{}).sqlstate ==
        "08006");
}

/// Verifies: [PROTOCOL CopyEnd 'N'] A copy under autocommit is committed as
/// it succeeds; one in a transaction runs under a savepoint of its own, so
/// its failure keeps the transaction's earlier work [PROTOCOL
/// Conversations].
TEST_CASE("a copy commits under autocommit, or keeps the transaction before it") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  static_cast<void>(conn.query("CREATE TABLE CA (ID INTEGER CHECK (ID > 0))"));
  static_cast<void>(conn.query("COMMIT"));
  conn.set_autocommit(true);
  turborel::copy committed = turborel::copy::into(conn, "CA", {"ID"});
  committed.send(turborel::params::of(1));
  CHECK(committed.finish() == 1);
  CHECK_FALSE(conn.in_transaction());
  conn.set_autocommit(false);
  static_cast<void>(conn.query("INSERT INTO CA VALUES (2)"));
  turborel::copy refused = turborel::copy::into(conn, "CA", {"ID"});
  refused.send(turborel::params::of(-3));
  CHECK(failure_of([&refused] { static_cast<void>(refused.finish()); }).has_value());
  CHECK(conn.in_transaction());
  CHECK(conn.query("SELECT ID FROM CA").size() == 2);
  static_cast<void>(conn.query("COMMIT"));
}
