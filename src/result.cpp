/// @file
/// @brief Results and their rows, read through turborel-c (RULES.md D2).

#include <turborel/result.hpp>

#include "capi.hpp"

#include <turborel/turborel.h>
#include <turborel/value.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace turborel {

std::size_t row::size() const noexcept { return trl_result_column_count(result_); }

value row::at(std::size_t column) const {
  const trl_value* found = trl_result_value(result_, index_, column);
  if (found == nullptr) {
    detail::raise_misuse("the row has no column " + std::to_string(column));
  }
  return value{found};
}

void result::freer::operator()(trl_result* result) const noexcept { trl_result_free(result); }

result::result(trl_result* result) noexcept : owned_{result}, view_{result} {}

result result::borrowed(const trl_result* result) noexcept {
  turborel::result view{nullptr};
  view.view_ = result;
  return view;
}

bool result::has_rows() const noexcept { return trl_result_has_rows(view_); }

std::vector<column> result::columns() const {
  std::vector<column> columns;
  const std::size_t count = trl_result_column_count(view_);
  columns.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    columns.push_back(column{
        .name = detail::maybe(trl_result_column_name(view_, i)),
        .type = data_type{trl_result_column_type(view_, i)},
        .declared_name = detail::maybe(trl_result_column_type_name(view_, i)),
        .type_written = detail::view(trl_result_column_type_written(view_, i)),
    });
  }
  return columns;
}

std::size_t result::size() const noexcept { return trl_result_row_count(view_); }

row result::at(std::size_t index) const {
  if (index >= size()) {
    detail::raise_misuse("the result has no row " + std::to_string(index));
  }
  return row{view_, index};
}

std::int32_t result::sqlcode() const noexcept { return trl_result_sqlcode(view_); }

std::string_view result::sqlstate() const noexcept { return detail::view(trl_result_sqlstate(view_)); }

bool result::more() const noexcept { return trl_result_more(view_); }

} // namespace turborel
