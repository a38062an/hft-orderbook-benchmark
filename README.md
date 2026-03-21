# HFT Orderbook Benchmark

Trading engine and benchmarking suite for analyzing order book data structures and system latency.

## Build Instructions

Requirements: CMake, C++17, Python 3.

```bash
mkdir -p build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

## Testing (GoogleTest + CTest)

Correctness tests are separated from performance benchmarks.

```bash
cmake -S . -B build
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

### Coverage (LLVM on macOS)

```bash
cmake -S . -B build-coverage -DHFT_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-coverage -j$(sysctl -n hw.ncpu)
cmake --build build-coverage --target coverage
```

HTML report:

- `build-coverage/coverage/html/index.html`

For dissertation methodology and evidence strategy, see `docs/testing/testing_strategy.md`.
For the step-by-step branch-closure workflow used in this repo, see `docs/testing/branch_coverage_iterative_workflow.md`.
For a first-time, practical explanation of GoogleTest + LLVM coverage (commands, metrics, validation, and troubleshooting), see `docs/testing/coverage_how_it_works.md`.

## Quick Start: Full Benchmark Sweep

To perform a complete research sweep of all implementations and generate dissertation-ready plots plus analysis tables, run these commands in order:

```bash
# 1. Direct Mode Sweep (Pure Algorithm Latency)
./build/benchmarks/orderbook_benchmark --mode direct --book all --scenario all --runs 5

# 2. Gateway Mode Sweep (Full Systemic Latency + Queuing)
./scripts/run_gateway_sweep.sh --scenario all --runs 5 --orders 10000

# 3. MPSC Concurrency Sweep (Multi-Producer Exchange Simulation)
./build/benchmarks/orderbook_benchmark --mode mpsc --book all --scenario mixed --producers all --orders 10000 --runs 5

# 4. Generate Organized Plots + Tables
python3 scripts/plot_benchmark.py results/results.csv
```

This will populate the `plots/` directory with organized subfolders (`direct/`, `gateway/`, `direct_vs_gateway/`, `mpsc/`, `scenario_validity/`, and `tables/`) for comparative analysis under varied market conditions.

## Core Components

### Exchange Server

Supports multiple implementations at runtime.

```bash
./build/src/hft_exchange_server --book [map|array|vector|hybrid|pool] --port 12345
```

**Extensibility**: The automation scripts dynamically discover supported order book types by querying the benchmark binary. If you add a new implementation to the `OrderBookFactory`, it will be automatically included in both the `direct --book all` and `run_gateway_sweep.sh` tests.

### Benchmark Tool

Supports two latency measurement modes:

1. **Direct**: Algorithmic latency (in-process). Use `--book all --scenario all` to test every implementation across every market condition sequentially.
2. **Gateway**: Systemic latency (TCP/FIX).

**Note on Gateway Mode**: The client cannot change the server's implementation at runtime. If using `gateway` mode, the server and client must be started with the same `--book` label. If you need to automate a full gateway comparison, you should use a script to restart the server between runs.

```bash
# Direct Mode
# Tests all implementations across all scenarios sequentially
./build/benchmarks/orderbook_benchmark --mode direct --book all --scenario all

# Gateway Mode (Manually)
# 1. Start hft_exchange_server in one terminal
./build/src/hft_exchange_server --book array
# 2. Run benchmark in another terminal
./build/benchmarks/orderbook_benchmark --mode gateway --book array

# Gateway Mode (Automated)
# This script automatically restarts the server for each book type
# Pass 'all' to test every market scenario across every book
./scripts/run_gateway_sweep.sh all

# MPSC Mode (Multi-Producer Concurrency Sweep)
# Simulates multiple concurrent gateway clients all pushing into one matching engine
# --producers all automatically runs 1, 2, 4, and 8 producers in sequence
./build/benchmarks/orderbook_benchmark --mode mpsc --book all --scenario mixed --producers all
# Or test a specific producer count
./build/benchmarks/orderbook_benchmark --mode mpsc --book all --scenario mixed --producers 4
```

### 3. MPSC Mode — Multi-Producer Exchange Simulation

`--mode mpsc` directly addresses the question: *"What happens to latency and throughput as concurrent producer count scales?"*

Unlike the SPSC-based Gateway mode (one client, one TCP stream), MPSC spawns **N producer threads** all pushing into a lockfree `moodycamel::ConcurrentQueue` with a **single consumer** (the matching engine). This models a realistic exchange where multiple algorithmic clients submit orders simultaneously.

**Key metrics produced:**

| Column | What it measures |
| --- | --- |
| `Latency(ns)` / `Que(ns)` | Mean time from producer stamp → consumer receive. Measures queue contention. |
| `P99(ns)` | 99th percentile queue latency — tail behaviour under concurrent load |
| `Eng(ns)` | Matching engine time (`addOrder + match`). Should stay **flat** as producers increase — proves engine isolation. |
| `Net(ns)` | Repurposed to display the **producer count** for that row |
| `Throughput` | Total orders/sec across all producers combined |
| `Match(ns)` | Dropped order count (should be 0; queue capacity is 16,384 entries) |

**Template Note**: `src/orderbooks/template_order_book.hpp` is intentionally a teaching scaffold with TODOs. It is provided to explain how to incorporate a new order book implementation and should not be used as a benchmark target until fully implemented.

**`--producers all`** runs producer counts 1 → 2 → 4 → 8 in a single command, storing each as a separate row in the CSV — ready for plotting a scalability curve.

```bash
./build/benchmarks/orderbook_benchmark --mode mpsc --book all --scenario mixed --producers all --orders 10000 --runs 5
```

## Custom FIX Protocol Specification

For the purpose of this HFT benchmark, we use a subset of the FIX 4.2 protocol with proprietary extensions (User-Defined Fields) to enable high-precision synchronization and timing.

### Proprietary Message Types

| MsgType (35) | Name           | Description                                            |
| :----------- | :------------- | :----------------------------------------------------- |
| `D`          | NewOrderSingle | Standard FIX order entry message.                      |
| `U1`         | StatsRequest   | Client requesting performance metrics from the server. |
| `U2`         | StatsResponse  | Server providing aggregated metrics to the client.     |

### Key Custom Tags

- **Tag 60 (TransactionTime)**: Encodes the client's `sendTimestamp` in nanoseconds since Epoch. The Server uses this to calculate True End-to-End Latency.
- **Tag 596 (SyncBarrier)**: Included in the `U1` message. It specifies the **Expected Order Count**. The server will "busy-wait" until the matching engine has processed exactly this many orders before replying. This prevents "cheating" where results are reported before the slow engine has finished draining the queue.

### Example Stats Flow

1. **Client Sends**: `8=FIX.4.2|35=U1|596=10000|10=000|` (I sent 10k orders, wait until they are all done).
2. **Server Logic**: `while(processed < 10000) { yield(); }`
3. **Server Replies**: `8=FIX.4.2|35=U2|Mean=176.50|Count=10000|...|`

---

## Custom Order Files (CSV)

You can provide a custom CSV file to simulate specific market scenarios. The CSV should follow the format: `Side,Price,Quantity` (e.g., `B,100,10` for Buy).

```bash
./build/benchmarks/orderbook_benchmark --mode gateway --csv my_orders.csv --csv_out results.csv
```

The tool prints summary statistics (Mean, P99, Max) and throughput to the console upon completion.

## Analysis and Visualization

### Unified Reporting

Consolidates client-side injection latency and server-side internal processing time into a single averaged CSV row.

- **Statistical Rigor**: When `--runs N` is used, the system automatically calculates the **Mean** and **Standard Deviation** for Latency, P99, and Throughput across all $N$ runs.
- **Reporting**: The console summary table displays results as `Mean ± StdDev`, and the CSV includes dedicated variance columns (`LatencyStdDev_ns`, `P99StdDev_ns`) for error-bar plotting.

### Plotting

```bash
python3 scripts/plot_benchmark.py results/results.csv
```

The script handles missing data gracefully:

- **Partial Data**: If you haven't run `gateway` yet, it will simply skip the gateway-specific charts.
- **Cross-Mode**: Direct-vs-gateway comparison plots are only generated when both modes are present.

Generates charts in `plots/`:

- `direct/latency_mean_heatmap.png`
- `direct/latency_slowdown_vs_best_heatmap.png`
- `direct/tail_p99_over_mean.png`
- `direct/operation_latency_breakdown_selected_scenarios.png`
- `direct/middle_tier_slowdown_heatmap.png`
- `direct/latency_stddev_no_vector_heatmap.png`
- `gateway/latency_by_scenario.png`
- `gateway/latency_by_scenario_no_vector.png`
- `gateway/latency_slowdown_no_vector_heatmap.png`
- `gateway/dense_full_latency_decomposition_no_vector.png`
- `gateway/latency_stddev_no_vector_heatmap.png`
- `direct_vs_gateway/middle_tier_slowdown_direct_vs_gateway.png`
- `mpsc/throughput_scaling.png`
- `mpsc/queue_vs_engine_scaling.png`
- `scenario_validity/mixed_profile.png`
- `tables/*.md` (rank and winner tables)

### Platform Notes

On macOS, thread pinning is restricted. Warnings are expected and do not prevent execution. Full pinning is supported on Linux.
