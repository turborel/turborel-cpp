# Appendix A: Types

[Contents](README.md)

This appendix will map every data type and value tag of the native
protocol, 1 to 27, to the C++ type the driver gives and takes,
with the bounds of each, such as the exact number's units and scale, a
timestamp's displacement and an interval's qualifier. It is filled at
milestone M1.

## The types defined so far

The driver reads and writes these through turborel-c once it wraps it;
their C++ types exist today (RULES.md D16).

| Tag | Protocol | C++ |
|---|---|---|
| 2 | exact number: units `i128`, scale `u32` | `turborel::decimal`, of a `turborel::int128` and a `std::uint32_t`; every value, with no loss |
| 8 | date | `turborel::date`, a `std::chrono::year_month_day`, 0001-01-01 to 9999-12-31 |
| 9 | time, WITH TIME ZONE or not | `turborel::time`: UTC since midnight in `std::chrono::microseconds`, the displacement in `std::chrono::minutes`, -12:59 to +14:00, zoned and precision |
| 11 | timestamp, WITH TIME ZONE or not | `turborel::timestamp`: a `std::chrono::sys_time` of microseconds, the displacement, zoned and precision |
| 13 | interval | `turborel::interval`: its qualifier, fields numbered 0 YEAR to 5 SECOND, and `int128` units, months or microseconds; `std::chrono::months` or `std::chrono::microseconds` where they fit |
| 27 | UUID | `turborel::uuid`, 16 `std::byte`s, written as turborel writes it |

A datetime's microseconds count from 0001-01-01 UTC, as Turborel keeps
them. A datetime out of 0001 to 9999 fails with SQLSTATE 22008, and a
displacement out of range with 22009, each with SQLCODE -301.
