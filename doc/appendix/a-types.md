# Appendix A: Types

[Contents](README.md)

This appendix maps every data type and value tag of the native
protocol, 1 to 27, to the C++ type the driver gives and takes, with the
bounds of each, such as the exact number's units and scale, a
timestamp's displacement and an interval's qualifier.

The driver reads and writes every type and value below through
turborel-c, with the bounds given; a reply that breaks one fails the
connection, SQLCODE -919, SQLSTATE 08006, unless the value alone is
damaged, which fails its statement, as the last section says.
[Values](../client/23-values.md) gives the members that read each value
and make each parameter.

## Data types

A column's or a parameter's type is a `turborel::data_type`, read by its
`kind()`, a `turborel::kind` numbered by the tag, its `length()`,
`precision()` and `scale()`, and `to_string()`, the type as SQL writes
it.

| Tag | Type | What it carries |
|---|---|---|
| 1 | CHARACTER | length; units characters or octets |
| 2 | exact numeric | precision and scale |
| 3 | approximate numeric | precision, in bits |
| 4 | a plug-in's type | its name, UTF-8; at most 2 parameters |
| 5 | CHARACTER VARYING | as CHARACTER |
| 6, 7 | BINARY, BINARY VARYING | length |
| 8 to 12 | DATE, TIME, TIME WITH TIME ZONE, TIMESTAMP, TIMESTAMP WITH TIME ZONE | precision |
| 13 | INTERVAL | start and end fields, 0 YEAR, 1 MONTH, 2 DAY, 3 HOUR, 4 MINUTE, 5 SECOND; leading and fractional precision |
| 14 | BOOLEAN | nothing |
| 15 | DECFLOAT | precision |
| 16 | array | maximum cardinality, 1 to 65535; element type |
| 17 | row | one or more fields, each a name and a type |
| 18, 19 | CLOB, BLOB | length, up to 64 bits; units |
| 22 | multiset | element type |
| 23 | structured type | its schema and name; its proper supertypes |
| 24 | reference type | the referenced type's names; its scope, if scoped |
| 25 | JSON | nothing |
| 26 | XML | primary modifier DOCUMENT, CONTENT or SEQUENCE; secondary UNTYPED, ANY or XMLSCHEMA, and of XMLSCHEMA the schema's names, its namespace and element if named; SEQUENCE is untyped |
| 27 | UUID | nothing |

Arrays, rows and multisets nest at most 16 deep, as Turborel's types do.
A distinct type is told as its source type, and a plug-in's type the
client does not know reads by its name, written `UNREGISTERED`, as
Turborel's own client writes it.

A result's or prepared statement's column also tells whether it is NOT
NULL, its `not_null`.
Granted the capability `declared-types`, which a connection asks for, a
column has the type it was declared with too: its `declared_name`, the
Information Schema's DATA_TYPE, as INTEGER, and its `type_written`, as
SQL writes it, so INTEGER is told from NUMERIC(10,0).

## Values

A value is a `turborel::value`, read by its `kind()`; its readers each
give one kind, and fail, SQLCODE -910, SQLSTATE TB910, of another.
`get<T>()` reads it as `T`, and `get<std::optional<T>>()` gives an empty
optional of the null value.

| Tag | Value | Read as | Sent as | Bounds |
|---|---|---|---|---|
| 0 | null | `is_null()`; an empty `std::optional` | `std::nullopt`, `nullptr`, an empty `std::optional` | |
| 1 | character string | `std::string_view`, `std::string` | `std::string_view`, `const char*` | UTF-8 |
| 2 | exact number | `turborel::decimal`; `std::int64_t` of scale 0 where it fits | `turborel::decimal`; an integer type, but `bool` and `char` | units the whole of `turborel::int128`; scale the whole of `std::uint32_t` |
| 3 | approximate number | `double`, its 64 bits kept | `double` | every bit kept: NaN, -0.0 and the infinities too |
| 4 | a plug-in's value | its text, `text()`; its bytes, `bytes()` | the value read, copied | kept and sent back unchanged |
| 6 | binary string | `std::span<const std::byte>` | `std::span<const std::byte>` | |
| 8 | date | `turborel::date`, a `std::chrono::year_month_day` | `turborel::date` | 0001-01-01 to 9999-12-31 |
| 9 | time | `turborel::time`: UTC since midnight, `std::chrono::microseconds`; displacement, `std::chrono::minutes`; zoned; precision | `turborel::time` | within a day; displacement -12:59 to +14:00 |
| 11 | timestamp | `turborel::timestamp`: a `std::chrono::sys_time` of microseconds, displacement, zoned, precision | `turborel::timestamp` | 0001 to 9999; displacement as a time's |
| 5, 7, 10, 12 | the types' own tags | read as 1, 6, 9 and 11, as turborel-protocol's reader takes them: a character string, a binary string, a time and a timestamp, a zone told inside; the driver sends only 1, 6, 9 and 11 | | |
| 13 | interval | `turborel::interval`: its qualifier and `turborel::int128` units, months of a year-month interval, microseconds of a day-time one; `std::chrono::months` or `std::chrono::microseconds` where they fit | `turborel::interval` | fields 0 to 5 |
| 14 | truth value | `bool` | `bool` | unknown is null |
| 15 | DECFLOAT | `turborel::decfloat`: coefficient `turborel::int128`, exponent `std::int32_t` | `turborel::decfloat` | canonical: at most 34 digits, no trailing zero, zero of exponent 0, adjusted exponent -6143 to 6144 |
| 16, 17 | array, row | `size()` and `at(i)`, each a `turborel::value` | | nested at most 16 deep |
| 20 | large object whole | `lob()`, a `turborel::lob_info`; its octets, `text()` or `bytes()` | `std::string_view`, `std::span<const std::byte>` | a character one's UTF-8 |
| 18, 19 | a large object's stored form, which no server sends | refused: the statement fails, SQLCODE -916, SQLSTATE TB916, the connection kept (RULES.md D8) | | |
| 21 | large object by locator | `lob()`: character or binary, its octets and characters, read as [Large Objects](../client/24-large-objects.md) says | a locator a `turborel::lob_writer` made | |
| 22 | multiset | `size()` and `at(i)` | | at most 65535 elements |
| 23 | structured value | `size()` and `at(i)`, its attributes | | |
| 24 | reference | `bytes()` | | |
| 25 | JSON | `json()`, JSON text; `bytes()`, the stored form of Turborel's RULES.md D89 | `turborel::json_text` | checked whole; nested at most 128 deep |
| 26 | XML | `text()`, its serialization as CONTENT, UTF-8; `turborel::xml_tree::of`, its nodes | `turborel::xml_text` | |
| 27 | UUID | `turborel::uuid`, 16 `std::byte`s | `turborel::uuid` | exactly 16 |

Every value reads as text too, by `to_string()`, as Turborel displays
it: a string as itself, a number as Turborel writes it, a datetime as its
literal's string, an array, row, multiset, structured value or
reference as its literal, NULL of the null value.

A datetime's microseconds count from 0001-01-01 UTC, as Turborel keeps
them. A datetime made out of 0001 to 9999 fails with SQLSTATE 22008, and
a displacement out of range with 22009, each with SQLCODE -301.

A JSON value's text is written as Turborel writes it: no white space,
members in the order written, an exact number's digits, its scale kept,
1.50 as `1.50`, a DECFLOAT plain, an approximate number as its shortest
numeral with an exponent, `1.5E0`. A JSON text made into a parameter
takes a number without an exponent as exact, one of more digits than an
exact number holds as a DECFLOAT, and one with an exponent as
approximate. Text no JSON value is fails, SQLCODE -301, SQLSTATE 22032;
text nesting arrays and objects past 128 fails before it is sent,
SQLCODE -900, SQLSTATE 0A000, as Turborel refuses it. A JSON value that
holds a datetime, or an approximate number that is not finite, has no
JSON text: `json()` fails, SQLSTATE 22032, and `bytes()` gives its stored
form.

## A value the driver cannot take

A reply whose frame is whole but holds a value the driver cannot take,
such as a truth value's byte other than 0 or 1, a damaged JSON document
or one of another version, or tag 18 or 19, fails its statement alone,
SQLCODE -916, SQLSTATE TB916, or -900, SQLSTATE 0A000, of a value past a
limit; the rest of the statement's replies are read past, and the
connection goes on. Only a malformed frame, cut short, with bytes after
its last field, or of a kind not known, loses the connection, SQLCODE
-919, SQLSTATE 08006.
