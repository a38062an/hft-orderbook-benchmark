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
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

### Coverage build and report (macOS/LLVM)

```bash
cmake -S . -B build-coverage -DHFT_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-coverage -j$(sysctl -n hw.ncpu)
cmake --build build-coverage --target coverage
```

Coverage HTML report output:

- `build-coverage/coverage/html/index.html`

## Threats to Validity and Mitigation

- **Single-client gateway assumption** may under-represent production exchange contention.
  - Mitigation: this is explicitly scoped in methodology; MPSC benchmark covers concurrency scaling separately.
- **Timing variability** in benchmark mode due to OS scheduling and hardware frequency.
  - Mitigation: repeated runs, warmup phases, and reporting of variance.
- **Coverage inflation risk** if tests only execute happy paths.
  - Mitigation: edge-case tests for out-of-range prices, pool exhaustion, incomplete FIX frames, and partial network frames.
