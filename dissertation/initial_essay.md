# Initial Essay Draft Starter

This file is a practical writing starter you can convert directly into LaTeX chapter prose.

## Core Claim

Array-indexed order books dominate direct-mode latency due to contiguous access and constant-time index mapping, while gateway mode is often dominated by transport and queueing overhead except in stress outlier cases.

## Key Numbers To Reuse

- Mixed direct mean (array): 210.72 ns
- Dense-full direct mean (vector): 16,082.10 ns
- Dense-full gateway outlier (vector): 81,741,096.56 ns
- Typical gateway Net component (non-outlier rows): roughly 0.95 to 1.95 ms

## Fast Chapter Draft Order (2-day plan)

1. Write Chapter 5 first (results + interpretation).
2. Write Chapter 2 hardware subsections to justify Chapter 5 causality.
3. Write Chapter 3 and 4 as concise design/implementation rationale.
4. Write Chapter 6 with explicit hypothesis verdicts.
5. Write Chapter 1 after all details are fixed.
6. Write abstract last.

## Must-Answer Questions In Text

- Why does vector collapse in dense books?
- Why does pool improve map in some scenarios but not all?
- Why do gateway rankings compress relative to direct mode?
- Which conclusions are robust versus potentially timing-noise sensitive?

## Red-Flag Gaps To Avoid

- Do not claim absolute nanosecond precision without chrono-overhead caveat.
- Do not claim strict worst-case behavior from single-run max values.
- Do not present gateway totals without Net/Que/Eng decomposition.
- Do not omit explicit functional correctness evidence.
