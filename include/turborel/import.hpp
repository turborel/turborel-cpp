/// @file
/// @brief Import [CLIENT Importing]: rows read from a source, a record at a
/// time, inserted into a table over a connection, a batch to each
/// transaction, by bulk copy where the server grants it. A source is the
/// program's, or delimited text, CSV or tab-separated, read from a
/// std::istream (RULES.md D2).

#ifndef TURBOREL_IMPORT_HPP
#define TURBOREL_IMPORT_HPP

#include <turborel/connection.hpp>
#include <turborel/error.hpp>
#include <turborel/export.hpp>
#include <turborel/params.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct trl_delimited;

namespace turborel {

/// How many rows an import commits at a time, unless told.
inline constexpr std::uint64_t import_batch = 1000;
/// The longest record delimited text holds by default, in bytes: the
/// protocol's longest message's.
inline constexpr std::size_t delimited_max_record = 16777216;

/// Where in a source a record began.
struct record_position {
  /// The record's number among the source's records of data, from 1.
  std::uint64_t record{};
  /// The line it began on, from 1; 0 of a source not made of lines.
  std::uint64_t line{};
};

/// Why a row, or an import, failed: the input's, or an SQL condition.
struct import_cause {
  /// Whether it is an SQL condition, the server's or one of a string too
  /// long for its column.
  bool sql{};
  /// What is wrong, in plain words, or the condition's message.
  std::string message;
  /// The condition, of an SQL one.
  std::optional<failure> condition;
};

/// A row an import skipped: where its record began, and why.
struct rejected_row {
  /// Where its record began.
  record_position at;
  /// Why it failed.
  import_cause cause;
};

/// What an import is to do.
struct import_options {
  /// The table the rows go into, as SQL names it.
  std::string table;
  /// How many rows are committed at a time; 0 for all at once.
  std::uint64_t batch = import_batch;
  /// Whether a row refused, by the server or as a string too long for its
  /// column, or of too many or too few fields, is skipped and given to
  /// `rejected`, rather than ending the import.
  bool skip = false;
  /// Given each row skipped. What it throws is thrown once the import ends.
  std::function<void(const rejected_row&)> rejected;
};

/// What an import did.
struct imported {
  /// The rows inserted and committed.
  std::uint64_t rows{};
  /// The rows skipped, each given to the handler.
  std::uint64_t rejected{};
};

/// The failure of an import [CLIENT Importing]: where the record that
/// stopped it began, why, the rows committed and how many of the batch
/// under way were rolled back. Of the input's cause, its SQLCODE is -301
/// and its SQLSTATE 22000.
class TURBOREL_CPP_EXPORT import_error : public error {
public:
  /// The failure.
  /// @param why Why the import stopped.
  /// @param at Where the record that stopped it began, if one did.
  /// @param committed The rows committed.
  /// @param rolled_back The rows of the batch under way rolled back.
  /// @param text The import's failure as turborel-client displays it.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): named for what they count, in the order turborel-client's ImportError gives them.
  import_error(import_cause why, std::optional<record_position> at, std::uint64_t committed, std::uint64_t rolled_back,
               const std::string& text);

  /// Why it stopped.
  /// @return The cause.
  [[nodiscard]] const import_cause& cause() const noexcept { return *cause_; }
  /// Where the record that stopped it began, if one did.
  /// @return The position.
  [[nodiscard]] std::optional<record_position> at() const noexcept { return at_; }
  /// The rows inserted and committed.
  /// @return The count.
  [[nodiscard]] std::uint64_t committed() const noexcept { return committed_; }
  /// The rows of the batch under way rolled back.
  /// @return The count.
  [[nodiscard]] std::uint64_t rolled_back() const noexcept { return rolled_back_; }

private:
  std::shared_ptr<const import_cause> cause_;
  std::optional<record_position> at_;
  std::uint64_t committed_;
  std::uint64_t rolled_back_;
};

/// Where rows to import come from: headings() is asked once, before the
/// first record; then record() until it gives none. A source holds no more
/// than a record at a time.
class TURBOREL_CPP_EXPORT import_source {
public:
  import_source() = default;
  import_source(const import_source&) = delete;
  import_source(import_source&&) = delete;
  import_source& operator=(const import_source&) = delete;
  import_source& operator=(import_source&&) = delete;
  /// Destroys the source.
  virtual ~import_source() = default;

  /// The names of its fields, or none, its fields then the table's
  /// columns in order.
  /// @return The names.
  /// @throws std::exception Of input that cannot be read, which ends the
  ///   import with its message as the input's cause.
  virtual std::optional<std::vector<std::string>> headings() = 0;

  /// Gives its next record's values into `values`, empty, and where it
  /// began; a source that knows no types gives text and nulls, cast to
  /// their columns' types as they are inserted.
  /// @param values Where the values go.
  /// @param at Where the record began.
  /// @return Whether it gave one; false at the end.
  /// @throws std::exception As headings() does.
  virtual bool record(params& values, record_position& at) = 0;
};

/// How delimited text is written.
struct dialect {
  /// What splits fields: one ASCII character, not a quote or a line break.
  char delimiter{','};
  /// Whether fields may be enclosed in double quotes, as CSV's are.
  bool quoted{true};
  /// Whether the first record names the fields.
  bool header{};
  /// What a field not enclosed in quotes is written as to be null.
  std::string null;
  /// The longest record, in bytes, its line break counted.
  std::size_t max_record{delimited_max_record};

  /// CSV (RFC 4180): fields split at commas, enclosed in quotes as they
  /// need; no header; an empty field null.
  /// @return The dialect.
  [[nodiscard]] static dialect csv();
  /// Tab-separated text: fields split at tabs, never quoted; no header;
  /// an empty field null.
  /// @return The dialect.
  [[nodiscard]] static dialect tsv();
};

/// Delimited text read a record at a time from a std::istream: a record a
/// line, a quoted field free to hold the delimiter, a doubled quote or a
/// line break; UTF-8, a byte order mark passed over.
class TURBOREL_CPP_EXPORT delimited_text {
public:
  /// Delimited text of `in`, written as `how` says.
  /// @param in The text, which must outlive this.
  /// @param how The dialect.
  /// @throws turborel::error Of a delimiter that is a quote or a line break
  ///   (-910).
  delimited_text(std::istream& in, const dialect& how);

  /// turborel-c's delimited text.
  /// @return It.
  [[nodiscard]] trl_delimited* get() const noexcept { return text_.get(); }

private:
  /// Frees delimited text.
  struct freer {
    /// Frees `made`.
    /// @param made It.
    void operator()(trl_delimited* made) const noexcept;
  };

  std::unique_ptr<trl_delimited, freer> text_;
  std::string null_;
};

/// Inserts a source's records into a table [CLIENT Importing]: through one
/// prepared INSERT, each value cast by the server to its column's type; a
/// batch committed at a time; by bulk copy where `copy` is granted, no row
/// is skipped and no column is a large object's. The session must have no
/// transaction active.
/// @param conn The connection.
/// @param source The source.
/// @param how What to do.
/// @return What it did.
/// @throws import_error Why it stopped.
imported import_rows(connection& conn, import_source& source, const import_options& how);

/// Inserts delimited text's records into a table, as the other
/// import_rows does.
/// @param conn The connection.
/// @param source The text.
/// @param how What to do.
/// @return What it did.
/// @throws import_error Why it stopped.
imported import_rows(connection& conn, delimited_text& source, const import_options& how);

} // namespace turborel

#endif // TURBOREL_IMPORT_HPP
