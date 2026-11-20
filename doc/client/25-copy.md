# Copy, Import and Export

[Contents](README.md)

Rows copied into a table, imported from delimited text or a source of
the program's own, and a query's rows exported as CSV or as the
protocol's frames.

## Copy

### NAME

`turborel::copy`, `turborel::copy_rows`: a bulk copy into a table.

### SYNOPSIS

```cpp
#include <turborel/copy.hpp>

namespace turborel {
inline constexpr std::size_t copy_chunk_bytes = 262144;

class copy {
public:
  static copy into(connection& conn, const std::string& table, const std::vector<std::string>& columns = {});
  copy& send(const params& row);
  std::uint64_t finish();
  void abandon();
};

std::uint64_t copy_rows(connection& conn, const std::string& table, const std::vector<std::string>& columns,
                        const std::function<bool(params&)>& next);
}
```

### DESCRIPTION

`into` begins a copy into a table by CopyIn, of the columns named in
each row's order, or of every column in the table's order; until it
ends, the connection sends only its rows, its end and Ping. `send` sends
a row, with those before it, as CopyData once they are about
`copy_chunk_bytes`, with no reply. `finish` ends it by CopyEnd, giving
the rows inserted, all or none; `abandon` gives it up, every row undone,
as the copy's destructor does of one not ended. A copy under autocommit
is committed as it succeeds; one in a transaction runs under a savepoint
of its own, so its failure keeps the transaction's earlier work.
`copy_rows` copies every row `next` gives, each into the `params` it is
handed, until it returns false; what it throws abandons the copy, and is
thrown.

### ERRORS

A row refused fails `finish` with its failure, its detail naming the row
as `row 7 of the copy`, every row undone. Without `copy` granted, `into`
fails with SQLCODE -919, SQLSTATE 08006, before sending.

### EXAMPLES

Made beforehand, as the owner:

```sql
CREATE TABLE READINGS (SENSOR INTEGER, VAL DECIMAL(6,2) CHECK (VAL >= 0));
```

```cpp
#include <turborel/turborel.hpp>

#include <cstdint>
#include <iostream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  turborel::copy into = turborel::copy::into(conn, "READINGS", {"SENSOR", "VAL"});
  for (int i = 0; i < 10000; ++i) {
    into.send(turborel::params::of(i % 7, turborel::decimal{.units = turborel::int128{i}, .scale = 2}));
  }
  std::cout << into.finish() << " rows copied\n";
  int next = 0;
  const std::uint64_t more = turborel::copy_rows(conn, "READINGS", {}, [&next](turborel::params& row) {
    if (next == 3) {
      return false;
    }
    row.add(9).add(turborel::decimal{.units = turborel::int128{++next}, .scale = 0});
    return true;
  });
  std::cout << more << " more\n";
  turborel::copy bad = turborel::copy::into(conn, "READINGS", {"SENSOR", "VAL"});
  bad.send(turborel::params::of(1, turborel::decimal{.units = turborel::int128{5}, .scale = 0}));
  bad.send(turborel::params::of(2, turborel::decimal{.units = turborel::int128{-5}, .scale = 0}));
  try {
    static_cast<void>(bad.finish());
  } catch (const turborel::error& e) {
    std::cout << e.sqlstate() << ", " << e.details().diagnostics.value_or(turborel::diagnostics{}).detail.value_or("")
              << '\n';
  }
  static_cast<void>(conn.query("COMMIT"));
  std::cout << conn.query("SELECT COUNT(*) FROM READINGS").at(0).at(0).to_string() << " in all\n";
}
```

```
10000 rows copied
3 more
23000, row 2 of the copy
10003 in all
```

## Export

### NAME

`turborel::export_rows`, `turborel::read_native`,
`turborel::export_format`.

### SYNOPSIS

```cpp
namespace turborel {
enum class export_format : std::uint8_t { csv, native };

std::uint64_t export_rows(connection& conn, const std::string& query, export_format format, std::ostream& out);
result read_native(std::istream& in);
}
```

### DESCRIPTION

`export_rows` runs a query and writes its rows to `out` as they come, a
batch at a time, giving the rows written: as CSV (RFC 4180), a header of
the columns' names then a record for each row, each ended by CR LF, a
null as nothing; or `native`, the protocol's own frames, a Description,
Rows and a Complete. `read_native` reads back what a native export
wrote: its columns and rows, every value as it was.

### ERRORS

A statement that is no query fails, as does one whose stream fails, by
its state or by throwing; the connection is kept. Frames that are cut
short or malformed fail `read_native` with SQLCODE -919, SQLSTATE 08006.

### EXAMPLES

Made beforehand, as the owner:

```sql
CREATE TABLE STAFF (ID INTEGER, NAME VARCHAR(20));
INSERT INTO STAFF VALUES (1, 'Ada'), (2, 'Grace, Hopper'), (3, NULL);
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>
#include <sstream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  std::ostringstream csv;
  turborel::export_rows(conn, "SELECT ID, NAME FROM STAFF ORDER BY ID", turborel::export_format::csv, csv);
  std::cout << csv.str();
  std::stringstream native;
  turborel::export_rows(conn, "SELECT ID, NAME FROM STAFF ORDER BY ID", turborel::export_format::native, native);
  const turborel::result back = turborel::read_native(native);
  std::cout << back.size() << " rows read back, " << back.columns().at(1).type_written << '\n';
}
```

```
ID,NAME
1,Ada
2,"Grace, Hopper"
3,
3 rows read back, CHARACTER VARYING(20)
```

## Import

### NAME

`turborel::import_rows`, `turborel::delimited_text`, `turborel::dialect`,
`turborel::import_source`, `turborel::import_options`,
`turborel::import_error`.

### SYNOPSIS

```cpp
#include <turborel/import.hpp>

namespace turborel {
inline constexpr std::uint64_t import_batch = 1000;

struct record_position { std::uint64_t record{}; std::uint64_t line{}; };
struct import_cause { bool sql{}; std::string message; std::optional<failure> condition; };
struct rejected_row { record_position at; import_cause cause; };
struct import_options {
  std::string table;
  std::uint64_t batch = import_batch;
  bool skip = false;
  std::function<void(const rejected_row&)> rejected;
};
struct imported { std::uint64_t rows{}; std::uint64_t rejected{}; };

class import_error : public error {
public:
  const import_cause& cause() const noexcept;
  std::optional<record_position> at() const noexcept;
  std::uint64_t committed() const noexcept;
  std::uint64_t rolled_back() const noexcept;
};

class import_source {
public:
  virtual std::optional<std::vector<std::string>> headings() = 0;
  virtual bool record(params& values, record_position& at) = 0;
};

struct dialect {
  char delimiter{','};
  bool quoted{true};
  bool header{};
  std::string null;
  std::size_t max_record;
  static dialect csv();
  static dialect tsv();
};

class delimited_text {
public:
  delimited_text(std::istream& in, const dialect& how);
};

imported import_rows(connection& conn, import_source& source, const import_options& how);
imported import_rows(connection& conn, delimited_text& source, const import_options& how);
}
```

### DESCRIPTION

`import_rows` inserts a source's records into a table, a record held at
a time: by copy where the server grants it and no column is a large
object's, else by INSERT; a batch of `batch` rows to each transaction,
0 for all at once. A source's headings name the columns its fields are,
or, of none, its fields are the table's columns in order. A field longer
than 64 KiB for a large object's column is sent by creating a large
object. With `skip`, a row that fails is skipped and given to `rejected`,
by INSERT, and counted.

A `delimited_text` reads CSV (RFC 4180) or tab-separated text from a
`std::istream`, as its `dialect` says: `csv()` of commas, quoted, no
header; `tsv()` of tabs, unquoted; a header naming the columns, and a
text read as null, where set. An `import_source` of the program's own
gives its headings and its records, each with where it began; what it
throws of `std::exception` stops the import as the input's failure, its
message the cause, and anything else is thrown as it was.

### ERRORS

What stops an import is thrown as an `import_error`, an `error`: the
cause, the input's or SQL's, with the condition of SQL's; where its
record began, if known; and the rows committed and those of the batch
under way rolled back. Its SQLSTATE is the condition's, or 22000 of the
input's, as a record of more fields than the table has columns, a quote
in a field not enclosed in quotes, or one not closed. An import begun in
a transaction, or of a table not named right, is refused before any row.

### EXAMPLES

Made beforehand, as the owner:

```sql
CREATE TABLE PEOPLE (ID INTEGER PRIMARY KEY, NAME VARCHAR(5));
```

```cpp
#include <turborel/turborel.hpp>

#include <iostream>
#include <sstream>

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  turborel::dialect csv = turborel::dialect::csv();
  csv.header = true;
  std::istringstream good{"id,name\r\n1,Ada\r\n2,\"Bri,n\"\r\n3,Dan   \n"};
  turborel::delimited_text people{good, csv};
  std::cout << turborel::import_rows(conn, people, {.table = "PEOPLE"}).rows << " imported\n";
  static_cast<void>(conn.query("COMMIT"));
  std::istringstream bad{"id,name\n4,Eve\n5,Fay\n6,Charlotte\n"};
  turborel::delimited_text more{bad, csv};
  try {
    static_cast<void>(turborel::import_rows(conn, more, {.table = "PEOPLE", .batch = 2}));
  } catch (const turborel::import_error& e) {
    std::cout << e.message() << '\n';
    std::cout << "record " << e.at().value_or(turborel::record_position{}).record << ", " << e.committed()
              << " committed\n";
  }
  std::istringstream skipping{"7,Gus\n7,Hal\n8,Ivy,x\n9,Jo\n"};
  turborel::delimited_text rows{skipping, turborel::dialect::csv()};
  const turborel::imported kept = turborel::import_rows(
      conn, rows, {.table = "PEOPLE", .skip = true, .rejected = [](const turborel::rejected_row& row) {
                     std::cout << "skipped record " << row.at.record << ": " << row.cause.message << '\n';
                   }});
  std::cout << kept.rows << " imported, " << kept.rejected << " skipped\n";
}
```

```
3 imported
a value of 9 characters, not spaces beyond 5, does not fit CHARACTER VARYING(5)
record 3, 2 committed
skipped record 2: PRIMARY KEY ANN.PEOPLE_PRIMARY1 (ID) of ANN.PEOPLE would be violated: a row of these values is there
skipped record 3: [DESIGN] 3 fields, for 2 columns
2 imported, 2 skipped
```

Made beforehand, as the owner:

```sql
CREATE TABLE PEOPLE (ID INTEGER PRIMARY KEY, NAME VARCHAR(20));
```

```cpp
#include <turborel/turborel.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

class staff final : public turborel::import_source {
public:
  std::optional<std::vector<std::string>> headings() override { return std::vector<std::string>{"ID", "NAME"}; }

  bool record(turborel::params& values, turborel::record_position& at) override {
    if (next_ == names_.size()) {
      return false;
    }
    at = turborel::record_position{.record = next_ + 1, .line = next_ + 1};
    values.add(static_cast<std::int64_t>(next_ + 1)).add(names_.at(next_));
    ++next_;
    return true;
  }

private:
  std::vector<std::string> names_{"Ada", "Grace", "Edsger"};
  std::size_t next_{};
};

int main() {
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::connect(turborel::target::tcp("127.0.0.1", 17439), "ann", trust);
  staff source;
  std::cout << turborel::import_rows(conn, source, {.table = "PEOPLE"}).rows << " imported\n";
}
```

```
3 imported
```

## STANDARD

The Native Protocol, version 1, its sections CopyIn 'I', CopyData 'J'
and CopyEnd 'N'; The Client Library, its sections Copying and exporting,
and Importing; RFC 4180, *Common Format and MIME Type for
Comma-Separated Values (CSV) Files*.

## SEE ALSO

[Copying, Importing and Exporting](../user/07-copying.md),
[Values](23-values.md), [Large Objects](24-large-objects.md)
