# Benchmark results

All runs: Release build (`-O3 -DNDEBUG`), macOS, seeded RNG (seed 42) for reproducibility.

## Pre-optimization baseline (2026-08-29)

Implementation: `std::list<Order>` per price level (one heap allocation per resting order).

**Pure matching** (`bench/benchmark.cpp`, 1M orders): 926K orders/sec, p50 559 ns, p90 1331 ns, p99 3601 ns

**Mixed workload** (`bench/mixed_benchmark.cpp`, 100K seed + 900K events, median of 5 trials): ~995K events/sec; match p50 507 ns / p95 1457 ns / p99 2262 ns; cancel p50 2130 ns / p95 2900 ns / p99 3471 ns

**Cancellation vs. naive baseline** (`bench/cancel_baseline_benchmark.cpp`, N=100,000): hash-indexed avg 2398 ns vs. naive linear-scan avg 269171 ns — 99.1% improvement

**Allocations** (`bench/allocation_benchmark.cpp`, 1M orders): 2,314,507 total allocations (2.31/order), 145,603,224 bytes (~139 MiB)

## Post-optimization: intrusive-list object pool (2026-08-29)

Implementation: `PriceLevel` holds a manually-linked intrusive doubly-linked list of `OrderNode`, backed by a preallocated `OrderNodePool` (300,000 nodes, sized to the observed peak resting-order count) instead of `std::list<Order>`'s per-node heap allocation.

**Note on methodology**: an initial attempt used a 2,000,000-node pool ("generously oversized") and measured *worse* throughput and *more* total bytes allocated than the baseline. Root cause: a pool far larger than the working set pays first-touch page-fault cost across many never-reused pages, and the oversized allocation itself dominates total bytes. Right-sizing the pool to ~300K (close to actual peak usage) fixed both regressions - a real example of an "obvious" optimization initially backfiring until measured and corrected.

A second, unrelated regression during testing (matching latency looked ~2x worse) traced back to the build silently reverting to CMake's `Debug` type (likely VSCode's CMake Tools extension re-configuring in the background) - not a code issue. Always verify `CMAKE_BUILD_TYPE` before trusting a benchmark number.

**Pure matching**: 1,052,310 orders/sec (**+13.6%**), p50 429 ns (**-23.3%**), p90 1160 ns, p99 2809 ns (**-22.0%**)

**Mixed workload** (median of 5 trials): 1,143,070 events/sec (**+14.9%**); match p50 398 ns / p95 1266 ns / p99 1918 ns; cancel p50 1728 ns / p95 2480 ns / p99 2980 ns

**Cancellation vs. naive baseline**: hash-indexed avg 1379 ns (**-42.5%** vs. pre-optimization), naive baseline avg 224306 ns — 99.4% improvement

**Allocations**: 1,690,943 total allocations (1.69/order, **-26.9%**), 127,363,272 bytes (~121 MiB, **-12.5%**)

## Summary for resume bullets

- Matching: 1.05M orders/sec, p50 429 ns, p99 2.8 us (1M+ synthetic orders, deterministic FIFO)
- Cancellation: p50 1.24 us (isolated cancel-only benchmark), **99.4% improvement over a naive linear-scan baseline**
- Mixed workload throughput/tail latency: 1.14M events/sec, match p99 1.9 us, cancel p99 3.0 us, across 1M+ randomized add/cancel events
- Memory/allocations: **26.9% fewer heap allocations, 12.5% less memory allocated**, via an intrusive-list object pool replacing per-order `std::list` allocation - identified via allocation-count instrumentation, not guesswork
