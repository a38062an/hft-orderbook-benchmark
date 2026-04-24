# HFT Order Book Benchmark Framework

A high-performance C++20 infrastructure for benchmarking automated trading strategies and order book data structures with nanosecond precision.

## 🚀 Quick Start (For Markers)

The system requires a C++20 compliant compiler (GCC 11+ or Clang 13+) and CMake 3.15+.

### 1. Build
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)
```

### 2. Run Direct Mode (Isolated Engine Test)
```bash
./build/benchmarks/orderbook_benchmark --mode direct --book array --scenario mixed --orders 1000000
```

### 3. Run Gateway Mode (Networked Test)
Start the gateway/server in one terminal:
```bash
./build/src/hft_exchange_server --book array --port 12345
```
Run the benchmark client in another:
```bash
./build/benchmarks/orderbook_benchmark --mode gateway --book array --port 12345 --orders 10000
```

## Testing (GoogleTest + CTest)

Correctness tests are separated from performance benchmarks.

Portable default: run unit tests first (`-L unit`).

```bash
cmake -S . -B build
cmake --build build -j$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)
ctest --test-dir build -L unit --output-on-failure
```

TCP integration path is opt-in:

```bash
cmake -S . -B build -DHFT_ENABLE_INTEGRATION_TESTS=ON
cmake --build build -j$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)
ctest --test-dir build -L integration --output-on-failure
```

### Coverage (LLVM on macOS)

```bash
cmake -S . -B build-coverage -DHFT_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-coverage -j$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)
cmake --build build-coverage --target coverage
```

HTML report:

- `build-coverage/coverage/html/index.html`

For dissertation methodology and evidence strategy, see `docs/testing/testing_strategy.md`.
For the step-by-step branch-closure workflow used in this repo, see `docs/testing/branch_coverage_iterative_workflow.md`.
For a first-time, practical explanation of GoogleTest + LLVM coverage (commands, metrics, validation, and troubleshooting), see `docs/testing/coverage_how_it_works.md`.
For users implementing a new orderbook (template, factory registration, correctness/perf validation), see `docs/user_manual_orderbook_developer.md`.

## Recommended Environment Split (macOS + WSL2/Linux)

This repository supports cross-platform development, but the measurement workflow is intentionally split:

- Development baseline for this project has been macOS for correctness and coverage iteration.
- macOS: correctness testing, unit/integration validation, and coverage workflow.
- WSL2/Linux: benchmark runs that require thread pinning and Linux perf counters.

### 4.6 Thread Pinning & Performance Isolation

For direct-mode benchmarking, the system supports explicit CPU core pinning to ensure deterministic results.

- **Internal Flag**: Use `--pin-core <id>` to bind the execution thread to a specific logical processor.
- **Verification**: On startup, the benchmark binary will output: `Thread successfully pinned to core <id>`.
- **System Support**: 
  - **Linux**: Uses `pthread_setaffinity_np`.
  - **macOS**: Uses `thread_policy_set` (handled as a priority hint by the Mach kernel).
  - **WSL2**: Fully supported; recommended for canonical dissertation measurements.

> [!TIP]
> Combine with Linux `taskset` for maximum isolation:
> `taskset -c 0 ./build/benchmarks/orderbook_benchmark --mode direct --pin-core 0`

Why this split exists:

- Linux provides stable CPU affinity controls (`taskset`, `sched_setaffinity`) used by this benchmark workflow.
- macOS can build and test the project, but strict thread pinning behavior is more restricted.
- Linux perf events used by `run_direct_gateway_with_perf.sh` are Linux-specific.
- **Gateway-mode latency decomposition (network/queue/engine) is measured via Linux TCP stack parameters and may not generalize to macOS or Windows.**

Plot generation (`python3 scripts/plot_benchmark.py results/results.csv`) can run on either environment. In this project, the canonical benchmark dataset and dissertation figures are produced from the Linux/WSL2 run artifacts.

## 60-Second Quickstart

If you are new to this repository, run this minimal flow first:

```bash
# 1) Build
cmake -S . -B build
cmake --build build -j$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)

# 2) Smoke test direct mode (single case)
./build/benchmarks/orderbook_benchmark --mode direct --book array --scenario mixed --runs 1 --orders 5000

# 3) Generate plots from latest CSV
python3 scripts/plot_benchmark.py results/results.csv
```

Expected success signals:

- benchmark prints a summary table with a `direct` row for `array/mixed`
- `results/results.csv` is created or updated
- plot files are generated under `plots/`

## Server Binary

The repository includes an HFT exchange server that accepts orders via TCP:

```bash
# See supported order book types
./build/src/hft_exchange_server --list_books

# Start server with specific order book (default: map)
./build/src/hft_exchange_server --book array --port 12345

# Show all server options
./build/src/hft_exchange_server --help
```

The server listens on TCP and matches orders using the selected order book structure. See `src/main.cpp` and client examples in `clients/` for protocol details.

If your build places executables in `build/bin/`, replace `build/src/` with `build/bin/` in the commands above.

## Quick Start: Full Benchmark Sweep

To run the full benchmark suite with the current pinning policy:

- **Direct mode**: pinned to one allowed CPU core.
- **Gateway mode**: unpinned.
- **MPSC mode**: optional, and pinned to one allowed CPU core when run via `--pin-core`.

Use the integrated runner for Direct + Gateway + Linux perf in one command.

### Capture Everything (Recommended)

This command executes the full matrix in one shot:

- Direct: all books x all scenarios
- Gateway: all books x all scenarios
- MPSC: all books x all scenarios x producers {1,2,4,8}

```bash
./scripts/run_direct_gateway_with_perf.sh --scenario all --runs 5 --orders 10000 --with-mpsc --mpsc-producers all
```

### Fresh Results (Sequential Commands)

Run these in order when you want a brand-new dataset and fresh perf artifacts:

```bash
# 1) Go to repo root
cd /root/projects/hft-orderbook-benchmark

# 2) Stop any leftover benchmark/server processes from interrupted runs
pkill -f "run_gateway_sweep.sh|hft_exchange_server|orderbook_benchmark" >/dev/null 2>&1 || true

# 3) Remove old aggregate and perf artifacts
rm -f results/results.csv results/perf/direct_perf_*.txt results/perf/manifest.csv

# 4) Run full matrix: direct + gateway + mpsc
./scripts/run_direct_gateway_with_perf.sh --scenario all --runs 5 --orders 10000 --with-mpsc --mpsc-producers all

# 5) Validate row counts
awk -F, 'NR>1{m[$1]++} END{print "direct="m["direct"]", gateway="m["gateway"]", mpsc="m["mpsc"]", total="m["direct"]+m["gateway"]+m["mpsc"]}' results/results.csv

# 6) Generate plots/tables
python3 scripts/plot_benchmark.py results/results.csv
```

Expected row counts after step 5: `direct=35`, `gateway=35`, `mpsc=140`, `total=210`.

If you are on WSL and `/usr/bin/perf` is kernel-mismatch wrapped, the script auto-falls back to `/usr/lib/linux-tools-*/perf` when available.
If perf is still unavailable, run without perf capture:

```bash
./scripts/run_direct_gateway_with_perf.sh --scenario all --runs 5 --orders 10000 --with-mpsc --mpsc-producers all --skip-perf
```

### Clean Run vs Append Run

`results/results.csv` is key-upserted by the benchmark writer:

- Direct/Gateway keys: `(mode, book, scenario)`
- MPSC keys: `(mode, book, scenario, Producers)`

What this means:

- Re-running the same key updates that row (it does not create duplicates).
- New keys are appended.

If you want a fully fresh dataset, delete the CSV first:

```bash
rm -f results/results.csv
./scripts/run_direct_gateway_with_perf.sh --scenario all --runs 5 --orders 10000 --with-mpsc --mpsc-producers all
```

If you also want fresh perf artifacts for that run, clear perf outputs too:

```bash
rm -f results/results.csv results/perf/direct_perf_*.txt results/perf/manifest.csv
./scripts/run_direct_gateway_with_perf.sh --scenario all --runs 5 --orders 10000 --with-mpsc --mpsc-producers all
```

After completion, generate plots:

```bash
python3 scripts/plot_benchmark.py results/results.csv
```

Quick validation after the full run:

```bash
awk -F, 'NR>1{m[$1]++} END{print "direct="m["direct"]", gateway="m["gateway"]", mpsc="m["mpsc"]", total="m["direct"]+m["gateway"]+m["mpsc"]}' results/results.csv
```

Expected counts for full matrix: `direct=35`, `gateway=35`, `mpsc=140`, `total=210`.

### Minimal Direct + Gateway Run

Use this when you only need direct vs gateway (no MPSC):

```bash
# 1. Direct (thread pinned) + Gateway (unpinned) + Linux perf recording
./scripts/run_direct_gateway_with_perf.sh --scenario all --runs 5 --orders 10000

# 2. Optional: MPSC Concurrency Sweep (single-core pinned)
CORE=$(taskset -pc $$ | sed -E 's/.*: *//' | cut -d, -f1 | cut -d- -f1)
taskset -c "$CORE" ./build/benchmarks/orderbook_benchmark --mode mpsc --book all --scenario mixed --producers all --orders 10000 --runs 5 --pin-core "$CORE"

# 3. Generate Organized Plots + Tables
python3 scripts/plot_benchmark.py results/results.csv
```

### What `manifest.csv` means

`results/perf/manifest.csv` is a lightweight run ledger (a manifest) that tracks what was executed and where its artifacts were written.

For each integrated run, it appends metadata like timestamp, scenario, run count, order count, chosen pin core, output CSV, and perf log path. This makes results reproducible and auditable.

### Why MPSC is optional

MPSC answers a different question from Direct/Gateway.

- Direct/Gateway compare algorithmic latency and end-to-end system latency.
- MPSC measures scaling behavior under concurrent producer load.

So MPSC is marked optional in the quick-start path: include it when you need concurrency-scaling evidence, skip it when you only need Direct vs Gateway.

### Results CSV Format

`results/results.csv` contains all benchmark measurements and is used for plot generation. Common columns:

| Column | Type | Example | Notes |
|--------|------|---------|-------|
| `Mode` | string | `direct` \| `gateway` \| `mpsc` | Benchmark mode |
| `Book` | string | `array` \| `map` \| `vector` \| ... | Order book implementation |
| `Scenario` | string | `mixed` \| `dense_full` \| ... | Market condition workload |
| `Latency_ns` | float | 210.72 | Mean latency for the row mode |
| `LatencyStdDev_ns` | float | 12.34 | Latency standard deviation across runs |
| `P99_ns` | float | 245.89 | 99th percentile latency |
| `P99StdDev_ns` | float | 8.12 | P99 standard deviation across runs |
| `Max_ns` | float | 298.34 | Maximum latency observed |
| `Network_ns` | float | 2,159,070.98 | Network component (gateway only) |
| `Queue_ns` | float | 565,998.96 | Queue wait component (gateway only) |
| `Engine_ns` | float | 920.43 | Engine processing component (gateway only) |
| `Producers` | int | 1, 2, 4, 8 | Producer count (MPSC only; 0 otherwise) |

Load in Python: `import pandas as pd; df = pd.read_csv('results/results.csv')`

For full column list, run: `head -1 results/results.csv`

The direct-mode perf counters and run metadata are recorded under `results/perf/`:

- `results/perf/direct_perf_<timestamp>.txt`
- `results/perf/manifest.csv`

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
./scripts/run_gateway_sweep.sh --scenario all --runs 5 --orders 10000

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
| `Match/Drop` | Dropped order count (should be 0; queue capacity is 16,384 entries) |

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
- **Tag 596 (SyncBarrier)**: Included in the `U1` message. It specifies the **Expected Order Count**. The server waits until the matching engine reaches this count, with a bounded timeout to avoid indefinite blocking. This prevents reporting results before the engine has had a chance to drain queued work.

### Example Stats Flow

1. **Client Sends**: `8=FIX.4.2|35=U1|596=10000|10=000|` (I sent 10k orders, wait until they are all done).
2. **Server Logic**: `while(processed < expected && elapsed < timeout) { yield(); }`
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

- macOS is fully supported for building, testing, and coverage collection.
- Linux/WSL2 is recommended for performance experiments that rely on explicit core pinning and perf counters.
- On WSL2, if `/usr/bin/perf` is a kernel-mismatch wrapper, use the integrated script fallback or install matching linux-tools.

### Cross-Architecture Comparability (Important)

Architecture and platform differences matter for absolute latency/throughput values.

- Do compare implementations within the same machine/OS/kernel configuration.
- Do use relative metrics (rankings, slowdown vs best, variance trends) when discussing portability of conclusions.
- Do not directly compare absolute ns-level values across different architectures (for example ARM64 vs x86_64) or different OS scheduler stacks.

For dissertation-quality reporting, always publish environment metadata with results (CPU model, core/thread count, cache sizes, OS/kernel version, and whether runs were pinned/unpinned).
