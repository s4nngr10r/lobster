#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "lobster/export.hpp"
#include "lobster/journal.hpp"

int main() {
  const auto journal = std::filesystem::temp_directory_path() / "lobster-test-export.bin";
  const auto csv = std::filesystem::temp_directory_path() / "lobster-test-export.csv";
  const auto arrow = std::filesystem::temp_directory_path() / "lobster-test-export.arrow";

  {
    lobster::JournalWriter writer(journal);
    writer.append({.ts_ns = 1,
                   .seq = 1,
                   .source_first_id = 1,
                   .source_last_id = 1,
                   .price_ticks = 100,
                   .qty = 10,
                   .side = lobster::Side::bid,
                   .kind = lobster::EventKind::snapshot_level});
    writer.append({.ts_ns = 2,
                   .seq = 2,
                   .source_first_id = 2,
                   .source_last_id = 2,
                   .price_ticks = 102,
                   .qty = 6,
                   .side = lobster::Side::ask,
                   .kind = lobster::EventKind::snapshot_level});
    writer.append({.ts_ns = 3,
                   .seq = 3,
                   .source_first_id = 3,
                   .source_last_id = 3,
                   .price_ticks = 100,
                   .qty = 14,
                   .side = lobster::Side::bid,
                   .kind = lobster::EventKind::book_level});
  }

  const auto rows = lobster::collect_top_of_book_rows(journal);
  assert(rows.size() == 3);
  assert(rows[1].spread_ticks && *rows[1].spread_ticks == 2);
  assert(rows[1].bid_depth_5 && *rows[1].bid_depth_5 == 10);
  assert(rows[1].ask_depth_5 && *rows[1].ask_depth_5 == 6);
  assert(rows[1].mid_price && *rows[1].mid_price == 101.0);
  assert(rows[1].microprice && *rows[1].microprice == 101.25);
  assert(rows[1].imbalance && *rows[1].imbalance == 0.25);
  assert(rows[2].microprice && *rows[2].microprice == 101.4);
  assert(rows[2].imbalance && *rows[2].imbalance == 0.4);

  lobster::export_top_of_book_csv(journal, csv);
  std::ifstream in(csv);
  std::vector<std::string> lines;
  for (std::string line; std::getline(in, line);) {
    lines.push_back(line);
  }
  assert(lines.size() == 4);
  assert(lines[0] == lobster::feature_csv_header());
  assert(lines[2] ==
         "2,2,2,2,100,10,102,6,10,6,10,6,10,6,1,1,2,101.000000,101.250000,0.250000");
  assert(lobster::arrow_export_available());
  lobster::export_top_of_book_arrow(journal, arrow);
  assert(std::filesystem::exists(arrow));
  assert(std::filesystem::file_size(arrow) > 0);
  return 0;
}
