/// @file
/// @brief Pipelines through turborel-c [CLIENT Pipelines].

#include <turborel/pipeline.hpp>

#include "capi.hpp"
#include "connection_state.hpp"

#include <turborel/connection.hpp>
#include <turborel/error.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/statement.hpp>
#include <turborel/turborel.h>

#include <cstddef>
#include <optional>
#include <string>

namespace turborel {

void pipeline::freer::operator()(trl_pipeline* made) const noexcept { trl_pipeline_free(made); }

pipeline::pipeline(connection& conn) : conn_{&conn} {
  trl_pipeline* made = nullptr;
  detail::check_on(conn, trl_pipeline_new(conn.get(), &made));
  pipeline_.reset(made);
}

pipeline& pipeline::query(const std::string& sql) {
  detail::check_on(*conn_, trl_pipeline_query(pipeline_.get(), sql.c_str()));
  return *this;
}

pipeline& pipeline::execute(const statement& prepared, const params& values) {
  detail::check_on(*conn_, trl_pipeline_execute(pipeline_.get(), prepared.get(), values.get()));
  return *this;
}

pipeline& pipeline::when_one_fails(on_error what) {
  const trl_on_error c_what = what == on_error::skip ? TRL_ON_ERROR_SKIP : TRL_ON_ERROR_CONTINUE;
  detail::check_on(*conn_, trl_pipeline_on_error(pipeline_.get(), c_what));
  return *this;
}

std::size_t pipeline::size() const noexcept { return trl_pipeline_count(pipeline_.get()); }

void pipeline::run() { detail::check_on(*conn_, trl_pipeline_run(pipeline_.get())); }

piped pipeline::outcome(std::size_t request) const noexcept {
  switch (trl_pipeline_outcome(pipeline_.get(), request)) {
  case TRL_PIPED_DONE:
    return piped::done;
  case TRL_PIPED_FAILED:
    return piped::failed;
  case TRL_PIPED_SKIPPED:
    break;
  }
  return piped::skipped;
}

result pipeline::result_of(std::size_t request) const {
  const trl_result* ran = trl_pipeline_result(pipeline_.get(), request);
  if (ran == nullptr) {
    detail::raise_misuse("request " + std::to_string(request) + " of the pipeline did not run");
  }
  return result::borrowed(ran);
}

std::optional<failure> pipeline::failure_of(std::size_t request) const {
  const trl_failure* failed = trl_pipeline_failure(pipeline_.get(), request);
  return failed == nullptr ? std::nullopt : std::optional<failure>{detail::failure_of(failed)};
}

} // namespace turborel
