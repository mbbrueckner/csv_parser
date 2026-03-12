#include <catch2/catch_test_macros.hpp>
#include <csv_parser.hpp>
#include <fstream>

TEST_CASE("Namespace und Klasse prüfen", "[csv_parser]") {
  csv::Document doc(';');
  REQUIRE(doc.getSeparator() == ';');
}

TEST_CASE("Factory Methode fromFile", "[csv_parser]") {
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