#include "../include/OrderBook.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <list>
#include <random>
#include <vector>

// Naive baseline: no ID index at all, just a linear scan through every
// resting order to find the one to cancel. Represents "what if we hadn't
// built the unordered_map<OrderID, OrderRef> index" - the thing the real
// cancelOrder's O(1) average lookup is being compared against.
void naiveCancel(std::list<Order>& orders, OrderID id) {
    for (auto it = orders.begin(); it != orders.end(); ++it) {
        if (it->id == id) {
            orders.erase(it);
            return;
        }
    }
}

double average(const std::vector<uint64_t>& v) {
    uint64_t sum = 0;
    for (uint64_t x : v) sum += x;
    return static_cast<double>(sum) / v.size();
}

uint64_t median(std::vector<uint64_t> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

int main() {
    constexpr int N = 100'000;
    constexpr Price MID = 1000;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> qtyDist(1, 100);

    // Identical order data feeds both structures, so the only difference
    // being measured is the cancellation strategy, nothing else.
    std::vector<Order> seedOrders;
    seedOrders.reserve(N);
    for (int i = 0; i < N; i++) {
        Order o;
        o.id = static_cast<uint64_t>(i) + 1;
        bool isBuy = (i % 2 == 0);
        o.side = isBuy ? Side::buy : Side::sell;
        o.order_type = OrderType::limit;
        o.price = isBuy ? (MID - 200 - i) : (MID + 200 + i);
        o.quantity = static_cast<uint32_t>(qtyDist(rng));
        o.timestamp = static_cast<uint64_t>(i);
        seedOrders.push_back(o);
    }

    // Cancel in shuffled order, not insertion order - otherwise a naive
    // scan would unfairly benefit from always finding the target near
    // the front (or back) of the list.
    std::vector<uint64_t> cancelIds;
    cancelIds.reserve(N);
    for (const auto& o : seedOrders) cancelIds.push_back(o.id);
    std::shuffle(cancelIds.begin(), cancelIds.end(), rng);

    // --- Real OrderBook: hash-indexed cancel ---
    OrderBook realBook;
    for (auto o : seedOrders) realBook.match(o); // copy: match() mutates its argument, seedOrders must stay pristine

    std::vector<uint64_t> realLatenciesNs;
    realLatenciesNs.reserve(N);
    for (uint64_t id : cancelIds) {
        auto t0 = std::chrono::steady_clock::now();
        realBook.cancelOrder(id);
        auto t1 = std::chrono::steady_clock::now();
        realLatenciesNs.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    }

    // --- Naive baseline: unindexed linear scan cancel ---
    std::list<Order> naiveOrders(seedOrders.begin(), seedOrders.end());

    std::vector<uint64_t> naiveLatenciesNs;
    naiveLatenciesNs.reserve(N);
    for (uint64_t id : cancelIds) {
        auto t0 = std::chrono::steady_clock::now();
        naiveCancel(naiveOrders, id);
        auto t1 = std::chrono::steady_clock::now();
        naiveLatenciesNs.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    }

    double realAvg = average(realLatenciesNs);
    double naiveAvg = average(naiveLatenciesNs);
    double improvementPct = (naiveAvg - realAvg) / naiveAvg * 100.0;

    std::cout << "N = " << N << " cancellations, identical data and order for both\n\n";
    std::cout << "Real (hash-indexed) cancel:\n";
    std::cout << "  avg: " << realAvg << " ns\n";
    std::cout << "  p50: " << median(realLatenciesNs) << " ns\n\n";
    std::cout << "Naive (linear scan) cancel:\n";
    std::cout << "  avg: " << naiveAvg << " ns\n";
    std::cout << "  p50: " << median(naiveLatenciesNs) << " ns\n\n";
    std::cout << "Improvement: " << improvementPct << "%\n";
}
