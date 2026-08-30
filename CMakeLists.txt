cmake_minimum_required(VERSION 3.20)
project(LowLatencyOrderBook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

include_directories(include)

add_executable(orderbook
    src/main.cpp
    src/OrderBook.cpp
)

add_executable(orderbook_bench
    bench/benchmark.cpp
    src/OrderBook.cpp
)

add_executable(orderbook_bench_mixed
    bench/mixed_benchmark.cpp
    src/OrderBook.cpp
)

add_executable(orderbook_bench_cancel_baseline
    bench/cancel_baseline_benchmark.cpp
    src/OrderBook.cpp
)