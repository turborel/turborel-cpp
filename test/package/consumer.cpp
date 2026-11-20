/// @file
/// @brief A program built against the installed package, which throws and
/// catches the driver's error through the installed headers and library
/// (RULES.md D9).

#include <turborel/turborel.hpp>

#include <cstdlib>
#include <stdexcept>

/// Exits with success if an error made of a failure is caught as a
/// `std::runtime_error` with its SQLSTATE intact.
int main() {
  try {
    throw turborel::error{turborel::failure{.sqlcode = -919, .sqlstate = "08006", .message = "lost"}};
  } catch (const turborel::error& e) {
    return e.sqlstate() == "08006" ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (const std::runtime_error&) {
    return EXIT_FAILURE;
  }
}
