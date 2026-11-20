# Copying, Importing and Exporting

[Contents](README.md)

Rows go into a table fastest by a copy, sent many to a message with no
reply but the copy's end, all or nothing. An import reads rows from
delimited text, or from a source of the program's own, a record at a
time, and inserts them a batch to each transaction. An export writes a
query's rows as they come, as CSV or as the protocol's own frames.
[Copy, Import and Export](../client/25-copy.md) gives each call in full.

## Copy

`copy::into(conn, table, columns)` begins a copy into a table, of the
columns named, or of every column; `send` sends a row, a `params` of its
values, with no reply; `finish` ends it, giving the rows inserted. A row
refused undoes every row, its failure's detail naming the row; a copy
abandoned, or destroyed before it finishes, inserts none.
`copy_rows(conn, table, columns, next)` copies every row a function
gives, until it gives none:

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

`export_rows(conn, query, format, stream)` runs a query and writes its
rows to a `std::ostream` as they come: as CSV, a header of the columns'
names then a record for each row, each ended by CR LF; or as the
protocol's own frames, which `read_native` reads back, every value as it
was:

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

`import_rows(conn, source, options)` inserts a source's records into a
table, by copy where the server grants it and no column is a large
object's, else by INSERT, a batch of 1000 to each transaction unless the
options say another, 0 for all at once. An import begins with no
transaction active. A `delimited_text` reads CSV or tab-separated text
from a `std::istream`, as its `dialect` says: a header naming the
columns, the delimiter, quoting, and a text for null. A record that
fails stops the import, its batch rolled back and those before kept,
`import_error` saying where it began, why, and the rows committed and
rolled back; with `skip`, it is skipped instead and given to the
`rejected` handler, by INSERT:

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

A source of the program's own is an `import_source`: its headings, if it
has them, then each record, with where it began:

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

A field longer than 64 KiB for a large object's column is sent by
creating a large object, as [Large Objects](06-large-objects.md) does.
