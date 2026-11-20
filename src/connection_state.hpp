/// @file
/// @brief What a connection holds beside turborel-c's handle (RULES.md
/// D10): its notice handler, an exception the handler threw, and the
/// program's stream; and the check each call on it ends with.

#ifndef TURBOREL_SRC_CONNECTION_STATE_HPP
#define TURBOREL_SRC_CONNECTION_STATE_HPP

#include <turborel/connection.hpp>
#include <turborel/turborel.h>

#include "capi.hpp"

#include <exception>
#include <memory>
#include <utility>

namespace turborel {

/// What a connection holds. It does not move, so turborel-c's callbacks
/// may keep its address as their context.
struct connection::state {
  /// The program's stream, of a connection opened over one; declared
  /// first, so it outlives the connection, which closes it as it is freed.
  std::unique_ptr<byte_stream> stream;
  /// The connection.
  detail::owner<trl_conn, trl_conn_free> conn;
  /// The program's notice handler, if it set one.
  notice_handler handler;
  /// What the handler threw, thrown again once turborel-c's call returns.
  std::exception_ptr pending;
};

namespace detail {

/// The source files' way to a connection's state.
struct conn_access {
  /// The state of `conn`.
  /// @param conn The connection.
  /// @return Its state.
  [[nodiscard]] static connection::state& of(const connection& conn) noexcept { return *conn.state_; }

  /// Takes turborel-c's connection out of `conn`, which then holds none:
  /// its notice handler, which names the state, cleared first.
  /// @param conn The connection.
  /// @return turborel-c's connection, the caller's.
  [[nodiscard]] static trl_conn* release(connection& conn) noexcept {
    connection::state& held = of(conn);
    static_cast<void>(trl_conn_set_notice_handler(held.conn.get(), nullptr, nullptr));
    held.handler = nullptr;
    return held.conn.release();
  }

  /// Gives a Notice to the handler of a connection's state, as turborel-c
  /// calls it, keeping what the handler throws until the call that read
  /// the Notice returns (R-RAII-2).
  /// @param context The state.
  /// @param notice The Notice.
  static void notice(void* context, const trl_failure* notice) noexcept;

  /// Whether `conn` is over the program's own stream.
  /// @param conn The connection.
  /// @return Whether it is.
  [[nodiscard]] static bool over_stream(const connection& conn) noexcept { return of(conn).stream != nullptr; }

  /// Ends a call on `conn`: throws what a notice handler threw during it,
  /// if one did, else the connection's failure of a status other than OK.
  /// @param conn The connection.
  /// @param status The call's status.
  /// @throws turborel::error As the status says.
  static void check(const connection& conn, trl_status status) {
    connection::state& held = of(conn);
    if (held.pending) {
      const std::exception_ptr thrown = std::exchange(held.pending, nullptr);
      std::rethrow_exception(thrown);
    }
    detail::check(status, trl_conn_failure(held.conn.get()));
  }
};

/// Ends a call on `conn`, as conn_access::check does.
/// @param conn The connection.
/// @param status The call's status.
/// @throws turborel::error As the status says.
inline void check_on(const connection& conn, trl_status status) { conn_access::check(conn, status); }

} // namespace detail

} // namespace turborel

#endif // TURBOREL_SRC_CONNECTION_STATE_HPP
