#include <catch2/catch_test_macros.hpp>
#include <csv_parser.hpp>

TEST_CASE("Namespace und Klasse prüfen", "[csv_parser]") {
  csv::Document doc(';'); 
  REQUIRE(doc.getSeparator() == ';');
}

TEST_CASE("Factory Methode fromFile", "[csv_parser]") {
  //prepare data
  {
      std::ofstream out("factory_test.csv");
      out << "ID;Name;Wert\n1;Test;100";
  }

  auto doc = csv::Document::fromFile("factory_test.csv", ';');

  REQUIRE(doc.rowCount() == 2);
  REQUIRE(doc.data()[1][1] == "Test");
}