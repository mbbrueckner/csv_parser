# csv_parser

A lightweight, header-only C++20 CSV parsing library with automatic type inference.

## Features

- **Header-only** — drop in a single `#include`
- **Automatic type inference** — samples the first N rows and votes each column as `INT`, `DOUBLE`, `BOOL`, or `STRING`
- **Explicit schema** — override inference by passing a `DType` list directly
- **Header row support** — read column names from the file or supply them yourself
- **RFC-4180 quoting** — fields containing the separator or `"` are handled correctly on both read and write
- **Null/empty cell handling** — empty cells, `NaN`, and `NULL` map to `std::monostate`
- **Column access** — select a column by name or index via `operator[]`, returning a lazy `ColumnView` with iterator support
- **Data manipulation** — add or remove rows and columns after loading

## Requirements

- C++20 or later
- CMake 3.14+ (for the test suite)
- [Catch2 v3](https://github.com/catchorg/Catch2) (for the test suite)

## Installation

The library is header-only. Copy `include/csv_parser.hpp` into your project and include it:

```cpp
#include "csv_parser.hpp"
```

Or point your build system at the `include/` directory.

## Quick start

```cpp
#include "csv_parser.hpp"

// Read a file — auto-detect types, treat first row as header
auto doc = csv::Document::fromFile("data.csv", {.sep = ',', .hasHeader = true});

// Inspect header
for (const auto &name : doc.columnNames())
    std::cout << name << " ";

// Iterate over data rows
for (const auto &row : doc.data()) {
    for (const auto &cell : row) {
        std::visit([](const auto &v) { std::cout << v << " "; }, cell);
    }
    std::cout << '\n';
}

// Write back to disk (header is preserved)
doc.toFile("out.csv");

// Access a column by name (requires header)
for (const auto &cell : doc["score"])
    std::cout << std::get<double>(cell) << '\n';

// Access a column by zero-based index
auto first = doc[0];
std::cout << first.size() << " rows\n";
```

## API reference

### `csv::DType`

```cpp
enum class DType { STRING, INT, DOUBLE, BOOL };
```

### `csv::CellValue`

```cpp
using CellValue = std::variant<std::monostate, long, double, std::string, bool>;
```

### `csv::Document`

#### Constructor

```cpp
explicit Document(char separator = ',', std::vector<std::string> columnNames = {});
```

Creates an empty document. Mostly used internally; prefer `fromFile` for loading data.

#### `csv::ParseOptions`

```cpp
struct ParseOptions {
    char                     sep        = ',';   // field delimiter
    size_t                   sampleRows = 100;   // rows used for type inference
    bool                     hasHeader  = false; // treat first line as header
    std::vector<DType>       dTypes     = {};    // explicit schema; skips voting
    std::vector<std::string> colNames   = {};    // override column names
};
```

#### `fromFile` (static factory)

```cpp
static Document fromFile(const std::string &filename, ParseOptions opts = {});
```

| Field | Description |
|---|---|
| `filename` | Path to the CSV file |
| `opts.sep` | Field delimiter (default `,`) |
| `opts.sampleRows` | Rows used for automatic type inference (default 100); all rows are still parsed |
| `opts.dTypes` | Explicit per-column type list; skips inference when non-empty |
| `opts.hasHeader` | Treat the first line as a header row (not included in `data()`) |
| `opts.colNames` | Override column names; if `hasHeader` is also `true`, the file's header line is consumed and discarded |

Returns an empty `Document` if the file cannot be opened or contains no data rows.

#### `toFile`

```cpp
void toFile(const std::string &filename);
```

Writes the document to a CSV file. The header row (if any) is written first, followed by data rows. String cells containing the separator or `"` are quoted per RFC-4180.

#### Accessors

```cpp
size_t                                    rowCount()    const;
const std::vector<std::vector<CellValue>> &data()       const;
const std::vector<std::string>            &columnNames() const;
char                                       getSeparator() const;
```

#### `operator[]` — column access

```cpp
ColumnView operator[](const std::string &name) const;  // by header name
ColumnView operator[](size_t col)              const;  // by zero-based index
```

Returns a `ColumnView` — a non-owning, read-only proxy for a single column. Throws `std::out_of_range` if the name or index is invalid.

#### `csv::Document::ColumnView`

A lightweight column proxy. The `Document` must outlive any `ColumnView` obtained from it.

```cpp
size_t          size()           const;  // number of rows
const CellValue &operator[](size_t row) const;  // throws on out-of-bounds

// Forward iterator — supports range-based for
for (const auto &cell : doc["name"]) { ... }
```

#### Data manipulation

```cpp
// Append a row (missing cells are filled with std::monostate)
doc.addRow({1L, 3.14, std::string("hello")});

// Remove the row at zero-based index
doc.removeRow(2);

// Append a new column (values must match rowCount(), or be empty for all-null)
doc.addColumn("score", csv::DType::DOUBLE, {1.0, 2.0, 3.0});

// Remove a column by index or by name
doc.removeColumn(0);
doc.removeColumn("score");
```

All four methods throw `std::invalid_argument` or `std::out_of_range` on invalid input.

## Example

The `example/` directory contains a worked example using `grades.csv`:

```
name,math,english,science
Alice,85,92,78
Bob,91,84,95
Charlie,73,88,82
Diana,96,79,91
```

The program ([example/main.cpp](example/main.cpp)):

1. Loads the CSV with a header row
2. Computes the per-student average across the grade columns
3. Appends an `average` column with those values
4. Adds a new student row (`Eve`)
5. Finds and prints the best-performing student
6. Writes the result to `grades_out.csv`

Build and run after compiling the project:

```bash
./example
```

Expected output:

```
=== Loaded 4 students ===

Alice → average: 85
Bob → average: 90
Charlie → average: 81
Diana → average: 88.6667

Added student Eve

Best student: Bob (90)

Results written to grades_out.csv
```

## Building and running tests

```bash
mkdir build && cd build
cmake ..
cmake --build .
./unit_tests   # run the test suite
./example      # run the grades example
```

## License

MIT
