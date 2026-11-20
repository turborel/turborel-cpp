/// @file
/// @brief Cursors [CLIENT Prepared statements] [PROTOCOL Execute 'E']: a
/// prepared query opened, its rows fetched a batch at a time, several of
/// one statement by their numbers; and a query's rows read as an input
/// range, a batch fetched as each is needed (RULES.md D2).

#ifndef TURBOREL_CURSOR_HPP
#define TURBOREL_CURSOR_HPP

#include <turborel/connection.hpp>
#include <turborel/export.hpp>
#include <turborel/params.hpp>
#include <turborel/result.hpp>
#include <turborel/statement.hpp>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>

namespace turborel {

/// The most rows of each batch a cursor fetches, unless told.
inline constexpr std::uint32_t default_batch_rows = 1000;

/// An open cursor of a prepared query: its statement and a number of the
/// program's own, 0 unless `cursors` is granted. It holds its last batch,
/// and refers to its connection and statement, which outlive it.
/// Destroyed open, it is closed, a failure then left for the connection's
/// next call to tell.
class TURBOREL_CPP_EXPORT cursor {
public:
  /// An input iterator over the cursor's rows, which fetches the next
  /// batch as the last row of one is passed. A row is valid until the
  /// iterator moves past its batch.
  class iterator {
  public:
    /// The iterator's concept.
    using iterator_concept = std::input_iterator_tag;
    /// What it gives.
    using value_type = row;
    /// The distance between two.
    using difference_type = std::ptrdiff_t;

    /// The end.
    iterator() noexcept = default;
    /// The first row of `of`.
    /// @param of The cursor.
    explicit iterator(cursor& of) noexcept : cursor_{&of} {}

    /// The row it is at.
    /// @return The row.
    [[nodiscard]] row operator*() const;
    /// Moves to the next row, fetching the next batch if need be.
    /// @return It.
    /// @throws turborel::error The fetch's failure.
    iterator& operator++();
    /// Moves to the next row.
    void operator++(int /*unused*/) { ++*this; }
    /// Whether it is past the last row.
    /// @param at The iterator.
    /// @return Whether it is.
    friend bool operator==(const iterator& at, std::default_sentinel_t /*end*/) noexcept { return at.at_end(); }

  private:
    /// Whether it is past the last row.
    [[nodiscard]] bool at_end() const noexcept;

    cursor* cursor_{};
    std::size_t index_{};
  };

  /// Opens a cursor of a query's statement and takes its first rows, at
  /// most `rows` [PROTOCOL Execute 'E'].
  /// @param conn The connection that prepared it.
  /// @param prepared The statement.
  /// @param values Its parameters' values.
  /// @param rows The most rows of each batch.
  /// @param number Its number among the statement's cursors; one other
  ///   than 0 needs `cursors`.
  /// @return The cursor.
  /// @throws turborel::error The statement's failure; of a number other
  ///   than 0 without `cursors`, sending nothing.
  [[nodiscard]] static cursor open(connection& conn, const statement& prepared, const params& values = params{},
                                   std::uint32_t rows = default_batch_rows, std::uint32_t number = 0);

  cursor(const cursor&) = delete;
  cursor& operator=(const cursor&) = delete;
  /// Moves the cursor.
  /// @param other The cursor moved from, left closed.
  cursor(cursor&& other) noexcept;
  /// Moves the cursor, closing the one this held.
  /// @param other The cursor moved from, left closed.
  /// @return This.
  cursor& operator=(cursor&& other) noexcept;
  /// Closes the cursor if it is open.
  ~cursor();

  /// The batch it holds: the first after open, then each fetch's.
  /// @return The batch.
  [[nodiscard]] const result& batch() const noexcept { return batch_; }

  /// Whether the cursor has rows it has not fetched.
  /// @return Whether it does.
  [[nodiscard]] bool more() const noexcept { return open_ && batch_.more(); }

  /// Fetches the next batch, at most `rows` [PROTOCOL Fetch 'F'].
  /// @param rows The most rows.
  /// @return The batch, which the cursor holds.
  /// @throws turborel::error As of a cursor not open (24000).
  const result& fetch(std::uint32_t rows = default_batch_rows);

  /// Closes the cursor [PROTOCOL Close 'C'].
  /// @throws turborel::error As of a cursor not open.
  void close();

  /// The open cursor's columns, with `describe` [PROTOCOL Describe 'Y'].
  /// @return A result of its columns and no row.
  /// @throws turborel::error If `describe` was not granted, or it failed.
  [[nodiscard]] result describe();

  /// Its rows, from the batch it holds on, each batch fetched as it is
  /// needed.
  /// @return An iterator at the first.
  [[nodiscard]] iterator begin() noexcept { return iterator{*this}; }

  /// Past its last row.
  /// @return The end.
  [[nodiscard]] static std::default_sentinel_t end() noexcept { return std::default_sentinel; }

private:
  cursor(connection& conn, const statement& prepared, std::uint32_t number, result first);

  /// Closes the cursor if it is open, throwing nothing.
  void close_quietly() noexcept;

  connection* conn_;
  const statement* statement_;
  std::uint32_t number_;
  std::uint32_t rows_{default_batch_rows};
  result batch_;
  bool open_;
};

} // namespace turborel

#endif // TURBOREL_CURSOR_HPP
