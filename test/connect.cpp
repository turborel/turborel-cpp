/// @file
/// @brief Live tests of connecting [CLIENT Connecting]: every transport
/// and mechanism, TLS, the login's refusals, and the SCRAM key cache,
/// against the run's server (R-TEST-8).

#include "support/frames.hpp"
#include "support/server.hpp"
#include "support/sockets.hpp"

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

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

/// The server's TLS target, its certificate checked against `name`.
turborel::target tls_target(const turborel::tls& settings, const std::optional<std::string>& name = "localhost") {
  return turborel::target::over_tls("127.0.0.1", test::live::port, settings, name);
}

/// The server's Unix socket.
turborel::target socket_target() { return turborel::target::local(test::live::path("turborel.sock")); }

} // namespace

/// Verifies: [CLIENT Connecting] Over TCP by trust the connection gives the
/// server's product and the session's authorization identifier, as Ready
/// told them, and Terminate ends it [PROTOCOL The Native Protocol].
TEST_CASE("a connection over TCP by trust tells its session") {
  LIVE_OR_SKIP();
  turborel::connection conn = test::live::trusted();
  CHECK(conn.server().starts_with("turborel "));
  CHECK(conn.authorization() == "ANN");
  CHECK(test::live::tcp().is_plain_tcp());
  CHECK_FALSE(test::live::tcp().is_tls());
  CHECK(test::live::tcp().host() == "127.0.0.1");
  conn.terminate();
  CHECK(conn.lost());
}

/// Verifies: [CLIENT Connecting] Over a Unix socket a password is sent whole,
/// and SCRAM proves it, neither refused as over plain TCP; a local target has
/// no host [PROTOCOL Mechanisms] [PROTOCOL Challenge 'C'] [PROTOCOL Answer
/// 'A'].
TEST_CASE("a Unix socket takes a password and SCRAM") {
  LIVE_OR_SKIP();
  TLS_OR_SKIP();
  turborel::mechanism password = turborel::mechanism::password(test::live::password);
  CHECK(turborel::connection::connect(socket_target(), "ann", password).authorization() == "ANN");
  turborel::mechanism scram = turborel::mechanism::scram(test::live::password);
  CHECK(turborel::connection::connect(socket_target(), "ann", scram).authorization() == "ANN");
  CHECK(scram.name() == "scram-sha-256");
  CHECK_FALSE(socket_target().is_plain_tcp());
  CHECK(socket_target().host() == std::nullopt);
}

/// Verifies: [CLIENT Connecting] Over TLS 1.3, SCRAM is SCRAM-SHA-256-PLUS
/// bound by tls-exporter, which a binding required shows; its keys are
/// kept in a cache, so the next login finds them [CLIENT The SCRAM key
/// cache].
TEST_CASE("TLS binds SCRAM to the channel and caches its keys") {
  CERTIFIED_OR_SKIP();
  const turborel::tls settings = test::live::tls();
  turborel::key_cache cache;
  turborel::mechanism scram = turborel::mechanism::scram(test::live::password);
  scram.bind_channel(turborel::channel_binding::require).with_cache(cache);
  for (int login = 0; login < 2; ++login) {
    CHECK(turborel::connection::connect(tls_target(settings), "ann", scram).authorization() == "ANN");
  }
  CHECK(cache.counts().hits == 1);
  CHECK(cache.counts().misses == 1);
  CHECK(cache.size() == 1);
  CHECK(tls_target(settings).is_tls());
  cache.clear();
  CHECK(cache.size() == 0);
}

/// Verifies: [CLIENT Connecting] A password is sent whole over TLS; a
/// certificate presented proves the user its common name names; trust
/// asked over TLS is still trust.
TEST_CASE("TLS takes a password, a certificate and trust") {
  CERTIFIED_OR_SKIP();
  const turborel::tls plain = test::live::tls();
  turborel::mechanism password = turborel::mechanism::password(test::live::password);
  CHECK(turborel::connection::connect(tls_target(plain), "ann", password).authorization() == "ANN");
  const turborel::tls presenting = test::live::tls(true);
  turborel::mechanism certificate = turborel::mechanism::certificate();
  CHECK(turborel::connection::connect(tls_target(presenting), "ann", certificate).authorization() == "ANN");
  turborel::mechanism trust = turborel::mechanism::trust();
  CHECK(turborel::connection::connect(tls_target(plain), "ann", trust).authorization() == "ANN");
}

/// Verifies: [CLIENT Connecting] TLS trusts the platform's authorities: the
/// files SSL_CERT_FILE names, in place of the store; an authority not
/// trusted, or a name the certificate is not of, refuses the connection.
TEST_CASE("TLS trusts the platform's authorities and checks the name") {
  CERTIFIED_OR_SKIP();
  const std::string ca = test::live::path("ca.pem");
  test::set_variable("SSL_CERT_FILE", ca.c_str());
  turborel::tls platform;
  platform.trust_platform();
  test::set_variable("SSL_CERT_FILE", nullptr);
  turborel::mechanism trust = turborel::mechanism::trust();
  CHECK(turborel::connection::connect(tls_target(platform), "ann", trust).authorization() == "ANN");
  turborel::tls other;
  other.trust_file(test::live::path("other-ca.pem"));
  CHECK(failure_of([&] { static_cast<void>(turborel::connection::connect(tls_target(other), "ann", trust)); }) ==
        "-919 08006");
  const turborel::tls right = test::live::tls();
  CHECK(failure_of([&] {
          static_cast<void>(turborel::connection::connect(tls_target(right, "elsewhere"), "ann", trust));
        }) == "-919 08006");
  CHECK_THROWS_AS(turborel::tls{}.trust_file(test::live::path("missing.pem")), turborel::error);
  CHECK_THROWS_AS(turborel::tls{}.present(test::live::path("missing.pem"), test::live::path("missing.key")),
                  turborel::error);
}

/// Verifies: [CLIENT Connecting] Over plain TCP, a password, and SCRAM not
/// asked for in the clear, are refused before anything is sent (SQLSTATE
/// 28000); SCRAM in the clear goes to a server of SCRAM_WITHOUT_TLS; a
/// wrong password is refused (28000), the key cache's entry dropped.
TEST_CASE("secrets are kept from plain TCP") {
  LIVE_OR_SKIP();
  TLS_OR_SKIP();
  turborel::mechanism password = turborel::mechanism::password(test::live::password);
  CHECK(failure_of([&] { static_cast<void>(turborel::connection::connect(test::live::tcp(), "ann", password)); }) ==
        "-919 28000");
  turborel::mechanism scram = turborel::mechanism::scram(test::live::password);
  CHECK(failure_of([&] { static_cast<void>(turborel::connection::connect(test::live::tcp(), "ann", scram)); }) ==
        "-919 28000");
  const turborel::key_cache cache;
  scram.in_the_clear().with_cache(cache);
  CHECK(turborel::connection::connect(test::live::tcp(), "ann", scram).authorization() == "ANN");
  CHECK(cache.size() == 1);
  turborel::mechanism wrong = turborel::mechanism::scram("not ann's");
  wrong.in_the_clear().with_cache(cache);
  CHECK(failure_of([&] {
          static_cast<void>(turborel::connection::connect(test::live::tcp(), "ann", wrong));
        }).ends_with("28000"));
  CHECK(cache.counts().misses == 2);
  turborel::mechanism trust = turborel::mechanism::trust();
  CHECK_THROWS_AS(trust.in_the_clear(), turborel::error);
  CHECK_THROWS_AS(trust.bind_channel(turborel::channel_binding::disable), turborel::error);
  CHECK_THROWS_AS(trust.with_cache(cache), turborel::error);
}

/// Verifies: [CLIENT Connecting] A TLS stream the program opened is bound
/// before SCRAM-SHA-256-PLUS, by its exported keys over TLS 1.3, and the
/// login a binding required takes over it.
TEST_CASE("a TLS stream of the program's own binds SCRAM") {
  CERTIFIED_OR_SKIP();
  std::optional<test::sockets::tls_opened> opened = test::sockets::tls(test::live::port, test::live::path("ca.pem"));
  REQUIRE(opened.has_value());
  turborel::mechanism scram = turborel::mechanism::scram(test::live::password);
  scram.bind_channel(turborel::channel_binding::require).bind(opened->certificate, opened->exporter);
  CHECK(scram.name() == "scram-sha-256-plus");
  turborel::connection conn = turborel::connection::open(std::move(opened->stream), "ann", scram);
  CHECK(conn.authorization() == "ANN");
  CHECK_THROWS_AS(conn.make_canceller(), turborel::error);
}

/// Verifies: [PROTOCOL Opening a connection] A version other than 1 is
/// refused (SQLSTATE 08004), and a user name that is not an identifier
/// before any exchange (28000); a refusal ends the connection
/// [PROTOCOL Error 'E'].
TEST_CASE("the server refuses a login it does not take") {
  LIVE_OR_SKIP();
  REQUIRE(test::sockets::available());
  turborel::mechanism trust = turborel::mechanism::trust();
  auto version_two = test::sockets::tcp(test::live::port, [](test::bytes& hello) { hello.at(5) = std::byte{2}; });
  CHECK(failure_of([&] {
          static_cast<void>(turborel::connection::open(std::exchange(version_two, nullptr), "ann", trust));
        }).ends_with("08004"));
  CHECK(failure_of([&] {
          static_cast<void>(turborel::connection::connect(test::live::tcp(), "1 not", trust));
        }).ends_with("28000"));
}

/// Verifies: [PROTOCOL Mechanisms] TLS settings trusting a file that cannot
/// be read, presenting a certificate that cannot be, or trusting no
/// authority for a target, each fail saying why; a
/// mechanism binds to a channel by the program's say, with or without
/// what the channel gives; a TLS target names its server or not.
TEST_CASE("mechanisms and TLS settings refuse what they cannot take") {
  TLS_OR_SKIP();
  turborel::tls settings;
  CHECK_THROWS_AS(settings.trust_file("/no/such/file.pem"), turborel::error);
  CHECK_THROWS_AS(settings.present("/no/such/chain.pem", "/no/such/key.pem"), turborel::error);
  turborel::mechanism scram = turborel::mechanism::scram("secret");
  scram.bind_channel(turborel::channel_binding::prefer).bind_channel(turborel::channel_binding::disable);
  scram.bind({}, {});
  CHECK(scram.name() == "scram-sha-256");
  CHECK_THROWS_AS(turborel::target::over_tls("127.0.0.1", 1, settings), turborel::error);
  settings.trust_platform();
  const turborel::target named = turborel::target::over_tls("127.0.0.1", 1, settings, std::string{"db.example"});
  const turborel::target unnamed = turborel::target::over_tls("127.0.0.1", 1, settings);
  CHECK(named.is_tls());
  CHECK_FALSE(unnamed.is_plain_tcp());
}

/// Verifies: [CLIENT Connecting] Over TLS 1.2, where no keys are exported,
/// SCRAM is SCRAM-SHA-256-PLUS bound by the server's certificate, which a
/// binding required shows: here TLS 1.2 is a proxy's of the test's own,
/// by the server's own certificate and key, which passes on to the server
/// over TLS of its own [PROTOCOL Mechanisms].
TEST_CASE("TLS 1.2 binds SCRAM by the server's certificate") {
  CERTIFIED_OR_SKIP();
  if (!test::sockets::available()) {
    SKIP("no sockets for a proxy of the test's own on this platform");
  }
  const test::sockets::tls12_proxy proxy{test::live::port, test::live::path("server.pem"),
                                         test::live::path("server.key")};
  turborel::mechanism scram = turborel::mechanism::scram(test::live::password);
  scram.bind_channel(turborel::channel_binding::require);
  const turborel::target over_tls12 =
      turborel::target::over_tls("127.0.0.1", proxy.port(), test::live::tls(), std::string{"localhost"});
  CHECK(turborel::connection::connect(over_tls12, "ann", scram).authorization() == "ANN");
}

/// Verifies: [CLIENT Connecting] A TLS 1.2 stream the program opened, which
/// exports no keys, is bound by the server's certificate alone before
/// SCRAM-SHA-256-PLUS [PROTOCOL Mechanisms].
TEST_CASE("a TLS 1.2 stream of the program's own binds SCRAM by the certificate") {
  CERTIFIED_OR_SKIP();
  std::optional<test::sockets::tls_opened> opened =
      test::sockets::tls(test::live::port, test::live::path("ca.pem"), true);
  REQUIRE(opened.has_value());
  CHECK(opened->exporter.empty());
  turborel::mechanism scram = turborel::mechanism::scram(test::live::password);
  scram.bind_channel(turborel::channel_binding::require).bind(opened->certificate, {});
  CHECK(turborel::connection::open(std::move(opened->stream), "ann", scram).authorization() == "ANN");
}

/// Verifies: [CLIENT Connecting] TLS trusts the platform's authorities of
/// the directory SSL_CERT_DIR names, OpenSSL's hashed names, and a file's
/// and the platform's both.
TEST_CASE("TLS trusts SSL_CERT_DIR's authorities, and a file's and the platform's both") {
  CERTIFIED_OR_SKIP();
  test::set_variable("SSL_CERT_FILE", test::live::path("none.pem").c_str());
  test::set_variable("SSL_CERT_DIR", test::live::path("certs").c_str());
  turborel::tls by_dir;
  by_dir.trust_platform();
  test::set_variable("SSL_CERT_DIR", nullptr);
  test::set_variable("SSL_CERT_FILE", nullptr);
  turborel::mechanism trust = turborel::mechanism::trust();
  CHECK(turborel::connection::connect(tls_target(by_dir), "ann", trust).authorization() == "ANN");
  test::set_variable("SSL_CERT_FILE", test::live::path("ca.pem").c_str());
  turborel::tls both;
  both.trust_file(test::live::path("other-ca.pem")).trust_platform();
  test::set_variable("SSL_CERT_FILE", nullptr);
  CHECK(turborel::connection::connect(tls_target(both), "ann", trust).authorization() == "ANN");
}

/// Verifies: [CLIENT The SCRAM key cache] An entry is found by the salt,
/// the iterations and the password: two users of one password, each of a
/// salt of its own, are two entries, and a wrong password none, its entry
/// dropped as the login that used it fails; at most 64 entries are kept,
/// the oldest dropped first; the cache is the process's memory alone,
/// another cache knowing none of it.
TEST_CASE("the key cache keeps 64 entries by salt and password") {
  LIVE_OR_SKIP();
  TLS_OR_SKIP();
  turborel::key_cache cache;
  const auto login = [&cache](const std::string& user, const std::string& secret) {
    turborel::mechanism scram = turborel::mechanism::scram(secret);
    scram.with_cache(cache);
    return failure_of([&] { static_cast<void>(turborel::connection::connect(socket_target(), user, scram)); });
  };
  for (int i = 0; i < 65; ++i) {
    test::live::user("KEYS" + std::to_string(i), "shared secret");
  }
  CHECK(login("keys0", "shared secret") == "none");
  CHECK(login("keys1", "shared secret") == "none");
  CHECK(cache.size() == 2);
  CHECK(cache.counts().hits == 0);
  CHECK(login("keys0", "not the secret") != "none");
  CHECK(cache.size() == 2);
  for (int i = 2; i < 65; ++i) {
    CHECK(login("keys" + std::to_string(i), "shared secret") == "none");
  }
  CHECK(cache.size() == 64);
  const turborel::key_cache::lookups before = cache.counts();
  CHECK(login("keys0", "shared secret") == "none");
  CHECK(cache.counts().misses == before.misses + 1);
  CHECK(login("keys64", "shared secret") == "none");
  CHECK(cache.counts().hits == before.hits + 1);
  CHECK(turborel::key_cache{}.size() == 0);
}
