cmake_minimum_required(VERSION 3.20)

project(lobster LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LOBSTER_DEPS REQUIRED IMPORTED_TARGET libcurl simdjson libwebsockets)
pkg_check_modules(OPENSSL REQUIRED IMPORTED_TARGET openssl)
option(LOBSTER_ENABLE_ARROW "Enable Apache Arrow export support" ON)
if(LOBSTER_ENABLE_ARROW)
  pkg_check_modules(ARROW QUIET IMPORTED_TARGET arrow)
  if(ARROW_FOUND)
    add_compile_definitions(LOBSTER_HAS_ARROW=1)
  else()
    message(STATUS "Apache Arrow not found; building without Arrow export")
  endif()
endif()

add_library(lobster_core
  src/transport/transport.cpp
  src/capture/bootstrap.cpp
  src/capture/binance_usdtm.cpp
  src/journal/journal_writer.cpp
  src/journal/journal_reader.cpp
  src/replay/replay_engine.cpp
  src/book/order_book.cpp
  src/features/top_of_book.cpp
  src/export/export_rows.cpp
)

target_include_directories(lobster_core PUBLIC include)
target_compile_options(lobster_core PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(lobster_core PUBLIC PkgConfig::LOBSTER_DEPS PkgConfig::OPENSSL)
if(ARROW_FOUND)
  target_link_libraries(lobster_core PUBLIC PkgConfig::ARROW)
endif()

add_executable(lobster-capture src/capture/main.cpp)
target_link_libraries(lobster-capture PRIVATE lobster_core)

add_executable(lobster-replay src/replay/main.cpp)
target_link_libraries(lobster-replay PRIVATE lobster_core)

add_executable(lobster-dump src/journal/main.cpp)
target_link_libraries(lobster-dump PRIVATE lobster_core)

add_executable(lobster-export src/export/main.cpp)
target_link_libraries(lobster-export PRIVATE lobster_core)

add_executable(lobster-bench-replay bench/bench_replay.cpp)
target_link_libraries(lobster-bench-replay PRIVATE lobster_core)

add_executable(lobster-bench-order-book bench/bench_order_book.cpp)
target_link_libraries(lobster-bench-order-book PRIVATE lobster_core)

add_executable(lobster-bench-write bench/bench_write.cpp)
target_link_libraries(lobster-bench-write PRIVATE lobster_core)

add_executable(lobster-bench-memory bench/bench_memory.cpp)
target_link_libraries(lobster-bench-memory PRIVATE lobster_core)

add_executable(test_transport tests/test_transport.cpp)
target_link_libraries(test_transport PRIVATE lobster_core)
add_test(NAME test_transport COMMAND test_transport)

add_executable(test_replay tests/test_replay.cpp)
target_link_libraries(test_replay PRIVATE lobster_core)
add_test(NAME test_replay COMMAND test_replay)

add_executable(test_order_book tests/test_order_book.cpp)
target_link_libraries(test_order_book PRIVATE lobster_core)
add_test(NAME test_order_book COMMAND test_order_book)

add_executable(test_bootstrap tests/test_bootstrap.cpp)
target_link_libraries(test_bootstrap PRIVATE lobster_core)
add_test(NAME test_bootstrap COMMAND test_bootstrap)

add_executable(test_binance_usdtm tests/test_binance_usdtm.cpp)
target_link_libraries(test_binance_usdtm PRIVATE lobster_core)
add_test(NAME test_binance_usdtm COMMAND test_binance_usdtm)

add_executable(test_export tests/test_export.cpp)
target_link_libraries(test_export PRIVATE lobster_core)
add_test(NAME test_export COMMAND test_export)
