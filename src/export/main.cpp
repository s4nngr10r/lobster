#include <cstdlib>
#include <iostream>
#include <string>

#include "lobster/export.hpp"

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "usage: lobster-export <--csv|--arrow> <journal-path> <output-path>\n";
    return EXIT_FAILURE;
  }

  try {
    const std::string mode = argv[1];
    if (mode == "--csv") {
      lobster::export_top_of_book_csv(argv[2], argv[3]);
      return EXIT_SUCCESS;
    }
    if (mode == "--arrow") {
      lobster::export_top_of_book_arrow(argv[2], argv[3]);
      return EXIT_SUCCESS;
    }
    throw std::runtime_error("unknown export mode");
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return EXIT_FAILURE;
  }
}
