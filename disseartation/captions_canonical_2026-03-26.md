# Thesis Figure Captions (Evidence-Anchored, Canonical Run 2026-03-26)

Canonical dataset:

- Source file: results/results.csv
- Generation command: ./scripts/run_direct_gateway_with_perf.sh --scenario all --runs 5 --orders 10000 --with-mpsc --mpsc-producers all
- Integrity: direct=35, gateway=35, mpsc=140, total=210 rows

Use these captions directly in the dissertation and keep the evidence anchors in notes.

## Evidence anchors used throughout

- Direct winners by scenario: plots/tables/rank_direct_vs_gateway.md
- Gateway winners by scenario: plots/tables/rank_direct_vs_gateway.md
- Mid-tier winners only: plots/tables/mid_tier_winners_no_array.md
- Operation winners (insert/cancel/lookup): plots/tables/operation_winners_direct.md
- Quantitative synthesis and variability notes: disseartation/results_analysis_2026-03-26.md

## Figure captions and backing evidence

### plots/direct/latency_mean_heatmap.png

Caption:
Direct-mode mean latency heatmap across scenarios and order book implementations. Array is the fastest implementation in six of seven scenarios, while hybrid is narrowly fastest in tight_spread.

Evidence:

- array best in fixed_levels (207.77 ns), mixed (210.72 ns), high_cancellation (210.01 ns), worst_case_fifo (207.41 ns), sparse_extreme (210.95 ns), dense_full (213.44 ns)
- hybrid best in tight_spread (340.69 ns)

### plots/direct/latency_slowdown_vs_best_heatmap.png

Caption:
Direct-mode slowdown relative to the scenario winner. Dense_full shows a pronounced degradation for vector, while most non-vector implementations remain within a narrower slowdown band.

Evidence:

- dense_full best is array at 213.44 ns
- dense_full vector is 16082.10 ns, approximately 75.3x slower than best

### plots/direct/tail_p99_over_mean.png

Caption:
Tail inflation map using P99-to-mean ratio in direct mode. Several map and vector cases exhibit elevated tail amplification relative to mean latency, indicating sensitivity to jitter and/or burst conditions.

Evidence:

- direct map tight_spread has high variance (CV 52.6%)
- direct map fixed_levels has high variance (CV 44.6%)
- see variability section in disseartation/results_analysis_2026-03-26.md

### plots/direct/operation_latency_breakdown_selected_scenarios.png

Caption:
Per-operation latency decomposition in direct mode for selected scenarios. Insert and lookup are consistently lowest for array, and cancellation advantage is visible in cancellation-heavy workloads.

Evidence:

- array has best insert and lookup in all listed scenarios in plots/tables/operation_winners_direct.md
- mixed cancel: array 46.04 ns (best)
- high_cancellation cancel: array 44.79 ns (best)

### plots/direct/middle_tier_slowdown_heatmap.png

Caption:
Relative slowdown among middle-tier implementations (map, hybrid, pool), normalized to the best middle-tier book per scenario. The winner changes by scenario, indicating workload-specific trade-offs rather than a universal middle-tier leader.

Evidence:

- direct mid-tier winners: hybrid (tight_spread), map (fixed_levels, mixed, high_cancellation, sparse_extreme, dense_full), pool (worst_case_fifo)
- source: plots/tables/mid_tier_winners_no_array.md

### plots/direct/latency_stddev_no_vector_heatmap.png

Caption:
Direct-mode standard deviation heatmap excluding vector outliers to expose variability among map, hybrid, pool, and array. Variability is scenario dependent and should constrain claims based on small mean differences.

Evidence:

- direct map tight_spread stddev 315.33 ns with mean 598.97 ns
- direct map fixed_levels stddev 149.81 ns with mean 335.78 ns

### plots/gateway/latency_by_scenario.png

Caption:
Gateway end-to-end mean latency by scenario and implementation. Vector in dense_full is an extreme outlier, demonstrating queue-amplification failure under sustained dense workload.

Evidence:

- gateway dense_full vector: 81,741,096.56 ns
- next-best dense_full non-vector winner (pool): 1,132,979.97 ns
- slowdown: approximately 72.1x

### plots/gateway/latency_by_scenario_no_vector.png

Caption:
Gateway end-to-end latency with vector excluded for comparative readability. No single non-vector implementation dominates all scenarios.

Evidence:

- pool wins dense_full, sparse_extreme, tight_spread, worst_case_fifo
- map wins fixed_levels and high_cancellation
- hybrid wins mixed
- source: plots/tables/rank_direct_vs_gateway.md

### plots/gateway/latency_slowdown_no_vector_heatmap.png

Caption:
Gateway slowdown heatmap excluding vector, normalized by the best non-vector implementation per scenario. Slowdown remains scenario-dependent and reflects combined transport, queueing, and engine effects.

Evidence:

- high_cancellation has large spread between map winner (341,738.08 ns) and pool (1,040,188.24 ns)
- worst_case_fifo has pool winner (805,203.93 ns) versus map (1,573,891.12 ns)

### plots/gateway/dense_full_latency_decomposition_no_vector.png

Caption:
Dense_full gateway decomposition (network, queue, engine) for non-vector implementations. Queue contribution increases under dense conditions, but network remains the largest component for non-vector rows.

Evidence:

- map dense_full: network 1,227,340.24 ns, queue 278,529.62 ns, engine 920.43 ns
- pool dense_full: network 947,795.49 ns, queue 184,272.94 ns, engine 911.54 ns
- hybrid dense_full: network 3,446,063.77 ns, queue 2,197,671.70 ns, engine 1,778.16 ns

### plots/gateway/latency_stddev_no_vector_heatmap.png

Caption:
Gateway-mode standard deviation heatmap excluding vector. Variability is non-negligible in selected scenarios, supporting conservative interpretation of close mean values.

Evidence:

- gateway map mixed: mean 2,159,070.98 ns, stddev 565,998.96 ns (CV 26.2%)
- gateway hybrid high_cancellation: mean 1,419,803.93 ns, stddev 364,140.62 ns (CV 25.6%)

### plots/direct_vs_gateway/middle_tier_slowdown_direct_vs_gateway.png

Caption:
Middle-tier slowdown comparison between direct and gateway modes. Direct mode shows clearer algorithmic separation; gateway mode compresses many differences due to dominant transport cost, except in queue-amplifying stress cases.

Evidence:

- non-vector gateway decomposition medians: Net share 98.7%, Queue share 1.2%, Engine share 0.028%
- source: disseartation/results_analysis_2026-03-26.md

### plots/mpsc/throughput_scaling.png

Caption:
MPSC throughput scaling across producer counts and implementations. Relative throughput hierarchy remains implementation-dependent, with array consistently in the highest tier and vector depressed in dense scenarios.

Evidence:

- dense_full vector throughput around 80k orders/s range
- dense_full array throughput around 3.3M to 4.1M orders/s range
- source: results/results.csv (mode=mpsc)

### plots/mpsc/queue_vs_engine_scaling.png

Caption:
MPSC queue latency versus engine latency across producer counts. In most book/scenario pairs engine latency remains relatively stable while queue latency captures contention effects.

Evidence:

- stable engine examples: pool/sparse_extreme ratio 1.007, array/fixed_levels ratio 1.007
- variable stress example: array/worst_case_fifo ratio 1.336
- source: disseartation/results_analysis_2026-03-26.md

### plots/scenario_validity/mixed_profile.png

Caption:
Mixed-scenario profile validation plot, showing mixed behavior rather than collapse into a single synthetic regime. The scenario remains suitable as a representative benchmark baseline.

Evidence:

- mixed direct ranking preserves meaningful spread across implementations (array 210.72 ns to pool 462.59 ns)
- mixed gateway winner differs from direct winner (hybrid in gateway, array in direct), confirming cross-mode behavior differences

## Scientific writing guardrails for caption usage

- Keep caption claims descriptive and bounded to this dataset.
- Use wording like "in this run" or "under this benchmark configuration".
- Do not extrapolate absolute latency values across different hardware/OS configurations.
- Where differences are small and variance is high, avoid strong causal language in captions and defer causality to body text.

## Gateway Mode Interpretation Notes

**Scenario-dependent winners despite network dominance:**

Although gateway-mode end-to-end latency is 98.7% network-dominated (median), the winning implementation still changes by scenario (pool wins dense_full, map wins fixed_levels, hybrid wins mixed, etc.). This appears paradoxical: if network is identical across all books, why do winners differ?

Answer: Queue and Engine components are scenario-dependent. When insertion volume increases (dense_full) or order placement becomes adversarial (worst_case_fifo), the queue response and engine processing time vary by implementation. These components remain ~1% of total latency, but they shift which implementation is fastest because the margins are narrow at the microsecond scale. In other words, the network tax is constant, but the algo-specific queue+engine cost is what determines ranking.

**Implication for dissertation narrative:** Gateway results validate that engine-internal efficiency still matters even when network dominates, but only at the margin. The practical takeaway is not "all implementations are equally good in gateway" but rather "network overhead dwarfs algorithmic differences, so deployment decisions should prioritize infrastructure (network, codec) over order book choice."
