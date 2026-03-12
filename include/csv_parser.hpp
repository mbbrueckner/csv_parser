#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace csv {

/**
 * @brief A class for reading and writing data in CSV format.
 * 
 * CSV contents are stored as a vector of rows (row = vector of strings).
 * Supports custom CSV separators.
 */
class Document {
private:
  //! Internal CSV row structure as a 2D string vector.
  std::vector<std::vector<std::string>> m_rows;
  
  //! Separator for CSV columns (default = ',').
  char m_separator;

public:
  /**
   * @brief Constructor with optional separator.
   * @param separator Separator for columns (default: ',').
   */
  explicit Document(char separator = ',') 
  : m_separator(separator) {}

  /**
   * @brief Returns the number of loaded rows.
   * @return Number of rows in m_rows.
   */
  size_t rowCount() const {
  return m_rows.size();
  }

  /// @brief Returns the current separator.
  char getSeparator() const {
  return m_separator;
  }

  /**
   * @brief Direct access to internal CSV data (non-const).
   * @return Reference to the 2D vector containing all rows.
   * @warning Modifying this can damage the internal structure.
   */
  std::vector<std::vector<std::string>>& data() {
    return m_rows;
  }

  /**
   * @brief Creates a Document from a CSV file.
   * @param filename Path to the CSV file.
   * @param separator Delimiter (default: ',').
   * @return Loaded Document (empty on errors).
   * 
   * Reads the file line by line and parses it with the given separator.
   * Ignores empty lines and closes the file automatically.
   */
  static Document fromFile(const std::string& filename, char separator = ',') {
  Document doc{separator};
  std::ifstream file{filename};

  if (!file.is_open()) {
    return doc;  // Empty Document on errors
  }

  std::string line;
  while (std::getline(file, line)) {
    std::vector<std::string> row;
    std::stringstream ss(line);
    std::string cell;

    while (std::getline(ss, cell, separator)) {
      row.push_back(cell);
    }
    if (!row.empty()) {
      doc.m_rows.push_back(row);
    }
  }
  return doc;
  }
};  // class Document

} // namespace csv
