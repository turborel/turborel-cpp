/// @file
/// @brief Pools of connections [CLIENT Pools]: a bounded set a program's
/// threads share. A pool hands out an idle connection that answers, or
/// makes one while fewer than its most are made, or waits for one to come
/// back; a connection comes back reset as its handle is destroyed. The
/// pool starts no thread; each wait is the waiting thread's own (RULES.md
/// D2).

#ifndef TURBOREL_POOL_HPP
#define TURBOREL_POOL_HPP

#include <turborel/connection.hpp>
#include <turborel/export.hpp>
#include <turborel/settings.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace turborel {

/// The connections a pool makes at most, and keeps idle at most, by default.
inline constexpr std::size_t pool_max = 10;
/// How long a pool's connection may stay idle by default.
inline constexpr std::chrono::minutes pool_idle_timeout{10};
/// How long a pool's connection lives by default.
inline constexpr std::chrono::hours pool_max_lifetime{1};
/// The most statements a connection given back may have left prepared and
/// still be reset by statements, of a server that grants no Reset; one
/// with more is closed rather than kept.
inline constexpr std::size_t pool_reset_most_statements = 256;
/// How long getting a connection waits by default.
inline constexpr std::chrono::seconds pool_wait{30};

/// How a pool keeps its connections [CLIENT Pools].
struct pool_config {
  /// The most connections made at once, handed out and idle.
  std::size_t max = pool_max;
  /// The most kept idle.
  std::size_t max_idle = pool_max;
  /// How long one may stay idle; zero, as long as it lives.
  std::chrono::milliseconds idle_timeout = pool_idle_timeout;
  /// How long one lives from when it was made; zero, without limit.
  std::chrono::milliseconds max_lifetime = pool_max_lifetime;
  /// How long getting one waits for one to come back.
  std::chrono::milliseconds wait = pool_wait;
  /// Whether an idle connection is checked to answer before it is handed out.
  bool check = true;
  /// Statements run, after the pool's own, as each connection comes back.
  std::vector<std::string> reset;
};

class pool;

namespace detail {
struct pool_shared;
} // namespace detail

/// A connection a pool handed out, used as a connection and given back,
/// reset, as it is destroyed.
class TURBOREL_CPP_EXPORT pooled {
public:
  pooled(const pooled&) = delete;
  pooled& operator=(const pooled&) = delete;
  /// Moves the handle.
  /// @param other The handle.
  pooled(pooled&& other) noexcept;
  /// Moves the handle, giving back the one this held.
  /// @param other The handle.
  /// @return This.
  pooled& operator=(pooled&& other) noexcept;
  /// Gives the connection back.
  ~pooled();

  /// The connection.
  /// @return It.
  [[nodiscard]] connection& operator*() noexcept { return conn_; }
  /// The connection.
  /// @return It.
  [[nodiscard]] connection* operator->() noexcept { return &conn_; }

private:
  friend class pool;

  pooled(std::shared_ptr<detail::pool_shared> from, connection conn) noexcept;

  /// Gives the connection back, if this holds one.
  void give_back() noexcept;

  std::shared_ptr<detail::pool_shared> from_;
  connection conn_;
  bool held_{true};
};

/// A pool of connections. Its copies are the same pool, for other threads.
class TURBOREL_CPP_EXPORT pool {
public:
  /// Makes the connections of a pool of the program's own.
  using connector = std::function<connection()>;

  /// A pool of the connections `how` makes, kept as `config` says.
  /// @param how The settings, which the pool keeps.
  /// @param config How the pool keeps its connections.
  /// @return The pool.
  /// @throws turborel::error Of a `max` of 0 (-910).
  [[nodiscard]] static pool of(settings how, const pool_config& config = pool_config{});

  /// A pool of the connections `make` makes, from any thread that gets one,
  /// as several may at once; one over the program's own stream is refused.
  /// @param make What makes a connection.
  /// @param config How the pool keeps its connections.
  /// @return The pool.
  /// @throws turborel::error Of a `max` of 0 (-910).
  [[nodiscard]] static pool of(connector make, const pool_config& config = pool_config{});

  /// Gets a connection: an idle one that answers, else a new one while the
  /// pool has room, else the first to come back within `wait`.
  /// @return The connection.
  /// @throws turborel::error If none came back in time (08001), or the
  ///   failure of making one.
  [[nodiscard]] pooled get();

  /// How many connections it has made, handed out and idle.
  /// @return How many.
  [[nodiscard]] std::size_t made() const;

  /// How many are idle.
  /// @return How many.
  [[nodiscard]] std::size_t idle() const;

private:
  explicit pool(std::shared_ptr<detail::pool_shared> held) noexcept : held_{std::move(held)} {}

  std::shared_ptr<detail::pool_shared> held_;
};

} // namespace turborel

#endif // TURBOREL_POOL_HPP
