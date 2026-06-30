#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "lobster/features.hpp"

namespace lobster {

std::vector<TopOfBookFeatures> collect_top_of_book_rows(
    const std::filesystem::path& journal_path);
void write_top_of_book_csv(const std::filesystem::path& output_path,
                           const std::vector<TopOfBookFeatures>& rows);
void export_top_of_book_csv(const std::filesystem::path& journal_path,
                            const std::filesystem::path& output_path);
bool arrow_export_available();
void export_top_of_book_arrow(const std::filesystem::path& journal_path,
                              const std::filesystem::path& output_path);
std::string feature_csv_header();
std::string feature_csv_row(const TopOfBookFeatures& row);

}  // namespace lobster
