/// @file
/// @brief Pipelines [CLIENT Pipelines]: a connection's requests queued,
/// written at once, a window at a time, and one outcome read for each, in
/// order, so a batch costs one round trip, not one a request (RULES.md D2).

#ifndef TURBOREL_PIPELINE_HPP
#define TURBOREL_PIPELINE_HPP

#include <turborel/connection.hpp>
#include <turborel/error.hpp>
#include <turborel/export.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/statement.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

struct trl_pipeline;

namespace turborel {

/// The most bytes of requests written before their replies are read.
inline constexpr std::size_t pipeline_window = 65536;

/// What a pipeline does after a request fails.
enum class on_error : std::uint8_t {
  /// Every request runs; a failure is its request's outcome alone.
  keep_going,
  /// The batch stops at its first failure: the requests after it undone, or
  /// never sent, each skipped.
  skip,
};

/// What one request of a pipeline came to.
enum class piped : std::uint8_t {
  /// It ran.
  done,
  /// It failed, as the server reported it.
  failed,
  /// Not run, or run and undone, after an earlier request failed under
  /// on_error::skip.
  skipped,
};

/// Requests queued on a connection, written at once; it runs once.
class TURBOREL_CPP_EXPORT pipeline {
public:
  /// An empty pipeline on `conn`, which it uses until destroyed;
  /// on_error::keep_going.
  /// @param conn The connection.
  explicit pipeline(connection& conn);

  /// Queues a statement, run as connection::query runs it.
  /// @param sql The statement.
  /// @return The pipeline.
  pipeline& query(const std::string& sql);

  /// Queues an execution of a prepared statement, as connection::execute
  /// runs it.
  /// @param prepared The statement, which must last until the pipeline runs.
  /// @param values Its parameters' values, copied.
  /// @return The pipeline.
  pipeline& execute(const statement& prepared, const params& values = params{});

  /// Says what the batch does after a request fails.
  /// @param what What.
  /// @return The pipeline.
  pipeline& when_one_fails(on_error what);

  /// How many requests are queued, or, once run, how many outcomes it has.
  /// @return How many.
  [[nodiscard]] std::size_t size() const noexcept;

  /// Writes the requests queued, a window at a time, and reads one outcome
  /// for each, in order. Under autocommit each request is its own
  /// transaction; under on_error::skip the batch is one transaction.
  /// @throws turborel::error Of a pipeline run already (-910), the
  ///   connection lost (08006), or a skipping batch that cannot be undone.
  void run();

  /// What a request came to.
  /// @param request Which, from 0, in the order queued.
  /// @return Its outcome; skipped past the last.
  [[nodiscard]] piped outcome(std::size_t request) const noexcept;

  /// The result of a request that ran.
  /// @param request Which.
  /// @return Its result, a view valid while the pipeline is.
  /// @throws turborel::error Of one that did not run (-910).
  [[nodiscard]] result result_of(std::size_t request) const;

  /// How a request failed.
  /// @param request Which.
  /// @return Its failure; none of one that did not fail.
  [[nodiscard]] std::optional<failure> failure_of(std::size_t request) const;

private:
  /// Frees a pipeline.
  struct freer {
    /// Frees `made`.
    /// @param made The pipeline.
    void operator()(trl_pipeline* made) const noexcept;
  };

  connection* conn_;
  std::unique_ptr<trl_pipeline, freer> pipeline_;
};

} // namespace turborel

#endif // TURBOREL_PIPELINE_HPP
