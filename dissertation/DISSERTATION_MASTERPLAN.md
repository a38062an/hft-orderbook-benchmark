# HFT Order Book Dissertation — Master Plan

**Project:** High-Frequency Trading Order Book Benchmarking in C++20
**University:** University of Manchester, School of Computer Science
**Target Grade:** First Class (80%+)
**Deadline:** ~27 days from start
**Word Target:** ~13,500 words
**Author:** Anthony Nguyen

---

## AUTHORSHIP AND TOOLING BOUNDARIES (Strict)

This dissertation is authored primarily by Anthony Nguyen. External tooling is
used for build troubleshooting, formatting support, and editorial quality
checks only.

- Tooling role: LaTeX troubleshooting, build automation, figure/table formatting,
   and technical feedback on draft structure.
- Human role: researching, writing all original prose, interpreting data, and
   constructing the argumentation.
- Writing ownership: full sections and chapter narrative remain human-authored;
   tooling support is limited to refinement, clarity, and formatting.

---

## CONTEXT FOR NEW CHAT SESSIONS

When starting a new session, paste this file and say:
"I am writing my Manchester CS dissertation on HFT order book benchmarking. This file is my master plan. Please act as a strict supervisor holding me to the 80%+ standard described below. I am currently working on [section name]. Here is my draft: [paste draft]."

---

## THE Official UoM Assessment Rubric (Targeting 80--100%)

Based on the `project_assessment.pdf`, your grader will evaluate the dissertation strictly across multiple dimensions. To secure a First-Class grade ("Excellent 80-89" or "Outstanding 90-100"), the dissertation must demonstrably hit these precise markers:

1. **Report Writing & Narrative**:
   - **80-89**: Faultless structure, coherent flow, highly professional writing. Evaluation of the background is comprehensively integrated into the project. Ideas from literature are seamlessly synthesised.
   - **90-100**: The reader is completely engaged, the writing is outstanding, and the narrative pulls them in effortlessly.
2. **Achievements - Complexity**:
   - **80-89**: Must delve deeply into a technical area and build extensively upon material from the scientific literature.
   - **90-100**: Innovatively combines ideas from several areas in an original way to address a long-standing hard problem.
3. **Achievements - Scale & Ownership**:
   - **80-89**: Huge demonstrable workload that is completely polished. Must show original thought (not just repeating existing work) by critically analysing and building upon others.
   - **90-100**: Absolute ownership of the problems. Evidence of deep reading around the topic establishing exactly where the work sits within the academic landscape.
4. **Achievements - Success**:
   - **80-100**: The developed solution must be highly convincing, grounded entirely in suitable academic principles, and frankly hard to improve upon given the timeframe.

## THE 80%+ ACADEMIC STANDARD (Non-Negotiable Rules)

### 1. The Golden Thread

Every sentence must do one of three things:

- Establish a requirement
- Explain how the requirement was built
- Prove whether the requirement was met

If a sentence does none of these, cut it.

### 2. Requirements Are Law

Every FR and NFR has an ID. Every Chapter 5 section must explicitly reference those IDs. Example: "This result validates NFR-3 (isolation) and NFR-2 (fairness)..."

### 3. Results vs Evaluation Are Strictly Separated

- Results: Pure empirical reporting. Numbers only. No opinion.
- Evaluation: Contextualise results against NFRs and hypotheses. Apply Amdahl's Law for gateway findings.

### 4. Defensive Academic Writing — The "Do Not Say" List

| Do NOT say | Say instead |
|---|---|
| "proves" | "strongly indicates", "the evidence suggests" |
| "obviously" / "clearly" | Let the numbers speak |
| "cache caused the slowdown" | "exhibits characteristics consistent with cache pressure" |
| "massive" / "incredible" | "a 75.3x degradation", use exact numbers |
| "I feel" / "I believe" | "The data indicates", "Observations suggest" |
| Universal claims | Bound to: "within the WSL2/AMD Ryzen 5 7600X environment..." |

### 5. Paragraph Structure (Every Technical Paragraph)

Claim -> Empirical Evidence -> Mechanism/Reasoning -> Link to Hypothesis/NFR

### 6. Information Density

Figure captions must be mini-abstracts. Don't say "Figure 4 shows performance." Say: "Figure 4 demonstrates a 75.3x latency degradation in the std::vector implementation under dense_full load, driven by dynamic capacity reallocation on the critical path."

---

## CANONICAL EVIDENCE (Ground Truth — Do Not Invent New Results)

| Fact | Value | Use in |
|---|---|---|
| Array wins direct mode | 6 of 7 scenarios | H1, Chapter 5 |
| Mixed direct mean (array) | 210.72 ns | H1, Chapter 5 |
| Dense_full: vector vs array | 16082.10 ns vs 213.44 ns = 75.3x slowdown | H1, NFR-3, Chapter 5 |
| Gateway network share | ~98.7% median | H3, Chapter 5 |
| Gateway dense_full vector outlier | 81,741,096.56 ns end-to-end | H3, queue-amplification |
| Dataset size | 35 direct, 35 gateway, 140 MPSC = 210 total rows | Chapter 5 methodology |
| Environment | WSL2 Ubuntu 24.04.2, AMD Ryzen 5 7600X, perf 6.8.12 | All chapters |
| Pinning | Thread-pinned workflow used for direct/MPSC | NFR-7 |

---

## HYPOTHESES (Do Not Modify)

**H1 (Ordering Structure and Locality)**
Index-oriented order-book layouts yield materially lower direct-mode latency than tree-based and sorted-vector designs across most scenarios, with cache behavior as a contributing (not sole) mechanism.

- Acceptance: array leads direct-mode mean latency in most scenarios; dense-full gap versus vector remains order-of-magnitude; perf evidence directionally consistent with higher memory-access pressure in slower variants.

**H2 (Allocation Strategy in Isolation)**
Memory-pooling benefits are implementation- and workload-dependent; pooling does not remove ordering-structure bottlenecks.

- Acceptance: pool improves over map only in selected scenarios; pool does not approach array where traversal dominates; strong allocator-causality requires dedicated allocation instrumentation.

**H3 (System-Level Masking)**
In gateway mode, network and queue components dominate end-to-end latency, reducing the practical impact of engine-internal data-structure differences except in queue-amplification failure cases.

- Acceptance: Net + Que dominates Eng in 34 of 35 non-vector rows; vector dense_full is clear boundary case showing queue-amplification failure.

---

## FORMAL REQUIREMENTS TABLES

### Functional Requirements

| ID | Requirement | Testable Criterion |
|---|---|---|
| FR-1 | Implement five distinct order book data structures | Five implementations present and benchmarkable |
| FR-2 | Support direct-mode benchmarking isolating engine latency | Direct mode runs without network stack involvement |
| FR-3 | Support gateway-mode benchmarking with end-to-end FIX protocol overhead | Gateway mode includes TCP, FIX parsing, and queue handoff |
| FR-4 | Support MPSC contention benchmarking with configurable producer counts | MPSC mode scales across producer configurations |
| FR-5 | Emit structured CSV output for all benchmark runs | CSV files produced with consistent schema per run |
| FR-6 | Include correctness test suite verifying semantic parity across implementations | All implementations produce identical outputs for equivalent inputs |

### Non-Functional Requirements

| ID | Requirement | Testable Criterion |
|---|---|---|
| NFR-1 | Correctness: semantic parity across all implementations | No variant has special-case logic biasing results |
| NFR-2 | Fairness: identical workload parameters across all implementations | Same run count, order count, scenario selection for all books |
| NFR-3 | Isolation: direct mode measures only algorithmic/data-structure latency | No transport overhead in direct-mode code path |
| NFR-4 | Realism: gateway mode includes network, queue, parsing, and engine latency | End-to-end timing supports dissertation's masking argument |
| NFR-5 | Reproducibility: pipeline regenerates canonical dataset without undocumented steps | Fresh run produces consistent artifact structure |
| NFR-6 | Statistical rigour: mean, standard deviation, and tail metrics reported | Variance metrics present and consistently derived |
| NFR-7 | Performance isolation: CPU pinning and scheduler isolation applied | Pinning policy documented and verified in results |
| NFR-8 | Portability discipline: absolute latency claims bounded to WSL2/AMD environment | Relative comparisons defensible; absolute numbers environment-scoped |
| NFR-9 | Claim discipline: no measurement overstated beyond evidence | Cache and allocator causality stated as correlation, not proven cause |

---

## WORD BUDGET

| Chapter | Target Words | Core Focus |
|---|---|---|
| 1. Introduction | ~1,200 | Problem, RQs, objectives, chapter map |
| 2. Background | ~3,500 | Cache hierarchy, lock-free concurrency, LOB mechanics, FIX protocol — all pointing forward to Ch5 |
| 3. Design | ~1,500 | FR/NFR tables, architecture overview, benchmark design rationale |
| 4. Implementation | ~1,800 | Novel engineering choices only: lock-free queue, zero-allocation strategy, benchmark harness |
| 5. Results & Evaluation | ~3,500 | Segregated results then evaluation, mapped to NFR IDs, Amdahl framing for gateway |
| 6. Conclusion | ~1,000 | Objectives revisited, limitations, future work, reflection |
| **Total** | **~13,500** | |

---

## WRITING ORDER (Follow This Exactly)

1. [COMPLETED] Requirements tables
2. **NOW -> Chapter 5, Section: Experimental Methodology**
3. Chapter 5, Section: Functional Validation
4. Chapter 5, Section: Latency Analysis — Direct Mode
    - 5.4.1 Scenario-Specific Latency Trends
    - 5.4.2 Architectural Mechanisms: Pool and Hybrid
    - 5.4.3 Tail Latency Distribution and Determinism
5. Chapter 5, Section: Gateway Mode — End-to-End Decomposition
6. Chapter 5, Section: Threats to Validity
7. Chapter 2 — Background (backfill theory to support Ch5 findings)
8. Chapter 3 — Design (requirements tables + architecture)
9. Chapter 4 — Implementation (novel choices only)
10. Chapter 1 — Introduction (write last, frame the whole story)
11. Chapter 6 — Conclusion + Reflection
12. Abstract (very last)

---

## SESSION-BY-SESSION WRITING BRIEFS

### SESSION 1 — Chapter 5: Experimental Methodology

**File:** `05_testing_evaluation.tex`
**Section:** `\section{Experimental Methodology}`

Write 4 paragraphs:

**Paragraph 1 — Environment declaration**

- Name: AMD Ryzen 5 7600X, WSL2 Ubuntu 24.04.2, perf 6.8.12, std::chrono wall-clock timing
- Acknowledge WSL2 virtualisation layer vs bare-metal
- Bound all absolute latency claims to this environment
- This pre-empts the marker's biggest criticism

**Paragraph 2 — Benchmark modes**

- Two modes prevent conflation of algorithmic and systemic latency
- Direct mode: in-process, CPU thread-pinned, no transport overhead -> maps to NFR-3
- Gateway mode: TCP stack + FIX decoder + lock-free MPSC queue -> maps to NFR-4
- Separation is deliberate and architecturally motivated

**Paragraph 3 — Dataset and statistical approach**

- 35 direct rows, 35 gateway rows, 140 MPSC rows, 210 total
- Primary metric: mean latency; standard deviation reported where variance is notable
- Acknowledge run count as pragmatic balance between confidence and stability
- Maps to NFR-5 and NFR-6

**Paragraph 4 — Hypotheses link**

- H1, H2, H3 serve as evaluative framework
- Each results section maps explicitly to its hypothesis and NFR IDs
- 2 sentences only

### SESSION 4 — Chapter 5: Latency Analysis (Direct Mode)

**File:** `05_testing_evaluation.tex`
**Section:** `\section{Latency Analysis - Direct Mode}`

**Part 1 — Mean Latency and IPC Mechanisms**

- Reference Table 5.3 data (Array @ 13.9ns vs Vector @ 145.5ns).
- Claim: Array wins via mechanical sympathy.
- Evidence: Perf data (843k vs 1.39m L1 misses in 'mixed').
- Mechanism: Contiguous memory access vs pointer chasing.

**Part 2 — Scenario Trends (5.4.1)**

- Contrast 'mixed' (narrow gap) vs 'dense_full' (19x Vector degradation).
- Data: Array stays stable @ 94ns; Vector collapses @ 1,879ns.
- Link to H1: Indexed layouts prove resilient to market data volume.

**Part 3 — Architectural Mechanisms (5.4.2)**

- Discuss PoolOrderBook's branching penalty (7.36% miss rate) vs Hybrid's hot/cold storage limits.
- Reasoning: SIMD/branching simplicity beats locality where logic is complex.

**Part 4 — Tail Latency & H2 (5.4.3)**

- Contrast Array's 125ns P99 with Vector's 6,016ns "dense" stall.
- Link to H2: Determinism of layout yields predictable tail performance.

### SESSION 5 — Chapter 5: Gateway Mode & H3

**Section:** `\section{Gateway Mode - End-to-End Decomposition}`

- Discuss the "Masking Effect" (99.8%+ latency is Network).
- Reference Table 5.5: stochastic jitter > engine speed.
- Validate H3: High-level optimizations irrelevant without kernel-bypass.
