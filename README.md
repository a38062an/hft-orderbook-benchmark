# HFT Orderbook Benchmark

## Build Instructions
System requirements: CMake, C++17 compatible compiler, Python 3.

```bash
mkdir -p build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

## Core Components

### Exchange Server
Matching engine server with integrated TCP gateway and FIX 4.2 parser.
```bash
./build/src/hft_exchange_server
```

### Order Sender Client
Generates FIX protocol orders with overlapping price ranges to simulate matching opportunities.
```bash
./build/src/tcp_order_sender [num_orders]
```

## Performance Benchmarking

### Benchmarking Suite
Tests multiple implementations (Map, Vector, Array, Hybrid, Pool) across scenarios including tight spreads, high cancellations, and read-heavy workloads.
```bash
./build/benchmarks/orderbook_benchmark
```
Results are saved to `results/` with the prefix `benchmark_breakdown_`.

### Scripted Execution
Automates multiple benchmark runs to minimize noise.
```bash
./scripts/run_benchmark.sh
```

### Data Analysis and Visualization
```bash
# Aggregate multiple CSV results
python3 scripts/combine_results.py

# Generate performance plots
python3 scripts/plot_results.py
```
Visualizations are exported to `docs/plots/`.

### Platform Compatibility
On macOS, strict thread pinning is restricted by the kernel. The `thread_policy_set` calls act as affinity hints. "Failed to set thread affinity" warnings on macOS are expected and do not prevent the benchmark from running. Full hardware pinning is available on Linux systems.
