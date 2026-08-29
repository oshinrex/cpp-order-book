#include "../include/OrderBook.hpp"
#include <iostream>
#include <random>
#include <algorithm> 

int main() {
    constexpr int NUM_ORDERS = 1'000'000;
    constexpr Price MID = 1000;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::normal_distribution<double> priceOffsetDist(0.0, 25.0);
    std::uniform_int_distribution<int> qtyDist(1, 100);

    OrderBook book;

    std::vector<uint64_t> latenciesNs;
    latenciesNs.reserve(NUM_ORDERS);

    auto benchStart = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_ORDERS; i++) {
        Order order;
        order.id = static_cast<uint64_t>(i) + 1; 
        order.side = sideDist(rng) == 0 ? Side::buy : Side::sell; 
        order.order_type = OrderType::limit; 
        order.price = MID + static_cast<Price>(priceOffsetDist(rng));
        order.quantity = static_cast<uint32_t>(qtyDist(rng));
        order.timestamp = static_cast<uint64_t>(i);
        
        auto t0 = std::chrono::steady_clock::now();
        book.match(order);
        auto t1 = std::chrono::steady_clock::now();

        latenciesNs.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    }

    auto benchEnd = std::chrono::steady_clock::now();

    double totalSeconds = std::chrono::duration<double>(benchEnd - benchStart).count();

    constexpr size_t WARMUP = 1000;
    std::vector<uint64_t> sorted(latenciesNs.begin() + WARMUP, latenciesNs.end());
    std::sort(sorted.begin(), sorted.end());

    uint64_t sum = 0;
    for (uint64_t ns : sorted) sum += ns; 
    double avgNs = static_cast<double>(sum) / sorted.size();

    auto percentile = [&](double p) {
        size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
        return sorted[idx];
    };

    std::cout << "Orders processed: " << NUM_ORDERS << "\n";
    std::cout << "Wall time: " << totalSeconds << " s\n";
    std::cout << "Throughput: " << (NUM_ORDERS / totalSeconds) << " orders/sec\n";
    std::cout << "Average latency: " << avgNs << " ns\n";
    std::cout << "p50: " << percentile(0.5) << " ns\n";
    std::cout << "p90: " << percentile(0.9) << " ns\n"; 
    std::cout << "p99: " << percentile(0.99) << " ns\n";
    std::cout << "max: " << sorted.back() << " ns\n";

    std::cout << "Generated and processed " << NUM_ORDERS << " orders.\n";
    std::cout << "Final book size: " << book.size() << "\n";
}