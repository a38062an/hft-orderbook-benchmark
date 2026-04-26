# MARKING REPORT
### Third Year Project Assessment
**High-Frequency Trading Order Book Benchmarking in C++20**
*Anthony Nguyen | Supervisor: Steve Pettifer | April 2026*

---

## Overall Grade Summary

| Component                                             | Score / 100 | Band                        |
| ----------------------------------------------------- | ----------- | --------------------------- |
| 3.1 Abstract & Introduction (15%)                     | 82          | Excellent                   |
| 3.2 Background & Theory (25%)                         | 78          | Above Expectations          |
| 3.3 Technical Quality, Methodology & Evaluation (35%) | 83          | Excellent                   |
| 3.4 Summary & Conclusions (15%)                       | 79          | Above Expectations          |
| 3.5 Presentation, Structure & Language (10%)          | 84          | Excellent                   |
| **REPORT OVERALL (weighted)**                         | **~81**     | **Excellent / First Class** |

> **Note:** The Screencast and Achievements components (15% and 30% of overall module grade respectively) are not assessed here as they require a live demonstration and video submission. This analysis covers the Report component only (55% of total module grade).
>
> **Weighted calculation:** (82 × 0.15) + (78 × 0.25) + (83 × 0.35) + (79 × 0.15) + (84 × 0.10) = 12.30 + 19.50 + 29.05 + 11.85 + 8.40 = **81.1**

---

## 3.1 Abstract and Introduction — 82 / 100 (Excellent)

> *Excellent — comprehensive, evidence-grounded, with a clear evaluation strategy*

### Strengths

- **The abstract is genuinely excellent.** It names all five implementations, states the three hypotheses clearly (H1/H2/H3), specifies the statistical methodology (paired sign-flip permutation tests, Holm-Bonferroni correction, 95% bootstrap CIs, n=30), and closes with a precise quantitative finding: the array achieves 214 ns mean latency in direct mode, while gateway transport consumes 88–96% of end-to-end latency. This maps directly to the Outstanding band descriptor.
- The introduction is well-structured and logically sequenced. It opens with domain motivation grounded in Aldridge (2013) and Bilokon & Gunduz (2023), moves cleanly to a problem statement, and then derives four specific, testable objectives.
- The three research hypotheses (H1 cache locality, H2 pooling limits, H3 masking) are clearly stated with operational acceptance criteria defined — e.g., H3 is accepted when transport accounts for >85% of end-to-end latency. This satisfies the mark scheme requirement for a "summary of evaluation strategy."
- The evaluation strategy is introduced early and its motivation justified: the two-mode (direct vs. gateway) philosophy directly addresses the research gap articulated in Section 1.2.
- The Contributions Overview (Section 1.6) is unusually clear for a BSc dissertation, enumerating four distinct contributions and framing the gap well against prior literature.

### Areas for Improvement

- The problem statement (Section 1.2) references the "Masking Effect" concept before formally defining it, which can confuse readers encountering the term for the first time. A one-sentence definition here would tighten the argument.
- Section 1.5 (Scope and Boundaries) is appropriately modest, but a brief forward-reference to the threats-to-validity section would better signal methodological self-awareness at this early stage.
- The abstract already borders on being slightly over-specified; while not a penalty under the mark scheme, minor pruning would make it even crisper.

### Justification

The abstract and introduction comfortably exceed the Excellent (80–89) descriptor. The abstract gives "a thorough overview covering most of the project's achievements and explains its setting very well." The introduction explains the project's setting, is supported by peer-reviewed literature (Aldridge, Bilokon, O'Hara, Aquilina), and derives an evaluation strategy from that literature. The minor weaknesses identified do not pull the work below this band; they would need to be substantially resolved to push it into Outstanding (90+), where the expectation is a "comprehensive and precise account... firmly based on peer-reviewed literature."

---

## 3.2 Background and Theory — 78 / 100 (Above Expectations)

> *Above Expectations — balanced, literature-grounded, very well understood*

### Strengths

- **The coverage of hardware-aware principles is unusually thorough for a BSc project.** Section 2.2 explains the cache hierarchy, spatial and temporal locality, cache lines (64 bytes), and the 80/20 access heuristic — all properly attributed to Patterson & Hennessy (2013). The pipeline flush diagram (Figure 2.3) and the AMD Zen 4 branch misprediction penalty (15–20 cycles) are specific and correct.
- Section 2.3 explains the three data structure families (tree-based, flat array, memory pool) in terms of hardware consequences, not just algorithmic complexity. The "pointer chasing vs contiguous memory" contrast is clearly grounded in both the literature and the experimental design.
- Section 2.4 on lock-free queues is well-written. The SPSC/MPMC distinction, the MESI protocol, false sharing, and the `alignas(64)` code snippet are all correctly described. The code snippet in Section 2.4.3 is an effective pedagogical tool.
- The FIX protocol section provides appropriate depth: the SOH delimiter, tag-value encoding, and the comparison to SBE/FAST are correct and relevant to the project's design choices.
- The Related Work section (2.5) is well-organised by theme (market microstructure theory, systems-design practice, performance methodology) and correctly identifies the gap being addressed. The critique of Thompson et al. (2011) and Desrochers (2013) as not establishing end-to-end behaviour is particularly astute.
- References are largely peer-reviewed (Aquilina et al. QJE 2022, Gould et al. QF 2013, Avellaneda & Stoikov QF 2008, Rosu RFS 2009) with appropriate technical books alongside them. The breadth is commendable.

### Areas for Improvement

- The kernel-bypass section (2.4.5) is somewhat brief. While DPDK and OpenOnload are mentioned and Figure 2.10 is clear, there is no discussion of the userspace polling model's CPU cost (spinning vs. interrupts tradeoff) or of SmartNIC/FPGA alternatives that appear in the literature cited in Chapter 6. Given the prominence of H3, a deeper treatment here would have been warranted.
- The LOB matching semantics are not explicitly explained — the mechanics of price-time priority matching matter for understanding why the matching engine's correctness relies on semantic parity across implementations.
- Section 2.5 could more explicitly map cited works to specific hypotheses (e.g., "Bilokon & Gunduz (2023) motivates H1 because..."), which would sharpen the reader's understanding of why each work matters to this project specifically.
- A few references are grey literature (Desrochers GitHub, Fog 2023 optimisation guide). These are appropriate for this project type, but a note acknowledging their non-peer-reviewed status would reflect better academic practice.

### Justification

The background clearly satisfies the Above Expectations (70–79) descriptor: the project's setting is "described based on the scientific literature in a balanced and comprehensive way" and "the content and relevance of the included literature is understood." The kernel-bypass coverage and the shallow treatment of LOB matching semantics are the main factors preventing entry into the Excellent (80–89) band, which requires "an appropriate level of detail covering all relevant developments" with "very well understood" relevance.

---

## 3.3 Technical Quality, Methodology and Evaluation — 83 / 100 (Excellent)

> *Excellent — exceptional statistical rigour; very thorough, reproducible evaluation*

### Strengths

- **The formal requirements framework (Tables 3.1 and 3.2) is unusually rigorous.** All six FRs and nine NFRs are testable and properly distinguished. Traceability from requirements to evaluation results (e.g., NFR-6 → Table 5.5 and Figure 5.6) is consistently maintained across the report.
- The Ports and Adapters / Hexagonal architecture (Section 3.3.1, Figure 3.2) is a principled design choice that directly enables scientific fairness (NFR-2). The justification — that the `IOrderBook` interface forces identical call overhead including vtable lookup cost — demonstrates a sophisticated understanding of C++ runtime polymorphism.
- **The statistical methodology (Section 5.1) is genuinely exemplary for a BSc project.** The use of B=10,000 bootstrap resamples for 95% CIs, paired sign-flip permutation tests (Pitman, 1937), and Holm-Bonferroni step-down correction reflects professional-grade experimental design. The justification for heavy right-hand tails in latency data is correctly articulated.
- The observer-effect quantification is a notable detail: the author measured timing overhead across 10,000 iterations, confirmed a standard deviation <10 ns, and correctly reasoned that this adds a constant but non-noisy baseline offset. This level of methodological self-awareness distinguishes excellent from merely good work.
- The five implementations are described at the right level of abstraction. Code listings (Appendices B–F) are clean, well-commented, and annotated with performance-relevant observations (e.g., `alignas(64)` in the SPSC queue, O(1) `priceToIndex` arithmetic, free-list branching in `PoolOrderBook`).
- The functional validation (Section 5.2) is thorough: parametrised contract testing via GoogleTest, 100% branch/line/function/region coverage (Table 5.1), and a golden-reference comparison strategy. Validation of the benchmark generator's output distribution (Figures 5.1 and 5.2) demonstrates integrity discipline.
- **The quad-timestamp decomposition strategy (Figure 5.10, Table 5.8) is elegant and effective.** Engine latency of 62 ns vs 2,014,045 ns network latency for `ArrayOrderBook` is a striking and clear result that directly supports H3.
- The `perf stat` hardware counter evidence in Section 5.4 — citing 843,745 vs 1,392,193 L1 misses for array vs map, and the extraordinary 144M misses for vector — gives the cache-locality claims genuine empirical weight beyond theoretical reasoning.
- The MPSC thread contention analysis (Section 5.6) adds significant breadth. Queue wait time growing from 1.02 ms to 1.55 ms as producers scale from 1 to 8 while engine time holds at 68–74 ns is a clean, well-supported result. The frequency-scaling residency explanation for the vector engine latency drop is a sophisticated hardware-aware insight.

### Areas for Improvement

- The WSL2 environment is a significant constraint. While absolute claims are correctly bounded as "environment-scoped" (NFR-8), the virtualisation tax and loopback-only networking mean gateway results are not representative of production conditions. Even a brief WSL2 vs bare-metal comparison would have substantially strengthened the H3 claims.
- The sparse_extreme scenario is excluded from gateway analysis for campaign-time reasons. This means H3 is tested only on the three highest-density workloads — precisely the scenarios most likely to show masking. A brief acknowledgement that this exclusion may bias the H3 result positively would have been more thorough.
- The Hybrid order book performing worse than the plain map in several scenarios is explained via tier-routing overhead (Section 5.4.1), but measuring the actual hot/cold branch frequency (e.g., via perf probes) would have converted a hypothesis into an empirical finding.
- Section 3.7 states the array layout is sized to fit the 32 KB L1 data cache, but per-level order queues allocate on the heap. There is a minor tension between the stated cache-fitting rationale and the actual memory layout that is not explicitly addressed.
- A power analysis or sample-size justification would demonstrate stronger statistical self-awareness, particularly for the gateway comparisons where bootstrap intervals sometimes overlap.

### Justification

This section clearly satisfies the Excellent (80–89) descriptor. The designs are "very well considered, clear, and easy to understand"; the evaluation reaches "a very high level" through rigorous statistical testing; and the author demonstrates "a very thorough understanding of the technical details." The limitations noted — particularly the WSL2 constraint and the sparse_extreme exclusion — are acknowledged in the Threats to Validity section, which itself reflects good practice. Reaching Outstanding (90+) would require near-zero faults and a more exhaustive investigation of platform effects and alternative causal mechanisms.

---

## 3.4 Summary and Conclusions — 79 / 100 (Above Expectations)

> *Above Expectations — thorough, honest, well-aligned with results; future work is field-aware*

### Strengths

- Section 6.1 (Synthesis of Results) is tight and accurate. All three hypotheses are addressed: H1 (array wins direct mode), H2 (pooling improves P99 jitter but cannot overcome structural penalties), and H3 (88–96% of gateway latency is transport). The quantitative evidence cited (214 ns, 1,152 ns P99 pool vs 2,573 ns map, p>0.05 in gateway) matches the evaluation chapter exactly.
- The Critical Reflection (Section 6.2) is unusually honest and specific. The three design choices that would be revised (exhaustive permutation testing from iteration 1, alternative socket baselines, longer replay windows) are each substantive and actionable rather than generic.
- Section 6.2.1's discussion of CRTP as an alternative to virtual dispatch for static polymorphism is a sophisticated insight. The observation that vtable overhead matters only when transport has been eliminated (DPDK/FPGA contexts) is correct and field-aware.
- The Limitations section (6.3) covers three genuinely distinct dimensions: architectural (WSL2 virtualisation), methodological (subset of scenario pairs), and physical (loopback vs discrete hardware). These align well with the threats identified in Section 5.8.
- Future Work (Section 6.6) identifies kernel bypass (DPDK/OpenOnload) and FPGA offload as logical next steps, substantiated by the H3 findings. These are ambitious and relevant rather than "more of the same."
- The Practical Recommendations (Section 6.5) correctly identify the workload-dependency of data structure selection, and the NASDAQ vs CME E-mini contrast from Section 5.4.1 is well-referenced.

### Areas for Improvement

- The conclusions do not revisit the `HybridOrderBook`'s anomalous 5.65 ms gateway latency in dense_full (Figure 5.12), which is a notable result deserving a closing comment given its divergence from the masking narrative.
- Section 6.4 (Professional and Ethical Issues) is somewhat formulaic. The MiFID II and Flash Crash discussion is relevant, but the "digital divide" argument citing Lewis (2014) is briefly raised and immediately dropped. A more developed treatment would strengthen this section.
- The Further Work section could mention order book reconstruction from live market feeds (e.g., ITCH parsing, as referenced in Section 2.1) as a natural next step grounded directly in the project's own design.

### Justification

The conclusions satisfy the Above Expectations (70–79) descriptor comprehensively. The synthesis is "very well summarised," the critical analysis is "thorough and honest," weaknesses are identified with "good solutions," and the future work is "ambitious, relevant and well thought out." The gap between this and Excellent (80–89) is primarily the Ethical Issues section, which lacks the depth of field knowledge needed for a more developed treatment.

---

## 3.5 Presentation, Structure and Language — 84 / 100 (Excellent)

> *Excellent — LaTeX-typeset, well-structured, professionally presented*

### Strengths

- The document is typeset in LaTeX with a consistent and professional appearance. Mathematical notation (bootstrap CI formula, pipeline flush table) is correctly rendered. Figures are scalable vector-quality and all are appropriately sized.
- Word count of 14,893 sits comfortably within the 10,000–15,000 word guideline.
- All figures and tables are numbered, captioned, and referred to from the main text without exception. Captions are informative rather than merely labelling — e.g., Figure 5.5 explains why the vector is excluded from the relative slowdown heatmap.
- The Glossary (Appendix A) defines 23 technical terms including Mechanical Sympathy, MESI Protocol, P-state, CRTP, and RDTSC — an excellent addition that makes the report accessible to a non-specialist marker.
- References are consistently formatted throughout, with full bibliographic information and access dates on all URLs. The mixture of peer-reviewed papers, technical books, and reputable grey literature is appropriate.
- The six appendices are well-chosen: each contains code directly referenced in the main text, with line-level comments explaining performance-critical design decisions, avoiding clutter in the main body while preserving reproducibility.

### Minor Issues

- A small number of figure captions are slightly long (e.g., Figure 2.3 runs to four lines), though this is a minor stylistic point.
- The abstract is not separated from Chapter 1 by a page break — a standard LaTeX thesis convention.
- One or two sentences in the evaluation chapter are compound enough to benefit from splitting.

### Justification

The report is written to a professional standard. The LaTeX typesetting, consistent numbering, comprehensive glossary, and well-chosen appendices satisfy the Excellent (80–89) descriptor: "very readable with only a few sentences causing a re-read," references "always correctly formatted," and "a professional eye is demonstrated." The issues identified are extremely minor and do not prevent the work from comfortably sitting in this band.

---

## Final Commentary

This is an outstanding BSc dissertation for its subject area. The project tackles a technically demanding topic — hardware-aware performance engineering in HFT, where nanosecond-scale decisions carry financial significance — and produces results that are both empirically rigorous and intellectually coherent.

**The most impressive aspects are:**

- The statistical methodology, which goes well beyond what is typically seen at BSc level. Paired sign-flip permutation tests with Holm-Bonferroni correction is methodologically sound and appropriate for heavy-tailed data.
- The quad-timestamp decomposition strategy, which elegantly proves H3 in a way that is both visually compelling (Figure 5.10) and numerically precise (Table 5.8).
- The `perf stat` hardware counter evidence, which transforms cache-locality claims from theoretical reasoning into measured observations.
- The methodological self-awareness throughout — the WSL2 constraint is correctly bounded, and the observer-effect quantification is a particularly mature detail.

The main factors limiting the overall mark to high-Excellent rather than Outstanding are: the WSL2 environment constraint limiting external validity of gateway results; the relatively brief treatment of kernel-bypass networking in the background chapter; and a few evaluation coverage gaps (sparse_extreme excluded from gateway, no power analysis for sample sizes). None of these are serious weaknesses, but collectively they prevent the work from reaching the near-faultless standard required for the Outstanding (90+) band.

---

| **REPORT OVERALL GRADE (weighted average)** | **~81 / 100 — First Class (Excellent)** |
| ------------------------------------------- | --------------------------------------- |