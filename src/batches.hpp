/// @file
/// @brief A query's rows given to the program's handler a batch at a time
/// [CLIENT Running statements], as turborel-c calls the handler, shared by
/// connections and the SQL-agent (RULES.md D11).

#ifndef TURBOREL_SRC_BATCHES_HPP
#define TURBOREL_SRC_BATCHES_HPP

#include <turborel/result.hpp>
#include <turborel/turborel.h>

#include <exception>
#include <functional>

namespace turborel::detail {

/// A batch handler's context: the program's handler, and what it threw.
struct batches {
  /// The program's handler.
  const std::function<void(const result&)>* handler;
  /// What it threw, thrown once the query returns.
  std::exception_ptr thrown;
};

/// Gives a batch to the program's handler, as turborel-c calls it, keeping
/// what the handler throws and giving it no more batches after (R-RAII-2).
/// @param context The batches.
/// @param batch The batch, borrowed.
void batch_to_handler(void* context, const trl_result* batch) noexcept;

} // namespace turborel::detail

#endif // TURBOREL_SRC_BATCHES_HPP
