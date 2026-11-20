/// @file
/// @brief The SQL-agent [CLIENT The SQL-agent]: a program's connections,
/// each named, one current. It runs CONNECT, SET CONNECTION and DISCONNECT
/// itself, as SQL-92's SQL-agent does, and sends every other statement to
/// the current connection. One thread uses it at a time (RULES.md D2).

#ifndef TURBOREL_AGENT_HPP
#define TURBOREL_AGENT_HPP

#include <turborel/connection.hpp>
#include <turborel/export.hpp>
#include <turborel/result.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace turborel {

namespace detail {
struct agent_state;
} // namespace detail

/// An SQL-agent.
class TURBOREL_CPP_EXPORT agent {
public:
  /// Makes the connection CONNECT asks for: to the server CONNECT TO names,
  /// none for CONNECT TO DEFAULT, as the user USER names, none for the
  /// connector's own.
  using connector =
      std::function<connection(const std::optional<std::string>& server, const std::optional<std::string>& user)>;

  /// An agent with no connection.
  /// @param make What makes its connections.
  explicit agent(connector make);

  /// An agent whose default connection, current, is `first`: as a client
  /// begins, connected.
  /// @param make What makes its other connections.
  /// @param first The connection, the agent's after.
  agent(connector make, connection first);

  agent(const agent&) = delete;
  agent& operator=(const agent&) = delete;
  /// Moves the agent.
  agent(agent&&) noexcept;
  /// Moves the agent.
  /// @return This.
  agent& operator=(agent&&) noexcept;
  /// Closes each connection it holds.
  ~agent();

  /// Runs one statement: CONNECT, SET CONNECTION or DISCONNECT itself, its
  /// result the completion, 00000, or 01002 of a DISCONNECT that met an
  /// error ending a connection; any other in the current connection's
  /// session.
  /// @param sql The statement.
  /// @return Its result.
  /// @throws turborel::error CONNECT: 0A001 in an active transaction,
  ///   28000, 2E000, 08002 of a name in use, 08001, 08004; SET CONNECTION:
  ///   0A001, 08003; DISCONNECT: 08003, 25000; another with 08003 of no
  ///   current connection.
  result query(const std::string& sql);

  /// The current connection's name.
  /// @return It; none for the default connection, or for none.
  [[nodiscard]] std::optional<std::string_view> current_name() const noexcept;

  /// Whether it has a current connection.
  /// @return Whether it does.
  [[nodiscard]] bool has_current() const noexcept;

  /// How many connections it holds, current and dormant.
  /// @return How many.
  [[nodiscard]] std::size_t size() const noexcept;

  /// Ends every connection by Terminate, every one ended though one fails.
  /// @throws turborel::error The first failure.
  void terminate();

private:
  std::unique_ptr<detail::agent_state> state_;
};

} // namespace turborel

#endif // TURBOREL_AGENT_HPP
