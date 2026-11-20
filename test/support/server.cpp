/// @file
/// @brief The live tests' server (R-TEST-8): started once, by its own
/// process ID, and stopped as the tests end. Only where a turboreld runs,
/// Linux and macOS; on Windows the live tests are skipped (R-PLAT-2).

#include "server.hpp"

#include "certs.hpp"

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/error.hpp>

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <csignal>
#include <fcntl.h>
#include <signal.h> // NOLINT(modernize-deprecated-headers): POSIX's kill, which <csignal> does not declare.
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

extern "C" {
#ifndef _WIN32
extern char** environ; // NOLINT(readability-redundant-declaration): POSIX declares it nowhere a program includes.
#endif
}

namespace test::live {

namespace {

/// How often, and how many times, the start looks for the server listening.
constexpr std::chrono::milliseconds poll_every{50};
/// See above: twenty seconds in all.
constexpr int polls = 400;

/// The server of the run, started once.
class server {
public:
  server() { start(); }
  server(const server&) = delete;
  server(server&&) = delete;
  server& operator=(const server&) = delete;
  server& operator=(server&&) = delete;
  ~server() {
    stop();
    if (!dir_.empty()) {
      std::error_code ignored;
      std::filesystem::remove_all(dir_, ignored);
    }
  }

  [[nodiscard]] bool running() const noexcept { return pid_ > 0; }
  [[nodiscard]] const std::string& why_not() const noexcept { return why_; }
  [[nodiscard]] bool certified() const noexcept { return certified_; }
  [[nodiscard]] const std::filesystem::path& dir() const noexcept { return dir_; }

  /// Makes a user once, by the owner.
  void user(const std::string& name, const std::string& secret) {
    const std::scoped_lock locked{lock_};
    if (made_.insert(name).second) {
      turborel::connection conn = trusted();
      static_cast<void>(conn.query("CREATE USER " + name + " PASSWORD '" + secret + "'"));
    }
  }

private:
  void start();
  void stop() noexcept;
  /// Waits for the server spawned to listen, giving the owner its
  /// schema, or for it to stop; `at` is its directory.
  void await(const std::string& at);
  /// The command that runs the server `named`, in the directory `at`,
  /// with TLS if `certified`.
  static std::vector<std::string> command(const char* named, const std::string& at, bool certified);
  [[nodiscard]] static bool listening();

  /// Gives the owner its schema, where the tests make their tables.
  static void give_schema() {
    turborel::connection conn = trusted();
    static_cast<void>(conn.query("CREATE SCHEMA AUTHORIZATION ANN"));
    static_cast<void>(conn.query("COMMIT"));
  }

  std::filesystem::path dir_;
  std::string why_;
  long pid_{};
  bool certified_{};
  std::mutex lock_;
  std::set<std::string> made_;
};

/// The words of a command, split at spaces.
std::vector<std::string> words_of(const std::string& command) {
  std::vector<std::string> words;
  std::istringstream in{command};
  std::string word;
  while (in >> word) {
    words.push_back(word);
  }
  return words;
}

#ifndef _WIN32

/// Spawns `words`, its input a pipe whose writing end `input` gets, its
/// output and errors to `log`.
long spawn(const std::vector<std::string>& words, const std::string& log, int& input) {
  std::vector<char*> argv;
  argv.reserve(words.size() + 1);
  for (const std::string& word : words) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast): posix_spawn's argv is not const, and does not change it.
    argv.push_back(const_cast<char*>(word.c_str()));
  }
  argv.push_back(nullptr);
  std::array<int, 2> ends{-1, -1};
  if (pipe(ends.data()) != 0) {
    return 0;
  }
  posix_spawn_file_actions_t actions{};
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, ends.at(0), STDIN_FILENO);
  posix_spawn_file_actions_addclose(&actions, ends.at(1));
  posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
  pid_t pid = 0;
  const int spawned = posix_spawnp(&pid, argv.front(), &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  close(ends.at(0));
  if (spawned != 0) {
    close(ends.at(1));
    return 0;
  }
  input = ends.at(1);
  return static_cast<long>(pid);
}

void server::start() {
  // NOLINTNEXTLINE(concurrency-mt-unsafe): read once, on the first live test's thread, before any other.
  const char* named = std::getenv("TURBOREL_CPP_TEST_SERVER");
  if (named == nullptr || *named == '\0') {
    why_ = "TURBOREL_CPP_TEST_SERVER names no server, so the live tests are skipped";
    return;
  }
  dir_ = std::filesystem::path{TURBOREL_CPP_LIVE_DIR} / ("run-" + std::to_string(getpid()));
  std::filesystem::create_directories(dir_);
  certified_ = make_certificates(dir_.string());
  const std::string at = dir_.string() + "/";
  int input = -1;
  pid_ = spawn(command(named, at, certified_), at + "server.log", input);
  if (pid_ <= 0) {
    why_ = "the server cannot be started";
    return;
  }
  const std::string secret = std::string{password} + "\n";
  static_cast<void>(write(input, secret.data(), secret.size()));
  close(input);
  await(at);
}

void server::await(const std::string& at) {
  for (int i = 0; i < polls; ++i) {
    int status = 0;
    if (waitpid(static_cast<pid_t>(pid_), &status, WNOHANG) == static_cast<pid_t>(pid_)) {
      pid_ = 0;
      why_ = "the server stopped as it started; " + at + "server.log says why";
      return;
    }
    if (listening()) {
      give_schema();
      return;
    }
    std::this_thread::sleep_for(poll_every);
  }
  stop();
  why_ = "the server did not listen in time";
}

std::vector<std::string> server::command(const char* named, const std::string& at, bool certified) {
  std::vector<std::string> words = words_of(named);
  for (const std::string& word : {std::string{"--memory"}, std::string{"--owner"}, std::string{owner}}) {
    words.push_back(word);
  }
  std::vector<std::string> settings{
      "SERVER.HOST=127.0.0.1",
      "SERVER.PORT=" + std::to_string(port),
      "SERVER.SOCKET=" + at + "turborel.sock",
      "SERVER.TRUST_LOCAL=ON",
      "SERVER.LOGIN_DELAY_MS=0",
      "SERVER.SCRAM_WITHOUT_TLS=ON",
  };
  if (certified) {
    for (const std::string& tls : {
             std::string{"SERVER.TLS=ON"},
             "SERVER.TLS_CERTIFICATE=" + at + "server.pem",
             "SERVER.TLS_KEY=" + at + "server.key",
             "SERVER.TLS_CLIENT_CA=" + at + "ca.pem",
         }) {
      settings.push_back(tls);
    }
  }
  for (const std::string& setting : settings) {
    words.emplace_back("--set");
    words.push_back(setting);
  }
  return words;
}

void server::stop() noexcept {
  if (pid_ <= 0) {
    return;
  }
  const auto pid = static_cast<pid_t>(pid_);
  kill(pid, SIGTERM);
  for (int i = 0; i < polls; ++i) {
    int status = 0;
    if (waitpid(pid, &status, WNOHANG) == pid) {
      pid_ = 0;
      return;
    }
    std::this_thread::sleep_for(poll_every);
  }
  kill(pid, SIGKILL);
  int status = 0;
  waitpid(pid, &status, 0);
  pid_ = 0;
}

#else

void server::start() { why_ = "no turboreld runs on Windows, so the live tests are skipped"; }

void server::stop() noexcept {}

#endif

bool server::listening() {
  try {
    turborel::mechanism trust = turborel::mechanism::trust();
    turborel::connection probe = turborel::connection::connect(turborel::target::tcp("127.0.0.1", port), owner, trust);
    probe.terminate();
    return true;
  } catch (const turborel::error&) {
    return false;
  }
}

/// The server of the run.
server& the_server() {
  static server started;
  return started;
}

} // namespace

bool running() { return the_server().running(); }

std::string why_not() { return the_server().why_not(); }

bool has_certificates() { return the_server().certified(); }

std::string path(const std::string& name) { return (the_server().dir() / name).string(); }

turborel::tls tls(bool present) {
  turborel::tls settings;
  settings.trust_file(path("ca.pem"));
  if (present) {
    settings.present(path("client.pem"), path("client.key"));
  }
  return settings;
}

turborel::target tcp() { return turborel::target::tcp("127.0.0.1", port); }

turborel::connection trusted(const turborel::connect_options& options) {
  turborel::mechanism trust = turborel::mechanism::trust();
  return turborel::connection::connect(tcp(), owner, trust, options);
}

void user(const std::string& name, const std::string& secret) { the_server().user(name, secret); }

} // namespace test::live
