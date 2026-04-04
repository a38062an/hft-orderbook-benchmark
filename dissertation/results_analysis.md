# Results Analysis (Canonical)

Source dataset: `results/results.csv` generated via:

```bash
./scripts/run_direct_gateway_with_perf.sh --scenario all --runs 5 --orders 10000 --with-mpsc --mpsc-producers all
```

Dataset integrity check: direct=35, gateway=35, mpsc=140, total=210 rows.

## 0) Scenario definitions and rationale

Each scenario represents a distinct market microstructure pattern observed in real HFT systems:

- **fixed_levels**: Orders concentrated on two price levels (bid/ask). Tests hot-path cache locality; representative of tight spreads with stable quotes.
- **tight_spread**: All orders within 2 ticks of midpoint. Extreme case of price concentration; tests best-case locality and in-order execution.
- **mixed**: Heterogeneous order placement (55% near touch, 30% deeper, 15% extreme) plus 18% cancellation. Real-world proxy for typical market conditions.
- **high_cancellation**: 40% cancellation rate with uniform price distribution. Tests cancellation-heavy market regimes (e.g., fleeting quotes, order management churn).
- **sparse_extreme**: Orders across wide price range (±1000 ticks). Tests worst-case tree traversal and memory footprint.
- **dense_full**: 100x more orders in same price range as mixed, all inserted before matching. Tests insertion volume, allocation behavior, and queue saturation.
- **worst_case_fifo**: Artificial adversarial case: orders inserted far from best bid/ask, matched in FIFO order. Tests sorting and tree rebalancing worst paths.

## 1) Direct-mode findings (algorithmic latency)

### Scenario winners (mean latency)

- dense_full: array, 213.44 ns
- fixed_levels: array, 207.77 ns
- high_cancellation: array, 210.01 ns
- mixed: array, 210.72 ns
- sparse_extreme: array, 210.95 ns
- tight_spread: hybrid, 340.69 ns
- worst_case_fifo: array, 207.41 ns

Interpretation:

- Array is strongest in six of seven direct scenarios.
- Tight_spread is the only direct scenario where hybrid is marginally faster than array.

### Mixed scenario ranking (direct)

- array: 210.72 ns
- map: 413.61 ns
- vector: 432.98 ns
- hybrid: 458.38 ns
- pool: 462.59 ns

Interpretation:

- In the representative mixed regime, array is about 1.96x faster than map and about 2.20x faster than pool.

### Dense-full degradation (vector)

- direct dense_full best (array): 213.44 ns
- direct dense_full vector: 16082.10 ns
- direct slowdown: 75.3x

Interpretation:

- The vector structure exhibits workload-sensitive collapse in dense insert-heavy conditions.

## 2) Gateway-mode findings (end-to-end)

### Winners by scenario (excluding vector)

- dense_full: pool, 1132979.97 ns
- fixed_levels: map, 948241.73 ns
- high_cancellation: map, 341738.08 ns
- mixed: hybrid, 1362459.91 ns
- sparse_extreme: pool, 1327070.91 ns
- tight_spread: pool, 958460.01 ns
- worst_case_fifo: pool, 805203.93 ns

Interpretation:

- No single non-vector implementation dominates all gateway scenarios.
- Pool is best in four scenarios, map in two, hybrid in one.

### Decomposition shares (non-vector gateway rows)

End-to-end latency decomposed into three components:

- **Network** (Net): client→server round-trip via TCP/IP stack. Mean 94.2%, median 98.7%.
- **Queue** (Que): order waiting in matching engine queue. Mean 5.8%, median 1.2%.
- **Engine** (Eng): matching engine processing (validate, insert, match). Mean 0.040%, median 0.028%.

Interpretation:

- For non-vector rows, end-to-end latency is overwhelmingly network-path dominated.
- Engine-internal differences are usually a small fraction of total gateway latency.
- Queue effects emerge in dense/stressful workloads but remain secondary except in outlier cases.

### Queue-amplification evidence

Largest queue components include:

- vector/dense_full: queue 18,403,401.73 ns, total 81,741,096.56 ns
- hybrid/dense_full: queue 2,197,671.70 ns, total 5,645,513.63 ns
- map/dense_full: queue 278,529.62 ns, total 1,506,790.29 ns

Interpretation:

- Vector dense_full is not just slower in engine time; it triggers severe queue backlog, creating an end-to-end failure mode.

## 3) Variability and statistical caution

High-variance flags were identified with coefficient of variation (CV) over 25%.
Examples:

- direct map/tight_spread: CV 52.6%
- direct map/fixed_levels: CV 44.6%
- gateway map/mixed: CV 26.2%

Implication:

- Claims should prioritize robust ranking patterns and large effect sizes.
- Avoid over-interpreting small pairwise differences where variance is high.

## 4) MPSC scaling stability check

Engine latency stability was evaluated per (book, scenario) as max/min over producer counts 1/2/4/8.

Stable examples:

- pool/sparse_extreme: ratio 1.007
- array/fixed_levels: ratio 1.007
- map/fixed_levels: ratio 1.011

More variable examples:

- array/worst_case_fifo: ratio 1.336
- array/high_cancellation: ratio 1.161

Interpretation:

- Many MPSC cases show stable engine latency as producer count increases.
- A few workload-specific cases show larger variation and should be discussed as stress sensitivity.

## 5) Scientifically safe claims for dissertation text

Use:

- "Under this pinned-run dataset, array is the direct-mode winner in six of seven scenarios."
- "Gateway mode is dominated by Net latency for non-vector rows, with median Net share near 98.7%."
- "Vector in dense_full demonstrates a queue-amplification failure mode, reaching 81.7 ms end-to-end mean latency."

Avoid:

- universal claims that one implementation always wins in gateway mode
- absolute cross-machine claims from this one environment
- legacy references to old gateway values from prior datasets

## 6) Mechanism validation addendum (cache + allocator proxy)

Targeted `perf stat` measurements were run in direct mode for:

- books: `array`, `map`, `vector`
- scenarios: `mixed`, `dense_full`
- command baseline: `--runs 10 --orders 10000`
- perf binary: `/usr/lib/linux-tools-6.8.0-106/perf` (WSL wrapper `/usr/bin/perf` was not usable)

Key measured values:

- mixed / array: cache-miss ratio 14.47%, L1-dcache-load-misses 861,456, task-clock 77.28 ms
- mixed / map: cache-miss ratio 12.06%, L1-dcache-load-misses 1,407,790, task-clock 96.05 ms
- mixed / vector: cache-miss ratio 11.41%, L1-dcache-load-misses 1,397,536, task-clock 100.62 ms
- dense_full / array: cache-miss ratio 14.60%, L1-dcache-load-misses 674,099, task-clock 73.75 ms
- dense_full / map: cache-miss ratio 6.38%, L1-dcache-load-misses 1,188,878, task-clock 155.48 ms
- dense_full / vector: cache-miss ratio 1.25%, L1-dcache-load-misses 148,441,143, task-clock 1,919.02 ms
- mixed / pool: cache-miss ratio 6.40%, L1-dcache-load-misses 6,189,447, page-faults 22,677, task-clock 196.62 ms
- dense_full / pool: cache-miss ratio 3.14%, L1-dcache-load-misses 5,745,192, page-faults 22,662, task-clock 231.28 ms

Interpretation for viva safety:

- Cache-miss *ratio* alone does not explain latency ranking.
- In dense_full, vector has very low generic miss ratio but catastrophic absolute L1 misses and task-clock inflation.
- A safer claim is: data-structure execution path dominates latency, with cache behavior as a contributing mechanism rather than a sole cause.
- Allocator causality is still not isolated by page-fault proxies alone; dedicated allocation instrumentation remains required for strong H2 confirmation.
- In these proxy runs, pool does not show lower paging pressure than map (it is substantially higher), so H2 must be framed as conditional and implementation-specific rather than universally true.

### WSL2-derived cache hit-rate evidence

Using `L1-dcache-loads` and `L1-dcache-load-misses`, an L1 data-cache hit rate was derived per case:

- formula: `L1_hit_rate = 1 - (L1-dcache-load-misses / L1-dcache-loads)`
- summary file: `results/perf/validation_real/cache_hit_summary.csv`

Observed L1 hit rates:

- array/mixed: 99.744%
- map/mixed: 99.670%
- vector/mixed: 99.685%
- pool/mixed: 98.919%
- array/dense_full: 99.790%
- map/dense_full: 99.830%
- vector/dense_full: 98.378%
- pool/dense_full: 99.321%

Interpretation:

- Yes, cache hit-rate evidence is measurable on this WSL2 host for L1 data cache.
- LLC-specific hit-rate analysis is still limited here because `LLC-loads`/`LLC-load-misses` were reported as unsupported.
- L1 hit-rate differences are informative but do not fully explain runtime gaps alone; task-clock and absolute miss volume remain necessary companion evidence.

### L2 counter feasibility on this WSL2 host

L2 request counters were probed and collected successfully using:

- events: `l2_cache_req_stat.all`, `l2_cache_req_stat.dc_hit_in_l2`, `l2_cache_req_stat.ic_dc_miss_in_l2`
- summary artifact: `results/perf/validation_real/l2_summary.csv`

Representative values:

- array/mixed: l2_dc_hit_ratio 0.52408
- map/mixed: l2_dc_hit_ratio 0.50700
- pool/mixed: l2_dc_hit_ratio 0.46122
- vector/dense_full: l2_dc_hit_ratio 0.87917 (with very high task-clock at 2101.00 ms)

Interpretation:

- L2 counters are available and measurable on this host.
- These L2 ratios should be interpreted as request-hit proportions for the selected event definitions, not a full architectural "L2 hit rate" across all access classes.
- LLC-level conclusions still remain limited due to unsupported LLC events.

### Measurement provenance (for reproducibility)

- Host file: `results/perf/validation_real/host_provenance.txt`
- Kernel: Linux 6.6.87.2-microsoft-standard-WSL2
- Distro: Ubuntu 24.04.2 LTS
- CPU: AMD Ryzen 5 7600X (12 logical CPUs)
- Perf binary: `/usr/lib/linux-tools-6.8.0-106/perf` (`/usr/bin/perf` wrapper was kernel-mismatched)
- Benchmark command family: `./build/benchmarks/orderbook_benchmark --mode direct --book <book> --scenario <scenario> --runs 10 --orders 10000`

### Viva-ready paragraph (8 lines)

On this WSL2 host, cache behavior was measured with Linux perf using direct-mode runs (`--runs 10 --orders 10000`) across `array`, `map`, `vector`, and `pool` for `mixed` and `dense_full`. L1 data-cache hit rates were derived from `L1-dcache-loads` and `L1-dcache-load-misses`, and all case-level outputs were summarized in `results/perf/validation_real/cache_hit_summary.csv`. L2 request counters were also measurable (`l2_cache_req_stat.*`) and summarized in `results/perf/validation_real/l2_summary.csv`. LLC-specific counters (`LLC-loads`, `LLC-load-misses`) were not supported on this machine, so LLC hit-rate claims were intentionally excluded. The evidence therefore supports bounded mechanistic statements: L1/L2 behavior can be discussed with measured data, but full last-level-cache conclusions cannot. In dense_full, vector shows extreme task-clock inflation and very high absolute L1 miss volume, consistent with a stressed memory-access path. These results are reported as within-environment evidence (Linux WSL2, Ubuntu 24.04.2, kernel 6.6.87.2, perf 6.8.12), not universal cross-platform constants. This framing is deliberately conservative for viva-level scientific validity.
