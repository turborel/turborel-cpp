# turborel::error

[Contents](README.md)

Every failure of the driver, a statement's, a connection's or the
driver's own, is thrown as a `turborel::error`, declared in
`<turborel/error.hpp>` and included by `<turborel/turborel.hpp>`. A call
either does what it says or throws one; no failure is a return code.

## SYNOPSIS

```cpp
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

A `failure` holds what the protocol's failure holds: its SQLCODE,
negative for an exception; its SQLSTATE, five characters; its message in
plain words; the clause of a standard it cites, if any; where in the
statement it was found, counting characters, if known; and whether the
session's transaction is still active after it. Granted the capability
`diagnostics`, the server says more, and `diagnostics` holds it: the
severity, ERROR, FATAL, WARNING or NOTICE; a detail, such as the key a
constraint refused; a hint; the trigger or routine running; and the
schema, table, column, data type and constraint it is about, each where
it is known.

An `error` is a `std::runtime_error`, so a program that catches those
catches it. `details` gives its failure whole, and `sqlcode`, `sqlstate`
and `message` its commonest parts. Its `what()` reads as turborel-client
shows a failure, the rule in brackets and the position after it where the
failure has them:

```text
SQLCODE -103 (SQLSTATE 2A000): table ANN.NOPE is not defined [X3.135-1986 §5.4 SR5] (at character 15)
```

Copying an `error` never throws: its copies share one failure.

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

## SEE ALSO

[Errors](../user/10-errors.md), [Appendix B: Errors](../appendix/b-errors.md)
