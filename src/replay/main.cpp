#include <cstdlib>
#include <iostream>

#include "lobster/book.hpp"
#include "lobster/replay.hpp"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: lobster-replay <journal-path>\n";
    return EXIT_FAILURE;
  }

  try {
    lobster::OrderBook book;
    lobster::ReplayEngine replay;
    replay.replay(argv[1], [&](const lobster::BookEvent& event) { book.apply(event); });

    const auto top = book.top_of_book();
    std::cout << "bid=";
    if (top.best_bid_price && top.best_bid_qty) {
      std::cout << *top.best_bid_price << "@" << *top.best_bid_qty;
    } else {
      std::cout << "none";
    }
    std::cout << " ask=";
    if (top.best_ask_price && top.best_ask_qty) {
      std::cout << *top.best_ask_price << "@" << *top.best_ask_qty;
    } else {
      std::cout << "none";
    }
    std::cout << '\n';
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
