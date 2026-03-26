# Results Analysis (Canonical run: 2026-03-26)

Source dataset: `results/results.csv` generated via:

```bash
./scripts/run_direct_gateway_with_perf.sh --scenario all --runs 5 --orders 10000 --with-mpsc --mpsc-producers all
```

Dataset integrity check: direct=35, gateway=35, mpsc=140, total=210 rows.

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

- Net share: mean 94.2%, median 98.7%
- Queue share: mean 5.8%, median 1.2%
- Engine share: mean 0.040%, median 0.028%

Interpretation:

- For non-vector rows, end-to-end latency is overwhelmingly network-path dominated.
- Engine-internal differences are usually a small fraction of total gateway latency.

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
