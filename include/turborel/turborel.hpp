/// @file
/// @brief The whole of turborel-cpp: include this header to use the
/// driver.

#ifndef TURBOREL_TURBOREL_HPP
#define TURBOREL_TURBOREL_HPP

#include <turborel/error.hpp>

/// The C++ driver for turborel, a relational database system.
///
/// It wraps turborel-c, which speaks turborel's native protocol, version 1,
/// in C++20: owners of turborel-c's handles that free them as they go,
/// failures thrown as turborel::error, rows as a range, datetimes in
/// std::chrono and exact numbers in a decimal type of its own (RULES.md
/// D1, D2). The driver is in alpha, version 0.0.0, the turborel release it
/// is made for; it holds the error type alone so far.
namespace turborel {}

#endif // TURBOREL_TURBOREL_HPP
