# Testing Strategy for Dissertation

## Scope and Positioning

This project separates:

- **Correctness testing** (GoogleTest): verifies algorithm and system behavior.
- **Performance benchmarking** (existing benchmark binaries): measures latency/throughput.

This separation is important for dissertation rigor because correctness must be established before performance claims are interpreted.

## Assumptions

- Gateway mode in dissertation analysis uses a **single-client assumption**.
- Multi-client ingestion is not the primary evaluation axis for order book comparison.
- A small set of integration tests still validates robustness of parser + networking path.

## Test Pyramid

1. **Unit tests (fast, deterministic, CI-friendly)**

- Order book contract across map/vector/array/hybrid/pool
- Array-specific validation (range/tick alignment)
- Pool-specific behavior (exhaustion)
- SPSC queue semantics and stress
- FIX parser framing and message-type handling
- Matching engine state transitions and metrics counters
- Factory behavior for type registration

1. **Integration tests (real sockets + real queue + real matching engine)**

- Single client sends order through TCP gateway to matching engine
- Partial TCP frame handling across recv boundaries

1. **Benchmarks (already present)**

- Direct mode, gateway mode, MPSC mode
- Scenario sweeps and statistical summaries

## Coverage Policy

Coverage is measured with LLVM instrumentation (`-fprofile-instr-generate -fcoverage-mapping`) and reported from unit tests.

Recommended dissertation reporting:

- Report **line coverage** and **branch coverage proxy** (from LLVM region/branch data where available).
- Exclude test code and third-party dependencies.
- Present coverage as evidence of validation depth, not as correctness proof.

## Industry-Aligned Practices Used

- Contract-style parametrized tests for interchangeable implementations.
- Deterministic unit tests with minimal timing assumptions.
- Real integration tests where protocol framing and transport behavior matter.
- Clear separation between correctness and performance tests.

## Commands

### Standard build and test

```bash
cmake -S . -B build
cmake --build build -j$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)
ctest --test-dir build -L unit --output-on-failure
```

### Integration TCP path (opt-in)

```bash
cmake -S . -B build -DHFT_ENABLE_INTEGRATION_TESTS=ON
cmake --build build -j$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)
ctest --test-dir build -L integration --output-on-failure
```

### Coverage build and report (macOS/LLVM)

```bash
cmake -S . -B build-coverage -DHFT_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-coverage -j$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)
cmake --build build-coverage --target coverage
```

Coverage HTML report output:

- `build-coverage/coverage/html/index.html`

## Threats to Validity and Mitigation

- **Single-client gateway assumption** may under-represent production exchange contention.
  - Mitigation: this is explicitly scoped in methodology; MPSC benchmark covers concurrency scaling separately.
- **Timing variability** in benchmark mode due to OS scheduling and hardware frequency.
  - Mitigation: repeated runs, warmup phases, and reporting of variance.
## Canonical Benchmark Environment

For the dissertation results (2026-03-26 dataset), all performance benchmarks were executed on the following canonical platform:

- **Hardware**: AMD Ryzen 5 7600X (6 Cores / 12 Threads) @ 4.7GHz.
- **System**: WSL2 (Windows Subsystem for Linux 2) on Windows 11.
- **OS**: Ubuntu 24.04.2 LTS (Kernel 6.6.87.2).
- **Core Pinning**: Performance runs were executed with explicit thread-to-core pinning to physical cores to minimize context-switch noise.
- **Perf Analysis**: Linux `perf` events (L1/L2, task-clock) collected via the WSL-specific `linux-tools-6.8.0-106` package.

See `results/perf/validation_real/host_provenance.txt` for exact run metadata.

## Thread Pinning & Isolation Strategy

To achieve nanosecond-level precision, the project employs a "double-enforcement" pinning strategy:

1.  **OS-Level Enforcement**: The `run_direct_gateway_with_perf.sh` script uses `taskset -c <core>` to restrict the process to a specific physical core.
2.  **Process-Level Confirmation**: The `orderbook_benchmark` binary uses the `--pin-core <id>` flag, which internally calls `pthread_setaffinity_np` (on Linux) or `thread_policy_set` (on macOS) to ensure the benchmark thread itself is bound to the target core.
3.  **Verification**: The binary explicitly verifies the affinity mask and prints "Thread successfully pinned to core X" to `stdout` upon successful initialization.

### Rationale for Direct vs. Gateway Modes

-   **Direct/MPSC Modes**: Pinning is **mandatory**. This minimizes L1/L2 cache evictions and OS scheduler interference, allowing us to measure the raw algorithmic latency of the order book structures.
-   **Gateway Mode**: Pinning is **intentionally disabled** for automated sweeps. Since this mode involves two distinct processes (Client and Server) communicating over a loopback socket, pinning both to the same core would introduce significant context-switching overhead, while pinning them to different cores would vary by host architecture. Instead, we allow the OS scheduler to manage the pair, focusing the measurement on the end-to-end network stack and queueing effects.
