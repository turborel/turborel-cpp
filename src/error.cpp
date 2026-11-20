/// @file
/// @brief The driver's error (RULES.md D10): its text, made once as it is
/// thrown, in the form turborel-client shows a failure.

#include <turborel/error.hpp>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace turborel {

namespace {

/// The text of `what_failed`, as turborel-client's `Display` of a failure
/// writes it, so a message reads the same from every client.
std::string describe(const failure& what_failed) {
  std::string text = "SQLCODE " + std::to_string(what_failed.sqlcode) + " (SQLSTATE " + what_failed.sqlstate +
                     "): " + what_failed.message;
  if (what_failed.rule) {
    text += " [" + *what_failed.rule + "]";
  }
  if (what_failed.position) {
    text += " (at character " + std::to_string(*what_failed.position) + ")";
  }
  return text;
}

} // namespace

error::error(failure what_failed)
    : std::runtime_error(describe(what_failed)), failure_(std::make_shared<const failure>(std::move(what_failed))) {}

const failure& error::details() const noexcept { return *failure_; }

std::int32_t error::sqlcode() const noexcept { return failure_->sqlcode; }

std::string_view error::sqlstate() const noexcept { return failure_->sqlstate; }

std::string_view error::message() const noexcept { return failure_->message; }

} // namespace turborel
