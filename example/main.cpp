#include <csv_parser.hpp>
#include <iostream>
#include <numeric>

int main() {
    //read csv
    auto doc = csv::Document::fromFile("../example/grades.csv",
                                       {.hasHeader = true});

    std::cout << "=== Loaded " << doc.rowCount() << " students ===\n\n";

    //calculate averages per student
    std::vector<csv::CellValue> averages;

    for (size_t r = 0; r < doc.rowCount(); ++r) {
        const auto& row = doc.data()[r];

        double sum = 0.0;
        size_t count = 0;
        for (size_t c = 1; c < row.size(); ++c) {
            if (std::holds_alternative<long>(row[c])) {
                sum += static_cast<double>(std::get<long>(row[c]));
                ++count;
            }
        }

        double avg = count > 0 ? sum / count : 0.0;
        averages.push_back(avg);

        std::cout << std::get<std::string>(row[0])
                  << " average: " << avg << "\n";
    }

    // add averages column
    doc.addColumn("average", csv::DType::DOUBLE, averages);

    //add new student 
    doc.addRow({std::string("Eve"), 88L, 95L, 91L, 91.33});
    std::cout << "\nAdded student Eve\n";

    // find best student
    auto avgCol = doc["average"];
    size_t bestIdx = 0;
    double bestAvg = 0.0;

    for (size_t r = 0; r < avgCol.size(); ++r) {
        double val = std::get<double>(avgCol[r]);
        if (val > bestAvg) {
            bestAvg = val;
            bestIdx = r;
        }
    }

    std::cout << "\nBest student: "
              << std::get<std::string>(doc.data()[bestIdx][0])
              << " (" << bestAvg << ")\n";

    // write back
    doc.toFile("../examples/grades_out.csv");
    std::cout << "\nResults written to grades_out.csv\n";

    return 0;
}