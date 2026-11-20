/// @file
/// @brief Connections [CLIENT Connecting]: a session of the server's,
/// opened by a target, a user and a mechanism, by a URI, or over a stream
/// the program opened; what it was granted and what the server tells it;
/// its session's control; and how it ends, or its statement is cancelled
/// from another thread (RULES.md D2).

#ifndef TURBOREL_CONNECTION_HPP
#define TURBOREL_CONNECTION_HPP

#include <turborel/connect.hpp>
#include <turborel/error.hpp>
#include <turborel/export.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/statement.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct trl_conn;
struct trl_canceller;

namespace turborel {

/// A stream the program opened, such as a TLS stream of its own, that a
/// connection reads and writes through [CLIENT Connecting]. The caller
/// answers for its privacy: no mechanism is refused over it.
class TURBOREL_CPP_EXPORT byte_stream {
public:
  byte_stream() = default;
  byte_stream(const byte_stream&) = delete;
  byte_stream(byte_stream&&) = delete;
  byte_stream& operator=(const byte_stream&) = delete;
  byte_stream& operator=(byte_stream&&) = delete;
  /// Closes nothing: close() is the connection's to call.
  virtual ~byte_stream() = default;

  /// Reads at most `into.size()` bytes.
  /// @param into Where they go.
  /// @return How many, 0 at the stream's end; none if it failed.
  virtual std::optional<std::size_t> read(std::span<std::byte> into) = 0;

  /// Writes some of `bytes`, at least one.
  /// @param bytes The bytes.
  /// @return How many; none if it failed.
  virtual std::optional<std::size_t> write(std::span<const std::byte> bytes) = 0;

  /// Closes the stream, once, as the connection is freed.
  virtual void close() noexcept = 0;

  /// Whether the stream bounds how long each read and write waits; one
  /// that does not waits as its maker set it [CLIENT Timeouts].
  /// @return Whether it does.
  [[nodiscard]] virtual bool bounds() const noexcept { return false; }

  /// Bounds how long each read and write waits, of a stream that bounds();
  /// zero for no bound.
  /// @param wait How long.
  /// @return Whether it could.
  virtual bool bound(std::chrono::milliseconds wait) noexcept {
    static_cast<void>(wait);
    return false;
  }
};

/// The server's time for a statement [CLIENT What the server tells].
struct timing {
  /// From when a worker began it until its completion was ready.
  std::chrono::microseconds total{};
  /// Its request's wait for a worker.
  std::chrono::microseconds queued{};
  /// Its wait for locks.
  std::chrono::microseconds locks{};
};

/// What a server is, to a connection's session [CLIENT Several servers].
enum class server_role : std::uint8_t {
  /// It takes changes.
  read_write,
  /// It takes none: a replica following its group.
  read_only,
  /// A replica whose role the session cannot see.
  unknown,
};

class canceller;

namespace detail {
struct conn_access;
} // namespace detail

/// A connection, and the session it holds. One thread uses it at a time;
/// a canceller of it cancels its statement from another. Every failure is
/// thrown as turborel::error. Destroyed, it closes without Terminate.
class TURBOREL_CPP_EXPORT connection {
public:
  /// Given each Notice as it comes, as a failure of SQLCODE 0.
  using notice_handler = std::function<void(const failure&)>;

  /// Connects to `where` as `user`, proved by `proof` [CLIENT Connecting].
  /// To plain TCP, `password` and SCRAM not in the clear are refused before
  /// anything is sent (SQLSTATE 28000).
  /// @param where Where the server is.
  /// @param user The user, as an identifier, as "ann" for ANN.
  /// @param proof What proves the user.
  /// @param options What to ask for and how long to wait.
  /// @return The connection.
  /// @throws turborel::error If it cannot connect or log in.
  [[nodiscard]] static connection connect(const target& where, const std::string& user, mechanism& proof,
                                          const connect_options& options = connect_options{});

  /// Opens a connection over a stream the program opened, which it then
  /// owns and closes. A TLS stream binds SCRAM by mechanism::bind first.
  /// @param stream The stream.
  /// @param user The user.
  /// @param proof What proves the user.
  /// @param options What to ask for, and of a stream that bounds itself how
  ///   long it waits.
  /// @return The connection.
  /// @throws turborel::error If it cannot log in.
  [[nodiscard]] static connection open(std::unique_ptr<byte_stream> stream, const std::string& user, mechanism& proof,
                                       const connect_options& options = connect_options{});

  /// Connects as a `turborel://` URI says, with the rest of the settings
  /// [CLIENT The connection URI] [CLIENT Connecting by settings].
  /// @param uri The URI.
  /// @return The connection.
  /// @throws turborel::error If the URI cannot be read (08001), or no
  ///   server is connected to.
  [[nodiscard]] static connection open(const std::string& uri);

  /// A connection turborel-c made, which it owns and frees.
  /// @param made The connection.
  explicit connection(trl_conn* made);

  connection(const connection&) = delete;
  connection& operator=(const connection&) = delete;
  /// Moves the connection; its handler and stream move with it.
  connection(connection&&) noexcept;
  /// Moves the connection, closing the one this held.
  /// @return This.
  connection& operator=(connection&&) noexcept;
  /// Closes the connection, if open, without Terminate.
  ~connection();

  /// Whether the connection is lost: every call on it then fails.
  /// @return Whether it is.
  [[nodiscard]] bool lost() const noexcept;

  /// The server's product, as Ready told it.
  /// @return It, as "turborel 0.0.0 (alpha)".
  [[nodiscard]] std::string_view server() const noexcept;

  /// The session's authorization identifier, as Ready told it.
  /// @return It, as "ANN".
  [[nodiscard]] std::string_view authorization() const noexcept;

  /// Whether the server granted a capability [CLIENT What a connection asks
  /// for].
  /// @param name The capability, as capability::status.
  /// @return Whether it did.
  [[nodiscard]] bool granted(std::string_view name) const;

  /// The capabilities granted, in the order of the protocol's table.
  /// @return Their names.
  [[nodiscard]] std::vector<std::string_view> granted() const;

  /// A session setting as the server last told it, with no round trip
  /// [CLIENT What the server tells].
  /// @param name The setting, as "SESSION.TIME_ZONE" or "AUTHORIZATION".
  /// @return Its value, empty of a null; none if none of that name was told.
  [[nodiscard]] std::optional<std::string_view> setting(std::string_view name) const;

  /// Every setting the server told, in the order first told.
  /// @return Each name and value.
  [[nodiscard]] std::vector<std::pair<std::string_view, std::string_view>> settings() const;

  /// Changes the read timeout, for each read or write after; zero for none.
  /// @param wait The timeout.
  /// @throws turborel::error Of a stream its maker bounds (08006).
  void set_read_timeout(std::chrono::milliseconds wait);

  /// The read timeout; zero for none.
  /// @return It.
  [[nodiscard]] std::chrono::milliseconds read_timeout() const noexcept;

  /// Takes replies of at most `bytes` bytes, a longer one losing the
  /// connection; 16 MiB at first.
  /// @param bytes The bytes, at most 16 MiB.
  /// @throws turborel::error Of 0 or past 16 MiB (-910).
  void set_max_body(std::uint32_t bytes);

  /// Asks whether the server and the connection are alive: Ping, answered
  /// by Pong [CLIENT Controlling the session].
  /// @throws turborel::error If `ping` was not granted (08006, the
  ///   connection kept), or the connection is lost.
  void ping();

  /// Ends the session, its transaction as SESSION.COMMIT_AT_END says, and
  /// waits for the server to close. The connection is lost after.
  /// @throws turborel::error Of a commit that failed (40002), or of the
  ///   connection.
  void terminate();

  /// What cancels this connection's statement from another thread.
  /// @return The canceller.
  /// @throws turborel::error Of a connection over the program's own stream
  ///   (-910).
  [[nodiscard]] canceller make_canceller();

  /// Makes each statement its own transaction, committed as it succeeds or
  /// rolled back as it fails, with no COMMIT sent [CLIENT Running
  /// statements]; off at first.
  /// @param on Whether.
  void set_autocommit(bool on);

  /// Whether autocommit is on.
  /// @return Whether it is.
  [[nodiscard]] bool autocommit() const noexcept;

  /// Whether the session's transaction is active, as the server said after
  /// the last statement.
  /// @return Whether it is.
  [[nodiscard]] bool in_transaction() const noexcept;

  /// The SQLSTATE of the last statement's completion if it was a warning.
  /// @return It, of class 01; none otherwise.
  [[nodiscard]] std::optional<std::string_view> warning() const noexcept;

  /// The rows the last statement changed, or a query's rows, with
  /// `row-counts`.
  /// @return The count, if told.
  [[nodiscard]] std::optional<std::uint64_t> rows_affected() const noexcept;

  /// The identity values the last INSERT generated, as SQL writes them, in
  /// the order of its rows, with `generated-values`.
  /// @return The values.
  [[nodiscard]] std::vector<std::string_view> generated() const;

  /// The server's time for the last statement, with `row-counts`.
  /// @return The time, if told.
  [[nodiscard]] std::optional<timing> server_time() const noexcept;

  /// The log position of the last commit a statement told, with
  /// `commit-position`.
  /// @return The position, if told.
  [[nodiscard]] std::optional<std::uint64_t> commit_position() const noexcept;

  /// The last statement's conditions, with `conditions`.
  /// @return Each, as a failure of SQLCODE 0 or 100.
  [[nodiscard]] std::vector<failure> conditions() const;

  /// Drops the last statement's conditions, so each is told once.
  void clear_conditions() noexcept;

  /// Gives each Notice to `handler` as it comes, on the thread that reads
  /// it; an empty handler keeps the last 64 instead. A handler that throws
  /// has its exception thrown once the call that read the Notice returns.
  /// @param handler The handler, or none.
  void on_notice(notice_handler handler);

  /// The notices kept, the oldest first.
  /// @return Each, as a failure of SQLCODE 0.
  [[nodiscard]] std::vector<failure> notices() const;

  /// Drops the notices kept.
  void clear_notices() noexcept;

  /// Puts the session back as it was at login, with `reset`: its
  /// transaction rolled back, its prepared statements, cursors, temporary
  /// tables and settings gone, its user kept.
  /// @throws turborel::error If `reset` was not granted, or it failed.
  void reset();

  /// Authenticates the connection again as another user, whose new session
  /// it then is, with `change-user`.
  /// @param user The user.
  /// @param proof What proves the user.
  /// @throws turborel::error If `change-user` was not granted, or of a
  ///   refusal, which ends the connection.
  void change_user(const std::string& user, mechanism& proof);

  /// Sets attributes every later statement carries, read in SQL by
  /// QUERY_ATTRIBUTE(name), until set again; none clears them.
  /// @param attributes Each name, 1 to 64 bytes, and value, at most 1,024.
  /// @throws turborel::error If `query-attributes` was not granted, or past
  ///   the protocol's bounds, 32 of them (08006, the connection kept).
  void set_query_attributes(const std::vector<std::pair<std::string, std::string>>& attributes);

  /// Runs one statement by Query [CLIENT Running statements]
  /// [PROTOCOL Query 'Q']: a query's columns and every row, or another
  /// statement's completion. Under autocommit it is its own transaction.
  /// @param sql The statement, UTF-8.
  /// @return Its result.
  /// @throws turborel::error The statement's failure.
  result query(const std::string& sql);

  /// Runs one statement with its `?` parameters: prepared, executed with
  /// `first` and `values`, then deallocated.
  /// @param sql The statement.
  /// @param first Its first parameter's value, as params::add takes it.
  /// @param values The rest of its parameters' values.
  /// @return Its result.
  /// @throws turborel::error The statement's failure.
  template <typename First, typename... Rest>
  result query(const std::string& sql, const First& first, const Rest&... values) {
    return query_with(sql, params::of(first, values...));
  }

  /// Runs one statement with its `?` parameters given as `values`.
  /// @param sql The statement.
  /// @param values Its parameters' values.
  /// @return Its result.
  /// @throws turborel::error The statement's failure.
  result query_with(const std::string& sql, const params& values);

  /// Runs one statement with its `?` parameters given as text, which the
  /// server assigns to each parameter's type, as libpq's PQexecParams.
  /// @param sql The statement.
  /// @param values Each parameter's text, or none for the null value.
  /// @return Its result.
  /// @throws turborel::error The statement's failure.
  result query_text(const std::string& sql, const std::vector<std::optional<std::string>>& values);

  /// Runs one statement by Query, giving a query's rows to `handler` a
  /// batch at a time, as they come [CLIENT Running statements].
  /// @param sql The statement.
  /// @param handler Given each batch, valid for the call alone. What it
  ///   throws is thrown once the statement's replies are read.
  /// @return Of a query, its columns and no row; of another statement,
  ///   its completion.
  /// @throws turborel::error The statement's failure.
  result query_batches(const std::string& sql, const std::function<void(const result&)>& handler);

  /// Prepares a statement, its dynamic parameters written `?`
  /// [CLIENT Prepared statements].
  /// @param sql The statement.
  /// @return The statement.
  /// @throws turborel::error The statement's failure.
  [[nodiscard]] statement prepare(const std::string& sql);

  /// Executes a prepared statement with values: a query's rows, a CALL's
  /// output parameters as one row, another statement's completion.
  /// @param prepared The statement.
  /// @param values Its parameters' values.
  /// @return Its result.
  /// @throws turborel::error The statement's failure.
  result execute(const statement& prepared, const params& values = params{});

  /// Describes a prepared statement again, with `describe`.
  /// @param prepared The statement.
  /// @return The statement as now described, of the same number.
  /// @throws turborel::error If `describe` was not granted, or it failed.
  [[nodiscard]] statement describe(const statement& prepared);

  /// Deallocates a prepared statement in the session [PROTOCOL Deallocate
  /// 'D'].
  /// @param prepared The statement.
  /// @throws turborel::error As of a cursor of it open (24000).
  void deallocate(const statement& prepared);

  /// Runs a text of several statements as one request, each in order,
  /// until one fails, with `multi-statement`.
  /// @param sql The statements.
  /// @return What they came to.
  /// @throws turborel::error If `multi-statement` was not granted, or the
  ///   connection is lost.
  ran query_many(const std::string& sql);

  /// The role of the server, as the session sees it [CLIENT Several
  /// servers].
  /// @return The role.
  /// @throws turborel::error If it cannot be read.
  [[nodiscard]] server_role role();

  /// turborel-c's connection, for the calls that take it.
  /// @return It.
  [[nodiscard]] trl_conn* get() const noexcept;

private:
  friend struct detail::conn_access;
  struct state;

  std::unique_ptr<state> state_;
};

/// What cancels a connection's statement from another thread, by the key
/// its Ready gave, over a connection of its own [CLIENT Running
/// statements] [PROTOCOL Cancel 'Z'].
class TURBOREL_CPP_EXPORT canceller {
public:
  /// A canceller turborel-c made, which it owns and frees, its key cleared.
  /// @param made The canceller.
  explicit canceller(trl_canceller* made) noexcept;

  /// Cancels the statement the connection is running, giving up after 10
  /// seconds; the statement fails with SQLCODE -920, SQLSTATE TB920.
  /// @throws turborel::error If the server cannot be reached.
  void cancel();

  /// Cancels as cancel() does, giving up after `within`.
  /// @param within How long, zero for no limit.
  /// @throws turborel::error If the server cannot be reached in time.
  void cancel(std::chrono::milliseconds within);

private:
  /// Frees a canceller.
  struct freer {
    /// Frees `made`.
    /// @param made The canceller.
    void operator()(trl_canceller* made) const noexcept;
  };

  std::unique_ptr<trl_canceller, freer> canceller_;
};

} // namespace turborel

#endif // TURBOREL_CONNECTION_HPP
