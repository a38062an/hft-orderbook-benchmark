# HFT Orderbook Benchmark

Trading engine and benchmarking suite for analyzing order book data structures and system latency.

## Build Instructions
Requirements: CMake, C++17, Python 3.

```bash
mkdir -p build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

## Quick Start: Full Benchmark Sweep
To perform a complete research sweep of all implementations and generate dissertation-ready plots, run these three commands in order:

```bash
# 1. Direct Mode Sweep (Pure Algorithm Latency)
./build/benchmarks/orderbook_benchmark --mode direct --book all --scenario all --runs 5

# 2. Gateway Mode Sweep (Full Systemic Latency + Queuing)
./scripts/run_gateway_sweep.sh all 5 10000

# 3. Generate Dissertation Plots
python3 scripts/plot_benchmark.py results/results.csv
```
This will populate the `plots/` directory with comparative analysis of every order book implementation under various market conditions.

## Core Components

### Exchange Server
Supports multiple implementations at runtime.
```bash
./build/src/hft_exchange_server --book [map|array|vector|hybrid|pool] --port 12345
```
**Extensibility**: The automation scripts dynamically discover supported order book types by querying the benchmark binary. If you add a new implementation to the `OrderBookFactory`, it will be automatically included in both the `direct --book all` and `run_gateway_sweep.sh` tests.

### Benchmark Tool
Supports two latency measurement modes:
1.  **Direct**: Algorithmic latency (in-process). Use `--book all --scenario all` to test every implementation across every market condition sequentially.
2.  **Gateway**: Systemic latency (TCP/FIX). 

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
Consolidates client-side injection latency and server-side internal processing time (Wire-to-Match) into a single CSV row.
- **Appending**: Successive runs with the same `--csv_out` will append new rows to the file.
- **Statistics**: The plotting script automatically aggregates multiple runs of the same "Mode + Book" combination, showing the mean and variance (error bars).

### Plotting
```bash
python3 scripts/plot_benchmark.py results/results.csv
```
The script handles missing data gracefully:
- **Partial Data**: If you haven't run `gateway` yet, it will simply skip the gateway-specific charts.
- **Side-by-Side**: The `systemic_comparison.png` is only generated if both modes are present in the CSV.

Generates charts in `plots/`:
- `latency_direct.png`
- `systemic_comparison.png`
- `throughput_gateway.png`
- `wire_to_match_server.png`

### Platform Notes
On macOS, thread pinning is restricted. Warnings are expected and do not prevent execution. Full pinning is supported on Linux.
