/// @file
/// @brief Prepared statements [CLIENT Prepared statements]: their number,
/// parameters and columns; and what several statements run as one request
/// came to [CLIENT Controlling the session] (RULES.md D2).

#ifndef TURBOREL_STATEMENT_HPP
#define TURBOREL_STATEMENT_HPP

#include <turborel/error.hpp>
#include <turborel/export.hpp>
#include <turborel/result.hpp>
#include <turborel/value.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

struct trl_statement;
struct trl_ran;

namespace turborel {

/// The mode of the procedure parameter a `?` is the argument of, as the
/// protocol codes it [PROTOCOL Parameter modes].
enum class parameter_mode : std::uint8_t {
  /// An input, as every `?` of a statement other than a CALL is.
  in = 1,
  /// An input, and a column of the CALL's row.
  inout = 2,
  /// No input; a column of the CALL's row.
  out = 4,
};

/// A `?` of a prepared statement, an OUT one's too, in the order written
/// [PROTOCOL Parameter modes].
struct marker {
  /// Its parameter's mode: of a CALL of an SQL-invoked procedure, granted
  /// `parameter-modes`, the procedure's; else IN.
  parameter_mode mode;
  /// The ordinal position, from 1, of the procedure parameter it is the
  /// argument of; none of a statement whose Prepared told no procedure.
  std::optional<std::uint32_t> position;
  /// Its type, a view valid while the statement is.
  data_type type;
  /// Its value's place among the values execute() takes, of IN and INOUT;
  /// none of OUT.
  std::optional<std::size_t> parameter;
  /// The column of the CALL's row that gives its value back, of OUT and
  /// INOUT; none of IN.
  std::optional<std::size_t> column;
};

/// A prepared statement, its dynamic parameters written `?`
/// [PROTOCOL Prepare 'P'] [PROTOCOL Prepared 'P']. The session keeps it
/// until connection::deallocate or the session's end; the object only
/// names it.
class TURBOREL_CPP_EXPORT statement {
public:
  /// A statement turborel-c prepared, which it owns and frees.
  /// @param made The statement.
  explicit statement(trl_statement* made) noexcept;

  /// How many dynamic parameters it takes, its inputs alone.
  /// @return The count.
  [[nodiscard]] std::size_t parameter_count() const noexcept;

  /// The type a parameter takes.
  /// @param index Which, from 0.
  /// @return Its type.
  /// @throws turborel::error Past the last (-910).
  [[nodiscard]] data_type parameter_type(std::size_t index) const;

  /// Its columns: a query's, or of a CALL one for each `?` of an OUT or
  /// INOUT parameter; none of another statement.
  /// @return The columns, views valid while the statement is.
  [[nodiscard]] std::vector<column> columns() const;

  /// Every `?` it has, in the order written: of a CALL of an SQL-invoked
  /// procedure, granted `parameter-modes`, which connecting asks for, its
  /// OUT ones too, each with its parameter's mode and position; else its
  /// inputs, each IN [PROTOCOL Parameter modes].
  /// @return The markers, their types views valid while the statement is.
  [[nodiscard]] std::vector<marker> markers() const;

  /// The specific schema of the procedure a CALL calls, as its Prepared
  /// named it with `parameter-modes`.
  /// @return It, a view valid while the statement is; none of another
  ///   statement.
  [[nodiscard]] std::optional<std::string_view> procedure_schema() const noexcept;

  /// The specific name of the procedure a CALL calls, as its Prepared named
  /// it with `parameter-modes`.
  /// @return It, a view valid while the statement is; none of another
  ///   statement.
  [[nodiscard]] std::optional<std::string_view> procedure_name() const noexcept;

  /// turborel-c's statement.
  /// @return It.
  [[nodiscard]] const trl_statement* get() const noexcept { return statement_.get(); }

private:
  /// Frees a statement's handle.
  struct freer {
    /// Frees `made`.
    /// @param made The statement.
    void operator()(trl_statement* made) const noexcept;
  };

  std::unique_ptr<trl_statement, freer> statement_;
};

/// What several statements run as one request came to, with
/// `multi-statement` [CLIENT Controlling the session].
class TURBOREL_CPP_EXPORT ran {
public:
  /// What turborel-c ran, which it owns and frees.
  /// @param made It.
  explicit ran(trl_ran* made) noexcept;

  /// How many statements came to an outcome.
  /// @return The count.
  [[nodiscard]] std::size_t size() const noexcept;

  /// A statement's outcome, by its place.
  /// @param index Which, from 0.
  /// @return Its result, a view valid while this is.
  /// @throws turborel::error Past the last (-910).
  [[nodiscard]] result at(std::size_t index) const;

  /// The failure that stopped the rest.
  /// @return It; none if every statement ran.
  [[nodiscard]] std::optional<failure> stopped_by() const;

private:
  /// Frees what ran.
  struct freer {
    /// Frees `made`.
    /// @param made It.
    void operator()(trl_ran* made) const noexcept;
  };

  std::unique_ptr<trl_ran, freer> ran_;
};

} // namespace turborel

#endif // TURBOREL_STATEMENT_HPP
