/// @file
/// @brief Tests of turborel::decimal, the protocol's exact number
/// (RULES.md D16).

#include <turborel/decimal.hpp>

#include <turborel/int128.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>

/// Verifies: [PROTOCOL Values] An exact number is its units over ten to
/// its scale, and its text is turborel-client's: zeros padded before the
/// point, the sign before them, and past a scale of 18 the power of ten.
TEST_CASE("a decimal's text is turborel-client's") {
  CHECK(turborel::decimal{.units = turborel::int128{1}, .scale = 0}.to_string() == "1");
  CHECK(turborel::decimal{.units = turborel::int128{-1250}, .scale = 2}.to_string() == "-12.50");
  CHECK(turborel::decimal{.units = turborel::int128{5}, .scale = 3}.to_string() == "0.005");
  CHECK(turborel::decimal{.units = turborel::int128{-5}, .scale = 3}.to_string() == "-0.005");
  CHECK(turborel::decimal{.units = turborel::int128{123}, .scale = 3}.to_string() == "0.123");
  CHECK(turborel::decimal{.units = turborel::int128{5}, .scale = 18}.to_string() == "0.000000000000000005");
  CHECK(turborel::decimal{.units = turborel::int128{125}, .scale = 40}.to_string() == "125E-40");
  CHECK(turborel::decimal{.units = turborel::int128{-125}, .scale = 19}.to_string() == "-125E-19");
}

/// Verifies: [PROTOCOL Values] Every exact number the protocol carries, the
/// full range of its units at any scale, reads back from its text with the
/// same units and scale.
TEST_CASE("a decimal reads back from its text with no loss") {
  for (const turborel::decimal value : {
           turborel::decimal{.units = turborel::int128::max(), .scale = 18},
           turborel::decimal{.units = turborel::int128::min(), .scale = 0},
           turborel::decimal{.units = turborel::int128::min(), .scale = 7},
           turborel::decimal{.units = turborel::int128{-5}, .scale = 3},
           turborel::decimal{.units = turborel::int128::max(), .scale = 4'294'967'295},
       }) {
    CHECK(turborel::decimal::parse(value.to_string()) == value);
  }
  CHECK(turborel::decimal::parse("+1.50") == turborel::decimal{.units = turborel::int128{150}, .scale = 2});
  CHECK(turborel::decimal{.units = turborel::int128{10}, .scale = 1} !=
        turborel::decimal{.units = turborel::int128{100}, .scale = 2});
}

/// Verifies: [DESIGN] D16 Text that writes no exact number, or units out
/// of range, reads as none.
TEST_CASE("a decimal refuses text that writes none") {
  for (const auto* bad : {
           "",
           "-",
           "1.",
           ".5",
           "1e-3",
           "1E-",
           "E-3",
           "1E-4294967296",
           "1E-x",
           "--1",
           "1.2.3",
           "1. 5",
           "170141183460469231731687303715884105728",
           "17014118346046923173168730371588410572.8",
           "170141183460469231731687303715884105728E-2",
       }) {
    CHECK(turborel::decimal::parse(bad) == std::nullopt);
  }
}
