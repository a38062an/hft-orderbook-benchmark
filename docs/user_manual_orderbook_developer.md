# User Manual: Adding and Validating a New Order Book

This manual is for users who are actively implementing a new order book and need a reliable path to validate correctness and performance.

## 1) What You Are Building

A new order book must satisfy the `IOrderBook` contract in `src/core/i_order_book.hpp`:

- `addOrder(const Order&)`
- `cancelOrder(OrderId)`
- `modifyOrder(OrderId, Quantity)`
- `match()`
- `getOrderCount()`
- `getBestBid()`
- `getBestAsk()`

## 2) Implementation Path (Template + Factory)

### Step A: Start from template

Template file:

- `src/orderbooks/template_order_book.hpp`

Warning: `template_order_book.hpp` is a teaching scaffold with TODO stubs. Do not register it in the factory and do not benchmark it until you have copied it to a new implementation and completed all required methods.

Recommended workflow:

1. Copy template to your own header/source pair, for example:
   - `src/orderbooks/my_order_book.hpp`
   - `src/orderbooks/my_order_book.cpp`
2. Rename class from `TemplateOrderBook` to `MyOrderBook`.
3. Implement all virtual methods from `IOrderBook`.
4. Add your files to `src/CMakeLists.txt` under `hft_core` sources.

### Step B: Register in factory

Factory file:

- `src/core/order_book_factory.hpp`

Update both places:

1. `getSupportedTypes()` add your public string key (for example `"mybook"`).
2. `create(type)` add construction branch returning `std::make_unique<MyOrderBook>()`.

### Step C: Verify discovery

```bash
./build/benchmarks/orderbook_benchmark --list_books
./build/src/hft_exchange_server --list_books
```

Your key should appear in both outputs.

## 3) Portable Validation Flow (Recommended)

### Unit correctness only (portable default)

```bash
cmake -S . -B build
cmake --build build -j$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)
ctest --test-dir build -L unit --output-on-failure
```

Why this is portable:

- no Linux perf dependency
- no socket-integration dependency
- runs on macOS/Linux/WSL2 using GoogleTest + CTest

### Integration TCP path (explicit opt-in)

```bash
cmake -S . -B build -DHFT_ENABLE_INTEGRATION_TESTS=ON
cmake --build build -j$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)
ctest --test-dir build -L integration --output-on-failure
```

Use this when validating gateway/parser/socket behavior, not for quick portable iteration.

## 4) Benchmark Validation For Your New Book

### Direct mode (algorithmic behavior)

```bash
./build/benchmarks/orderbook_benchmark --mode direct --book mybook --scenario all --runs 5 --orders 10000
```

### Gateway mode (end-to-end system path)

```bash
./scripts/run_gateway_sweep.sh --book mybook --scenario all --runs 5 --orders 10000
```

### Optional MPSC mode (concurrency scaling)

```bash
./build/benchmarks/orderbook_benchmark --mode mpsc --book mybook --scenario mixed --producers all --runs 5 --orders 10000
```

## 5) Command Syntax Cheat Sheet

Benchmark binary:

- `--mode`: `direct | gateway | mpsc`
- `--book`: order book key (`map`, `array`, `vector`, `hybrid`, `pool`, or your key)
- `--scenario`: scenario name or `all`
- `--runs`: repeat count used for summary statistics
- `--orders`: number of synthetic orders per run
- `--producers`: MPSC producers (`1`, `2`, `4`, `8`, or `all`)
- `--csv_out`: output CSV path for results
- `--list_books`: print factory-supported book keys
- `--list_scenarios`: print scenario keys

Server binary:

- `--book`: factory key for active engine implementation
- `--port`: TCP listen port
- `--list_books`: print available implementations

## 6) What Automatically Protects You

- Factory can-create-all test: ensures each advertised key is constructible
  - `tests/unit/order_book_factory_test.cpp`
- Contract test auto-uses all factory-supported types
  - `tests/unit/orderbook_contract_test.cpp`
- Branch stress includes all built-in books (including pool)
  - `tests/unit/orderbook_branch_stress_test.cpp`

This means once your new type is added to factory, contract coverage includes it without manually editing test parameter lists.

## 7) New Orderbook Acceptance Checklist

Use this checklist before claiming a new implementation is "good enough":

- `--list_books` includes your new key in benchmark and server binaries
- unit suite passes: `ctest --test-dir build -L unit --output-on-failure`
- contract tests pass for your type (automatically included through factory registration)
- branch stress and factory tests are green
- direct benchmark runs for your type in at least `mixed` and `dense_full`
- no obvious regression versus baseline books for your target metric (latency or throughput)
- if network behavior is part of your claim, integration tests pass with `-DHFT_ENABLE_INTEGRATION_TESTS=ON`

## 8) Development Baseline and Portability Notes

- Day-to-day development baseline for this project has been macOS (correctness + coverage workflow).
- Linux/WSL2 is used for perf-heavy benchmarking and decomposition counters.
- If your goal is "is my new orderbook correct?", prefer unit tests first (`-L unit`).

## 9) Common Failure Modes

1. Added class but forgot factory registration
   - Symptom: `--book mybook` fails or `--list_books` does not show it.
2. Added factory key but forgot CMake source inclusion
   - Symptom: compile/link errors referencing missing symbols.
3. Contract mismatch (`modifyOrder`, best price semantics)
   - Symptom: contract tests fail under `OrderBookContractTest`.
4. Integration assumed in portable runs
   - Symptom: socket tests flaky on constrained environments.
   - Fix: run `-L unit` by default and `-L integration` explicitly when needed.
5. IDE shows `'gtest/gtest.h' file not found` but build/tests pass
   - Cause: editor indexing is not using the active CMake configure/compile database.
   - Reality check: this project fetches GoogleTest with `FetchContent` and compile commands include `_deps/googletest-src/googletest/include`.
   - Fix: re-run configure (`cmake -S . -B build`), make sure VS Code uses this workspace's `build/compile_commands.json`, then reload CMake Tools configuration.
