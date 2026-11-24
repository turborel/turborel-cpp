/// @file
/// @brief Tests of the driver's error, turborel::error (RULES.md D10).

#include <turborel/error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

/// A failure of a table not defined, with every field of the protocol's
/// layout set, its tail too, each to a value of its own, so that a field
/// read in place of another shows.
turborel::failure undefined_table() {
  return turborel::failure{
      .sqlcode = -103,
      .sqlstate = "42000",
      .message = "table ANN.NOPE is not defined",
      .rule = "X3.135-1986 §5.4 SR5",
      .position = 15,
      .active = true,
      .diagnostics =
          turborel::diagnostics{
              .severity = "ERROR",
              .detail = "no table or view of that name",
              .hint = "create it, or name another",
              .context = "routine ANN.P",
              .schema = "ANN",
              .table = "NOPE",
              .column = "ID",
              .data_type = "INTEGER",
              .constraint = "NOPE_PK",
          },
  };
}

} // namespace

/// Verifies: [PROTOCOL Composite fields] An error keeps every field of the
/// failure it is made of, SQLCODE, SQLSTATE, message, rule, position, the
/// active flag and the tail of tags 7 to 15, as the protocol writes them.
TEST_CASE("an error keeps every field of its failure") {
  const turborel::error e{undefined_table()};

  CHECK(e.details() == undefined_table());
  CHECK(e.sqlcode() == -103);
  CHECK(e.sqlstate() == "42000");
  CHECK(e.message() == "table ANN.NOPE is not defined");
  CHECK(e.details().diagnostics.value_or(turborel::diagnostics{}).constraint == "NOPE_PK");
}

/// Verifies: [DESIGN] D10 An error is a std::runtime_error whose what() reads
/// as turborel-client's failure does, its rule and position only where the
/// failure has them, and whose copies share the failure without throwing.
TEST_CASE("an error is a runtime_error that reads as turborel-client's failure") {
  STATIC_REQUIRE(std::is_base_of_v<std::runtime_error, turborel::error>);
  STATIC_REQUIRE(std::is_nothrow_copy_constructible_v<turborel::error>);

  const turborel::error cited{undefined_table()};
  CHECK(std::string{cited.what()} == "SQLCODE -103 (SQLSTATE 42000): table ANN.NOPE is not defined "
                                     "[X3.135-1986 §5.4 SR5] (at character 15)");

  const turborel::error plain{turborel::failure{.sqlcode = -919, .sqlstate = "08006", .message = "lost"}};
  CHECK(std::string{plain.what()} == "SQLCODE -919 (SQLSTATE 08006): lost");

  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization): the copy is what is tested.
  const turborel::error copy = cited;
  CHECK(&copy.details() == &cited.details());

  // Thrown from a call, so no compiler finds the assertion's code after it
  // unreachable.
  const auto rethrow = [&plain] { throw turborel::error{plain.details()}; };
  CHECK_THROWS_AS(rethrow(), std::runtime_error);
}
