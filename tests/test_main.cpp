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

  auto doc = csv::Document::fromFile("factory_test.csv", {.sep = ';'});

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
  auto doc = csv::Document::fromFile("schema_override.csv", {.dTypes = {csv::DType::STRING}});

  REQUIRE(doc.rowCount() == 3);
  REQUIRE(std::holds_alternative<std::string>(doc.data()[0][0]));
  REQUIRE(std::get<std::string>(doc.data()[0][0]) == "1");
}

TEST_CASE("fromFile: explicit dTypes mixed schema", "[csv_parser][fromFile]") {
  {
      std::ofstream out("schema_mixed.csv");
      out << "Alice,30,1.75\nBob,25,1.80";
  }

  auto doc = csv::Document::fromFile("schema_mixed.csv",
      {.dTypes = {csv::DType::STRING, csv::DType::INT, csv::DType::DOUBLE}});

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

  auto doc = csv::Document::fromFile("quoted_separator.csv", {.sep = ';'});

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

  auto doc = csv::Document::fromFile("header_read.csv", {.hasHeader = true});

  REQUIRE(doc.columnNames() == std::vector<std::string>{"id", "name", "score"});
  REQUIRE(doc.rowCount() == 2); // header row not counted as data
}

TEST_CASE("fromFile: columnNames overrides header row", "[csv_parser][header]") {
  {
    std::ofstream out("header_override.csv");
    out << "a,b,c\n1,2,3";
  }

  auto doc = csv::Document::fromFile("header_override.csv",
                                     {.hasHeader = true, .colNames = {"x", "y", "z"}});

  REQUIRE(doc.columnNames() == std::vector<std::string>{"x", "y", "z"});
  REQUIRE(doc.rowCount() == 1); // header row still consumed, not in data
}

TEST_CASE("fromFile: columnNames without hasHeader does not skip first row", "[csv_parser][header]") {
  {
    std::ofstream out("header_no_skip.csv");
    out << "1,Alice\n2,Bob";
  }

  auto doc = csv::Document::fromFile("header_no_skip.csv", {.colNames = {"id", "name"}});

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

  auto doc = csv::Document::fromFile("header_tofile_in.csv", {.hasHeader = true});
  REQUIRE(doc.columnNames() == std::vector<std::string>{"id", "name"});

  doc.toFile("header_tofile_out.csv");

  // Re-parse with hasHeader=true and verify header survives the roundtrip
  auto reparsed = csv::Document::fromFile("header_tofile_out.csv", {.hasHeader = true});
  REQUIRE(reparsed.columnNames() == std::vector<std::string>{"id", "name"});
  REQUIRE(reparsed.rowCount() == doc.rowCount());
  REQUIRE(reparsed.data() == doc.data());
}


TEST_CASE("ColumnView: access column by index", "[csv_parser][ColumnView]") {
  {
      std::ofstream out("cv_by_index.csv");
      out << "Alice,30,1.75\nBob,25,1.80";
  }
  auto doc = csv::Document::fromFile("cv_by_index.csv",
      {.dTypes = {csv::DType::STRING, csv::DType::INT, csv::DType::DOUBLE}});

  auto col = doc[1]; // Age column

  REQUIRE(col.size() == 2);
  REQUIRE(std::get<long>(col[0]) == 30);
  REQUIRE(std::get<long>(col[1]) == 25);
}

TEST_CASE("ColumnView: access column by header name", "[csv_parser][ColumnView]") {
  {
    std::ofstream out("cv_named.csv");
    out << "name,score\nAlice,9.5\nBob,8.0";
  }

  auto doc = csv::Document::fromFile("cv_named.csv", {.hasHeader = true});

  auto col = doc["name"];

  REQUIRE(col.size() == 2);
  REQUIRE(std::get<std::string>(col[0]) == "Alice");
  REQUIRE(std::get<std::string>(col[1]) == "Bob");
}

TEST_CASE("ColumnView: range-for iteration", "[csv_parser][ColumnView]") {
  {
    std::ofstream out("cv_iter.csv");
    out << "val\n1\n2\n3";
  }

  auto doc = csv::Document::fromFile("cv_iter.csv", {.hasHeader = true});

  std::vector<long> vals;
  for (const auto& cell : doc["val"])
    vals.push_back(std::get<long>(cell));

  REQUIRE(vals == std::vector<long>{1, 2, 3});
}

TEST_CASE("ColumnView: operator[] throws out_of_range for bad row", "[csv_parser][ColumnView]") {
  {
      std::ofstream out("cv_throw_row.csv");
      out << "val\n1\n2\n3";
  }
  auto doc = csv::Document::fromFile("cv_throw_row.csv", {.hasHeader = true});
  auto col = doc[0];
  REQUIRE_THROWS_AS(col[999], std::out_of_range);
}

TEST_CASE("ColumnView: operator[](size_t) throws for bad column index", "[csv_parser][ColumnView]") {
  {
      std::ofstream out("cv_throw_col.csv");
      out << "val\n1\n2\n3";
  }
  auto doc = csv::Document::fromFile("cv_throw_col.csv", {.hasHeader = true});
  REQUIRE_THROWS_AS(doc[99], std::out_of_range);
}

TEST_CASE("ColumnView: operator[](string) throws for unknown column name", "[csv_parser][ColumnView]") {
  {
      std::ofstream out("cv_throw_name.csv");
      out << "name,score\nAlice,9.5\nBob,8.0";
  }
  auto doc = csv::Document::fromFile("cv_throw_name.csv", {.hasHeader = true});
  REQUIRE_THROWS_AS(doc["nonexistent"], std::out_of_range);
}


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


TEST_CASE("fromFile: NaN maps to monostate", "[csv_parser][monostate]") {
    {
        std::ofstream out("null_nan.csv");
        out << "value\nNaN\n42";
    }

    auto doc = csv::Document::fromFile("null_nan.csv", {.hasHeader = true});

    REQUIRE(doc.rowCount() == 2);
    REQUIRE(std::holds_alternative<std::monostate>(doc.data()[0][0]));
    REQUIRE(std::holds_alternative<long>(doc.data()[1][0]));
}

TEST_CASE("fromFile: NULL maps to monostate", "[csv_parser][monostate]") {
    {
        std::ofstream out("null_null.csv");
        out << "value\nNULL\n42";
    }

    auto doc = csv::Document::fromFile("null_null.csv", {.hasHeader = true});

    REQUIRE(doc.rowCount() == 2);
    REQUIRE(std::holds_alternative<std::monostate>(doc.data()[0][0]));
}

TEST_CASE("fromFile: empty cell maps to monostate", "[csv_parser][monostate]") {
    {
        std::ofstream out("null_empty_cell.csv");
        out << "a,b\n1,\n2,3";
    }

    auto doc = csv::Document::fromFile("null_empty_cell.csv",
                                       {.hasHeader = true, .dTypes = {csv::DType::INT, csv::DType::INT}});

    REQUIRE(doc.rowCount() == 2);
    REQUIRE(std::holds_alternative<std::monostate>(doc.data()[0][1]));
    REQUIRE(std::get<long>(doc.data()[1][1]) == 3);
}


TEST_CASE("fromFile: empty lines in file body are skipped", "[csv_parser][whitespace]") {
    {
        std::ofstream out("empty_lines.csv");
        out << "1\n\n2\n\n3";
    }

    auto doc = csv::Document::fromFile("empty_lines.csv");

    REQUIRE(doc.rowCount() == 3);
    REQUIRE(std::get<long>(doc.data()[0][0]) == 1);
    REQUIRE(std::get<long>(doc.data()[1][0]) == 2);
    REQUIRE(std::get<long>(doc.data()[2][0]) == 3);
}

TEST_CASE("fromFile: Windows line endings (CRLF) are handled", "[csv_parser][whitespace]") {
    {
        std::ofstream out("crlf.csv", std::ios::binary);
        out << "id,name\r\n1,Alice\r\n2,Bob\r\n";
    }

    auto doc = csv::Document::fromFile("crlf.csv", {.hasHeader = true});

    REQUIRE(doc.columnNames() == std::vector<std::string>{"id", "name"});
    REQUIRE(doc.rowCount() == 2);
    REQUIRE(std::get<std::string>(doc.data()[0][1]) == "Alice");
    REQUIRE(std::get<std::string>(doc.data()[1][1]) == "Bob");
}

TEST_CASE("fromFile: 1 and 0 are voted as INT not BOOL", "[csv_parser][schema]") {
    {
        std::ofstream out("bool_vs_int.csv");
        out << "1\n0\n1";
    }

    auto doc = csv::Document::fromFile("bool_vs_int.csv");

    REQUIRE(doc.rowCount() == 3);
    REQUIRE(std::holds_alternative<long>(doc.data()[0][0]));
    REQUIRE(std::get<long>(doc.data()[0][0]) == 1);
}

TEST_CASE("fromFile: yes/no column is voted as BOOL", "[csv_parser][schema]") {
    {
        std::ofstream out("bool_yes_no.csv");
        out << "yes\nno\nyes";
    }

    auto doc = csv::Document::fromFile("bool_yes_no.csv");

    REQUIRE(doc.rowCount() == 3);
    REQUIRE(std::holds_alternative<bool>(doc.data()[0][0]));
    REQUIRE(std::get<bool>(doc.data()[0][0]) == true);
    REQUIRE(std::get<bool>(doc.data()[1][0]) == false);
}

TEST_CASE("addRow: appends row with correct values", "[csv_parser][mutation]") {
    csv::Document doc(',', {"name", "age"});
    doc.addRow({std::string("Alice"), 30L});

    REQUIRE(doc.rowCount() == 1);
    REQUIRE(std::get<std::string>(doc.data()[0][0]) == "Alice");
    REQUIRE(std::get<long>(doc.data()[0][1]) == 30);
}

TEST_CASE("addRow: missing cells filled with monostate", "[csv_parser][mutation]") {
    csv::Document doc(',', {"name", "age", "score"});
    doc.addRow({std::string("Alice")});  

    REQUIRE(doc.rowCount() == 1);
    REQUIRE(std::holds_alternative<std::monostate>(doc.data()[0][1]));
    REQUIRE(std::holds_alternative<std::monostate>(doc.data()[0][2]));
}

TEST_CASE("addRow: throws on too many values", "[csv_parser][mutation]") {
    csv::Document doc(',', {"name"});
    doc.addRow({std::string("Alice")});  

    REQUIRE_THROWS_AS(
        doc.addRow({std::string("Bob"), 99L}),
        std::invalid_argument);
}

TEST_CASE("removeRow: removes correct row", "[csv_parser][mutation]") {
    csv::Document doc(',', {"name"});
    doc.addRow({std::string("Alice")});
    doc.addRow({std::string("Bob")});
    doc.addRow({std::string("Charlie")});

    doc.removeRow(1);  

    REQUIRE(doc.rowCount() == 2);
    REQUIRE(std::get<std::string>(doc.data()[0][0]) == "Alice");
    REQUIRE(std::get<std::string>(doc.data()[1][0]) == "Charlie");
}

TEST_CASE("removeRow: throws on invalid index", "[csv_parser][mutation]") {
    csv::Document doc(',', {"name"});
    doc.addRow({std::string("Alice")});

    REQUIRE_THROWS_AS(doc.removeRow(99), std::out_of_range);
}

TEST_CASE("addColumn: appends column with values", "[csv_parser][mutation]") {
    csv::Document doc(',', {"name"});
    doc.addRow({std::string("Alice")});
    doc.addRow({std::string("Bob")});

    doc.addColumn("age", csv::DType::INT, {30L, 25L});

    REQUIRE(doc.columnNames().size() == 2);
    REQUIRE(doc.columnNames()[1] == "age");
    REQUIRE(std::get<long>(doc.data()[0][1]) == 30);
    REQUIRE(std::get<long>(doc.data()[1][1]) == 25);
}

TEST_CASE("addColumn: default fills with monostate", "[csv_parser][mutation]") {
    csv::Document doc(',', {"name"});
    doc.addRow({std::string("Alice")});
    doc.addRow({std::string("Bob")});

    doc.addColumn("score", csv::DType::DOUBLE);

    REQUIRE(std::holds_alternative<std::monostate>(doc.data()[0][1]));
    REQUIRE(std::holds_alternative<std::monostate>(doc.data()[1][1]));
}

TEST_CASE("addColumn: throws on wrong values size", "[csv_parser][mutation]") {
    csv::Document doc(',', {"name"});
    doc.addRow({std::string("Alice")});

    REQUIRE_THROWS_AS(
        doc.addColumn("age", csv::DType::INT, {30L, 25L}),  // 2 Werte, 1 Zeile
        std::invalid_argument);
}

TEST_CASE("removeColumn: removes by index", "[csv_parser][mutation]") {
    csv::Document doc(',', {"name", "age", "score"});
    doc.addRow({std::string("Alice"), 30L, 9.5});

    doc.removeColumn(1);  // age entfernen

    REQUIRE(doc.columnNames().size() == 2);
    REQUIRE(doc.columnNames()[0] == "name");
    REQUIRE(doc.columnNames()[1] == "score");
    REQUIRE(doc.data()[0].size() == 2);
}

TEST_CASE("removeColumn: removes by name", "[csv_parser][mutation]") {
    csv::Document doc(',', {"name", "age"});
    doc.addRow({std::string("Alice"), 30L});

    doc.removeColumn("age");

    REQUIRE(doc.columnNames().size() == 1);
    REQUIRE(doc.columnNames()[0] == "name");
}

TEST_CASE("removeColumn: throws on invalid index", "[csv_parser][mutation]") {
    csv::Document doc(',', {"name"});
    doc.addRow({std::string("Alice")});

    REQUIRE_THROWS_AS(doc.removeColumn(99), std::out_of_range);
}

TEST_CASE("removeColumn: throws on unknown name", "[csv_parser][mutation]") {
    csv::Document doc(',', {"name"});
    REQUIRE_THROWS_AS(doc.removeColumn("nonexistent"), std::out_of_range);
}