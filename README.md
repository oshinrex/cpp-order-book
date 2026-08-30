# cpp-order-book

A price-time priority limit order book and matching engine in C++20, with market/limit orders, partial fills, cancellations, and modifications — built and benchmarked to back up every number below with a reproducible run, not a guess.

## Features

- Price-time priority matching: highest bid / lowest ask first, FIFO within a price level
- Market and limit orders; market orders never rest unfilled
- Partial fills, cancellations, and modifications (with correct exchange-style semantics — see below)
- O(1) average order cancellation by ID via a hash index
- An intrusive-list object pool instead of per-order heap allocation
- A benchmarking suite covering throughput, latency percentiles, a naive-baseline comparison for cancellation, and heap allocation instrumentation

## Architecture

```
Order  ──┐
         ├─▶  PriceLevel  (intrusive linked list of OrderNode, FIFO order)
Trade  ──┘         │
                    ▼
              OrderBook
        ┌───────────┴───────────┐
   bids: map<Price,           asks: map<Price,
   PriceLevel, greater>       PriceLevel>
        │                          │
        └───────────┬──────────────┘
                     ▼
        orderIndex: unordered_map<OrderID, OrderRef>
                     │
                     ▼
              OrderNodePool  (preallocated OrderNode storage)
```

- **`Order`** — a single order: id, side, type (limit/market), price, quantity, timestamp.
- **`PriceLevel`** — the FIFO queue of orders resting at one price, stored as an intrusive doubly-linked list (`OrderNode* head`/`tail`) rather than `std::list`, so nodes come from a preallocated pool instead of individual heap allocations.
- **`OrderNodePool`** — preallocated `OrderNode` storage plus a free list; hands out and reclaims nodes in O(1) with no allocation in the common path.
- **`OrderBook`** — owns two `std::map<Price, PriceLevel>` (bids sorted highest-first, asks lowest-first) and an `unordered_map<OrderID, OrderRef>` index for O(1) average cancel/modify by ID. Exposes `addOrder`, `cancelOrder`, `modifyOrder`, `match`, `bestBid`/`bestAsk`, `size`/`empty`, `printBook`.
- **`Trade`** — a completed fill: buyer id, seller id, price, quantity, timestamp.

## Complexity

| Operation | Complexity | Why |
|---|---|---|
| `addOrder` | O(log L) | L = number of distinct price levels (not total orders) — inserting/finding a key in the `std::map` red-black tree. Node acquisition from the pool is O(1). |
| `cancelOrder` | O(log L) average | O(1) average hash lookup in `orderIndex` gives the exact node directly; unlinking it from the intrusive list is O(1). The O(log L) comes from erasing the price level's map entry when it empties, or reading it via `bids[price]`. |
| `modifyOrder` | O(1) or O(log L) | A same-price quantity *decrease* updates in place, O(1) — the order keeps its queue position. A price change or quantity *increase* is a cancel + re-add (O(log L)), sending the order to the back of the new queue, matching real exchange behavior. |
| `match` | O(k log L) | k = number of price levels fully consumed by this call (proportional to fills produced), each requiring an O(log L) map lookup/erase. |
| `bestBid` / `bestAsk` | O(1) | `std::map` keeps bids/asks sorted; the best price is always `begin()`. |

`std::list` (now an intrusive list over a pool) was chosen for each price level specifically because `std::list::erase(iterator)`/pool-release is O(1) and doesn't invalidate other iterators/pointers — a `std::vector` would need to shift every element after an erase and invalidate every pointer past that point, which would silently corrupt every other `OrderRef` pointing into that price level.

## Design decisions worth knowing

- **Modify semantics**: a same-price quantity decrease keeps the order's queue position (real exchanges do this — you're not adding new risk to the book). A price change or quantity increase loses queue position and goes to the back, because you're materially changing what you're offering.
- **Market orders never rest**: an unfilled market order's remaining quantity is discarded rather than added to the book, matching IOC-style semantics.
- **Single-threaded matching core, on purpose**: correctness of price-time priority is much easier to reason about (and prove via tests) without concurrent access to the book. This is also how most real matching engines are built — concurrency lives at the edges (order intake, market data publishing), not inside the matching decision itself.
- **Object pool sizing matters more than "bigger is safer"**: an early attempt sized the pool at 2,000,000 nodes and made performance *worse*, not better — a pool far larger than the actual working set pays first-touch page-fault cost as it's walked, and the allocation itself dominates total memory. Right-sizing to the observed peak resting-order count (~300K) fixed it. See `BENCHMARKS.md` for the full story.

## Building

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Always pass `-DCMAKE_BUILD_TYPE=Release` explicitly when reconfiguring — IDE tooling (VSCode's CMake extension, in this project's case) can silently reset the build type to `Debug`, which invalidates any benchmark numbers taken afterward (no `-O3`, full bounds-checking overhead).

## Running

| Binary | What it does |
|---|---|
| `./build/orderbook` | Demo driver + the full assertion-based correctness suite (FIFO priority, bestBid/bestAsk, cancellation, modify in-place vs. requeue, market orders) |
| `./build/orderbook_bench` | Pure matching throughput/latency over 1M synthetic orders |
| `./build/orderbook_bench_mixed` | Mixed add/cancel workload, 5 trials, reports median throughput and per-operation-type percentiles |
| `./build/orderbook_bench_cancel_baseline` | A/B comparison: hash-indexed `cancelOrder` vs. a naive linear-scan baseline |
| `./build/orderbook_bench_allocations` | Heap allocation count and total bytes, via instrumented `operator new`/`delete` |

## Benchmark results

Full methodology, raw numbers, and the two bugs found while measuring (an oversized pool, and a silently-reverted build type) are in [`BENCHMARKS.md`](BENCHMARKS.md). Summary, all Release builds, seeded for reproducibility:

- **Matching**: 1.05M orders/sec, p50 429 ns, p99 2.8 μs (1M+ synthetic orders)
- **Cancellation**: p50 1.24 μs — a 99.4% improvement over a naive linear-scan baseline
- **Mixed workload** (matching + insertion + cancellation, 1M+ randomized events): 1.14M events/sec, match p99 1.9 μs, cancel p99 3.0 μs
- **Memory**: 26.9% fewer heap allocations, 12.5% less memory allocated, via an intrusive-list object pool replacing per-order `std::list` allocation

## Testing

No test framework dependency, correctness is proven with `assert()`-based checks in `main.cpp` covering FIFO priority at a shared price level, `bestBid`/`bestAsk` correctness (including fallback to the next level after a cancel), full removal of an order and its emptied price level, both branches of `modifyOrder`, and market orders against an empty book and a partial fill. All pass after the object-pool rewrite with zero regressions.

## Possible future work

- Multi-threaded order intake via an SPSC queue feeding a single-threaded matching core
- A market-data publisher (best bid/ask + depth snapshots on each book change)
- Order/trade persistence and session replay
