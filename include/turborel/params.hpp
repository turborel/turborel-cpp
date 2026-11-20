/// @file
/// @brief The values a program sends as a prepared statement's parameters,
/// or as a copy's row [CLIENT Prepared statements] [PROTOCOL Values]
/// (RULES.md D2, D16).

#ifndef TURBOREL_PARAMS_HPP
#define TURBOREL_PARAMS_HPP

#include <turborel/datetime.hpp>
#include <turborel/decimal.hpp>
#include <turborel/export.hpp>
#include <turborel/interval.hpp>
#include <turborel/uuid.hpp>
#include <turborel/value.hpp>

#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

struct trl_params;

namespace turborel {

namespace detail {
struct params_access;
} // namespace detail

/// A JSON value sent as its JSON text, read by the server as turborel
/// reads JSON [PROTOCOL Values].
struct json_text {
  /// The text, UTF-8.
  std::string_view text;
};

/// An XML value sent as its serialization [CLIENT XML values].
struct xml_text {
  /// The serialization, UTF-8.
  std::string_view text;
};

/// Values, in order, sent as a prepared statement's parameters or as a
/// copy's row. Each is copied as it is added.
class TURBOREL_CPP_EXPORT params {
public:
  /// No values.
  /// @throws std::bad_alloc If none can be made.
  params();

  params(const params&) = delete;
  params& operator=(const params&) = delete;
  /// Moves the list; the one moved from holds none.
  /// @param other The list.
  params(params&& other) noexcept : owned_{std::move(other.owned_)}, list_{std::exchange(other.list_, nullptr)} {}
  /// Moves the list.
  /// @param other The list.
  /// @return This.
  params& operator=(params&& other) noexcept {
    owned_ = std::move(other.owned_);
    list_ = std::exchange(other.list_, nullptr);
    return *this;
  }
  ~params() = default;

  /// The values of `values`, in order, each added as add() adds it.
  /// @param values The values.
  /// @return The list.
  /// @throws turborel::error As add() does.
  template <typename... Values>
  [[nodiscard]] static params of(const Values&... values) {
    params list;
    (list.add(passed(values)), ...);
    return list;
  }

  /// Adds the null value.
  /// @return The list.
  params& add(std::nullopt_t /*null*/);
  /// Adds the null value.
  /// @return The list.
  params& add(std::nullptr_t /*null*/);
  /// Adds a character string.
  /// @param text The text, UTF-8.
  /// @return The list.
  /// @throws turborel::error Of text not UTF-8 (-910).
  params& add(std::string_view text);
  /// Adds a character string.
  /// @param text The text, UTF-8, null-ended.
  /// @return The list.
  /// @throws turborel::error Of text not UTF-8 (-910).
  params& add(const char* text) { return add(std::string_view{text}); }
  /// Adds a binary string.
  /// @param octets The octets.
  /// @return The list.
  params& add(std::span<const std::byte> octets);
  /// Adds an exact number.
  /// @param number The number.
  /// @return The list.
  params& add(const decimal& number);
  /// Adds an integer, as an exact number of scale 0.
  /// @param number The integer.
  /// @return The list.
  template <std::integral Integer>
    requires(!std::same_as<Integer, bool> && !std::same_as<Integer, char>)
  params& add(Integer number) {
    return add(decimal{.units = int128{static_cast<std::int64_t>(number)}, .scale = 0});
  }
  /// Adds an approximate number, its bits kept.
  /// @param number The number.
  /// @return The list.
  params& add(double number);
  /// Adds a truth value.
  /// @param truth The truth value.
  /// @return The list.
  params& add(bool truth);
  /// Adds a date.
  /// @param day The date.
  /// @return The list.
  /// @throws turborel::error Of a day out of 0001 to 9999 (22008).
  params& add(const date& day);
  /// Adds a time, of TIME WITH TIME ZONE if it is zoned.
  /// @param moment The time.
  /// @return The list.
  /// @throws turborel::error Out of range (22008, 22009).
  params& add(const time& moment);
  /// Adds a timestamp, of TIMESTAMP WITH TIME ZONE if it is zoned.
  /// @param instant The timestamp.
  /// @return The list.
  /// @throws turborel::error Out of range (22008, 22009).
  params& add(const timestamp& instant);
  /// Adds an interval.
  /// @param span The interval.
  /// @return The list.
  params& add(const interval& span);
  /// Adds a decimal floating-point number, as turborel makes one of it.
  /// @param number The number.
  /// @return The list.
  /// @throws turborel::error Out of DECFLOAT's range (-910).
  params& add(const decfloat& number);
  /// Adds a UUID.
  /// @param id The UUID.
  /// @return The list.
  params& add(const uuid& id);
  /// Adds a JSON value of its JSON text.
  /// @param json The text.
  /// @return The list.
  /// @throws turborel::error Of text no JSON value is (22018).
  params& add(const json_text& json);
  /// Adds an XML value of its serialization.
  /// @param xml The serialization.
  /// @return The list.
  /// @throws turborel::error Of text not UTF-8 (-910).
  params& add(const xml_text& xml);
  /// Adds a copy of a value a result holds, of any kind.
  /// @param copied The value.
  /// @return The list.
  params& add(const value& copied);
  /// Adds a value, or the null value if it is empty.
  /// @param maybe The value, or none.
  /// @return The list.
  template <typename T>
  params& add(const std::optional<T>& maybe) {
    return maybe ? add(*maybe) : add(std::nullopt);
  }

  /// How many values it holds.
  /// @return The count.
  [[nodiscard]] std::size_t size() const noexcept;

  /// Empties it, for the next execution.
  void clear() noexcept;

  /// turborel-c's list, for the calls that take it.
  /// @return It.
  [[nodiscard]] trl_params* get() const noexcept { return list_; }

private:
  friend struct detail::params_access;

  /// A value of() adds, as add() takes it: an array of characters, a
  /// string literal, as its first character's address; any other as it is.
  template <typename T>
  [[nodiscard]] static decltype(auto) passed(const T& value) noexcept {
    if constexpr (std::is_array_v<T>) {
      return static_cast<const std::remove_extent_t<T>*>(value);
    } else {
      return std::as_const(value);
    }
  }

  /// Frees a list.
  struct freer {
    /// Frees `list`.
    /// @param list The list.
    void operator()(trl_params* list) const noexcept;
  };

  /// A view of a list turborel-c lends a callback, for the call alone.
  explicit params(trl_params* lent) noexcept : list_{lent} {}

  std::unique_ptr<trl_params, freer> owned_;
  trl_params* list_{};
};

} // namespace turborel

#endif // TURBOREL_PARAMS_HPP
