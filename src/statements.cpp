/// @file
/// @brief Statements through turborel-c [CLIENT Running statements]
/// [CLIENT Prepared statements]: Query, its rows whole or a batch at a
/// time; prepared statements; several statements as one request.

#include <turborel/connection.hpp>
#include <turborel/statement.hpp>

#include "batches.hpp"
#include "capi.hpp"
#include "connection_state.hpp"

#include <turborel/error.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/turborel.h>
#include <turborel/value.hpp>

#include <cstddef>
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace turborel {

void detail::batch_to_handler(void* context, const trl_result* batch) noexcept {
  auto* held = static_cast<batches*>(context);
  if (held->thrown) {
    return;
  }
  try {
    (*held->handler)(result::borrowed(batch));
  } catch (...) {
    held->thrown = std::current_exception();
  }
}

void statement::freer::operator()(trl_statement* made) const noexcept { trl_statement_free(made); }

statement::statement(trl_statement* made) noexcept : statement_{made} {}

std::size_t statement::parameter_count() const noexcept { return trl_statement_parameter_count(get()); }

data_type statement::parameter_type(std::size_t index) const {
  const trl_data_type* type = trl_statement_parameter_type(get(), index);
  if (type == nullptr) {
    detail::raise_misuse("the statement has no parameter " + std::to_string(index));
  }
  return data_type{type};
}

std::vector<column> statement::columns() const {
  std::vector<column> columns;
  columns.reserve(trl_statement_column_count(get()));
  for (std::size_t i = 0; i < trl_statement_column_count(get()); ++i) {
    columns.push_back(column{
        .name = detail::maybe(trl_statement_column_name(get(), i)),
        .type = data_type{trl_statement_column_type(get(), i)},
        .declared_name = detail::maybe(trl_statement_column_type_name(get(), i)),
        .type_written = detail::view(trl_statement_column_type_written(get(), i)),
        .not_null = trl_statement_column_not_null(get(), i),
    });
  }
  return columns;
}

void ran::freer::operator()(trl_ran* made) const noexcept { trl_ran_free(made); }

ran::ran(trl_ran* made) noexcept : ran_{made} {}

std::size_t ran::size() const noexcept { return trl_ran_count(ran_.get()); }

result ran::at(std::size_t index) const {
  const trl_result* outcome = trl_ran_result(ran_.get(), index);
  if (outcome == nullptr) {
    detail::raise_misuse("no statement came to outcome " + std::to_string(index));
  }
  return result::borrowed(outcome);
}

std::optional<failure> ran::stopped_by() const {
  const trl_failure* stopped = trl_ran_failure(ran_.get());
  return stopped == nullptr ? std::nullopt : std::optional<failure>{detail::failure_of(stopped)};
}

result connection::query(const std::string& sql) {
  trl_result* made = nullptr;
  const trl_status status = trl_query(get(), sql.c_str(), &made);
  result outcome{made};
  detail::check_on(*this, status);
  return outcome;
}

result connection::query_with(const std::string& sql, const params& values) {
  const statement prepared = prepare(sql);
  try {
    result outcome = execute(prepared, values);
    deallocate(prepared);
    return outcome;
  } catch (const error&) {
    // The statement's failure is what the program is given; one of
    // deallocating after it changes nothing the session has not told.
    if (!lost()) {
      static_cast<void>(trl_deallocate(get(), prepared.get()));
    }
    throw;
  }
}

result connection::query_text(const std::string& sql, const std::vector<std::optional<std::string>>& values) {
  std::vector<const char*> texts;
  texts.reserve(values.size());
  for (const std::optional<std::string>& text : values) {
    texts.push_back(text ? text->c_str() : nullptr);
  }
  trl_result* made = nullptr;
  const trl_status status = trl_query_params(get(), sql.c_str(), texts.size(), texts.data(), &made);
  result outcome{made};
  detail::check_on(*this, status);
  return outcome;
}

result connection::query_batches(const std::string& sql, const std::function<void(const result&)>& handler) {
  detail::batches context{.handler = &handler, .thrown = nullptr};
  trl_result* made = nullptr;
  const trl_status status = trl_query_batches(get(), sql.c_str(), detail::batch_to_handler, &context, &made);
  result outcome{made};
  if (context.thrown) {
    std::rethrow_exception(context.thrown);
  }
  detail::check_on(*this, status);
  return outcome;
}

statement connection::prepare(const std::string& sql) {
  trl_statement* made = nullptr;
  const trl_status status = trl_prepare(get(), sql.c_str(), &made);
  statement prepared{made};
  detail::check_on(*this, status);
  return prepared;
}

result connection::execute(const statement& prepared, const params& values) {
  trl_result* made = nullptr;
  const trl_status status = trl_execute(get(), prepared.get(), values.get(), &made);
  result outcome{made};
  detail::check_on(*this, status);
  return outcome;
}

statement connection::describe(const statement& prepared) {
  trl_statement* made = nullptr;
  const trl_status status = trl_describe(get(), prepared.get(), &made);
  statement described{made};
  detail::check_on(*this, status);
  return described;
}

void connection::deallocate(const statement& prepared) {
  detail::check_on(*this, trl_deallocate(get(), prepared.get()));
}

ran connection::query_many(const std::string& sql) {
  trl_ran* made = nullptr;
  const trl_status status = trl_query_many(get(), sql.c_str(), &made);
  ran outcome{made};
  detail::check_on(*this, status);
  return outcome;
}

} // namespace turborel
