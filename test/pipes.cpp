/// @file
/// @brief Named pipes on Windows [CLIENT Timeouts] [CLIENT Connecting]:
/// one not there refused as the system says, and one every instance of
/// which is busy waited for within the connect timeout alone. CMake builds
/// it on Windows alone.

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h> // NOLINT(misc-include-cleaner): Windows declares its pipes in headers of its own.

namespace {

using namespace std::chrono_literals; // NOLINT(google-build-using-namespace): chrono's literals read as the waits do.

/// The bytes a pipe holds each way.
constexpr DWORD pipe_bytes = 4096;

/// What connecting to the pipe `name` by trust, waiting 100 ms while it is
/// busy, throws: its SQLSTATE and message.
std::string connecting(const std::string& name) {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connect_options options;
  options.timeouts(100ms, 0ms);
  try {
    static_cast<void>(turborel::connection::connect(turborel::target::local(name), "ann", trust, options));
  } catch (const turborel::error& e) {
    return std::string{e.sqlstate()} + " " + std::string{e.message()};
  }
  return "none";
}

} // namespace

/// Verifies: [CLIENT Connecting] A named pipe not there is refused, as the
/// system says it (SQLSTATE 08006).
TEST_CASE("a named pipe not there is refused") {
  CHECK(connecting(R"(\\.\pipe\turborel-cpp-none)").starts_with("08006 cannot connect: "));
}

/// Verifies: [CLIENT Timeouts] A named pipe every instance of which is busy
/// is waited for within the connect timeout, then refused, saying the
/// server did not answer in time.
TEST_CASE("a busy named pipe is waited for in time") {
  const std::string name = R"(\\.\pipe\turborel-cpp-busy-)" + std::to_string(GetCurrentProcessId());
  HANDLE server = CreateNamedPipeA(name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, pipe_bytes,
                                   pipe_bytes, 0, nullptr);
  REQUIRE(server != INVALID_HANDLE_VALUE);
  HANDLE holder = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
  REQUIRE(holder != INVALID_HANDLE_VALUE);
  const std::string failed = connecting(name);
  CHECK(failed.find("the server did not answer in time") != std::string::npos);
  CHECK(failed.find("every instance of the pipe is busy") != std::string::npos);
  static_cast<void>(CloseHandle(holder));
  static_cast<void>(CloseHandle(server));
}
