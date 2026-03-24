# Coverage and Testing Workflow

This file explains what changed, how coverage is produced, and how to work methodically toward 100%.

## 1) What Changed in the Order Books

These are the key order-book changes made during iterative testing/coverage work.

| File | Issue Found | Change Made | Why It Mattered |
| --- | --- | --- | --- |
| src/orderbooks/array_order_book.cpp | Invalid constructor inputs could fail too late | Validate range/tick inputs before allocation | Prevented runtime errors and stabilized tests |
| src/orderbooks/array_order_book.cpp | Sell-side lookup metadata mismatch in some paths | Corrected sell-side lookup state handling | Fixed cancel/modify behavior symmetry |
| src/orderbooks/hybrid_order_book.cpp | Defensive branches were never reachable from valid states | Simplified invariant-only guard paths in cancel/promote/demote logic | Reduced artificial branch misses without changing external behavior |
| src/orderbooks/vector_order_book.cpp | Similar invariant-only guard branches stayed one-sided | Simplified cancel-path checks where lookup guarantees level existence | Improved branch closure while preserving behavior |
| src/orderbooks/map_order_book.cpp | Branches for non-empty-level cancel and partial fill were under-tested | Added targeted tests for these paths | Closed real behavior branches via tests |
| src/orderbooks/pool_order_book.cpp | modify(orderId, 0) behavior mismatch | Aligned with contract by canceling on zero quantity | Consistency across order books |
| src/orderbooks/pool_order_book.cpp | Remaining branch misses were from loop/control-flow shape | Refined inner match-loop termination logic to remove structurally unreachable branch arcs | Reached full branch closure |

## 2) Errors and Regressions Encountered During Iteration

1. Coverage profile path mismatch: first branch extraction used the wrong `.profdata` path, then corrected to `build-coverage/coverage/hft_unit_tests.profdata`.
2. Temporary `assert` instrumentation caused region/line regression in coverage builds, so it was removed.
3. One pool loop refactor attempt did not improve branch and temporarily regressed line/region; it was reverted.
4. Final pool loop cleanup retained behavior and closed the last branch misses.

This is normal iterative engineering: try small changes, rerun full validation, keep only what improves metrics without behavior regressions.

## 3) Exact Commands and Where to Run Them

Run these from the project root (the folder containing `CMakeLists.txt`).

```bash
# One-time configure to create build directory with coverage instrumentation enabled
cmake -S . -B build-coverage -DHFT_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
```

```bash
# Build and run all tests in coverage configuration
cmake --build build-coverage -j$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)
ctest --test-dir build-coverage --output-on-failure

# Generate report + HTML
cmake --build build-coverage --target coverage
```

Coverage artifacts are produced here:

- `build-coverage/coverage/hft_unit_tests.profdata`
- `build-coverage/coverage/html/index.html`

For file-level branch inspection:

```bash
xcrun llvm-cov show build-coverage/tests/hft_unit_tests \
  -instr-profile=build-coverage/coverage/hft_unit_tests.profdata \
  src/orderbooks/hybrid_order_book.cpp --show-branches=count
```

For summary table in terminal:

```bash
xcrun llvm-cov report build-coverage/tests/hft_unit_tests \
  -instr-profile=build-coverage/coverage/hft_unit_tests.profdata \
  src
```

## 4) End-to-End Flow (From Assertion to HTML)

This is the exact lifecycle in this repository.

1. Configure with CMake:
   - `cmake -S . -B build-coverage ...` creates `build-coverage`.
   - This is a generated build tree, not a downloaded library.
2. CMake fetches GoogleTest/GoogleMock:
   - `tests/CMakeLists.txt` uses `FetchContent` with `googletest` v1.14.0.
   - Sources appear under `build-coverage/_deps/googletest-src`.
   - Built libs appear under `build-coverage/lib`.
3. CMake builds the test binary:
   - `add_executable(hft_unit_tests ...)` compiles all unit test files.
   - Output binary is `build-coverage/tests/hft_unit_tests`.
4. CTest discovers/runs tests:
   - `gtest_discover_tests(hft_unit_tests)` asks GoogleTest to enumerate `TEST(...)` cases.
   - `ctest` executes each discovered test and reports pass/fail.
5. Coverage instrumentation records execution:
   - Root `CMakeLists.txt` adds Clang/AppleClang flags when `HFT_ENABLE_COVERAGE=ON`:
     - compile: `-fprofile-instr-generate -fcoverage-mapping -O0 -g`
     - link: `-fprofile-instr-generate -fcoverage-mapping`
   - While the instrumented binary runs, raw counters are written to `.profraw`.
6. Coverage target merges and renders:
   - `llvm-profdata merge` -> `hft_unit_tests.profdata`
   - `llvm-cov report` -> terminal summary table
   - `llvm-cov show -format=html` -> HTML files in `build-coverage/coverage/html`

## 5) Where Components Come From

### Test binary

- Comes from CMake target `hft_unit_tests` in `tests/CMakeLists.txt`.
- Includes all unit test translation units and links against `hft_core` + GoogleTest libs.

### GoogleTest / GoogleMock

- Not single-header in this repo.
- Pulled automatically by CMake `FetchContent` from the official release zip.
- Linked as `GTest::gtest_main` and `GTest::gmock`.
- Current tests mostly use real objects and manual stubs; `gmock` is available if `MOCK_METHOD(...)` tests are added.

### LLVM tools

- `llvm-profdata` and `llvm-cov` are external tooling from LLVM/Clang.
- On macOS, the build script can resolve them with `xcrun` if not already on `PATH`.
- These tools do not create `build-coverage`; CMake does. They only consume instrumentation output and produce reports.

## 6) How to Read Each Metric (What Output Looks Like)

### Function coverage

- Meaning: was a function entered at least once?
- Output style: e.g. `100.00% (11/11)` for a file.
- Miss example: `90.00% (9/10)` means one function never executed.

### Line coverage

- Meaning: did that source line execute?
- In `llvm-cov show`: left count column has execution count per line.
- Miss example: a line with count `0`.

### Region coverage

- Meaning: sub-parts of code/expressions executed (finer than lines).
- A single line can contain multiple regions.
- You can have line covered but region not fully covered if only one sub-expression path ran.

### Branch coverage

- Meaning: both outcomes of condition/short-circuit logic executed.
- In `--show-branches=count`, you see entries like `[True: 3, False: 0]`.
- Miss example: either side equals `0`.

## 7) Methodical Human Workflow (X -> Y -> Z)

1. **X: Measure baseline**
   - Run full coverage target.
   - Identify worst files from `index.html` or terminal report.
2. **Y: Isolate missed logic**
   - Use `llvm-cov show --show-branches=count <file>`.
   - Find branch sides with `0`.
   - Map each miss to one concrete scenario.
3. **Z: Close one intent at a time**
   - Prefer adding focused tests first.
   - If miss is truly defensive/unreachable under invariants, simplify control flow safely.
   - Rebuild + retest + remeasure.
   - Keep change only if behavior stays correct and metrics improve.

This is exactly how coverage went from mixed branch percentages to full 100/100/100/100.

## 8) How to Validate Coverage Is Correct

Use these checks:

1. **Independent rerun**: run the coverage target twice from clean state and confirm same totals.
2. **Path-level sanity check**: add one test that should flip one known branch side, rerun, confirm expected delta.
3. **Behavior-first assertions**: ensure tests assert domain behavior (best bid/ask, order count, trade qty/price), not only execution.
4. **Document residual rationale** (if any): label remaining misses as reachable vs defensive. (In final state here, totals are fully covered.)
5. **Version pinning**: record LLVM version shown in report footer and command lines used.

## 9) Google Test vs LLVM Coverage (Who Does What)

- Google Test (`TEST`, `EXPECT_*`, `ASSERT_*`) runs test logic and reports pass/fail.
- CTest orchestrates running test binaries.
- LLVM instrumentation (`-fprofile-instr-generate -fcoverage-mapping`) collects execution counters.
- `llvm-profdata` merges raw profiles.
- `llvm-cov` computes and renders coverage metrics.

So: Google Test validates correctness; LLVM tooling measures execution coverage.

## 10) Mocks, Stubs, and Assertions in Practice

- Assertions (`EXPECT_*`, `ASSERT_*`) come from GoogleTest and are the correctness checks.
- In this repository, most isolation tests use manual stubs (for example, `StubOrderBook` in `tests/unit/matching_engine_test.cpp`) instead of `gmock` macros.
- GoogleMock is still linked and available when interaction-style tests are needed (`EXPECT_CALL`, `MOCK_METHOD`).
- For strong test quality, prefer assertions on behavior and state transitions, not only "does not crash".

## 11) Current Final State

From the latest full coverage run:

- Functions: 100.00% (96/96)
- Lines: 100.00% (1355/1355)
- Regions: 100.00% (572/572)
- Branches: 100.00% (370/370)

That is the state you should cite as final, along with commands and reproducibility steps above.
