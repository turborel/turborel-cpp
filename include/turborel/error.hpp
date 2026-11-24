/// @file
/// @brief The driver's error: a statement's or a connection's failure,
/// thrown as an exception (RULES.md D10), with every field of the protocol's
/// failure [PROTOCOL Composite fields] and its diagnostics [PROTOCOL Errors
/// and notices].

#ifndef TURBOREL_ERROR_HPP
#define TURBOREL_ERROR_HPP

#include <turborel/export.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace turborel {

/// What a failure says beyond its SQLSTATE and message, when the server
/// granted the capability `diagnostics`: its tail, tags 7 to 15
/// [PROTOCOL Errors and notices]. Each field is empty where the failure
/// says nothing of it.
struct diagnostics {
  /// ERROR, FATAL, WARNING or NOTICE (tag 7).
  std::optional<std::string> severity;
  /// More than the message says, as the key a constraint refused (tag 8).
  std::optional<std::string> detail;
  /// What may be done about it (tag 9).
  std::optional<std::string> hint;
  /// The trigger or routine running, as `trigger S.T` (tag 10).
  std::optional<std::string> context;
  /// The schema of what it is about (tag 11).
  std::optional<std::string> schema;
  /// The table or view it is about (tag 12).
  std::optional<std::string> table;
  /// The column it is about (tag 13).
  std::optional<std::string> column;
  /// The data type of the value it is about (tag 14).
  std::optional<std::string> data_type;
  /// The constraint broken (tag 15).
  std::optional<std::string> constraint;

  /// Whether two hold the same fields.
  friend bool operator==(const diagnostics&, const diagnostics&) = default;
};

/// A failure as the protocol writes it [PROTOCOL Composite fields]: what
/// went wrong, the rule it cites, where in the statement, and whether a
/// transaction is still active.
struct failure {
  /// Its SQLCODE, negative for an exception.
  std::int32_t sqlcode{};
  /// Its SQLSTATE, five characters.
  std::string sqlstate;
  /// What went wrong, in plain words.
  std::string message;
  /// The clause of a standard it cites, if it cites one.
  std::optional<std::string> rule;
  /// Where in the statement, counting characters, if known.
  std::optional<std::uint64_t> position;
  /// Whether the session's transaction is active after it.
  bool active{};
  /// Its tail, if the server granted `diagnostics`.
  std::optional<turborel::diagnostics> diagnostics;

  /// Whether two hold the same fields.
  friend bool operator==(const failure&, const failure&) = default;
};

/// The exception every failure of the driver is thrown as (RULES.md D10).
///
/// It is a `std::runtime_error`, so code that catches those catches it, and
/// its `what()` reads as turborel-client shows a failure:
/// `SQLCODE -103 (SQLSTATE 42000): message [rule] (at character 15)`.
/// Copying it never throws: the failure is shared by its copies.
class TURBOREL_CPP_EXPORT error : public std::runtime_error {
public:
  /// An error of `what_failed`.
  /// @param what_failed The failure, as the server or the driver gave it.
  /// @throws std::bad_alloc If its text cannot be allocated.
  explicit error(failure what_failed);

  /// The failure, whole.
  /// @return Its failure, as it was made.
  [[nodiscard]] const failure& details() const noexcept;

  /// Its SQLCODE.
  /// @return The failure's SQLCODE.
  [[nodiscard]] std::int32_t sqlcode() const noexcept;

  /// Its SQLSTATE.
  /// @return The failure's SQLSTATE, five characters.
  [[nodiscard]] std::string_view sqlstate() const noexcept;

  /// What went wrong, in plain words, without the SQLCODE and SQLSTATE
  /// that `what()` adds.
  /// @return The failure's message.
  [[nodiscard]] std::string_view message() const noexcept;

private:
  std::shared_ptr<const failure> failure_;
};

} // namespace turborel

#endif // TURBOREL_ERROR_HPP
