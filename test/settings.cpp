/// @file
/// @brief Connection settings, as turborel-client's own tests read them
/// (R-TEST-9): the URI, the service file, the password file, the
/// environment and the order the layers are laid in; and connecting by
/// settings against the run's server (R-TEST-8): the role asked, several
/// servers, TLS by mode, SCRAM by the password file and its key cache,
/// channel binding, a client certificate, a Unix socket, and `prefer`
/// falling back to plain TCP.

#include "support/frames.hpp"
#include "support/server.hpp"
#include "support/sockets.hpp"

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/error.hpp>
#include <turborel/result.hpp>
#include <turborel/settings.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

using namespace std::chrono_literals; // NOLINT(google-build-using-namespace): chrono's literals read as the waits do.

/// An environment of the test's own, no variable set but those given.
turborel::settings::variables environment(std::map<std::string, std::string, std::less<>> set = {}) {
  return [set = std::move(set)](std::string_view name) -> std::optional<std::string> {
    const auto found = set.find(name);
    return found == set.end() ? std::nullopt : std::optional<std::string>{found->second};
  };
}

/// The settings `options` give, laid in an environment of nothing.
turborel::settings laid(const turborel::options& options) {
  return turborel::settings::resolve(options, environment());
}

/// A scratch file of `text`, named for `name`, in the build's directory.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the file's name then its text, as the call reads.
std::string scratch(const std::string& name, const std::string& text) {
  const std::filesystem::path path = std::filesystem::path{TURBOREL_CPP_LIVE_DIR} / ("settings-" + name);
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out{path, std::ios::binary};
  out << text;
  return path.string();
}

/// What `call` throws as the driver's error: its SQLSTATE and message.
template <typename Call>
std::string failure_of(const Call& call) {
  try {
    call();
  } catch (const turborel::error& e) {
    return std::string{e.sqlstate()} + " " + std::string{e.message()};
  }
  return "none";
}

/// Whether host `i` of `laid` is `name` at `port`, local if `local`.
bool host_is(const turborel::settings& laid, std::size_t i, std::string_view name, std::uint16_t port,
             bool local = false) {
  const turborel::server_address host = laid.host(i);
  return host.name == name && host.port == port && host.local == local;
}

/// The URI of the run's server, `hosts` and `query` its own.
std::string uri_of(const std::string& hosts, const std::string& query) {
  return "turborel://ann@" + hosts + "/?" + query;
}

/// The live server's TCP address, as a URI's host gives it.
std::string server_at(std::string_view host = "127.0.0.1") {
  return std::string{host} + ":" + std::to_string(test::live::port);
}

/// Whether `uri` connects.
bool connects(const std::string& uri) {
  const std::string failed = failure_of([&uri] { static_cast<void>(turborel::settings::from_uri(uri).connect()); });
  if (failed != "none") {
    UNSCOPED_INFO(uri << ": " << failed);
  }
  return failed == "none";
}

/// Whether `uri` fails with `sqlstate`, its message holding `part`.
bool fails_as(const std::string& uri, std::string_view sqlstate, std::string_view part) {
  const std::string failed = failure_of([&uri] { static_cast<void>(turborel::settings::from_uri(uri).connect()); });
  const bool same = failed.starts_with(sqlstate) && failed.find(part) != std::string::npos;
  if (!same) {
    UNSCOPED_INFO(uri << ": " << failed);
  }
  return same;
}

} // namespace

/// Verifies: [CLIENT The connection URI] A URI gives its user, password,
/// hosts each of its port, an IPv6 address in brackets, a local socket
/// percent-encoded, and its query's settings; percent-encoding is decoded
/// in each part; the password is never given back as a setting.
TEST_CASE("a URI gives its user, hosts and settings") {
  const turborel::options options =
      turborel::options::from_uri("turborel://ann:p%40ss@db1:7000,[::1]:7001,db3,%2Frun%2Fs.sock/"
                                  "?tls_mode=verify-full&target=read-write&connect_timeout=5");
  CHECK(options.get(turborel::setting::user) == "ann");
  CHECK(options.get(turborel::setting::password) == "p@ss");
  CHECK(options.get(turborel::setting::service) == std::nullopt);
  const turborel::settings settings = laid(options);
  REQUIRE(settings.host_count() == 4);
  CHECK(host_is(settings, 0, "db1", 7000));
  CHECK(host_is(settings, 1, "::1", 7001));
  CHECK(host_is(settings, 2, "db3", turborel::default_port));
  CHECK(host_is(settings, 3, "/run/s.sock", 0, true));
  CHECK_THROWS_AS(settings.host(4), turborel::error);
  CHECK(settings.tls_mode() == turborel::tls_mode::verify_full);
  CHECK(settings.target() == turborel::target_role::read_write);
  CHECK(settings.connect_timeout() == 5s);
  CHECK(settings.get(turborel::setting::password) == std::nullopt);
  CHECK(settings.get(turborel::setting::tls_mode) == "verify-full");
  CHECK(settings.password_for(0) == "p@ss");
  const turborel::settings plain = turborel::settings::from_uri("turborel://ann@/");
  CHECK(plain.host_count() == 1);
  CHECK(host_is(plain, 0, "localhost", turborel::default_port));
  CHECK(plain.user() == "ann");
  CHECK(plain.connect_timeout() == 30s);
}

/// Verifies: [CLIENT The connection URI] A URI of another scheme, of a path,
/// of a key that names no setting, of a pair without `=`, or of a `%` not
/// followed by two hexadecimal digits is refused (SQLSTATE 08001); so is a
/// word no setting takes, a port that is no number, and no user [CLIENT
/// Where settings come from].
TEST_CASE("a malformed URI or setting is refused") {
  for (const std::string uri : {
           "other://a@b",
           "turborel://a@b/db",
           "turborel://a@b/?colour=red",
           "turborel://a@b/?tls_mode",
           "turborel://a%4@b",
           "turborel://a%ff@b",
       }) {
    CAPTURE(uri);
    CHECK(failure_of([&uri] { static_cast<void>(turborel::options::from_uri(uri)); }).starts_with("08001"));
  }
  for (const std::string uri : {
           "turborel://a@b/?tls_mode=always",
           "turborel://a@b:port",
           "turborel://a@b/?connect_timeout=-1",
           "turborel://b",
       }) {
    CAPTURE(uri);
    const turborel::options options = turborel::options::from_uri(uri);
    CHECK(failure_of([&options] { static_cast<void>(laid(options)); }).starts_with("08001"));
  }
  CHECK(failure_of([] {
          static_cast<void>(turborel::settings::from_uri("other://a@b"));
        }).find("a connection URI begins with turborel://") != std::string::npos);
  CHECK(failure_of([] {
          static_cast<void>(turborel::settings::from_uri("turborel://b"));
        }).starts_with("08001 no user is named"));
}

/// Verifies: [CLIENT Where settings come from] A setting the program gives
/// prevails over the service's, which prevails over the environment's,
/// which prevails over the default; the environment names the service and
/// its file when the program does not; an empty variable gives nothing.
TEST_CASE("settings are laid program, service, environment, default") {
  const std::string file =
      scratch("precedence", "# servers\n[prod]\nhost = prod-db\nport = 7500\ntls_mode = require\n\n[test]\nhost = "
                            "test-db\n");
  const turborel::settings::variables env = environment({
      {"TURBOREL_SERVICE", "prod"},
      {"TURBOREL_SERVICE_FILE", file},
      {"TURBOREL_USER", "env-user"},
      {"TURBOREL_PORT", "7600"},
      {"TURBOREL_TLS_MODE", "disable"},
      {"TURBOREL_APPLICATION_NAME", ""},
  });
  turborel::options program;
  program.set(turborel::setting::tls_mode, "verify-ca");
  const turborel::settings settings = turborel::settings::resolve(program, env);
  REQUIRE(settings.host_count() == 1);
  CHECK(host_is(settings, 0, "prod-db", 7500));
  CHECK(settings.tls_mode() == turborel::tls_mode::verify_ca);
  CHECK(settings.user() == "env-user");
  CHECK(settings.get(turborel::setting::application_name) == std::nullopt);
  CHECK(settings.channel_binding() == turborel::channel_binding::prefer);
  program.set(turborel::setting::service, "test");
  CHECK(host_is(turborel::settings::resolve(program, env), 0, "test-db", 7600));
  const turborel::settings::variables throwing = [](std::string_view) -> std::optional<std::string> {
    throw std::runtime_error{"no environment"};
  };
  CHECK_THROWS_AS(turborel::settings::resolve(program, throwing), std::runtime_error);
  std::filesystem::remove(file);
}

/// Verifies: [CLIENT The service file] A service file's section is its
/// lines' settings; a section that is not there, a setting before any
/// section, a line without `=`, a key that names no setting, and a service
/// or service file set by a service are refused, naming the line; a file
/// that cannot be read is refused.
TEST_CASE("a service file is read by section") {
  const auto service = [](const std::string& text, const std::string& name) {
    const std::string file = scratch("service", text);
    turborel::options options;
    options.set(turborel::setting::service_file, file).set(turborel::setting::service, name);
    return laid(options);
  };
  const turborel::settings read = service("[a]\nuser=ann\n[b]\nuser = bob\n  host =  h  \n", "b");
  CHECK(read.user() == "bob");
  CHECK(host_is(read, 0, "h", turborel::default_port));
  for (const auto& [text, why] : std::map<std::string, std::string>{
           {"[a]\nuser=ann\n", "has no service a"},
           {"user=ann\n", "line 1: a setting comes before any section"},
           {"[a]\nuser\n", "line 2: a setting is <key> = <value>"},
           {"[a]\ncolour=red\n", "line 2: colour is no connection setting"},
           {"[a]\nservice=b\n", "line 2: a service cannot set service"},
           {"[a\n", "line 1: a section's name ends with ]"},
       }) {
    CAPTURE(text);
    const std::string name = text == "[a]\nuser=ann\n" ? "a-missing" : "a";
    const std::string failed = failure_of([&] { static_cast<void>(service(text, name)); });
    CHECK(failed.starts_with("08001"));
    CHECK(failed.find(text == "[a]\nuser=ann\n" ? "has no service a-missing" : why) != std::string::npos);
  }
  turborel::options missing;
  missing.set(turborel::setting::user, "ann").set(turborel::setting::service, "s");
  missing.set(turborel::setting::service_file, scratch("service", "") + "-none");
  CHECK(failure_of([&missing] { static_cast<void>(laid(missing)); }).find("cannot read") != std::string::npos);
}

/// Verifies: [CLIENT The password file] The password file gives the password
/// of its first line matching host, port and user, `*` matching any, `\:`
/// and `\\` taken as they are, an escaped `\*` matching only `*`; comments
/// and short lines are skipped; the settings' own password prevails over
/// it; a file that cannot be read gives none; a local socket is matched by
/// its path and the default port.
TEST_CASE("a password file matches by host, port and user") {
  const std::string text = "# passwords\nshort:line\ndb1:7422:ann:first\n*:*:ann:any\\:host\n\\:\\:1:*:bob:c:\\\\d\n"
                           "\\*:1:carl:star\n";
  const std::string file = scratch("passfile", text);
  const auto password = [&file](const std::string& host, const std::string& port, const std::string& user) {
    turborel::options options;
    options.set(turborel::setting::user, user).set(turborel::setting::passfile, file);
    options.set(turborel::setting::host, host).set(turborel::setting::port, port);
    return laid(options).password_for(0);
  };
  CHECK(password("db1", "7422", "ann") == "first");
  CHECK(password("db9", "1", "ann") == "any:host");
  CHECK(password("[::1]", "7", "bob") == "c:\\d");
  CHECK(password("x", "1", "carl") == std::nullopt);
  CHECK(password("*", "1", "carl") == "star");
  CHECK(password("db1", "7422", "nobody") == std::nullopt);
  turborel::options program;
  program.set(turborel::setting::user, "ann").set(turborel::setting::passfile, file);
  program.set(turborel::setting::host, "db1");
  const turborel::settings from_file = laid(program);
  CHECK(from_file.password_for(0) == "first");
  program.set(turborel::setting::password, "given");
  CHECK(laid(program).password_for(0) == "given");
  const std::string local = scratch("passfile-local", "/run/s.sock:7422:ann:local\n");
  turborel::options socket;
  socket.set(turborel::setting::user, "ann").set(turborel::setting::passfile, local);
  socket.set(turborel::setting::host, "/run/s.sock");
  CHECK(laid(socket).password_for(0) == "local");
  std::filesystem::remove(file);
  std::filesystem::remove(local);
  CHECK(from_file.password_for(0) == std::nullopt);
  CHECK(from_file.password_for(1) == std::nullopt);
}

/// Verifies: [CLIENT Where settings come from] Every variable of the
/// environment gives its setting.
TEST_CASE("every variable of the environment gives its setting") {
  const std::map<std::string, std::string_view, std::less<>> variables{
      {"TURBOREL_HOST", turborel::setting::host},
      {"TURBOREL_PORT", turborel::setting::port},
      {"TURBOREL_USER", turborel::setting::user},
      {"TURBOREL_PASSWORD", turborel::setting::password},
      {"TURBOREL_TLS_MODE", turborel::setting::tls_mode},
      {"TURBOREL_APPLICATION_NAME", turborel::setting::application_name},
      {"TURBOREL_CONNECT_TIMEOUT", turborel::setting::connect_timeout},
  };
  for (const auto& [variable, key] : variables) {
    CAPTURE(variable);
    const std::map<std::string_view, std::string, std::less<>> words{
        {turborel::setting::port, "7000"},
        {turborel::setting::tls_mode, "require"},
        {turborel::setting::connect_timeout, "9"},
    };
    const auto word = words.find(key);
    const std::string value = word == words.end() ? "v" : word->second;
    std::map<std::string, std::string, std::less<>> set{{variable, value}};
    if (key != turborel::setting::user) {
      set.emplace("TURBOREL_USER", "u");
    }
    const turborel::settings settings = turborel::settings::resolve(turborel::options{}, environment(set));
    if (key == turborel::setting::password) {
      CHECK(settings.password_for(0) == "v");
    } else if (key == turborel::setting::user) {
      CHECK(settings.user() == "v");
    } else if (key == turborel::setting::host) {
      CHECK(settings.host(0).name == "v");
    } else {
      CHECK(settings.get(key) == value);
    }
  }
}

/// Verifies: [CLIENT Where settings come from] The words of each kind are
/// read as named, one no setting takes refused, naming those it takes; a
/// list of hosts takes a list of ports, one each, or one port for all; a
/// list with an empty host, an IPv6 address whose bracket is not closed or
/// whose port is no number, a port that is no number, and a key that names
/// no setting are refused (SQLSTATE 08001).
TEST_CASE("words and lists are read, and malformed ones refused") {
  turborel::options options;
  options.set(turborel::setting::host, "a,b").set(turborel::setting::port, "1,2").set(turborel::setting::user, "u");
  options.set(turborel::setting::tls_system_roots, "true").set(turborel::setting::channel_binding, "require");
  options.set(turborel::setting::load_balance, "random");
  const turborel::settings settings = laid(options);
  CHECK(host_is(settings, 0, "a", 1));
  CHECK(host_is(settings, 1, "b", 2));
  CHECK(settings.tls_system_roots());
  CHECK(settings.channel_binding() == turborel::channel_binding::require);
  CHECK(settings.load_balance() == turborel::load_balance::random);
  CHECK(settings.target() == turborel::target_role::any);
  options.set(turborel::setting::target, "read-only");
  CHECK(laid(options).target() == turborel::target_role::read_only);
  options.set(turborel::setting::target, "primary");
  CHECK(failure_of([&options] { static_cast<void>(laid(options)); }) ==
        "08001 target is primary, not one of any, read-write, read-only");
  CHECK(failure_of([&options] { options.set("colour", "red"); }) == "08001 colour is no connection setting");
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): a setting's key then its value, as options::set takes them.
  const auto refused = [](std::string_view key, std::string_view value, std::string_view port = {}) {
    turborel::options some;
    some.set(turborel::setting::user, "u").set(key, value);
    if (!port.empty()) {
      some.set(turborel::setting::port, port);
    }
    return failure_of([&some] { static_cast<void>(laid(some)); }).starts_with("08001");
  };
  CHECK(refused(turborel::setting::host, "a,,b"));
  CHECK(refused(turborel::setting::host, "[::1"));
  CHECK(refused(turborel::setting::host, "[::1]:x"));
  CHECK(refused(turborel::setting::host, "a", "1,x"));
  for (const std::string_view word : {
           turborel::setting::channel_binding,
           turborel::setting::target,
           turborel::setting::load_balance,
           turborel::setting::tls_system_roots,
       }) {
    CAPTURE(word);
    CHECK(refused(word, "sometimes"));
  }
  turborel::options one_port;
  one_port.set(turborel::setting::user, "u").set(turborel::setting::host, "a,b,[::1]");
  one_port.set(turborel::setting::port, "9");
  const turborel::settings all_nine = laid(one_port);
  CHECK(all_nine.host_count() == 3);
  CHECK(host_is(all_nine, 0, "a", 9));
  CHECK(host_is(all_nine, 2, "::1", 9));
  CHECK(all_nine.key_cache_lookups().hits == 0);
}

/// Verifies: [CLIENT Connecting by settings] Settings of a URI connect,
/// giving a connection like a target's; `application_name` is sent as the
/// connection attribute `application`; a server alone is read-write, kept
/// of `target=read-write` and refused of `read-only` (SQLSTATE 08001)
/// [CLIENT Several servers].
TEST_CASE("a URI connects and names its program") {
  LIVE_OR_SKIP();
  turborel::connection conn =
      turborel::settings::from_uri(uri_of(server_at(), "tls_mode=disable&application_name=settings-test")).connect();
  CHECK(conn.authorization() == "ANN");
  const turborel::result named = conn.query("SELECT ATTRIBUTE_VALUE FROM INFORMATION_SCHEMA.CONNECTION_ATTRIBUTES "
                                            "WHERE ATTRIBUTE_NAME = 'application' AND ATTRIBUTE_VALUE = "
                                            "'settings-test'");
  CHECK(named.size() == 1);
  CHECK(conn.role() == turborel::server_role::read_write);
  static_cast<void>(conn.query("ROLLBACK"));
  CHECK(connects(uri_of(server_at(), "tls_mode=disable&target=read-write")));
  CHECK(fails_as(uri_of(server_at(), "tls_mode=disable&target=read-only"), "08001",
                 "the server is read-write, and read-only is asked"));
  turborel::options options;
  options.set(turborel::setting::user, "ann").set(turborel::setting::host, "127.0.0.1");
  options.set(turborel::setting::port, std::to_string(test::live::port)).set(turborel::setting::tls_mode, "disable");
  CHECK(turborel::settings::resolve(options).connect().query("VALUES 1").size() == 1);
}

/// Verifies: [CLIENT Several servers] Hosts are tried in the order given, at
/// each address a name resolves to, one that cannot be reached passed for
/// the next; with none kept, the failure names each address and why
/// (SQLSTATE 08001); in an order drawn at random, one is kept.
TEST_CASE("several servers are tried in turn") {
  LIVE_OR_SKIP();
  CHECK(connects(uri_of("127.0.0.1:1," + server_at(), "tls_mode=disable")));
  CHECK(connects(uri_of(server_at("localhost") + "," + server_at(), "tls_mode=disable&load_balance=random")));
  CHECK(fails_as(uri_of("127.0.0.1:1,127.0.0.1:2", "tls_mode=disable"), "08001",
                 "no server was kept: 127.0.0.1 (127.0.0.1:1): cannot connect"));
  CHECK(fails_as(uri_of("no-such-host.invalid:17439", "tls_mode=disable"), "08006", "cannot resolve the name"));
}

/// Verifies: [CLIENT TLS by mode] `require` begins TLS, its certificate not
/// checked; `verify-ca` checks it against the authorities trusted, not its
/// name; `verify-full` its name too, `server_name`'s if given; an authority
/// not the server's, or none trusted, refuses it.
TEST_CASE("TLS by mode") {
  CERTIFIED_OR_SKIP();
  const std::string ca = test::live::path("ca.pem");
  CHECK(connects(uri_of(server_at(), "tls_mode=require")));
  CHECK(connects(uri_of(server_at("localhost"), "tls_mode=verify-full&tls_roots=" + ca)));
  CHECK(connects(uri_of(server_at(), "tls_mode=verify-ca&tls_roots=" + ca + "&server_name=other.example")));
  CHECK(fails_as(uri_of(server_at(), "tls_mode=verify-full&tls_roots=" + ca + "&server_name=other.example"), "08006",
                 "certificate verify failed"));
  CHECK(fails_as(uri_of(server_at(), "tls_mode=verify-full&tls_roots=" + test::live::path("other-ca.pem")), "08006",
                 "certificate verify failed"));
  CHECK(fails_as(uri_of(server_at("localhost"), "tls_mode=verify-ca"), "08006", "no certificate authority is trusted"));
  CHECK(fails_as(uri_of(server_at(), "tls_mode=verify-full&tls_roots=" + test::live::path("none.pem")), "08006",
                 "cannot read"));
  CHECK(
      fails_as(uri_of(server_at(), "tls_mode=verify-ca&tls_system_roots=true"), "08006", "certificate verify failed"));
}

/// Verifies: [CLIENT Connecting by settings] A password is proved by
/// SCRAM-SHA-256, of the settings or the password file, over TLS; refused
/// over plain TCP before anything is sent (SQLSTATE 28000); the settings'
/// key cache shared by their connections [CLIENT The SCRAM key cache];
/// channel binding required refuses a connection not over TLS (SQLSTATE
/// 28000) and binds one over it [CLIENT TLS by mode].
TEST_CASE("SCRAM by the password file") {
  CERTIFIED_OR_SKIP();
  const std::string passfile =
      scratch("live-passfile", "127.0.0.1:" + std::to_string(test::live::port) + ":ann:" + test::live::password + "\n");
  const turborel::settings settings =
      turborel::settings::from_uri(uri_of(server_at(), "tls_mode=require&passfile=" + passfile));
  static_cast<void>(settings.connect());
  static_cast<void>(settings.connect());
  CHECK(settings.key_cache_lookups().hits == 1);
  CHECK(settings.key_cache_lookups().misses == 1);
  CHECK(fails_as(uri_of(server_at(), "tls_mode=disable&passfile=" + passfile), "28000", "is sent only over TLS"));
  CHECK(fails_as(uri_of(server_at(), "tls_mode=disable&channel_binding=require&passfile=" + passfile), "28000",
                 "channel binding is required"));
  CHECK(connects(uri_of(server_at(), "tls_mode=require&channel_binding=require&passfile=" + passfile)));
  CHECK(fails_as(uri_of(server_at(), "tls_mode=require&channel_binding=require"), "28000",
                 "channel binding is required"));
  CHECK(connects(uri_of(server_at(), "tls_mode=require&channel_binding=disable&passfile=" + passfile)));
  std::filesystem::remove(passfile);
  CHECK(fails_as("turborel://ann:wrong@" + server_at() + "," + server_at("localhost") + "/?tls_mode=require", "28000",
                 "no server was kept: 127.0.0.1 (127.0.0.1:17439): "));
  CHECK(connects(uri_of(server_at(), "tls_mode=disable&passfile=" + test::live::path(""))));
}

/// Verifies: [CLIENT Connecting by settings] With no password, the
/// certificate named is presented over TLS and proves the user; one named
/// without its key is refused; a Unix socket uses no TLS whatever the mode
/// [CLIENT TLS by mode].
TEST_CASE("a certificate, and a socket") {
  CERTIFIED_OR_SKIP();
  CHECK(connects(uri_of(server_at("localhost"), "tls_mode=verify-full&tls_roots=" + test::live::path("ca.pem") +
                                                    "&tls_certificate=" + test::live::path("client.pem") +
                                                    "&tls_key=" + test::live::path("client.key"))));
  CHECK(fails_as(uri_of(server_at("localhost"), "tls_mode=require&tls_certificate=" + test::live::path("client.pem")),
                 "08006", "presented with its key"));
  CHECK(fails_as(uri_of(server_at("localhost"), "tls_mode=require&tls_certificate=" + test::live::path("none.pem") +
                                                    "&tls_key=" + test::live::path("client.pem")),
                 "08006", "cannot read"));
  if (!test::sockets::available()) {
    SKIP("no Unix sockets on this platform");
  }
  turborel::options options;
  options.set(turborel::setting::user, "ann").set(turborel::setting::host, test::live::path("turborel.sock"));
  options.set(turborel::setting::tls_mode, "verify-full").set(turborel::setting::tls_roots, test::live::path("ca.pem"));
  CHECK(turborel::settings::resolve(options).connect().query("VALUES 1").size() == 1);
}

/// Verifies: [CLIENT TLS by mode] Under `prefer`, a server reached whose TLS
/// handshake fails is connected to again over plain TCP.
TEST_CASE("prefer falls back to plain TCP") {
  CERTIFIED_OR_SKIP();
  if (!test::sockets::available()) {
    SKIP("no sockets for a proxy of the test's own on this platform");
  }
  test::sockets::refusing_proxy proxy{test::live::port};
  CHECK(connects(uri_of("127.0.0.1:" + std::to_string(proxy.port()), "tls_mode=prefer")));
  CHECK(proxy.accepted() == 2);
}

/// Verifies: [CLIENT TLS by mode] Each TLS mode and channel binding is read
/// as its word names it, and the default of each: `prefer` for both, load
/// balancing off; settings of the environment the process has are laid as
/// the program's are, a word no setting takes refused (08001).
TEST_CASE("every TLS mode and channel binding is read") {
  const std::map<std::string, turborel::tls_mode, std::less<>> modes{
      {"disable", turborel::tls_mode::disable},         {"prefer", turborel::tls_mode::prefer},
      {"require", turborel::tls_mode::require},         {"verify-ca", turborel::tls_mode::verify_ca},
      {"verify-full", turborel::tls_mode::verify_full},
  };
  for (const auto& [word, mode] : modes) {
    CAPTURE(word);
    CHECK(turborel::settings::from_uri("turborel://ann@h/?tls_mode=" + word).tls_mode() == mode);
  }
  const std::map<std::string, turborel::channel_binding, std::less<>> bindings{
      {"disable", turborel::channel_binding::disable},
      {"prefer", turborel::channel_binding::prefer},
      {"require", turborel::channel_binding::require},
  };
  for (const auto& [word, binding] : bindings) {
    CAPTURE(word);
    CHECK(turborel::settings::from_uri("turborel://ann@h/?channel_binding=" + word).channel_binding() == binding);
  }
  const turborel::settings defaults = turborel::settings::from_uri("turborel://ann@h/");
  CHECK(defaults.tls_mode() == turborel::tls_mode::prefer);
  CHECK(defaults.load_balance() == turborel::load_balance::disable);
  CHECK_FALSE(defaults.tls_system_roots());
  turborel::options program;
  program.set(turborel::setting::user, "ann");
  CHECK(turborel::settings::resolve(program).user() == "ann");
  program.set(turborel::setting::tls_mode, "always");
  CHECK(failure_of([&program] { static_cast<void>(turborel::settings::resolve(program)); }).starts_with("08001"));
}

/// Verifies: [CLIENT Where settings come from] With none named, the service
/// file is ~/.turborel_service.conf and the password file ~/.turborel_pass,
/// of the home the process's environment names; TURBOREL_PASSFILE names
/// another [CLIENT The service file] [CLIENT The password file].
TEST_CASE("the files are the home's unless named") {
  const std::string home =
      std::filesystem::path{scratch("home/.turborel_pass", "db1:7422:ann:at home\n")}.parent_path().string();
  static_cast<void>(scratch("home/.turborel_service.conf", "[mine]\nhost = db1\nuser = ann\n"));
#ifdef _WIN32
  const char* const home_variable = "USERPROFILE";
#else
  const char* const home_variable = "HOME";
#endif
  const std::optional<std::string> was = test::variable(home_variable);
  test::set_variable(home_variable, home.c_str());
  turborel::options named;
  named.set(turborel::setting::service, "mine");
  const turborel::settings at_home = turborel::settings::resolve(named);
  test::set_variable(home_variable, was ? was->c_str() : nullptr);
  CHECK(host_is(at_home, 0, "db1", turborel::default_port));
  CHECK(at_home.user() == "ann");
  CHECK(at_home.password_for(0) == "at home");
  const std::string other = scratch("other-pass", "db1:7422:ann:elsewhere\n");
  turborel::options program;
  program.set(turborel::setting::user, "ann").set(turborel::setting::host, "db1");
  CHECK(turborel::settings::resolve(program, environment({{"TURBOREL_PASSFILE", other}})).password_for(0) ==
        "elsewhere");
  std::filesystem::remove_all(home);
  std::filesystem::remove(other);
}

/// Verifies: [CLIENT The password file] The password file is read through a
/// link to a regular file, and not when it is a directory.
TEST_CASE("a password file is read through a link, not as a directory") {
  const std::string file = scratch("linked-pass", "db1:7422:ann:linked\n");
  const std::filesystem::path link = std::filesystem::path{file}.parent_path() / "settings-pass-link";
  std::filesystem::remove(link);
  std::error_code linked;
  std::filesystem::create_symlink(file, link, linked);
  if (linked) {
    SKIP("no symbolic links here: " + linked.message());
  }
  const auto password = [](const std::string& passfile) {
    turborel::options options;
    options.set(turborel::setting::user, "ann").set(turborel::setting::host, "db1");
    options.set(turborel::setting::passfile, passfile);
    return laid(options).password_for(0);
  };
  CHECK(password(link.string()) == "linked");
  CHECK(password(link.parent_path().string()) == std::nullopt);
  std::filesystem::remove(link);
  std::filesystem::remove(file);
}
