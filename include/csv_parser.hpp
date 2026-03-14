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
 * auto doc = csv::Document::fromFile("data.csv", ',', 100, {}, true);
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
    return (lower == "true" || lower == "false" || lower == "1" ||
            lower == "0" || lower == "yes" || lower == "no");
  }

  /**
   * @brief Converts a boolean-like string to a `bool`.
   *
   * @param s The string to convert (case-insensitive).
   * @return  @c true for `"true"`, `"1"`, or `"yes"`; @c false otherwise.
   */
  static bool stringToBool(const std::string &s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "true" || lower == "1" || lower == "yes")
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
   * @param filename    Path to the CSV file.
   * @param sep         Field delimiter (default: `','`).
   * @param sampleRows  Number of rows used for automatic type inference
   *                    (default: 100). All remaining rows are still read
   *                    and converted using the inferred schema.
   * @param dTypes      Explicit per-column type list. When non-empty,
   *                    schema voting is skipped entirely.
   * @param hasHeader   When `true`, the first line of the file is treated
   *                    as a header row and not included in `data()`.
   * @param columnNames Explicit column names. When non-empty these override
   *                    the header read from the file.
   * @return            Parsed Document. Returns an empty Document if the
   *                    file cannot be opened or contains no data rows.
   */
  static Document fromFile(const std::string &filename, char sep = ',',
                           size_t sampleRows = 100,
                           std::vector<DType> dTypes = {},
                           const bool hasHeader = false,
                           std::vector<std::string> columnNames = {}) {
    Document doc(sep);
    std::ifstream file(filename);

    if (!file.is_open())
      return doc;

    std::vector<std::string> dataLines;
    std::string line;
    std::vector<std::string> fileHeader;

    if (!columnNames.empty()) {
      if (hasHeader)
        std::getline(file, line); // skip header row
      doc.m_header = columnNames;
    } else if (hasHeader && std::getline(file, line)) {
      doc.m_header = split(line, sep);
    }

    //  Sampling first N data rows
    while (dataLines.size() < sampleRows && std::getline(file, line)) {
      if (!line.empty())
        dataLines.push_back(line);
    }

    if (dataLines.empty())
      return doc;

    std::vector<std::vector<std::string>> rawSamples;
    for (const auto &l : dataLines)
      rawSamples.push_back(split(l, sep));

    if (!dTypes.empty()) {
      // use provided schema directly
      doc.m_schema = dTypes;
    } else {
      // vote schema from sample rows
      size_t numCols = rawSamples[0].size();
      for (size_t col = 0; col < numCols; ++col) {
        bool couldBeInt = true;
        bool couldBeDouble = true;
        bool couldBeBool = true;
        for (const auto &row : rawSamples) {
          if (col >= row.size() || row[col].empty())
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
      auto rawRow = split(line, sep);
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
  void toFile(const std::string &filename) {
    std::ofstream file(filename);

    if (!file.is_open())
      return;

    if (!m_header.empty()) {
      for (size_t c = 0; c < m_header.size(); ++c) {
        if (c > 0)
          file << m_separator;
        file << m_header[c];
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
                // quote strings that contain the separator or quotes
                if (val.find(m_separator) != std::string::npos ||
                    val.find('"') != std::string::npos) {
                  std::string escaped = val;
                  size_t pos = 0;
                  while ((pos = escaped.find('"', pos)) != std::string::npos) {
                    escaped.insert(pos, 1, '"'); // double up quotes
                    pos += 2;
                  }
                  file << '"' << escaped << '"';
                } else {
                  file << val;
                }
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
