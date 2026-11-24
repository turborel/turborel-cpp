# Errors

[Contents](README.md)

## NAME

`turborel::error`: every failure of the driver, thrown as an exception.

## SYNOPSIS

```cpp
#include <turborel/error.hpp>

namespace turborel {

struct diagnostics {
  std::optional<std::string> severity, detail, hint, context;
  std::optional<std::string> schema, table, column, data_type, constraint;
};

struct failure {
  std::int32_t sqlcode{};
  std::string sqlstate;
  std::string message;
  std::optional<std::string> rule;
  std::optional<std::uint64_t> position;
  bool active{};
  std::optional<turborel::diagnostics> diagnostics;
};

class error : public std::runtime_error {
public:
  explicit error(failure what_failed);
  const failure& details() const noexcept;
  std::int32_t sqlcode() const noexcept;
  std::string_view sqlstate() const noexcept;
  std::string_view message() const noexcept;
};

}
```

## DESCRIPTION

A call either does what it says or throws a `turborel::error`; no
failure is a return code. A `failure` holds what the protocol's failure
holds: its SQLCODE, negative for an exception; its SQLSTATE, five
characters; its message in plain words; the clause of a standard it
cites, if any; where in the statement it was found, counting
characters, if known; and whether the session's transaction is still
active after it. Granted the capability `diagnostics`, the server says
more, and `diagnostics` holds it: the severity, ERROR, FATAL, WARNING or
NOTICE; a detail, such as the key a constraint refused; a hint; the
trigger or routine running; and the schema, table, column, data type and
constraint it is about, each where it is known.

An `error` is a `std::runtime_error`, so a program that catches those
catches it. `details` gives its failure whole, and `sqlcode`, `sqlstate`
and `message` its commonest parts. Its `what()` reads as turborel-client
shows a failure, the rule in brackets and the position after it where
the failure has them:

```text
SQLCODE -103 (SQLSTATE 42000): table ANN.NOPE is not defined [X3.135-1986 §5.4 SR5] (at character 15)
```

Copying an `error` never throws: its copies share one failure.

A failure of class 08, but 08003, ends the connection, as one of class
28 does as a connection is made; every call on a lost connection then
fails with -919, 08006. Every other failure leaves the connection ready
for the next statement, its transaction as `active` says. A warning
does not fail its call: it is the completion's SQLSTATE, a condition or
a notice. Memory not had is thrown as `std::bad_alloc`, not as an
`error`, as the standard library throws it. What a program's own code
throws in a call the driver makes of it, a handler, a stream's read or a
connector, is thrown as it was, once the driver's call returns.
`turborel::import_error`, an `error` too, says what stopped an import
([Copy, Import and Export](25-copy.md)). [Appendix B:
Errors](../appendix/b-errors.md) lists every code the driver gives.

## ERRORS

None: it is what the driver's other calls throw.

## EXAMPLES

```cpp
#include <turborel/turborel.hpp>

#include <iostream>

int main() {
  try {
    throw turborel::error{turborel::failure{.sqlcode = -919, .sqlstate = "08006", .message = "lost"}};
  } catch (const turborel::error& e) {
    std::cout << e.what() << '\n' << e.sqlstate() << '\n';
  }
}
```

```text
SQLCODE -919 (SQLSTATE 08006): lost
08006
```

## STANDARD

[PROTOCOL Composite fields], the failure; [PROTOCOL Errors and notices],
its diagnostics.

## SEE ALSO

[Errors and Diagnostics](../user/10-errors.md),
[Appendix B: Errors](../appendix/b-errors.md)
