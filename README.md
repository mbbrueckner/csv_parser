# csv_parser

A lightweight, header-only C++17 CSV parsing library with automatic type inference.

## Features

- **Header-only** — drop in a single `#include`
- **Automatic type inference** — samples the first N rows and votes each column as `INT`, `DOUBLE`, `BOOL`, or `STRING`
- **Explicit schema** — override inference by passing a `DType` list directly
- **Header row support** — read column names from the file or supply them yourself
- **RFC-4180 quoting** — fields containing the separator or `"` are handled correctly on both read and write
- **Null/empty cell handling** — empty cells, `NaN`, and `NULL` map to `std::monostate`

## Requirements

- C++17 or later
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
auto doc = csv::Document::fromFile("data.csv", ',', 100, {}, true);

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

#### `fromFile` (static factory)

```cpp
static Document fromFile(
    const std::string        &filename,
    char                      sep         = ',',
    size_t                    sampleRows  = 100,
    std::vector<DType>        dTypes      = {},
    bool                      hasHeader   = false,
    std::vector<std::string>  columnNames = {}
);
```

| Parameter | Description |
|---|---|
| `filename` | Path to the CSV file |
| `sep` | Field delimiter (default `,`) |
| `sampleRows` | Rows used for automatic type inference (default 100); all rows are still parsed |
| `dTypes` | Explicit per-column type list; skips inference when non-empty |
| `hasHeader` | Treat the first line as a header row (not included in `data()`) |
| `columnNames` | Override column names; if `hasHeader` is also `true`, the file's header line is consumed and discarded |

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

## Building and running tests

```bash
mkdir build && cd build
cmake ..
cmake --build .
./unit_tests
```

## License

MIT
