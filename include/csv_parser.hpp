/**
 * @file csv_parser.hpp
 * @brief Lightweight CSV parsing library with automatic type inference.
 *
 * Provides the `csv::Document` class for reading, typing, and writing
 * delimiter-separated value files. Column types are either inferred
 * automatically by sampling the first N rows, or supplied explicitly by
 * the caller. An optional header row can be read from the file or provided
 * directly as a list of column names.
 *
 * ### Quick start
 * @code{.cpp}
 * // Read a file, auto-detect types, treat first row as header
 * auto doc = csv::Document::fromFile("data.csv", {.hasHeader = true});
 *
 * // Access header and data
 * for (const auto &name : doc.columnNames()) { ... }
 * for (const auto &row  : doc.data())        { ... }
 *
 * // Write back to disk
 * doc.toFile("out.csv");
 * @endcode
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace csv {

/**
 * @brief Data type used for a CSV column.
 *
 * Used both when supplying an explicit schema to `Document::fromFile` and
 * when reporting the inferred schema after automatic type voting.
 */
enum class DType {
  STRING, ///< Column contains string values.
  INT,    ///< Column contains integer (long) values.
  DOUBLE, ///< Column contains floating-point (double) values.
  BOOL    ///< Column contains boolean values (true/false/yes/no/1/0).
};

/**
 * @brief A single CSV cell value.
 *
 * Holds one of the following alternatives:
 * - `std::monostate` — empty or null cell (NaN / NULL in source)
 * - `long`           — integer value
 * - `double`         — floating-point value
 * - `std::string`    — string value
 * - `bool`           — boolean value
 */
using CellValue = std::variant<std::monostate, long, double, std::string, bool>;

/**
 * @brief Configuration options for Document::fromFile.
 *
 * All fields have sensible defaults so callers only need to specify
 * what differs from the standard case.
 *
 * @code{.cpp}
 * // Only set what you need:
 * auto doc = Document::fromFile("data.csv", {.hasHeader = true});
 * auto doc = Document::fromFile("data.csv", {.sep = ';', .hasHeader = true});
 * @endcode
 */
struct ParseOptions {
    char sep                         = ',';   ///< Field delimiter.
    size_t sampleRows                = 100;   ///< Rows used for type inference.
    bool hasHeader                   = false; ///< Treat first line as header.
    std::vector<DType> dTypes        = {};    ///< Explicit schema; skips voting.
    std::vector<std::string> colNames = {};   ///< Override column names.
};

/**
 * @brief Represents a parsed CSV document.
 *
 * Supports automatic schema inference by sampling the first N rows, or an
 * explicit column-type schema supplied by the caller. An optional header row
 * is stored separately from the data rows and can be written back to disk by
 * `toFile`.
 *
 * Instances are created through the static factory `fromFile` rather than
 * constructed directly (except for empty documents).
 */
class Document {
private:
  std::vector<std::vector<CellValue>>
      m_data; ///< Parsed rows of typed cell values.

  std::vector<std::string> m_header; ///< Column names (empty if none supplied).

  std::vector<DType> m_schema; ///< Per-column type schema used during parsing.

  char m_separator; ///< Separator for CSV columns (default: ',').

  /**
   * @brief Splits a string by a delimiter into a vector of tokens.
   *
   * Handles RFC-4180-style quoting: fields wrapped in double-quotes may
   * contain the delimiter, and a literal `"` is represented as `""`.
   *
   * @param s   The string to split.
   * @param sep The delimiter character.
   * @return    Vector of unquoted substrings between delimiters.
   */
  static std::vector<std::string> split(const std::string &s, char sep) {
    std::vector<std::string> tokens;
    std::string token;
    bool inQuotes = false;
    for (size_t i = 0; i < s.size(); ++i) {
      char c = s[i];
      if (inQuotes) {
        if (c == '"') {
          // escaped quote ("") stays as a single quote in the value
          if (i + 1 < s.size() && s[i + 1] == '"') {
            token += '"';
            ++i;
          } else {
            inQuotes = false;
          }
        } else {
          token += c;
        }
      } else {
        if (c == '"') {
          inQuotes = true;
        } else if (c == sep) {
          tokens.push_back(token);
          token.clear();
        } else {
          token += c;
        }
      }
    }
    tokens.push_back(token);
    return tokens;
  }

  /**
   * @brief Checks whether a string can be parsed as a base-10 integer.
   * @param s The string to test.
   * @return  @c true if @p s is a valid integer with no trailing characters.
   */
  static bool isInteger(const std::string &s) {
    if (s.empty())
      return false;
    char *p;
    strtol(s.c_str(), &p, 10);
    return *p == 0;
  }

  /**
   * @brief Checks whether a string can be parsed as a floating-point number.
   * @param s The string to test.
   * @return  @c true if @p s is a valid double with no trailing characters.
   */
  static bool isDouble(const std::string &s) {
    if (s.empty())
      return false;
    char *p;
    strtod(s.c_str(), &p);
    return *p == 0;
  }

  /**
   * @brief Checks whether a string represents a boolean value.
   *
   * Recognised values (case-insensitive): `true`, `false`, `1`, `0`, `yes`,
   * `no`.
   *
   * @param s The string to test.
   * @return  @c true if @p s matches one of the recognised boolean literals.
   */
  static bool isBoolean(const std::string &s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return (lower == "true" || lower == "false" || lower == "yes" || lower == "no");
  }

  /**
   * @brief Converts a boolean-like string to a `bool`.
   *
   * @param s The string to convert (case-insensitive).
   * @return  @c true for `"true"`or `"yes"`; @c false otherwise.
   */
  static bool stringToBool(const std::string &s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "true" || lower == "yes")
      return true;
    return false;
  }

  /**
   * @brief Converts a raw string cell to a typed @ref CellValue.
   *
   * Empty strings, `"NaN"`, and `"NULL"` are mapped to `std::monostate`.
   * If a numeric conversion throws, the raw string is returned as a fallback.
   *
   * @param val  The raw string value from the CSV.
   * @param type The target data type.
   * @return     Typed @ref CellValue, or the original string on conversion
   *             failure.
   */
  CellValue convert(const std::string &val, DType type) {
    if (val.empty() || val == "NaN" || val == "NULL")
      return std::monostate{};
    try {
      if (type == DType::INT)
        return std::stol(val);
      if (type == DType::DOUBLE)
        return std::stod(val);
      if (type == DType::BOOL)
        return stringToBool(val);
    } catch (...) {
      return val;
    } // Fallback to String
    return val;
  }

public:


  /**
   * @brief A non-owning, read-only view of a single column in the document.
   *
   * `ColumnView` is a lightweight proxy that references the document's
   * internal data grid — it holds no copies of cells. Obtain one via
   * `Document::operator[]`.
   *
   * Supports:
   * - Random access by row index (`operator[]`).
   * - Range-based `for` loops via a built-in forward iterator.
   * - `size()` to query the number of rows.
   *
   * @note The `Document` that produced this view must outlive it.
   */
  class ColumnView {
      const std::vector<std::vector<CellValue>>& m_data;
      size_t m_col;
  public:
      /**
       * @brief Constructs a view over column @p col of @p data.
       * @param data Reference to the document's 2-D data grid.
       * @param col  Zero-based column index to project.
       */
      ColumnView(const std::vector<std::vector<CellValue>>& data, size_t col)
          : m_data(data), m_col(col) {}

      /// Returns the number of rows (i.e. the number of cells in this column).
      size_t size() const { return m_data.size(); }

      /**
       * @brief Returns the cell at row @p row.
       * @param row Zero-based row index.
       * @return Const reference to the `CellValue` at that position.
       * @throws std::out_of_range if @p row is out of bounds.
       */
      const CellValue& operator[](size_t row) const {
          return m_data.at(row).at(m_col);
      }

      /**
       * @brief Forward iterator for range-based `for` loops over column cells.
       */
      struct Iterator {
          const std::vector<std::vector<CellValue>>& data;
          size_t col, row;
          /// Dereferences the iterator to the current cell.
          const CellValue& operator*() const { return data[row][col]; }
          /// Advances to the next row.
          Iterator& operator++() { ++row; return *this; }
          /// Returns `true` while the iterator has not reached the end.
          bool operator!=(const Iterator& o) const { return row != o.row; }
      };

      /// Returns an iterator to the first row.
      Iterator begin() const { return {m_data, m_col, 0}; }
      /// Returns a past-the-end iterator.
      Iterator end()   const { return {m_data, m_col, m_data.size()}; }
  }; // class ColumnView

  /**
   * @brief Constructs an empty Document.
   *
   * @param separator   Field delimiter (default: `','`).
   * @param columnNames Optional list of column names stored as the header.
   *                    Can also be supplied later via `fromFile`.
   */
  explicit Document(char separator = ',', std::vector<std::string> columnNames = {}) : m_separator(separator), m_header(columnNames) {}

  /**
   * @brief Returns the number of data rows.
   *
   * Header rows are not counted; only rows stored in `m_data` are included.
   *
   * @return Number of rows in the document.
   */
  size_t rowCount() const { return m_data.size(); }

  /**
   * @brief Returns the parsed data as a 2-D grid of typed cell values.
   * @return Const reference to the internal row/column data.
   */
  const std::vector<std::vector<CellValue>> &data() const { return m_data; }

  /**
   * @brief Returns the column names (header).
   *
   * The vector is empty when no header was read from the file and none was
   * provided via `columnNames` in the constructor or `fromFile`.
   *
   * @return Const reference to the vector of column name strings.
   */
  const std::vector<std::string> &columnNames() const { return m_header; }

  /**
   * @brief Returns the field delimiter used by this document.
   * @return The separator character.
   */
  char getSeparator() const { return m_separator; }


  /**
   * @brief Returns a `ColumnView` for the column with the given header name.
   *
   * @param name The column name to look up (case-sensitive).
   * @return A `ColumnView` referencing that column.
   * @throws std::out_of_range if @p name is not found in the header.
   * @pre The document must have been created with a header (via `fromFile`
   *      with `hasHeader = true`, or via the `columnNames` parameter).
   */
  ColumnView operator[](const std::string& name) const {
      auto it = std::find(m_header.begin(), m_header.end(), name);
      if (it == m_header.end())
          throw std::out_of_range("Column not found: " + name);
      return ColumnView(m_data, std::distance(m_header.begin(), it));
  }

  /**
   * @brief Returns a `ColumnView` for the column at zero-based index @p col.
   *
   * @param col Zero-based column index.
   * @return A `ColumnView` referencing that column.
   * @throws std::out_of_range if @p col is >= the number of columns.
   */
  ColumnView operator[](size_t col) const {
      size_t numCols = m_data.empty() ? 0 : m_data[0].size();
      if (col >= numCols)
          throw std::out_of_range("Column index out of range");
      return ColumnView(m_data, col);
  }
  
  /**
   * @brief Creates a Document by reading and parsing a CSV file.
   *
   * **Schema resolution order:**
   * 1. If `dTypes` is non-empty it is used directly.
   * 2. Otherwise the first `sampleRows` data rows are examined and each
   *    column is assigned the most specific type that fits all sampled
   *    values (INT > DOUBLE > BOOL > STRING).
   *
   * **Header resolution order:**
   * 1. If `columnNames` is non-empty those names are used as the header.
   *    If `hasHeader` is also `true` the first line is consumed and
   *    discarded so it is not treated as a data row.
   * 2. Else if `hasHeader` is `true` the first line is parsed and stored
   *    as the header.
   * 3. Otherwise no header is set.
   *
   * @param filename Path to the CSV file.
   * @param opts     Parsing options (see @ref ParseOptions). All fields have
   *                 sensible defaults; only set what differs from the standard
   *                 case. Relevant fields:
   *                 - `sep`        — field delimiter (default `','`).
   *                 - `sampleRows` — rows sampled for type inference (default 100).
   *                 - `dTypes`     — explicit schema; skips voting when non-empty.
   *                 - `hasHeader`  — treat first line as header when `true`.
   *                 - `colNames`   — explicit column names; override file header.
   * @return         Parsed Document. Returns an empty Document if the file
   *                 cannot be opened or contains no data rows.
   */
  static Document fromFile( const std::string &filename,
                            ParseOptions opts = {}) {
    Document doc(opts.sep);
    std::ifstream file(filename);

    if (!file.is_open())
      return doc;

    std::vector<std::string> dataLines;
    std::string line;

    auto stripCR = [](std::string &s) {
      if (!s.empty() && s.back() == '\r')
        s.pop_back();
    };

    if (!opts.colNames.empty()) {
      if (opts.hasHeader)
        std::getline(file, line); // skip header row
      doc.m_header = opts.colNames;
    } else if (opts.hasHeader && std::getline(file, line)) {
      stripCR(line);
      doc.m_header = split(line, opts.sep);
    }

    //  Sampling first N data rows
    while (dataLines.size() < opts.sampleRows && std::getline(file, line)) {
      stripCR(line);
      if (!line.empty())
        dataLines.push_back(line);
    }

    if (dataLines.empty())
      return doc;

    std::vector<std::vector<std::string>> rawSamples;
    for (const auto &l : dataLines)
      rawSamples.push_back(split(l, opts.sep));

    if (!opts.dTypes.empty()) {
      // use provided schema directly
      doc.m_schema = opts.dTypes;
    } else {
      // vote schema from sample rows
      size_t numCols = rawSamples[0].size();
      for (size_t col = 0; col < numCols; ++col) {
        bool couldBeInt = true;
        bool couldBeDouble = true;
        bool couldBeBool = true;
        for (const auto &row : rawSamples) {
          if (col >= row.size() || row[col].empty()|| row[col] == "NaN" || row[col] == "NULL")
            continue;
          if (!isInteger(row[col]))
            couldBeInt = false;
          if (!isDouble(row[col]))
            couldBeDouble = false;
          if (!isBoolean(row[col]))
            couldBeBool = false;
        }
        if (couldBeInt)
          doc.m_schema.push_back(DType::INT);
        else if (couldBeDouble)
          doc.m_schema.push_back(DType::DOUBLE);
        else if (couldBeBool)
          doc.m_schema.push_back(DType::BOOL);
        else
          doc.m_schema.push_back(DType::STRING);
      }
    }

    // convert samples
    for (const auto &rawRow : rawSamples) {
      std::vector<CellValue> row;
      for (size_t i = 0; i < doc.m_schema.size(); ++i) {
        row.push_back(
            doc.convert(i < rawRow.size() ? rawRow[i] : "", doc.m_schema[i]));
      }
      doc.m_data.push_back(row);
    }

    // read to end
    while (std::getline(file, line)) {
      stripCR(line);
      if (line.empty())
        continue;
      auto rawRow = split(line, opts.sep);
      std::vector<CellValue> row;
      for (size_t i = 0; i < doc.m_schema.size(); ++i) {
        row.push_back(
            doc.convert(i < rawRow.size() ? rawRow[i] : "", doc.m_schema[i]));
      }
      doc.m_data.push_back(row);
    }

    return doc;
  }

  /**
   * @brief Writes the document to a CSV file.
   *
   * If the document has a header (`columnNames()` is non-empty) it is written
   * as the first line. Data rows follow, one per line, with fields separated
   * by the document's separator. String cells that contain the separator
   * character or a double-quote are wrapped in double-quotes, with any
   * embedded quotes doubled (`"` → `""`). Boolean cells are written as
   * `true` or `false`. Empty cells (`std::monostate`) produce no output.
   *
   * @param filename Destination file path. The file is created or truncated.
   *                 Does nothing if the file cannot be opened.
   */
  void toFile(const std::string &filename) const {
    std::ofstream file(filename);

    if (!file.is_open())
      return;

    auto writeField = [&](const std::string &s) {
      if (s.find(m_separator) != std::string::npos ||
          s.find('"') != std::string::npos ||
          s.find('\n') != std::string::npos ||
          s.find('\r') != std::string::npos) {
        std::string escaped = s;
        size_t pos = 0;
        while ((pos = escaped.find('"', pos)) != std::string::npos) {
          escaped.insert(pos, 1, '"');
          pos += 2;
        }
        file << '"' << escaped << '"';
      } else {
        file << s;
      }
    };

    if (!m_header.empty()) {
      for (size_t c = 0; c < m_header.size(); ++c) {
        if (c > 0)
          file << m_separator;
        writeField(m_header[c]);
      }
      file << '\n';
    }

    for (size_t r = 0; r < m_data.size(); ++r) {
      const auto &row = m_data[r];

      for (size_t c = 0; c < row.size(); ++c) {
        if (c > 0)
          file << m_separator;

        std::visit(
            [&](const auto &val) {
              using T = std::decay_t<decltype(val)>;

              if constexpr (std::is_same_v<T, std::monostate>) {
                // write nothing (empty cell)
              } else if constexpr (std::is_same_v<T, std::string>) {
                writeField(val);
              } else if constexpr (std::is_same_v<T, bool>) {
                file << (val ? "true" : "false");
              } else {
                file << val;
              }
            },
            row[c]);
      }
      file << '\n';
    }
  }
}; // class Document

} // namespace csv
