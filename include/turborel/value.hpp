/// @file
/// @brief The values a result holds, read by their kind as C++ types
/// [PROTOCOL Values] [CLIENT Running statements], and the types of columns
/// and parameters [PROTOCOL Data types] (RULES.md D2, D16).

#ifndef TURBOREL_VALUE_HPP
#define TURBOREL_VALUE_HPP

#include <turborel/datetime.hpp>
#include <turborel/decimal.hpp>
#include <turborel/export.hpp>
#include <turborel/int128.hpp>
#include <turborel/interval.hpp>
#include <turborel/uuid.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

struct trl_value;
struct trl_data_type;

namespace turborel {

/// The kind of a value or of a type: the protocol's tag of it
/// [PROTOCOL Values] [PROTOCOL Data types].
enum class kind : std::uint8_t {
  /// The null value, tag 0.
  null = 0,
  /// A character string, tag 1.
  character = 1,
  /// An exact number, tag 2.
  exact = 2,
  /// An approximate number, tag 3.
  approximate = 3,
  /// A plug-in type's value, tag 4.
  plugin = 4,
  /// CHARACTER VARYING, a type's kind; its values come as `character`.
  character_varying = 5,
  /// A binary string, tag 6.
  binary = 6,
  /// BINARY VARYING, a type's kind; its values come as `binary`.
  binary_varying = 7,
  /// A date, tag 8.
  date = 8,
  /// A time, tag 9.
  time = 9,
  /// TIME WITH TIME ZONE, a type's kind; its values come as `time`.
  time_zoned = 10,
  /// A timestamp, tag 11.
  timestamp = 11,
  /// TIMESTAMP WITH TIME ZONE, a type's kind; its values come as `timestamp`.
  timestamp_zoned = 12,
  /// An interval, tag 13.
  interval = 13,
  /// A truth value, tag 14.
  boolean = 14,
  /// A decimal floating-point number, tag 15.
  decfloat = 15,
  /// An array, tag 16.
  array = 16,
  /// A row, tag 17.
  row = 17,
  /// CHARACTER LARGE OBJECT, a type's kind.
  clob = 18,
  /// BINARY LARGE OBJECT, a type's kind.
  blob = 19,
  /// A large object sent whole, tag 20.
  lob = 20,
  /// A large object by its locator, tag 21.
  lob_locator = 21,
  /// A multiset, tag 22.
  multiset = 22,
  /// A structured value, tag 23.
  structured = 23,
  /// A reference, tag 24.
  reference = 24,
  /// A JSON value, tag 25.
  json = 25,
  /// An XML value, tag 26.
  xml = 26,
  /// A UUID, tag 27.
  uuid = 27,
};

/// A decimal floating-point number, DECFLOAT: its coefficient times ten to
/// its exponent, as value tag 15 carries it [PROTOCOL Values].
struct decfloat {
  /// Its coefficient.
  int128 coefficient;
  /// Its exponent.
  std::int32_t exponent{};

  /// Whether two have the same coefficient and exponent.
  friend bool operator==(const decfloat&, const decfloat&) noexcept = default;
};

/// What a large object value is [PROTOCOL Values] [CLIENT Large objects].
struct lob_info {
  /// Whether it is a character large object, else a binary one.
  bool character{};
  /// Whether it came by its locator, else whole in its row.
  bool located{};
  /// Its octets.
  std::uint64_t octets{};
  /// Its characters, of a character one.
  std::uint64_t characters{};

  /// Whether two say the same.
  friend bool operator==(const lob_info&, const lob_info&) noexcept = default;
};

/// The type of a column or a parameter [PROTOCOL Data types]: a view, valid
/// while the result or statement it came from is.
class TURBOREL_CPP_EXPORT data_type {
public:
  /// A view of turborel-c's type.
  /// @param type The type, or null.
  explicit data_type(const trl_data_type* type) noexcept : type_{type} {}

  /// Its kind: of the protocol's types, those of the values' tags.
  /// @return Its kind; null of no type.
  [[nodiscard]] turborel::kind kind() const noexcept;

  /// Its length: of a character or binary string or a large object.
  /// @return The length, 0 of another kind.
  [[nodiscard]] std::uint64_t length() const noexcept;

  /// Its precision: of an exact number, its digits; of an approximate
  /// one, its bits; of a datetime, its fraction's digits.
  /// @return The precision, 0 of another kind.
  [[nodiscard]] std::uint32_t precision() const noexcept;

  /// An exact number's scale.
  /// @return The scale, 0 of another kind.
  [[nodiscard]] std::uint32_t scale() const noexcept;

  /// The type as SQL writes it, as INTEGER, VARCHAR(20) or NUMERIC(9,2).
  /// @return Its text.
  /// @throws turborel::error Of no type.
  [[nodiscard]] std::string to_string() const;

private:
  const trl_data_type* type_;
};

/// A value a result holds [PROTOCOL Values]: a view, valid while its
/// result, or batch, is. Each reader gives one kind of value as a C++
/// type, and fails, as a misuse (SQLCODE -910, SQLSTATE TB910), of a value
/// of another kind.
class TURBOREL_CPP_EXPORT value {
public:
  /// A view of turborel-c's value.
  /// @param viewed The value, or null, read as the null value.
  explicit value(const trl_value* viewed) noexcept : value_{viewed} {}

  /// Its kind, as its tag says [PROTOCOL Values].
  /// @return Its kind.
  [[nodiscard]] turborel::kind kind() const noexcept;

  /// Whether it is the null value.
  /// @return Whether it is.
  [[nodiscard]] bool is_null() const noexcept;

  /// The text of a character string, a CLOB sent whole, an XML value or a
  /// plug-in value: UTF-8.
  /// @return The text, valid while the value is.
  /// @throws turborel::error Of another kind.
  [[nodiscard]] std::string_view text() const;

  /// The octets of a binary string, a BLOB sent whole, a JSON value's
  /// stored form, a UUID or a plug-in value.
  /// @return The octets, valid while the value is.
  /// @throws turborel::error Of another kind.
  [[nodiscard]] std::span<const std::byte> bytes() const;

  /// An exact number, every one the protocol carries.
  /// @return It.
  /// @throws turborel::error Of another kind.
  [[nodiscard]] decimal as_decimal() const;

  /// An exact number of scale 0 that fits 64 bits.
  /// @return It.
  /// @throws turborel::error Of another kind (-910), or of a number with a
  ///   fraction or past 64 bits (SQLSTATE 22003).
  [[nodiscard]] std::int64_t as_int64() const;

  /// An approximate number, its bits kept: NaN and -0.0 as they are.
  /// @return It.
  /// @throws turborel::error Of another kind.
  [[nodiscard]] double as_double() const;

  /// A truth value.
  /// @return It.
  /// @throws turborel::error Of another kind.
  [[nodiscard]] bool as_bool() const;

  /// A date.
  /// @return It.
  /// @throws turborel::error Of another kind, or out of 0001 to 9999.
  [[nodiscard]] date as_date() const;

  /// A time, WITH TIME ZONE or not.
  /// @return It.
  /// @throws turborel::error Of another kind, or out of range.
  [[nodiscard]] time as_time() const;

  /// A timestamp, WITH TIME ZONE or not.
  /// @return It.
  /// @throws turborel::error Of another kind, or out of range.
  [[nodiscard]] timestamp as_timestamp() const;

  /// An interval, of any qualifier.
  /// @return It.
  /// @throws turborel::error Of another kind.
  [[nodiscard]] interval as_interval() const;

  /// A decimal floating-point number.
  /// @return It.
  /// @throws turborel::error Of another kind.
  [[nodiscard]] decfloat as_decfloat() const;

  /// A UUID.
  /// @return It.
  /// @throws turborel::error Of another kind.
  [[nodiscard]] uuid as_uuid() const;

  /// A JSON value as JSON text, as turborel writes it.
  /// @return The text.
  /// @throws turborel::error Of another kind.
  [[nodiscard]] std::string json() const;

  /// What a large object value is, whole or by its locator.
  /// @return It.
  /// @throws turborel::error Of another kind.
  [[nodiscard]] lob_info lob() const;

  /// How many elements an array or a multiset has, fields a row, or
  /// attributes a structured value.
  /// @return The count; 0 of another kind.
  [[nodiscard]] std::size_t size() const noexcept;

  /// An element, field or attribute, by its place.
  /// @param index Which, from 0.
  /// @return It.
  /// @throws turborel::error Past the last, or of another kind.
  [[nodiscard]] value at(std::size_t index) const;

  /// The value as turborel displays it: a string as itself, a number as
  /// turborel writes it, a datetime as its literal's string, NULL.
  /// @return The text.
  [[nodiscard]] std::string to_string() const;

  /// The value as `T`: std::string_view or std::string of text,
  /// std::span<const std::byte> of octets, decimal, std::int64_t,
  /// double, bool, date, time, timestamp, interval, decfloat or uuid;
  /// std::optional of any of them, which is empty of the null value.
  /// @tparam T The type.
  /// @return It.
  /// @throws turborel::error As the reader of `T` does, or of the null
  ///   value read as other than std::optional.
  template <typename T>
  [[nodiscard]] T get() const {
    if constexpr (is_optional<T>::value) {
      if (is_null()) {
        return std::nullopt;
      }
      return get<typename T::value_type>();
    } else {
      return read<T>();
    }
  }

  /// turborel-c's value, for the calls that take it.
  /// @return It.
  [[nodiscard]] const trl_value* c_value() const noexcept { return value_; }

private:
  /// Whether `T` is a std::optional.
  template <typename T>
  struct is_optional : std::false_type {};

  /// See above.
  template <typename T>
  struct is_optional<std::optional<T>> : std::true_type {};

  /// Fails unless the value is not null, read as `T`.
  void not_null() const;

  /// The value as `T`, not optional.
  template <typename T>
  [[nodiscard]] T read() const {
    not_null();
    if constexpr (std::same_as<T, std::string_view>) {
      return text();
    } else if constexpr (std::same_as<T, std::string>) {
      return std::string{text()};
    } else if constexpr (std::same_as<T, std::span<const std::byte>>) {
      return bytes();
    } else if constexpr (std::same_as<T, decimal>) {
      return as_decimal();
    } else if constexpr (std::same_as<T, std::int64_t>) {
      return as_int64();
    } else if constexpr (std::same_as<T, double>) {
      return as_double();
    } else if constexpr (std::same_as<T, bool>) {
      return as_bool();
    } else {
      return read_more<T>();
    }
  }

  /// The value as one of the other types `get` takes.
  template <typename T>
  [[nodiscard]] T read_more() const {
    if constexpr (std::same_as<T, date>) {
      return as_date();
    } else if constexpr (std::same_as<T, time>) {
      return as_time();
    } else if constexpr (std::same_as<T, timestamp>) {
      return as_timestamp();
    } else if constexpr (std::same_as<T, interval>) {
      return as_interval();
    } else if constexpr (std::same_as<T, decfloat>) {
      return as_decfloat();
    } else {
      static_assert(std::same_as<T, uuid>, "get takes the types value's readers give");
      return as_uuid();
    }
  }

  const trl_value* value_;
};

} // namespace turborel

#endif // TURBOREL_VALUE_HPP
