# Values

[Contents](README.md)

The values a result holds and a statement's parameters take, for each of
the protocol's types: their readers, the C++ types they give, and the
types of columns and parameters. [Appendix A: Types](../appendix/a-types.md)
maps every tag to its C++ type, with its bounds.

## Values read

### NAME

`turborel::value`, `turborel::kind`: a value of a result, read as its
C++ type.

### SYNOPSIS

```cpp
#include <turborel/value.hpp>

namespace turborel {
enum class kind : std::uint8_t { null = 0, character = 1, exact = 2, /* ... */ uuid = 27 };

class value {
public:
  turborel::kind kind() const noexcept;
  bool is_null() const noexcept;
  std::string_view text() const;
  std::span<const std::byte> bytes() const;
  decimal as_decimal() const;
  std::int64_t as_int64() const;
  double as_double() const;
  bool as_bool() const;
  date as_date() const;
  time as_time() const;
  timestamp as_timestamp() const;
  interval as_interval() const;
  decfloat as_decfloat() const;
  uuid as_uuid() const;
  std::string json() const;
  lob_info lob() const;
  std::size_t size() const noexcept;
  value at(std::size_t index) const;
  std::string to_string() const;
  template <typename T> T get() const;
};
}
```

### DESCRIPTION

A `value` is a view of a value a result holds, valid while the result,
or the batch a handler is given, is. Its `kind()` is the tag the value
was sent with, `kind::character` of a CHARACTER VARYING column's value
too, as the protocol writes it.

Each reader gives one kind: `text()` the UTF-8 of a character string, a
large object sent whole, an XML value or a plug-in's value; `bytes()` the
octets of a binary string, a large object sent whole, a JSON value's
stored form, a UUID or a plug-in's value; `as_decimal()` every exact
number, and `as_int64()` one of scale 0 that fits 64 bits;
`as_double()` an approximate number, its bits kept; `as_date()`,
`as_time()` and `as_timestamp()` the datetimes, WITH TIME ZONE or not;
`as_interval()`, `as_decfloat()` and `as_uuid()` theirs; `json()` a JSON
value as JSON text, as Turborel writes it; `lob()` what a large object
is, whole or by its locator. An array, a row, a multiset or a structured
value has `size()` elements, fields or attributes, each `at(i)`.

`get<T>()` reads by the type asked for: `std::string_view` or
`std::string` of text, `std::span<const std::byte>` of octets,
`decimal`, `std::int64_t`, `double`, `bool`, `date`, `time`,
`timestamp`, `interval`, `decfloat` or `uuid`. `get<std::optional<T>>()`
gives an empty optional of the null value, and `T` of any other; every
other reader fails of the null value. `to_string()` gives any value as
Turborel displays it, NULL of the null value.

### ERRORS

A reader of another kind than the value's, or of the null value, fails
with SQLCODE -910, SQLSTATE TB910, saying what the value is not. An exact
number that does not fit `std::int64_t`, or whose scale is not 0, fails
`as_int64()` with SQLCODE -301, SQLSTATE 22003. `json()` of a JSON value
that holds a datetime, or an approximate number that is not finite,
fails with -301, 22032: no JSON text writes it. `at(i)` past the last
fails with -910, TB910.

### EXAMPLES

```cpp
#include <turborel/turborel.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  const turborel::result read = conn.query(
      "VALUES (12.50, CAST(7 AS INTEGER), 2.5E0, 'tea', DATE '2026-11-20', "
      "TIMESTAMP '2026-11-20 09:30:00.250000', INTERVAL '1 02:03:04' DAY TO SECOND, TRUE, "
      "CAST(NULL AS VARCHAR(5)))");
  const turborel::row row = read.at(0);
  std::cout << row.get<turborel::decimal>(0).to_string() << '\n';
  std::cout << row.get<std::int64_t>(1) + 1 << '\n';
  std::cout << row.get<double>(2) * 2 << '\n';
  std::cout << row.get<std::string>(3) << '\n';
  const turborel::date day = row.get<turborel::date>(4);
  std::cout << static_cast<int>(day.day.year()) << ' ' << static_cast<unsigned>(day.day.month()) << '\n';
  const turborel::timestamp at = row.get<turborel::timestamp>(5);
  std::cout << static_cast<int>(at.precision) << ' ' << (at.instant.time_since_epoch().count() % 1000000) << '\n';
  std::cout << row.get<turborel::interval>(6).microseconds().value_or(std::chrono::microseconds{}).count() << '\n';
  std::cout << std::boolalpha << row.get<bool>(7) << '\n';
  std::cout << row.get<std::optional<std::string>>(8).value_or("(null)") << '\n';
  for (std::size_t i = 0; i < 9; ++i) {
    std::cout << row.at(i).to_string() << (i < 8 ? ", " : "\n");
  }
}
```

```
12.50
8
5
tea
2026 11
6 250000
93784000000
true
(null)
12.50, 7, 2.5E0, tea, 2026-11-20, 2026-11-20 09:30:00.250000, 1 02:03:04.000000, TRUE, NULL
```

## Data types

### NAME

`turborel::data_type`, `turborel::column`: the type of a column or a
parameter.

### SYNOPSIS

```cpp
#include <turborel/value.hpp>
#include <turborel/result.hpp>

namespace turborel {
class data_type {
public:
  turborel::kind kind() const noexcept;
  std::uint64_t length() const noexcept;
  std::uint32_t precision() const noexcept;
  std::uint32_t scale() const noexcept;
  std::string to_string() const;
};

struct column {
  std::optional<std::string_view> name;
  data_type type;
  std::optional<std::string_view> declared_name;
  std::string_view type_written;
  bool not_null;
};
}
```

### DESCRIPTION

A `data_type` is a view, valid while its result or statement is. Its
`kind()` is the type's tag, `kind::character_varying` of CHARACTER
VARYING; `length()` a string's or a large object's; `precision()` an
exact number's digits, an approximate one's bits, a datetime's fraction's
digits; `scale()` an exact number's. `to_string()` writes the type as
SQL does, as `CHARACTER VARYING(20)` or `NUMERIC(9,2)`.

A `column` of a result or a prepared statement holds its `name`, none of
a column a query left unnamed, and its `type`. Granted `declared-types`,
which a connection asks for, `declared_name` is the Information Schema's
DATA_TYPE of the type it was declared with, as INTEGER, and
`type_written` that type as SQL writes it; without it, `type_written`
writes the type its tag says, NUMERIC(10,0) of an INTEGER. A result's
or prepared statement's column says whether it is NOT NULL, `not_null`.

## Parameters

### NAME

`turborel::params`, `turborel::json_text`, `turborel::xml_text`: the
values of a statement's `?` parameters.

### SYNOPSIS

```cpp
#include <turborel/params.hpp>

namespace turborel {
struct json_text { std::string_view text; };
struct xml_text { std::string_view text; };

class params {
public:
  params();
  template <typename... Values> static params of(const Values&... values);
  params& add(std::nullopt_t);
  params& add(std::nullptr_t);
  params& add(std::string_view text);
  params& add(const char* text);
  params& add(std::span<const std::byte> octets);
  params& add(const decimal& number);
  template <std::integral Integer> params& add(Integer number);  // not bool or char
  params& add(double number);
  params& add(bool truth);
  params& add(const date& day);
  params& add(const time& moment);
  params& add(const timestamp& instant);
  params& add(const interval& span);
  params& add(const decfloat& number);
  params& add(const uuid& id);
  params& add(const json_text& json);
  params& add(const xml_text& xml);
  params& add(const value& copied);
  template <typename T> params& add(const std::optional<T>& maybe);
  std::size_t size() const noexcept;
  void clear() noexcept;
};
}
```

### DESCRIPTION

A `params` is a list of values, one for each `?` of a statement in
order, made by `add` a value at a time or by `params::of(values...)` at
once. Each `add` takes the C++ type of a value's kind; an integer is an
exact number of scale 0; `std::nullopt`, `nullptr` or an empty
`std::optional` the null value; `json_text` a JSON value of its text,
read as Turborel reads it; `xml_text` an XML value of its serialization;
a `value` read from a result is copied, of whatever kind. The server
assigns each to the parameter's type, as SQL's assignment says. `size()`
is how many it holds, and `clear()` empties it. A `params` is moved, not
copied.

### ERRORS

`add` of JSON text no JSON value is fails with SQLCODE -301, SQLSTATE
22032, and of text nesting arrays and objects past 128, Turborel's limit,
with -900, 0A000, before anything is sent; the list is as it was. A
datetime out of its range fails as it is added ([Datetimes and
intervals](#datetimes-and-intervals)).

### EXAMPLES

```cpp
#include <turborel/turborel.hpp>

#include <chrono>
#include <iostream>
#include <optional>

int main() {
  using namespace std::chrono_literals;
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  turborel::params values;
  values.add(turborel::decimal{.units = turborel::int128{-1250}, .scale = 2})
      .add(42)
      .add("Ada")
      .add(turborel::date{.day = std::chrono::year{2026} / 11 / 20})
      .add(turborel::timestamp{.instant = std::chrono::sys_days{std::chrono::year{2026} / 11 / 20} + 9h, .precision = 6})
      .add(std::optional<double>{})
      .add(turborel::json_text{R"({"tea":[1.50,"green"]})"})
      .add(turborel::uuid::parse("919108f7-52d1-4320-9bac-f847db4148a8").value_or(turborel::uuid{}));
  const turborel::result back = conn.query_with(
      "VALUES (CAST(? AS DECIMAL(9,2)), CAST(? AS INTEGER), CAST(? AS VARCHAR(5)), CAST(? AS DATE), "
      "CAST(? AS TIMESTAMP), CAST(? AS DOUBLE PRECISION), CAST(? AS JSON), CAST(? AS UUID))",
      values);
  for (std::size_t i = 0; i < values.size(); ++i) {
    std::cout << back.at(0).at(i).to_string() << '\n';
  }
}
```

```
-12.50
42
Ada
2026-11-20
2026-11-20 09:00:00.000000
NULL
{"tea":[1.50,"green"]}
919108f7-52d1-4320-9bac-f847db4148a8
```

## Exact numbers and 128-bit integers

### NAME

`turborel::decimal`, `turborel::int128`: exact numbers as the protocol
carries them.

### SYNOPSIS

```cpp
#include <turborel/decimal.hpp>

namespace turborel {
class int128 {
public:
  constexpr int128() noexcept;
  constexpr explicit int128(std::int64_t value) noexcept;
  static constexpr int128 from_halves(std::int64_t high, std::uint64_t low) noexcept;
  static constexpr int128 min() noexcept;
  static constexpr int128 max() noexcept;
  constexpr std::int64_t high() const noexcept;
  constexpr std::uint64_t low() const noexcept;
  std::string to_string() const;
  static std::optional<int128> parse(std::string_view text) noexcept;
};

struct decimal {
  int128 units;
  std::uint32_t scale{};
  std::string to_string() const;
  static std::optional<decimal> parse(std::string_view text);
};
}
```

### DESCRIPTION

An exact number is its units, a 128-bit integer, divided by ten to its
scale, so every value the protocol carries is held with no loss: C++20
has no 128-bit integer, and no decimal type. `int128` is two's
complement, its `high()` and `low()` halves; it compares and orders as
an integer, and is written and read as decimal text. A `decimal`'s
`to_string()` writes it as Turborel does, `-12.50` of units -1250 and
scale 2, or `5E-40` of a scale past 18, and `parse` reads what it writes:
an optional sign, digits, then either a point and digits or `E-` and the
scale; text of no such number, or of units out of range, gives none. These types carry
values and do no arithmetic (RULES.md D16).

## Datetimes and intervals

### NAME

`turborel::date`, `turborel::time`, `turborel::timestamp`,
`turborel::interval`, `turborel::datetime_fields`: datetimes in
`std::chrono` types.

### SYNOPSIS

```cpp
#include <turborel/datetime.hpp>
#include <turborel/interval.hpp>

namespace turborel {
struct date { std::chrono::year_month_day day; };

struct time {
  std::chrono::microseconds utc{};
  std::chrono::minutes displacement{};
  bool zoned{};
  std::uint8_t precision{};
  std::chrono::hh_mm_ss<std::chrono::microseconds> local() const;
};

struct timestamp {
  std::chrono::sys_time<std::chrono::microseconds> instant;
  std::chrono::minutes displacement{};
  bool zoned{};
  std::uint8_t precision{};
  std::chrono::local_time<std::chrono::microseconds> local() const;
};

enum class interval_field : std::uint8_t { year, month, day, hour, minute, second };
struct interval_qualifier {
  interval_field start, end;
  std::uint32_t leading_precision{}, fractional_precision{};
  constexpr bool year_month() const noexcept;
};
struct interval {
  interval_qualifier qualifier;
  int128 units;
  std::optional<std::chrono::months> months() const noexcept;
  std::optional<std::chrono::microseconds> microseconds() const noexcept;
};
}
```

### DESCRIPTION

A date is a `std::chrono::year_month_day`. A time is its UTC since
midnight, and a timestamp its instant in UTC, each with its
displacement, whether it is WITH TIME ZONE, and its precision, the
digits of its fraction; `local()` adds the displacement. Each maps to the
protocol's fields, `datetime_fields`, and back by `fields()` and
`from_fields()`, its microseconds counted from 0001-01-01 UTC, as
Turborel keeps them. An interval is its qualifier and its units, months
of a year-month interval, microseconds of a day-time one, a 128-bit
integer; `months()` or `microseconds()` gives them as `std::chrono`'s
where they are of that kind and fit.

### ERRORS

A datetime out of 0001 to 9999 fails with SQLCODE -301, SQLSTATE 22008,
and a displacement out of -12:59 to +14:00 with 22009, as it is made of
fields or added as a parameter.

## DECFLOAT and UUIDs

### NAME

`turborel::decfloat`, `turborel::uuid`.

### SYNOPSIS

```cpp
namespace turborel {
struct decfloat { int128 coefficient; std::int32_t exponent{}; };

class uuid {
public:
  static constexpr std::size_t octet_count = 16;
  constexpr uuid() noexcept;
  constexpr explicit uuid(std::span<const std::byte, octet_count> octets) noexcept;
  constexpr const std::array<std::byte, octet_count>& octets() const noexcept;
  std::string to_string() const;
  static std::optional<uuid> parse(std::string_view text);
};
}
```

### DESCRIPTION

A DECFLOAT is its coefficient times ten to its exponent. A UUID is its 16
octets; `to_string()` writes it as Turborel does, in lower case with
hyphens, and `parse` reads text as Turborel's CAST reads it: 32
hexadecimal digits in either case, white space around them trimmed,
braced or not, a hyphen allowed after each group of four digits but the
last; it gives none of other text.

## XML nodes

### NAME

`turborel::xml_tree`, `turborel::xml_node`, `turborel::xml_kind`: an
XML value's nodes.

### SYNOPSIS

```cpp
#include <turborel/xml.hpp>

namespace turborel {
enum class xml_kind : std::uint8_t { document, element, attribute, text, comment, processing_instruction };

class xml_node {
public:
  xml_kind kind() const noexcept;
  std::optional<std::string_view> name() const noexcept;
  std::optional<std::string_view> namespace_uri() const noexcept;
  std::optional<std::string_view> prefix() const noexcept;
  std::optional<std::string_view> value() const noexcept;
  std::size_t child_count() const noexcept;
  xml_node child(std::size_t index) const;
  std::size_t attribute_count() const noexcept;
  xml_node attribute(std::size_t index) const;
};

class xml_tree {
public:
  static xml_tree parse(std::string_view text);
  static xml_tree of(const value& xml);
  xml_node root() const noexcept;
};
}
```

### DESCRIPTION

An XML value is read as its serialization, its `text()`; `xml_tree::of`
parses it into nodes, as `xml_tree::parse` parses any text, its root a
document node. Each node has its kind, its name, namespace and prefix
where it has them, its value, of an attribute, text, a comment or a
processing instruction, and its children and attributes, in order. A
node is a view, valid while its tree is.

### ERRORS

Text that is not well formed fails with SQLCODE -301, SQLSTATE 2200N;
`of` of a value that is not XML with -910, TB910; a child or attribute
past the last with -910, TB910.

## STANDARD

The values of The Native Protocol, version 1, its sections Values and
fields, Data types, Values and JSON values; their C++ types are the
driver's own (RULES.md D16).

## SEE ALSO

[Appendix A: Types](../appendix/a-types.md),
[Statements and Cursors](22-statements.md),
[Large Objects](24-large-objects.md)
