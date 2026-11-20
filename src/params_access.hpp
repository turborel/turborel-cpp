/// @file
/// @brief The source files' way to a view of a parameter list turborel-c
/// lends a callback, as a copy's or an import's source fills it.

#ifndef TURBOREL_SRC_PARAMS_ACCESS_HPP
#define TURBOREL_SRC_PARAMS_ACCESS_HPP

#include <turborel/params.hpp>
#include <turborel/turborel.h>

namespace turborel::detail {

/// Makes views of lent lists.
struct params_access {
  /// A view of `lent`, for the callback alone.
  /// @param lent The list.
  /// @return The view.
  [[nodiscard]] static params lent(trl_params* lent) noexcept { return params{lent}; }
};

} // namespace turborel::detail

#endif // TURBOREL_SRC_PARAMS_ACCESS_HPP
