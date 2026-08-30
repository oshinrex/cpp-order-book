#include "../include/OrderBook.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <vector>

struct Stats {
    double avg;
    uint64_t p50, p90, p95, p99, max;
};

Stats computeStats(std::vector<uint64_t> latencies, size_t warmup) {
    std::vector<uint64_t> sorted(latencies.begin() + warmup, latencies.end());
    std::sort(sorted.begin(), sorted.end());

    uint64_t sum = 0;
    for (uint64_t ns : sorted) sum += ns;

    auto percentile = [&](double p) {
        size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
        return sorted[idx];
    };

    return Stats{
        static_cast<double>(sum) / sorted.size(),
        percentile(0.50), percentile(0.90), percentile(0.95), percentile(0.99),
        sorted.back()
    };
}

void printStats(const std::string& label, const Stats& s) {
    std::cout << label << "\n";
    std::cout << "  avg: " << s.avg << " ns\n";
    std::cout << "  p50: " << s.p50 << " ns\n";
    std::cout << "  p90: " << s.p90 << " ns\n";
    std::cout << "  p95: " << s.p95 << " ns\n";
    std::cout << "  p99: " << s.p99 << " ns\n";
    std::cout << "  max: " << s.max << " ns\n";
}

struct TrialResult {
    double throughput;
    Stats matchStats;
    Stats cancelStats;
};

// Runs one full pass of the mixed workload (seed + 900k add/cancel events)
// and returns its aggregate stats. Same RNG seed every call -> identical
// workload each trial, so any spread across trials reflects timing noise,
// not different input data.
TrialResult runTrial() {
    constexpr int SEED_ORDERS = 100'000;
    constexpr Price MID = 1000;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> qtyDist(1, 100);

    OrderBook book;

    std::vector<uint64_t> liveIds;
    liveIds.reserve(SEED_ORDERS);

    for (int i = 0; i < SEED_ORDERS; i++) {
        Order seed;
        seed.id = static_cast<uint64_t>(i) + 1;
        bool isBuy = (i % 2 == 0);
        seed.side = isBuy ? Side::buy : Side::sell;
        seed.order_type = OrderType::limit;
        seed.price = isBuy ? (MID - 200 - i) : (MID + 200 + i);
        seed.quantity = static_cast<uint32_t>(qtyDist(rng));
        seed.timestamp = static_cast<uint64_t>(i);

        book.match(seed);
        liveIds.push_back(seed.id);
    }

    constexpr int NUM_EVENTS = 900'000;
    constexpr double CANCEL_PROBABILITY = 0.1;

    std::uniform_int_distribution<int> sideDist(0, 1);
    std::normal_distribution<double> priceOffsetDist(0.0, 25.0);
    std::uniform_real_distribution<double> eventDist(0.0, 1.0);

    std::vector<uint64_t> matchLatenciesNs, cancelLatenciesNs;
    matchLatenciesNs.reserve(NUM_EVENTS);
    cancelLatenciesNs.reserve(NUM_EVENTS);

    auto benchStart = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_EVENTS; i++) {
        if (!liveIds.empty() && eventDist(rng) < CANCEL_PROBABILITY) {
            std::uniform_int_distribution<size_t> idxDist(0, liveIds.size() - 1);
            size_t idx = idxDist(rng);
            uint64_t idToCancel = liveIds[idx];
            liveIds[idx] = liveIds.back();
            liveIds.pop_back();

            auto t0 = std::chrono::steady_clock::now();
            book.cancelOrder(idToCancel);
            auto t1 = std::chrono::steady_clock::now();
            cancelLatenciesNs.push_back(
                std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        } else {
            Order order;
            order.id = static_cast<uint64_t>(SEED_ORDERS + i) + 1;
            order.side = sideDist(rng) == 0 ? Side::buy : Side::sell;
            order.order_type = OrderType::limit;
            order.price = MID + static_cast<Price>(priceOffsetDist(rng));
            order.quantity = static_cast<uint32_t>(qtyDist(rng));
            order.timestamp = static_cast<uint64_t>(i);

            auto t0 = std::chrono::steady_clock::now();
            book.match(order);
            auto t1 = std::chrono::steady_clock::now();
            matchLatenciesNs.push_back(
                std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        }
    }

    auto benchEnd = std::chrono::steady_clock::now();
    double totalSeconds = std::chrono::duration<double>(benchEnd - benchStart).count();

    return TrialResult{
        NUM_EVENTS / totalSeconds,
        computeStats(matchLatenciesNs, 1000),
        computeStats(cancelLatenciesNs, 100)
    };
}

uint64_t median(std::vector<uint64_t> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

int main() {
    constexpr int TRIALS = 5;
    std::vector<TrialResult> results;
    results.reserve(TRIALS);

    for (int t = 0; t < TRIALS; t++) {
        std::cout << "Running trial " << (t + 1) << "/" << TRIALS << "...\n";
        results.push_back(runTrial());
    }

    std::vector<double> throughputs;
    std::vector<uint64_t> matchP50s, matchP95s, matchP99s;
    std::vector<uint64_t> cancelP50s, cancelP95s, cancelP99s;

    for (const auto& r : results) {
        throughputs.push_back(r.throughput);
        matchP50s.push_back(r.matchStats.p50);
        matchP95s.push_back(r.matchStats.p95);
        matchP99s.push_back(r.matchStats.p99);
        cancelP50s.push_back(r.cancelStats.p50);
        cancelP95s.push_back(r.cancelStats.p95);
        cancelP99s.push_back(r.cancelStats.p99);
    }

    std::cout << "\n=== Individual trials ===\n";
    for (size_t i = 0; i < results.size(); i++) {
        std::cout << "Trial " << (i + 1) << ": throughput=" << results[i].throughput
                   << " match.p99=" << results[i].matchStats.p99
                   << " cancel.p99=" << results[i].cancelStats.p99 << "\n";
    }

    std::cout << "\n=== Median across " << TRIALS << " trials ===\n";
    std::cout << "Throughput: " << median(throughputs) << " events/sec\n";
    std::cout << "Match  p50: " << median(matchP50s) << " ns\n";
    std::cout << "Match  p95: " << median(matchP95s) << " ns\n";
    std::cout << "Match  p99: " << median(matchP99s) << " ns\n";
    std::cout << "Cancel p50: " << median(cancelP50s) << " ns\n";
    std::cout << "Cancel p95: " << median(cancelP95s) << " ns\n";
    std::cout << "Cancel p99: " << median(cancelP99s) << " ns\n";
}
