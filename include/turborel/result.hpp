/// @file
/// @brief What a statement came to [CLIENT Running statements]: a query's
/// columns and its rows, as a range, or another statement's completion,
/// its SQLCODE and SQLSTATE (RULES.md D2).

#ifndef TURBOREL_RESULT_HPP
#define TURBOREL_RESULT_HPP

#include <turborel/export.hpp>
#include <turborel/value.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

struct trl_result;

namespace turborel {

/// A column of a query's rows, or of a prepared statement [PROTOCOL
/// Composite fields] [PROTOCOL Declared types]: views, valid while what it
/// came from is.
struct column {
  /// Its name, or none of a column the query left unnamed.
  std::optional<std::string_view> name;
  /// Its type, as its tag says.
  data_type type{nullptr};
  /// The name of the type it was declared with, as the Information
  /// Schema's DATA_TYPE gives it, as INTEGER; none if the server did not
  /// tell it, without `declared-types`.
  std::optional<std::string_view> declared_name;
  /// Its type as SQL writes it: as declared, where told, else as its tag
  /// says, as NUMERIC(10,0) of INTEGER.
  std::string_view type_written;
};

/// A row of a result: a view, valid while its result is.
class TURBOREL_CPP_EXPORT row {
public:
  /// The row `index` of `result`.
  /// @param result The result.
  /// @param index Which, from 0.
  row(const trl_result* result, std::size_t index) noexcept : result_{result}, index_{index} {}

  /// How many values it holds, one for each column.
  /// @return The count.
  [[nodiscard]] std::size_t size() const noexcept;

  /// A value, by its column.
  /// @param column Which, from 0.
  /// @return The value.
  /// @throws turborel::error Past the last column (-910).
  [[nodiscard]] value at(std::size_t column) const;

  /// A value, by its column, as `T`, as value::get gives it.
  /// @tparam T The type.
  /// @param column Which, from 0.
  /// @return The value.
  /// @throws turborel::error As at() and value::get do.
  template <typename T>
  [[nodiscard]] T get(std::size_t column) const {
    return at(column).get<T>();
  }

private:
  const trl_result* result_;
  std::size_t index_;
};

/// What a statement came to: a query's columns and rows, or another
/// statement's completion; or a batch of a cursor's rows. It owns its
/// values, or, as a batch given to a handler, borrows them for the call.
/// Its rows are a random-access range of row.
class TURBOREL_CPP_EXPORT result {
public:
  /// An iterator over the rows.
  class iterator {
  public:
    /// The iterator's category.
    using iterator_category = std::random_access_iterator_tag;
    /// The iterator's concept.
    using iterator_concept = std::random_access_iterator_tag;
    /// What it gives.
    using value_type = row;
    /// The distance between two.
    using difference_type = std::ptrdiff_t;

    /// The end of no result.
    iterator() noexcept = default;
    /// The row `index` of `result`.
    /// @param result The result.
    /// @param index Which.
    iterator(const trl_result* result, std::size_t index) noexcept : result_{result}, index_{index} {}

    /// The row it is at.
    /// @return The row.
    [[nodiscard]] row operator*() const noexcept { return row{result_, index_}; }
    /// The row `n` after it.
    /// @param n How many after.
    /// @return The row.
    [[nodiscard]] row operator[](difference_type n) const noexcept { return *(*this + n); }
    /// Moves to the next row.
    /// @return It.
    iterator& operator++() noexcept { return *this += 1; }
    /// Moves to the next row.
    /// @return It, before it moved.
    iterator operator++(int /*unused*/) noexcept { return step(1); }
    /// Moves to the row before.
    /// @return It.
    iterator& operator--() noexcept { return *this -= 1; }
    /// Moves to the row before.
    /// @return It, before it moved.
    iterator operator--(int /*unused*/) noexcept { return step(-1); }
    /// Moves `n` rows.
    /// @param n How many.
    /// @return It.
    iterator& operator+=(difference_type n) noexcept {
      index_ = static_cast<std::size_t>(static_cast<difference_type>(index_) + n);
      return *this;
    }
    /// Moves `n` rows back.
    /// @param n How many.
    /// @return It.
    iterator& operator-=(difference_type n) noexcept { return *this += -n; }
    /// The iterator `n` rows on.
    /// @param at Where from.
    /// @param n How many.
    /// @return It.
    friend iterator operator+(iterator at, difference_type n) noexcept { return at += n; }
    /// The iterator `n` rows on.
    /// @param n How many.
    /// @param at Where from.
    /// @return It.
    friend iterator operator+(difference_type n, iterator at) noexcept { return at += n; }
    /// The iterator `n` rows back.
    /// @param at Where from.
    /// @param n How many.
    /// @return It.
    friend iterator operator-(iterator at, difference_type n) noexcept { return at -= n; }
    /// The rows between two.
    /// @param to The later.
    /// @param from The earlier.
    /// @return How many.
    friend difference_type operator-(const iterator& to, const iterator& from) noexcept {
      return static_cast<difference_type>(to.index_) - static_cast<difference_type>(from.index_);
    }
    /// Whether two are at the same row.
    /// @param left One.
    /// @param right The other.
    /// @return Whether they are.
    friend bool operator==(const iterator& left, const iterator& right) noexcept { return left.index_ == right.index_; }
    /// Their order.
    /// @param left One.
    /// @param right The other.
    /// @return How `left` orders against `right`.
    friend std::strong_ordering operator<=>(const iterator& left, const iterator& right) noexcept {
      return left.index_ <=> right.index_;
    }

  private:
    /// Moves `n` rows, giving where it was.
    iterator step(difference_type n) noexcept {
      iterator before = *this;
      *this += n;
      return before;
    }

    const trl_result* result_{};
    std::size_t index_{};
  };

  /// A result of turborel-c's, which it owns and frees.
  /// @param result The result.
  explicit result(trl_result* result) noexcept;

  /// A view of a result turborel-c lends, as a batch to a handler, valid
  /// for the call alone.
  /// @param result The result.
  /// @return The view.
  [[nodiscard]] static result borrowed(const trl_result* result) noexcept;

  /// Whether it is a query's, of columns and rows.
  /// @return Whether it is.
  [[nodiscard]] bool has_rows() const noexcept;

  /// Its columns, of a query's result.
  /// @return The columns, views valid while the result is.
  [[nodiscard]] std::vector<column> columns() const;

  /// How many rows it holds.
  /// @return The count.
  [[nodiscard]] std::size_t size() const noexcept;

  /// Whether it holds no row.
  /// @return Whether it does not.
  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

  /// A row, by its place.
  /// @param index Which, from 0.
  /// @return The row.
  /// @throws turborel::error Past the last (-910).
  [[nodiscard]] row at(std::size_t index) const;

  /// The first row.
  /// @return An iterator at it.
  [[nodiscard]] iterator begin() const noexcept { return iterator{get(), 0}; }

  /// Past the last row.
  /// @return An iterator past it.
  [[nodiscard]] iterator end() const noexcept { return iterator{get(), size()}; }

  /// The completion's SQLCODE: 0, or 100 of a query that returned no
  /// row; 0 of a batch that left its cursor with more.
  /// @return The SQLCODE.
  [[nodiscard]] std::int32_t sqlcode() const noexcept;

  /// The completion's SQLSTATE: 00000, 02000, or 01... of a warning;
  /// empty of a batch that left its cursor with more.
  /// @return The SQLSTATE, valid while the result is.
  [[nodiscard]] std::string_view sqlstate() const noexcept;

  /// Whether a cursor's batch left the cursor with more rows: Suspended
  /// rather than Complete [PROTOCOL Suspended 'S'].
  /// @return Whether it did.
  [[nodiscard]] bool more() const noexcept;

  /// turborel-c's result, for the calls that take it.
  /// @return It.
  [[nodiscard]] const trl_result* get() const noexcept { return view_; }

private:
  /// Frees a result.
  struct freer {
    /// Frees `result`.
    /// @param result The result.
    void operator()(trl_result* result) const noexcept;
  };

  std::unique_ptr<trl_result, freer> owned_;
  const trl_result* view_;
};

} // namespace turborel

#endif // TURBOREL_RESULT_HPP
