#include <catch2/catch_test_macros.hpp>
#include <csv_parser.hpp>
#include <fstream>

TEST_CASE("check namespace and classes ", "[csv_parser]") {
  csv::Document doc(';');
  REQUIRE(doc.getSeparator() == ';');
}

TEST_CASE("Factory method fromFile", "[csv_parser]") {
  {
      std::ofstream out("factory_test.csv");
      out << "ID;Name;Wert\n1;Test;100";
  }

  auto doc = csv::Document::fromFile("factory_test.csv", ';');

  REQUIRE(doc.rowCount() == 2);
  REQUIRE(doc.data()[1][1] == csv::CellValue{std::string("Test")});
}

TEST_CASE("fromFile: non-existent file returns empty document", "[csv_parser][fromFile]") {
  auto doc = csv::Document::fromFile("does_not_exist.csv");
  REQUIRE(doc.rowCount() == 0);
}

TEST_CASE("fromFile: auto schema votes INT for integer column", "[csv_parser][fromFile]") {
  {
      std::ofstream out("schema_vote.csv");
      out << "42\n7\n13";
  }

  auto doc = csv::Document::fromFile("schema_vote.csv");

  REQUIRE(doc.rowCount() == 3);
  REQUIRE(std::holds_alternative<long>(doc.data()[0][0]));
  REQUIRE(std::get<long>(doc.data()[0][0]) == 42);
}

TEST_CASE("fromFile: auto schema votes BOOL for boolean column", "[csv_parser][fromFile]") {
  {
      std::ofstream out("schema_bool.csv");
      out << "true\nfalse\nyes\nno";
  }

  auto doc = csv::Document::fromFile("schema_bool.csv");

  REQUIRE(doc.rowCount() == 4);
  REQUIRE(std::holds_alternative<bool>(doc.data()[0][0]));
  REQUIRE(std::get<bool>(doc.data()[0][0]) == true);
  REQUIRE(std::get<bool>(doc.data()[1][0]) == false);
  REQUIRE(std::get<bool>(doc.data()[2][0]) == true);
  REQUIRE(std::get<bool>(doc.data()[3][0]) == false);
}

TEST_CASE("fromFile: auto schema votes DOUBLE for decimal column", "[csv_parser][fromFile]") {
  {
      std::ofstream out("schema_double.csv");
      out << "1.5\n2.7\n3.0";
  }

  auto doc = csv::Document::fromFile("schema_double.csv");

  REQUIRE(doc.rowCount() == 3);
  REQUIRE(std::holds_alternative<double>(doc.data()[0][0]));
  REQUIRE(std::get<double>(doc.data()[0][0]) == 1.5);
}

TEST_CASE("fromFile: auto schema keeps STRING for text column", "[csv_parser][fromFile]") {
  {
      std::ofstream out("schema_string.csv");
      out << "hello\nworld\nfoo";
  }

  auto doc = csv::Document::fromFile("schema_string.csv");

  REQUIRE(doc.rowCount() == 3);
  REQUIRE(std::holds_alternative<std::string>(doc.data()[0][0]));
}

TEST_CASE("fromFile: explicit dTypes overrides schema voting", "[csv_parser][fromFile]") {
  {
      std::ofstream out("schema_override.csv");
      out << "1\n2\n3";
  }

  // Column would be voted as INT, but we force STRING
  auto doc = csv::Document::fromFile("schema_override.csv", ',', 100, {csv::DType::STRING});

  REQUIRE(doc.rowCount() == 3);
  REQUIRE(std::holds_alternative<std::string>(doc.data()[0][0]));
  REQUIRE(std::get<std::string>(doc.data()[0][0]) == "1");
}

TEST_CASE("fromFile: explicit dTypes mixed schema", "[csv_parser][fromFile]") {
  {
      std::ofstream out("schema_mixed.csv");
      out << "Alice,30,1.75\nBob,25,1.80";
  }

  auto doc = csv::Document::fromFile("schema_mixed.csv", ',', 100,
      {csv::DType::STRING, csv::DType::INT, csv::DType::DOUBLE});

  REQUIRE(doc.rowCount() == 2);
  REQUIRE(std::holds_alternative<std::string>(doc.data()[0][0]));
  REQUIRE(std::holds_alternative<long>(doc.data()[0][1]));
  REQUIRE(std::holds_alternative<double>(doc.data()[0][2]));
  REQUIRE(std::get<std::string>(doc.data()[0][0]) == "Alice");
  REQUIRE(std::get<long>(doc.data()[0][1]) == 30);
  REQUIRE(std::get<double>(doc.data()[0][2]) == 1.75);
}

TEST_CASE("fromFile: separator inside quoted string cell is parsed correctly", "[csv_parser][fromFile]") {
  {
    std::ofstream out("quoted_separator.csv");
    out << "ID;Name;Note\n1;\"Max;Mustermann\";\"Hello;World\"";
  }

  auto doc = csv::Document::fromFile("quoted_separator.csv", ';');

  REQUIRE(doc.rowCount() == 2);
  REQUIRE(std::holds_alternative<std::string>(doc.data()[1][1]));
  REQUIRE(std::holds_alternative<std::string>(doc.data()[1][2]));
  REQUIRE(std::get<std::string>(doc.data()[1][1]) == "Max;Mustermann");
  REQUIRE(std::get<std::string>(doc.data()[1][2]) == "Hello;World");
}

// ── columnNames / header tests ───────────────────────────────────────────────

TEST_CASE("Document constructor accepts columnNames", "[csv_parser][header]") {
  csv::Document doc(',', {"id", "name", "score"});
  REQUIRE(doc.columnNames() == std::vector<std::string>{"id", "name", "score"});
}

TEST_CASE("fromFile: hasHeader reads header row from file", "[csv_parser][header]") {
  {
    std::ofstream out("header_read.csv");
    out << "id,name,score\n1,Alice,9.5\n2,Bob,8.0";
  }

  auto doc = csv::Document::fromFile("header_read.csv", ',', 100, {}, true);

  REQUIRE(doc.columnNames() == std::vector<std::string>{"id", "name", "score"});
  REQUIRE(doc.rowCount() == 2); // header row not counted as data
}

TEST_CASE("fromFile: columnNames overrides header row", "[csv_parser][header]") {
  {
    std::ofstream out("header_override.csv");
    out << "a,b,c\n1,2,3";
  }

  auto doc = csv::Document::fromFile("header_override.csv", ',', 100, {}, true,
                                     {"x", "y", "z"});

  REQUIRE(doc.columnNames() == std::vector<std::string>{"x", "y", "z"});
  REQUIRE(doc.rowCount() == 1); // header row still consumed, not in data
}

TEST_CASE("fromFile: columnNames without hasHeader does not skip first row", "[csv_parser][header]") {
  {
    std::ofstream out("header_no_skip.csv");
    out << "1,Alice\n2,Bob";
  }

  auto doc = csv::Document::fromFile("header_no_skip.csv", ',', 100, {}, false,
                                     {"id", "name"});

  REQUIRE(doc.columnNames() == std::vector<std::string>{"id", "name"});
  REQUIRE(doc.rowCount() == 2); // no row skipped
}

TEST_CASE("fromFile: no header leaves columnNames empty", "[csv_parser][header]") {
  {
    std::ofstream out("header_empty.csv");
    out << "1,2\n3,4";
  }

  auto doc = csv::Document::fromFile("header_empty.csv");

  REQUIRE(doc.columnNames().empty());
}

TEST_CASE("toFile: header is written as first line", "[csv_parser][header][toFile]") {
  {
    std::ofstream out("header_tofile_in.csv");
    out << "id,name\n1,Alice\n2,Bob";
  }

  auto doc = csv::Document::fromFile("header_tofile_in.csv", ',', 100, {}, true);
  REQUIRE(doc.columnNames() == std::vector<std::string>{"id", "name"});

  doc.toFile("header_tofile_out.csv");

  // Re-parse with hasHeader=true and verify header survives the roundtrip
  auto reparsed = csv::Document::fromFile("header_tofile_out.csv", ',', 100, {}, true);
  REQUIRE(reparsed.columnNames() == std::vector<std::string>{"id", "name"});
  REQUIRE(reparsed.rowCount() == doc.rowCount());
  REQUIRE(reparsed.data() == doc.data());
}

// ── existing toFile test ──────────────────────────────────────────────────────

TEST_CASE("toFile: roundtrip preserves parsed values", "[csv_parser][toFile]") {
  {
    std::ofstream out("roundtrip.csv");
    out << "Name,Age,Score\nAlice,30,1.5\n\"Bob, Jr.\",25,2.75";
  }

  auto original = csv::Document::fromFile("roundtrip.csv");

  original.toFile("roundtrip_out.csv");

  auto reparsed = csv::Document::fromFile("roundtrip_out.csv");

  REQUIRE(reparsed.rowCount() == original.rowCount());
  REQUIRE(reparsed.data() == original.data());
}