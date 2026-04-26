#!/usr/bin/env python3
"""Analyse benchmark run data using scipy for confidence intervals and hypothesis tests.

This script is a library-backed alternative to analyze_hypothesis_results.py.
It produces the same three output files (cell_summary.csv, pairwise_vs_baseline.csv,
hypothesis_summary.md) but delegates all statistical computation to scipy and numpy,
making the results independently verifiable against widely-audited implementations.

Statistical approach
--------------------
- 95% bootstrap percentile confidence intervals (scipy.stats.bootstrap, n=10,000 resamples).
- Paired sign-flip permutation test for pairwise comparisons
  (scipy.stats.permutation_test, n=20,000 permutations).
- One-sided sign test via scipy.stats.binomtest.
- Holm-Bonferroni step-down correction for multiple comparisons per mode.

Usage
-----
    python3 scripts/hypothesis_testing/analyze_results_scipy.py \\
        --input results/results.csv \\
        --output-dir results/analysis_scipy \\
        --baseline array
"""

from __future__ import annotations

import argparse
import csv
import pathlib
from collections import defaultdict
from typing import Dict, List, Tuple

import numpy as np
from scipy import stats as sp


# ---------------------------------------------------------------------------
# Statistical helpers
# ---------------------------------------------------------------------------

def bootstrap_ci_mean(
    values: List[float],
    n_resamples: int,
    alpha: float,
    seed: int,
) -> Tuple[float, float]:
    """Return the lower and upper bounds of a bootstrap percentile CI for the mean.

    Parameters
    ----------
    values:      Raw observations for one (mode, scenario, book) cell.
    n_resamples: Number of bootstrap resamples to draw.
    alpha:       Two-sided significance level (e.g. 0.05 for a 95% CI).
    seed:        Random seed for reproducibility.
    """
    data = np.array(values, dtype=float)
    result = sp.bootstrap(
        (data,),
        np.mean,
        n_resamples=n_resamples,
        confidence_level=1.0 - alpha,
        method="percentile",
        random_state=np.random.default_rng(seed),
    )
    return float(result.confidence_interval.low), float(result.confidence_interval.high)


def bootstrap_ci_mean_diff(
    diffs: List[float],
    n_resamples: int,
    alpha: float,
    seed: int,
) -> Tuple[float, float]:
    """Return the bootstrap percentile CI for the mean of paired differences.

    Parameters
    ----------
    diffs:       Per-replicate differences (comparator latency - baseline latency).
    n_resamples: Number of bootstrap resamples.
    alpha:       Two-sided significance level.
    seed:        Random seed for reproducibility.
    """
    data = np.array(diffs, dtype=float)
    result = sp.bootstrap(
        (data,),
        np.mean,
        n_resamples=n_resamples,
        confidence_level=1.0 - alpha,
        method="percentile",
        random_state=np.random.default_rng(seed),
    )
    return float(result.confidence_interval.low), float(result.confidence_interval.high)


def paired_permutation_pvalue(diffs: List[float], n_resamples: int, seed: int) -> float:
    """Return the two-sided p-value from a paired sign-flip permutation test.

    The null hypothesis is that the two implementations share the same latency
    distribution; i.e. that the observed mean difference could arise under random
    sign assignment.  The test statistic is the mean of the differences.

    Parameters
    ----------
    diffs:       Per-replicate differences (comparator latency - baseline latency).
    n_resamples: Number of random sign-flip permutations for the Monte Carlo estimate.
    seed:        Random seed for reproducibility.
    """
    data = np.array(diffs, dtype=float)
    result = sp.permutation_test(
        (data,),
        np.mean,
        permutation_type="samples",
        n_resamples=n_resamples,
        alternative="two-sided",
        random_state=seed,
    )
    return float(result.pvalue)


def sign_test_pvalue_one_sided(diffs: List[float]) -> float:
    """Return the one-sided p-value from an exact binomial sign test.

    Tests the directional hypothesis that the baseline is faster than the
    comparator (i.e. that comparator - baseline > 0 more than half the time).

    Parameters
    ----------
    diffs: Per-replicate differences (comparator latency - baseline latency).
    """
    positive_count = sum(1 for d in diffs if d > 0)
    tied_count = sum(1 for d in diffs if d == 0)
    n_effective = len(diffs) - tied_count
    if n_effective == 0:
        return 1.0
    result = sp.binomtest(positive_count, n_effective, p=0.5, alternative="greater")
    return float(result.pvalue)


def apply_holm_bonferroni(rows: List[Dict], p_key: str, out_key: str) -> None:
    """Apply the Holm-Bonferroni step-down correction in place.

    Sorts rows by raw p-value, multiplies by the decreasing family-size factor,
    enforces monotonicity via a running maximum, then writes the adjusted value
    back to each row under out_key.

    Parameters
    ----------
    rows:    List of result dicts for a single mode (shared family).
    p_key:   Key of the raw p-value in each dict.
    out_key: Key under which the adjusted p-value is written.
    """
    if not rows:
        return
    indexed = sorted(enumerate(rows), key=lambda item: item[1][p_key])
    m = len(rows)
    adjusted = [0.0] * m
    running_max = 0.0
    for rank, (original_index, row) in enumerate(indexed, start=1):
        adj = min(1.0, row[p_key] * (m - rank + 1))
        running_max = max(running_max, adj)
        adjusted[original_index] = running_max
    for i, row in enumerate(rows):
        row[out_key] = adjusted[i]


# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------

def load_cells(
    input_path: pathlib.Path,
) -> Dict[Tuple[str, str, str, str], List[float]]:
    """Read the results CSV and group observations by (mode, scenario, book, metric)."""
    cells: Dict[Tuple[str, str, str, str], List[float]] = defaultdict(list)
    metrics = ["latency_ns", "throughput"]
    with input_path.open("r", newline="") as f:
        for row in csv.DictReader(f):
            for m in metrics:
                try:
                    val = float(row[m])
                except (KeyError, ValueError):
                    continue
                key = (row["mode"], row["scenario"], row["book"], m)
                if row["mode"] == "mpsc":
                    # For MPSC, we group by producer count (stored in pin_core)
                    key = (row["mode"], row["scenario"] + "_c" + row["pin_core"], row["book"], m)
                    # Also add a scenario-agnostic key for aggregated MPSC analysis (as used in dissertation)
                    agg_key = (row["mode"], "aggregated_c" + row["pin_core"], row["book"], m)
                    cells[agg_key].append(val)
                cells[key].append(val)
    return cells


def load_replicate_map(
    input_path: pathlib.Path,
) -> Dict[Tuple[str, str, str, str], Dict[int, float]]:
    """Read the results CSV and index each observation by replicate number.

    Returns a mapping (mode, scenario, book, metric) -> {replicate_id: value}
    """
    replicate_map: Dict[Tuple[str, str, str, str], Dict[int, float]] = defaultdict(dict)
    metrics = ["latency_ns", "throughput"]
    with input_path.open("r", newline="") as f:
        for row in csv.DictReader(f):
            try:
                rep = int(row["replicate"])
            except (KeyError, ValueError):
                continue
            for m in metrics:
                try:
                    val = float(row[m])
                except (KeyError, ValueError):
                    continue
                key = (row["mode"], row["scenario"], row["book"], m)
                replicate_map[key][rep] = val
    return replicate_map


# ---------------------------------------------------------------------------
# Output writers
# ---------------------------------------------------------------------------

def write_cell_summary(
    cells: Dict[Tuple[str, str, str, str], List[float]],
    output_path: pathlib.Path,
    bootstrap_iters: int,
    alpha: float,
    seed: int,
) -> None:
    """Write per-cell summary statistics including bootstrap confidence intervals."""
    with output_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "mode", "scenario", "book", "metric",
            "n", "mean", "sd", "median", "p95",
            "ci95_low", "ci95_high",
        ])
        for (mode, scenario, book, metric), values in sorted(cells.items()):
            arr = np.array(values, dtype=float)
            ci_low, ci_high = bootstrap_ci_mean(values, bootstrap_iters, alpha, seed)
            writer.writerow([
                mode, scenario, book, metric,
                len(values),
                round(float(np.mean(arr)), 6),
                round(float(np.std(arr, ddof=1)) if len(values) > 1 else 0.0, 6),
                round(float(np.median(arr)), 6),
                round(float(np.percentile(arr, 95)), 6),
                round(ci_low, 6),
                round(ci_high, 6),
            ])


def write_pairwise_results(
    replicate_map: Dict[Tuple[str, str, str, str], Dict[int, float]],
    output_path: pathlib.Path,
    baseline_book: str,
    bootstrap_iters: int,
    perm_iters: int,
    alpha: float,
    seed: int,
) -> List[Dict]:
    """Compute and write pairwise comparisons between each comparator and the baseline.

    Defaults to analyzing 'latency_ns' for pairwise tests.
    """
    metric = "latency_ns"
    scenario_mode_pairs = sorted({(m, s) for (m, s, b, met) in replicate_map if met == metric})
    pairwise_results: List[Dict] = []

    with output_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "mode", "scenario", "baseline", "comparator",
            "n_pairs", "mean_diff_ns",
            "ci95_diff_low_ns", "ci95_diff_high_ns",
            "baseline_faster_fraction",
            "perm_p_two_sided", "sign_p_one_sided",
            "perm_p_holm", "sign_p_holm",
        ])

        for mode, scenario in scenario_mode_pairs:
            baseline_reps = replicate_map.get((mode, scenario, baseline_book, metric), {})
            comparator_books = sorted(
                book
                for (m, s, book, met) in replicate_map
                if m == mode and s == scenario and book != baseline_book and met == metric
            )

            for comparator in comparator_books:
                comparator_reps = replicate_map.get((mode, scenario, comparator), {})
                common_reps = sorted(set(baseline_reps) & set(comparator_reps))
                if len(common_reps) < 3:
                    continue

                diffs = [comparator_reps[r] - baseline_reps[r] for r in common_reps]
                mean_diff = float(np.mean(diffs))
                ci_low, ci_high = bootstrap_ci_mean_diff(diffs, bootstrap_iters, alpha, seed)
                faster_fraction = sum(1 for d in diffs if d > 0) / len(diffs)
                perm_p = paired_permutation_pvalue(diffs, perm_iters, seed)
                sign_p = sign_test_pvalue_one_sided(diffs)

                result = {
                    "mode": mode,
                    "scenario": scenario,
                    "baseline": baseline_book,
                    "comparator": comparator,
                    "n_pairs": float(len(common_reps)),
                    "mean_diff_ns": mean_diff,
                    "ci95_diff_low_ns": ci_low,
                    "ci95_diff_high_ns": ci_high,
                    "baseline_faster_fraction": faster_fraction,
                    "perm_p_two_sided": perm_p,
                    "sign_p_one_sided": sign_p,
                }
                pairwise_results.append(result)

        # Apply Holm-Bonferroni correction within each mode family.
        all_modes = sorted({r["mode"] for r in pairwise_results})
        for mode in all_modes:
            mode_subset = [r for r in pairwise_results if r["mode"] == mode]
            apply_holm_bonferroni(mode_subset, "perm_p_two_sided", "perm_p_holm")
            apply_holm_bonferroni(mode_subset, "sign_p_one_sided", "sign_p_holm")

        for result in sorted(pairwise_results, key=lambda r: (r["mode"], r["scenario"], r["comparator"])):
            writer.writerow([
                result["mode"], result["scenario"],
                result["baseline"], result["comparator"],
                int(result["n_pairs"]),
                round(result["mean_diff_ns"], 6),
                round(result["ci95_diff_low_ns"], 6),
                round(result["ci95_diff_high_ns"], 6),
                round(result["baseline_faster_fraction"], 6),
                round(result["perm_p_two_sided"], 8),
                round(result["sign_p_one_sided"], 8),
                round(result.get("perm_p_holm", 1.0), 8),
                round(result.get("sign_p_holm", 1.0), 8),
            ])

    return pairwise_results


def write_markdown_summary(
    pairwise_results: List[Dict],
    input_path: pathlib.Path,
    cell_summary_path: pathlib.Path,
    output_path: pathlib.Path,
    baseline_book: str,
    alpha: float,
) -> None:
    """Write a human-readable markdown summary of all pairwise comparisons."""
    with output_path.open("w") as f:
        f.write("# Hypothesis Testing Summary (scipy)\n\n")
        f.write(f"- Input: `{input_path}`\n")
        f.write(f"- Baseline: `{baseline_book}`\n")
        f.write(f"- Alpha: {alpha}\n\n")

        f.write("## Cell-Level Confidence Intervals\n\n")
        f.write(f"- See `{cell_summary_path}` for 95% bootstrap confidence intervals per (mode, scenario, book).\n\n")

        f.write("## Pairwise Baseline Comparisons\n\n")
        f.write(
            "Comparator minus baseline differences were tested using paired sign-flip "
            "permutation tests and one-sided sign tests, with Holm-Bonferroni correction "
            "applied per mode.\n\n"
        )

        if not pairwise_results:
            f.write("No valid pairwise comparisons were available.\n")
            return

        for mode in sorted({r["mode"] for r in pairwise_results}):
            f.write(f"### Mode: {mode}\n\n")
            mode_rows = [r for r in pairwise_results if r["mode"] == mode]
            for result in sorted(mode_rows, key=lambda r: (r["scenario"], r["comparator"])):
                baseline_advantage = (
                    result.get("sign_p_holm", 1.0) < alpha and result["mean_diff_ns"] > 0
                )
                verdict = "supports baseline advantage" if baseline_advantage else "inconclusive"
                f.write(
                    f"- {result['scenario']} vs {result['comparator']}: "
                    f"mean diff={result['mean_diff_ns']:.2f} ns "
                    f"(95% CI {result['ci95_diff_low_ns']:.2f} to {result['ci95_diff_high_ns']:.2f}), "
                    f"Holm sign p={result.get('sign_p_holm', 1.0):.4g} -> {verdict}.\n"
                )
            f.write("\n")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Analyse benchmark run data with scipy-backed statistical tests"
    )
    p.add_argument("--input", required=True, help="Path to results CSV")
    p.add_argument("--output-dir", default="", help="Output directory (default: sibling analysis_scipy/)")
    p.add_argument("--baseline", default="array", help="Baseline book for pairwise tests")
    p.add_argument("--alpha", type=float, default=0.05, help="Significance level")
    p.add_argument("--bootstrap-iters", type=int, default=10000, help="Bootstrap resamples for CIs")
    p.add_argument("--perm-iters", type=int, default=20000, help="Permutations for p-value estimate")
    p.add_argument("--seed", type=int, default=123, help="Random seed for reproducibility")
    return p


def main() -> int:
    args = build_parser().parse_args()
    input_path = pathlib.Path(args.input).resolve()

    if args.output_dir:
        output_dir = pathlib.Path(args.output_dir).resolve()
    else:
        output_dir = input_path.parent / "analysis_scipy"
    output_dir.mkdir(parents=True, exist_ok=True)

    cells = load_cells(input_path)
    replicate_map = load_replicate_map(input_path)

    cell_summary_path = output_dir / "cell_summary.csv"
    write_cell_summary(cells, cell_summary_path, args.bootstrap_iters, args.alpha, args.seed)
    print(f"Wrote: {cell_summary_path}")

    pairwise_path = output_dir / "pairwise_vs_baseline.csv"
    pairwise_results = write_pairwise_results(
        replicate_map, pairwise_path, args.baseline,
        args.bootstrap_iters, args.perm_iters, args.alpha, args.seed,
    )
    print(f"Wrote: {pairwise_path}")

    md_path = output_dir / "hypothesis_summary.md"
    write_markdown_summary(pairwise_results, input_path, cell_summary_path, md_path, args.baseline, args.alpha)
    print(f"Wrote: {md_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
