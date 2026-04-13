# Hypothesis Testing Upgrade Plan

## Why this is needed

Current methodology states `n=5` runs and reports averages without confidence intervals.
For dissertation-level statistical rigor, this is weak for inferential claims, especially when comparing close implementations.

## Objectives

1. Keep canonical dataset untouched.
2. Collect run-level replicated data in a separate artifact path.
3. Report uncertainty (95% confidence intervals) and formal hypothesis tests.
4. Map statistical outcomes directly to H1/H2/H3 wording in Chapter 5.

## Data separation rule

All new artifacts go to:

- `results/hypothesis_testing/<timestamp>/...`

This avoids modifying `results/results.csv` and protects existing dissertation figures.

## What was added

- `scripts/hypothesis_testing/run_hypothesis_campaign.py`
- `scripts/hypothesis_testing/analyze_hypothesis_results.py`
- `scripts/hypothesis_testing/run_and_analyze.sh`

These scripts generate:

- `raw_run_results.csv`: one row per replicate-case (run-level data)
- `failures.csv`: execution failures, if any
- `manifest.json`: campaign metadata and provenance
- `analysis/cell_summary.csv`: per-cell means, SD, median, P95, CI95
- `analysis/pairwise_vs_baseline.csv`: baseline-comparator paired tests
- `analysis/hypothesis_summary.md`: narrative-ready summary

## Recommended experimental design

### Phase A (Direct mode; primary)

Use direct mode first to strengthen H1/H2 with high statistical confidence.

- Scenarios: all 7
- Books: all 5
- Replicates: 30 per (scenario, book)
- Orders per replicate: 10,000
- Mode: direct
- Pinning: fixed core (`--pin-core 0` or allowed-core equivalent)

Total direct runs: `7 * 5 * 30 = 1050`.

### Phase B (Gateway mode; targeted)

For H3, test a reduced set of scenarios where masking/backlog claims matter most.

- Scenarios: `mixed,dense_full,tight_spread`
- Books: all 5
- Replicates: 20 per cell
- Mode: gateway

Total gateway runs: `3 * 5 * 20 = 300`.

## Statistical method (implemented)

### 1) Confidence intervals (cell-level)

For each (mode, scenario, book), estimate 95% CI for mean latency using bootstrap resampling.

### 2) Pairwise tests (baseline = array)

For each (mode, scenario), compare each comparator against baseline using paired replicate differences:

- difference: `d_i = latency_comparator_i - latency_baseline_i`
- permutation sign-flip test (two-sided)
- one-sided exact sign test for directional hypothesis (baseline faster)
- Holm-Bonferroni correction for multiple comparisons (per mode)

### 3) Decision rule

A comparison supports baseline advantage when:

- corrected one-sided p-value `< 0.05`
- and mean difference `> 0`
- and CI95 of difference does not straddle zero (preferred stronger criterion)

## Runbook

### Quick full direct campaign

```bash
python3 scripts/hypothesis_testing/run_hypothesis_campaign.py \
  --repo-root . \
  --reps-direct 30 \
  --reps-gateway 0 \
  --orders 10000 \
  --pin-core 0
```

### Analyze campaign output

```bash
python3 scripts/hypothesis_testing/analyze_hypothesis_results.py \
  --input results/hypothesis_testing/<timestamp>/raw_run_results.csv
```

### One-command path

```bash
bash scripts/hypothesis_testing/run_and_analyze.sh
```

## Dissertation text updates (recommended)

## Chapter 5 - Experimental Methodology

Replace the sentence claiming only `n=5` averaging with:

- repeated run-level sampling (`n=30` direct; targeted `n=20` gateway)
- 95% confidence intervals for latency estimates
- paired hypothesis tests with Holm correction

## Chapter 5 - Results/Evaluation

For each major claim, include:

1. Point estimate (mean difference)
2. 95% CI
3. corrected p-value
4. effect direction and practical magnitude

Example template:

"In direct `dense_full`, array outperformed vector by 15,800 ns mean latency difference (95% CI [x, y], Holm-corrected sign-test p < 0.001), supporting H1 under this environment."

## Threats to Validity

Keep environment-bounded wording, but remove/soften the previous statistical weakness once the above campaign is complete.

## Acceptance checklist

1. `raw_run_results.csv` contains expected row count.
2. `failures.csv` is empty or explains omissions.
3. `cell_summary.csv` has CI95 for every reported cell.
4. `pairwise_vs_baseline.csv` includes corrected p-values.
5. Chapter 5 text references CI and corrected tests, not just averages.
