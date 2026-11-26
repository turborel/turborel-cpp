/// @file
/// @brief Bulk copy and export [CLIENT Copying and exporting]: rows sent
/// into a table with no round trip but the copy's end, all or nothing; a
/// query's rows written as they come, as CSV or as the protocol's own
/// frames, which read back with every value as it was (RULES.md D2).

#ifndef TURBOREL_COPY_HPP
#define TURBOREL_COPY_HPP

#include <turborel/connection.hpp>
#include <turborel/export.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

struct trl_copy;

namespace turborel {

/// About the most bytes of rows one CopyData carries.
inline constexpr std::size_t copy_chunk_bytes = 262144;

/// A copy into a table under way [PROTOCOL CopyIn 'I']: until it ends, the
/// connection sends only its rows, its end and Ping. Destroyed before it
/// ends, it is abandoned, every row of it undone.
class TURBOREL_CPP_EXPORT copy {
public:
  /// Begins a copy into `table`, of `columns` in each row's order, or of
  /// every column in the table's order if none is named.
  /// @param conn The connection, which the copy holds until it ends.
  /// @param table The table, as SQL names it.
  /// @param columns The columns, as SQL names them.
  /// @return The copy.
  /// @throws turborel::error If `copy` was not granted (08006, the
  ///   connection kept), or the connection is lost.
  [[nodiscard]] static copy into(connection& conn, const std::string& table,
                                 const std::vector<std::string>& columns = {});

  /// Sends a row, sent with those before it once they are about
  /// copy_chunk_bytes, with no reply [PROTOCOL CopyData 'J']. Granted
  /// `columns`, which connecting asks for, rows whose every value is null
  /// or a character string go by column, each cell its string's text, as
  /// the server takes it by row [PROTOCOL CopyColumns 'O']; any other row
  /// goes by row, in its turn.
  /// @param row The row's values.
  /// @return The copy.
  /// @throws turborel::error Of a value the protocol cannot carry, or the
  ///   connection lost.
  copy& send(const params& row);

  /// Ends the copy, its rows inserted, all or none [PROTOCOL CopyEnd 'N'].
  /// @return The rows inserted.
  /// @throws turborel::error The copy's failure, every row of it undone,
  ///   its detail naming the row, as "row 7 of the copy".
  std::uint64_t finish();

  /// Gives the copy up, every row of it undone.
  /// @throws turborel::error The connection's failure.
  void abandon();

private:
  /// Frees a copy, abandoning one not ended.
  struct freer {
    /// Frees `made`.
    /// @param made The copy.
    void operator()(trl_copy* made) const noexcept;
  };

  copy(connection& conn, trl_copy* made) noexcept : conn_{&conn}, copy_{made} {}

  connection* conn_;
  std::unique_ptr<trl_copy, freer> copy_;
};

/// Copies every row `next` gives into `table`, as copy::into, send and
/// finish do.
/// @param conn The connection.
/// @param table The table.
/// @param columns The columns; none for every column.
/// @param next Fills its argument, empty, with the next row's values,
///   saying whether it gave one; false at the end. What it throws abandons
///   the copy and is thrown.
/// @return The rows inserted.
/// @throws turborel::error As copy::finish does.
std::uint64_t copy_rows(connection& conn, const std::string& table, const std::vector<std::string>& columns,
                        const std::function<bool(params&)>& next);

/// How an export writes a query's rows.
enum class export_format : std::uint8_t {
  /// CSV (RFC 4180): a header of the columns' names, then a record for each
  /// row, each ended by CR LF.
  csv,
  /// The protocol's own frames: a Description, Rows, a Complete.
  native,
};

/// Runs a query and writes its rows as they come, a batch at a time.
/// @param conn The connection.
/// @param sql The query.
/// @param format How the rows are written.
/// @param out Where they go.
/// @return The rows written.
/// @throws turborel::error The query's failure; of a statement that is no
///   query; of a write that failed (08006, the connection kept).
std::uint64_t export_rows(connection& conn, const std::string& sql, export_format format, std::ostream& out);

/// Reads back native rows an export wrote: their columns and rows, every
/// value as it was.
/// @param in The rows.
/// @return The result.
/// @throws turborel::error Of input that is not native rows, or cut short
///   (08006).
[[nodiscard]] result read_native(std::istream& in);

} // namespace turborel

#endif // TURBOREL_COPY_HPP
