/**
 * @file csv_parser.hpp
 * @brief Lightweight CSV parsing library with automatic type inference.
 */

#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace csv {

/**
 * @brief Data type used for a CSV column.
 */
enum class DType {
  STRING, ///< Column contains string values.
  INT,    ///< Column contains integer values.
  DOUBLE, ///< Column contains floating-point values.
  BOOL    ///< Column contains boolean values.
};

/**
 * @brief A single CSV cell value; holds one of: empty (monostate), long,
 * double, string, or bool.
 */
using CellValue = std::variant<std::monostate, long, double, std::string, bool>;

/**
 * @brief Represents a parsed CSV document.
 *
 * Supports automatic schema inference by sampling the first N rows, or an
 * explicit column-type schema supplied by the caller.
 */
class Document {
private:
  std::vector<std::vector<CellValue>>
      m_data; ///< Parsed rows of typed cell values.
  //! Separator for CSV columns (default = ',').
  char m_separator;

  std::vector<DType> m_schema; ///< Per-column type schema used during parsing.

  /**
   * @brief Splits a string by a delimiter into a vector of tokens.
   * @param s   The string to split.
   * @param sep The delimiter character.
   * @return Vector of substrings between delimiters.
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
   * @brief Checks whether a string represents a valid integer.
   * @param s The string to test.
   * @return @c true if @p s can be fully parsed as a base-10 integer.
   */
  static bool isInteger(const std::string &s) {
    if (s.empty())
      return false;
    char *p;
    strtol(s.c_str(), &p, 10);
    return *p == 0;
  }

  /**
   * @brief Checks whether a string represents a valid floating-point number.
   * @param s The string to test.
   * @return @c true if @p s can be fully parsed as a double.
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
   * @param s The string to test (case-insensitive).
   * @return @c true if @p s is one of: true, false, 1, 0, yes, no.
   */
  static bool isBoolean(const std::string &s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return (lower == "true" || lower == "false" || lower == "1" ||
            lower == "0" || lower == "yes" || lower == "no");
  }

  /**
   * @brief Converts a string to a boolean value.
   * @param s The string to convert (case-insensitive).
   * @return @c true if @p s is "true", "1", or "yes"; @c false otherwise.
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
   * @param val  The raw string value from the CSV.
   * @param type The target data type.
   * @return Typed @ref CellValue, or @c std::monostate for empty/null values,
   *         or the original string if conversion fails.
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
   * @brief Constructor with optional separator.
   * @param separator Separator for columns (default: ',').
   */
  explicit Document(char separator = ',') : m_separator(separator) {}

  /**
   * @brief Returns the number of loaded rows.
   * @return Number of rows in m_rows.
   */
  size_t rowCount() const { return m_data.size(); }

  /**
   * @brief Returns the parsed data as a 2-D grid of typed cell values.
   * @return Const reference to the internal row/column data.
   */
  const std::vector<std::vector<CellValue>> &data() const { return m_data; }

  /// @brief Returns the current separator.
  char getSeparator() const { return m_separator; }

  /**
   * @brief Creates a Document from a CSV file.
   * @param filename Path to the CSV file.
   * @param separator Delimiter (default: ',').
   * @return Loaded Document (empty on errors).
   *
   * Reads the file line by line and parses it with the given separator.
   * Ignores empty lines and closes the file automatically.
   */
  static Document fromFile(const std::string &filename, char sep = ',',
                           size_t sampleRows = 100,
                           std::vector<DType> dTypes = {}) {
    Document doc(sep);
    std::ifstream file(filename);

    if (!file.is_open())
      return doc;

    std::vector<std::string> lines;
    std::string line;

    //  Sampling first N rows
    while (lines.size() < sampleRows && std::getline(file, line)) {
      if (!line.empty())
        lines.push_back(line);
    }

    if (lines.empty())
      return doc;

    std::vector<std::vector<std::string>> rawSamples;
    for (const auto &l : lines)
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

  void toCSV(const std::string &filename) {
    std::ofstream file(filename);

    if (!file.is_open())
      return;

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
