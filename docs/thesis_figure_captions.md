# Thesis Figure Captions and Prose

## Mixed Scenario Definition

### Methods Text

The mixed scenario was revised to represent a genuinely heterogeneous market regime rather than a duplicate of the tight-spread case. Specifically, the generator now combines three price-placement regimes within a single stream: 55% of orders are concentrated near the touch, 30% populate deeper resting liquidity levels, and 15% are placed at more extreme prices. In addition, moderate cancellation pressure is introduced through an 18% cancellation branch, producing a workload that is structurally distinct from both the dedicated high-cancellation scenario and the tightly clustered price-discovery cases. This makes the mixed scenario a more credible proxy for practical order flow in which concentrated quoting, deeper passive liquidity, and occasional outlier placements coexist.

## Figure Captions

### Mechanistic: direct/middle_tier_slowdown_heatmap.png

Direct-mode relative slowdown among the middle-tier data structures (`hybrid`, `pool`, and `map`), normalized to the best middle-tier implementation within each scenario. The figure shows that no single middle-tier design dominates across all workloads: `pool` leads in `dense_full`, `sparse_extreme`, and `worst_case_fifo`, `hybrid` leads in `fixed_levels`, `mixed`, and `tight_spread`, while `map` narrowly leads `high_cancellation`.

### Mechanistic: direct_vs_gateway/middle_tier_slowdown_direct_vs_gateway.png

Comparison of middle-tier relative slowdown in direct mode versus gateway mode. Each panel normalizes `hybrid`, `pool`, and `map` to the best middle-tier implementation in that scenario, allowing the extent to which algorithmic differences survive the system path to be compared directly. The direct panel exposes clear workload-dependent trade-offs, while the gateway panel shows that many of these differences compress once network and queueing effects are introduced.

### Mechanistic: gateway/latency_by_scenario_no_vector.png

Gateway-mode end-to-end latency across scenarios for `array`, `hybrid`, `pool`, and `map`, excluding the `vector` implementation so that relative differences between the remaining books remain visible. This figure is intended for comparative interpretation rather than outlier emphasis.

### Mechanistic: gateway/latency_slowdown_no_vector_heatmap.png

Gateway-mode relative slowdown across scenarios with `vector` excluded and each row normalized to the best no-vector implementation. This highlights where the systemic path preserves meaningful implementation differences and where the networked path compresses them.

### Mechanistic: gateway/dense_full_latency_decomposition_no_vector.png

Gateway-mode end-to-end latency decomposition for the `dense_full` scenario, excluding the `vector` implementation. The figure separates network, queue, engine, and residual components so that the relative contribution of queue amplification and engine cost can be assessed without the scale distortion introduced by the vector outlier.

## Mechanistic Reasoning (Why These Patterns Occur)

### direct/middle_tier_slowdown_heatmap.png

The middle-tier winners change because each structure optimizes a different hot path. `hybrid` benefits when activity is concentrated near top-of-book because hot vectors reduce repeated ordered-map traversals for frequently touched levels. `pool` tends to improve scenarios with heavier queue/FIFO pressure because intrusive preallocated nodes remove repeated allocator overhead for order insertion/removal. `map` remains competitive when consistent ordered traversal dominates and the extra hot/cold bookkeeping of `hybrid` does not amortize.

### direct_vs_gateway/middle_tier_slowdown_direct_vs_gateway.png

The compression effect is expected mechanically: direct mode isolates in-book algorithmic work, whereas gateway mode introduces network parsing, scheduling variance, and queue wait. These systemic components add a large common cost floor, so algorithmic gaps that are visible in direct mode appear reduced in gateway mode unless one implementation triggers queue amplification.

### gateway/latency_by_scenario_no_vector.png

Removing `vector` reveals the relative shape because the remaining implementations are closer in end-to-end magnitude. The observed reordering by scenario is consistent with workload-structure fit: scenarios with concentrated price interaction reward hot-path locality (`hybrid`), while broader or churn-heavy interaction can favor stable ordered traversal (`map`) or pool-managed node reuse (`pool`).

### gateway/latency_slowdown_no_vector_heatmap.png

The slowdown heatmap exposes where systemic overhead is scenario-selective rather than purely constant. High slowdown cells indicate that queue behavior and pacing effects are interacting with a structure’s update/match mechanics, not just with raw insert latency. In other words, the heatmap captures mismatch between workload shape and data-structure execution path under full pipeline conditions.

### gateway/dense_full_latency_decomposition_no_vector.png

The decomposition is the strongest mechanical evidence figure because it separates where latency is spent. If differences are mostly in `Network`, the bottleneck is outside the order book. If differences concentrate in `Queue`, the implementation is affecting service rate or burst handling. If `Engine` differs materially, the core matching and book-update path is the source. This allows claims about structure-specific effects without conflating them with transport overhead.

## Results Prose

### Middle-Tier Trade-offs

The middle-tier comparison reinforces the thesis argument that order book choice is scenario-dependent rather than globally rankable once the extreme endpoints are removed. In direct mode, `hybrid` is best in `fixed_levels` (103.39 ns), `mixed` (98.89 ns), and `tight_spread` (126.95 ns), which is consistent with its design assumption that a small number of hot levels near the spread can be served efficiently through the vector fast path. By contrast, `pool` is best in `dense_full` (154.02 ns), `sparse_extreme` (125.09 ns), and `worst_case_fifo` (136.03 ns), suggesting that once the workload either broadens across price space or places sustained pressure on per-order queue management, preallocated intrusive storage becomes more valuable than hot/cold partitioning. The `map` implementation only narrowly leads in `high_cancellation` (117.10 ns), where the middle tier is effectively compressed and all three designs perform similarly.

### Direct Versus Gateway Compression

The direct-versus-gateway comparison shows that algorithmic separation does not automatically translate into systemic separation. In direct mode, the middle-tier spread is material: for example, `hybrid` beats `pool` by roughly 13.7% in `mixed`, while `pool` beats `hybrid` by roughly 35.2% in `dense_full`. In gateway mode, these differences frequently compress or reorder. For instance, `hybrid` remains best in `mixed` (366,988.68 ns), but the gap to `array` is only about 2.5%, and the gap to `pool` is about 11.1%. Likewise, in `high_cancellation`, `hybrid` leads at 247,254.16 ns, but `map` follows at only 1.24x slowdown. This supports the interpretation that direct-mode latency is necessary to explain mechanism-level behavior, whereas gateway-mode latency is required to judge deployment relevance.

### Gateway End-to-End Latency Without Vector

Removing `vector` from the gateway end-to-end comparison exposes a more informative relative ordering among the remaining implementations. The refreshed results show that the best no-vector implementation varies substantially by workload: `map` leads `dense_full` (265,689.60 ns) and `fixed_levels` (375,910.44 ns), `hybrid` leads `high_cancellation` (247,254.16 ns), `mixed` (366,988.68 ns), and `worst_case_fifo` (285,011.51 ns), while `pool` leads `tight_spread` (376,865.81 ns). `array` becomes the best choice in `sparse_extreme` (226,784.69 ns). This supports a stronger chapter claim than a single-winner framing would allow: once the gateway path is included, the best implementation depends on how the workload interacts with queueing, network pacing, and engine behavior simultaneously.

### Gateway Relative Slowdown Without Vector

The no-vector gateway slowdown heatmap is useful because it converts visually similar microsecond-scale lines into explicit scenario-relative penalties. Two cases are particularly important. First, `sparse_extreme` is highly discriminative even in gateway mode: `array` is best, while `hybrid` degrades to 2.34x slowdown and `pool` to 2.16x slowdown. Second, `worst_case_fifo` preserves a meaningful distinction among the no-vector books, with `hybrid` best, `pool` at 1.41x slowdown, `array` at 1.60x, and `map` at 2.23x. In contrast, `mixed` is far more compressed, where `array` is 1.02x behind `hybrid`, `pool` is 1.11x behind, and `map` is 1.20x behind. This pattern is exactly the type of evidence that supports a scenario-dependent trade-off narrative.

### Dense Full Decomposition Without Vector

The updated dense-full decomposition clarifies that, once the `vector` outlier is removed, the principal difference among the remaining implementations is not engine time alone but the interaction between network and queue delay. In the refreshed gateway results, `map` is the best no-vector implementation in `dense_full` at 265,689.60 ns, while `pool` and `array` are both around 410,000 ns and `hybrid` reaches 494,307.99 ns. The decomposition allows the reader to see whether these gaps arise from a queue build-up, higher engine cost, or a combination of the two, which is more analytically useful than treating gateway latency as a single opaque scalar.

## Short Discussion Paragraph

Taken together, the new figures strengthen the dissertation argument in two ways. First, they make the middle-tier story legible without the dominance of the array/vector endpoints. Second, they show that the relative ordering observed in direct mode only partially survives the gateway path, which is exactly the kind of result that justifies separating algorithmic and systemic benchmarking rather than collapsing them into one ranking.
