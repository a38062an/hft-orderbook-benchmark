# Dissertation Alignment Notes (Current Project State)

## 1) Outdated Weakness Corrections

Replace any claim that tests are "commented out" with:

- Unit tests are actively discovered via GoogleTest/CTest.
- Integration tests are conditionally enabled by `HFT_ENABLE_INTEGRATION_TESTS`.
- Coverage workflow exists and is reproducible for supported toolchains.

Use this language in limitations instead:

- Current limitation is not absence of tests; the limitation is breadth of scenario-level correctness evidence and cross-platform timing stability.

## 2) Tightened Hypotheses (Distinct Acceptance Criteria)

### H1 (Ordering structure + locality)

Cache-local direct indexing yields materially lower direct-mode latency than tree and sorted-vector structures under multi-scenario workload evaluation.

Accept if:

- array outperforms map and vector in mean direct latency across most scenarios
- dense_full gap remains order-of-magnitude vs vector

### H2 (Allocation strategy in isolation)

Replacing general heap allocation with a pool improves latency only when allocation/deallocation overhead is a non-trivial fraction of total operation cost.

Accept if:

- pool improves over map in selected scenarios
- pool does not close the gap where tree traversal dominates (for example dense books)

### H3 (System-level masking)

In gateway mode, network and queue components dominate the end-to-end budget, reducing practical impact of engine-internal data structure differences except under queue-amplification failure modes.

Accept if:

- Net + Que dominates Eng in most gateway rows
- vector dense_full remains a visible exception due to queue amplification

## 3) Hardware Claim Bounding Text

Use once in Chapter 2 and reference in Chapter 5:

"Cycle and nanosecond examples in this report are machine-specific approximations derived from the test environment and are intended as explanatory order-of-magnitude guides, not universal constants across CPU microarchitectures, compilers, or operating systems."

Then in Chapter 5:

"Interpretation of absolute values follows the Chapter 2 caveat: relative differences within the same environment are stronger evidence than cross-machine absolute comparisons."

## 4) One-Month Dissertation Structure (After Code Freeze)

### Week 1 (immediately after submission freeze)

- Lock canonical dataset and regenerate all figures
- Write Chapter 5 first (results and interpretation)
- Draft hypothesis verdicts in Chapter 6

### Week 2

- Write Chapter 2 with explicit hardware-to-results mapping
- Add related work and citation density

### Week 3

- Write Chapters 3 and 4 tightly (design rationale, not code narration)
- Add functional and non-functional requirements table

### Week 4

- Finalize Chapter 1 and abstract
- Consistency pass: all claims map to figure/table and CSV values
- Final language/style pass and bibliography cleanup
