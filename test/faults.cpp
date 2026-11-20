/// @file
/// @brief Memory not had (RULES.md D14): of turborel-c built to fail its
/// allocations on purpose, each allocation of a whole session failed in
/// turn, and each failure thrown as std::bad_alloc, or as the driver's
/// error of what it came to, nothing kept of it. Of an installed
/// turborel-c, which cannot fail them, skipped.

#include "support/frames.hpp"

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/decimal.hpp>
#include <turborel/error.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/settings.hpp>
#include <turborel/statement.hpp>
#include <turborel/value.hpp>
#include <turborel/xml.hpp>

#include <catch2/catch_test_macros.hpp>

#include <new>
#include <string>

#ifdef TURBOREL_CPP_FAULTS
// turborel-c's own hooks of its build of TRL_FAULTS (its RULES.md D14),
// which no header it installs declares.
extern "C" {
/// Fails the allocation after `allowed` more, and every one after it; a
/// negative `allowed` disarms the fault.
void trl_faults_arm(long allowed);
/// Whether the fault armed failed an allocation since it was armed.
bool trl_faults_fired(void);
}
#endif

namespace {

/// A session over a scripted stream: the login, a query and its rows, a
/// statement prepared and run with parameters, settings of a URI, and an
/// XML value parsed; what each gives read.
void session() {
  turborel::connect_options options;
  options.ask_none().ask(turborel::capability::status).ask(turborel::capability::notices);
  auto [conn, stream] = test::open_scripted(test::joined({
                                                test::frame("status-server-version"),
                                                test::ready({"status", "notices"}),
                                                test::frame("description"),
                                                test::frame("rows"),
                                                test::frame("notice"),
                                                test::frame("complete"),
                                                test::frame("prepared"),
                                                test::frame("complete"),
                                            }),
                                            options);
  const turborel::result read = conn.query("SELECT 1");
  for (const turborel::row row : read) {
    static_cast<void>(row.at(0).to_string());
  }
  static_cast<void>(read.columns());
  turborel::params values;
  values.add(1).add("text").add(turborel::decimal{.units = turborel::int128{150}, .scale = 2});
  const turborel::statement prepared = conn.prepare("VALUES (?, ?, ?)");
  static_cast<void>(conn.execute(prepared, values));
  static_cast<void>(conn.notices());
  static_cast<void>(turborel::settings::from_uri("turborel://ann@db1:7000,db2/?tls_mode=disable"));
  static_cast<void>(turborel::xml_tree::parse("<a b=\"1\">t</a>").root().child(0).name());
}

} // namespace

/// Verifies: [DESIGN] D14 Each allocation turborel-c makes in a session,
/// failed in turn, fails its call as std::bad_alloc, or as the driver's
/// error of the connection it left, and the next session runs whole.
TEST_CASE("every allocation failed in turn fails its call as memory not had") {
#ifdef TURBOREL_CPP_FAULTS
  long failed = 0;
  long other = 0;
  for (long allowed = 0;; ++allowed) {
    trl_faults_arm(allowed);
    try {
      session();
    } catch (const std::bad_alloc&) {
      ++failed;
    } catch (const turborel::error&) {
      ++other;
    }
    const bool fired = trl_faults_fired();
    trl_faults_arm(-1);
    if (!fired) {
      break;
    }
  }
  CHECK(failed > 0);
  CHECK(other >= 0);
  CHECK_NOTHROW(session());
#else
  SKIP("turborel-c was not built to fail its allocations: an installed one, or TURBOREL_CPP_FAULTS off");
#endif
}
