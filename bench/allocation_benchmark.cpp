#include "../include/OrderBook.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <random>

// Overriding the global allocation operators lets us count every heap
// allocation the program makes, regardless of which container triggers it
// (std::list nodes, std::map nodes, std::unordered_map buckets, etc.).
// This is what the object pool optimization is meant to reduce.
static size_t g_allocCount = 0;
static size_t g_allocBytes = 0;

void* operator new(size_t size) {
    g_allocCount++;
    g_allocBytes += size;
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept {
    std::free(p);
}

int main() {
    constexpr int NUM_ORDERS = 1'000'000;
    constexpr Price MID = 10000;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::normal_distribution<double> priceOffsetDist(0.0, 25.0);
    std::uniform_int_distribution<int> qtyDist(1, 100);

    // Reset counters after all the setup above (rng, distributions) has
    // already done its own allocations, so we only measure what the
    // OrderBook itself allocates while processing orders.
    g_allocCount = 0;
    g_allocBytes = 0;

    OrderBook book;

    for (int i = 0; i < NUM_ORDERS; ++i) {
        Order order;
        order.id = static_cast<uint64_t>(i) + 1;
        order.side = sideDist(rng) == 0 ? Side::buy : Side::sell;
        order.order_type = OrderType::limit;
        order.price = MID + static_cast<Price>(priceOffsetDist(rng));
        order.quantity = static_cast<uint32_t>(qtyDist(rng));
        order.timestamp = static_cast<uint64_t>(i);

        book.match(order);
    }

    std::cout << "Orders processed: " << NUM_ORDERS << "\n";
    std::cout << "Final book size: " << book.size() << "\n";
    std::cout << "Total heap allocations: " << g_allocCount << "\n";
    std::cout << "Total bytes allocated: " << g_allocBytes << "\n";
    std::cout << "Allocations per order: "
              << static_cast<double>(g_allocCount) / NUM_ORDERS << "\n";
}
