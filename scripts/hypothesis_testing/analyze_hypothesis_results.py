#!/usr/bin/env python3
"""Analyze run-level benchmark output with confidence intervals and hypothesis tests."""

from __future__ import annotations

import argparse
import csv
import math
import pathlib
import random
import statistics
from collections import defaultdict
from typing import Dict, Iterable, List, Tuple


def percentile(sorted_vals: List[float], q: float) -> float:
    if not sorted_vals:
        return float("nan")
    if q <= 0:
        return sorted_vals[0]
    if q >= 1:
        return sorted_vals[-1]
    pos = (len(sorted_vals) - 1) * q
    lo = int(math.floor(pos))
    hi = int(math.ceil(pos))
    if lo == hi:
        return sorted_vals[lo]
    w = pos - lo
    return sorted_vals[lo] * (1.0 - w) + sorted_vals[hi] * w


def bootstrap_ci_mean(values: List[float], iters: int, alpha: float, rng: random.Random) -> Tuple[float, float]:
    n = len(values)
    means = []
    for _ in range(iters):
        sample = [values[rng.randrange(n)] for _ in range(n)]
        means.append(statistics.fmean(sample))
    means.sort()
    return percentile(means, alpha / 2.0), percentile(means, 1.0 - alpha / 2.0)


def bootstrap_ci_diff(values: List[float], iters: int, alpha: float, rng: random.Random) -> Tuple[float, float]:
    n = len(values)
    samples = []
    for _ in range(iters):
        sample = [values[rng.randrange(n)] for _ in range(n)]
        samples.append(statistics.fmean(sample))
    samples.sort()
    return percentile(samples, alpha / 2.0), percentile(samples, 1.0 - alpha / 2.0)


def paired_sign_flip_pvalue(diffs: List[float], iters: int, rng: random.Random) -> float:
    # Two-sided p-value on mean(diffs) under symmetric null via random sign flips.
    n = len(diffs)
    obs = abs(statistics.fmean(diffs))

    if n <= 20 and (1 << n) <= 200000:
        ge = 0
        total = 1 << n
        for mask in range(total):
            s = 0.0
            for i, d in enumerate(diffs):
                s += d if ((mask >> i) & 1) else -d
            if abs(s / n) >= obs - 1e-12:
                ge += 1
        return ge / total

    ge = 0
    for _ in range(iters):
        s = 0.0
        for d in diffs:
            s += d if rng.random() < 0.5 else -d
        if abs(s / n) >= obs - 1e-12:
            ge += 1
    return (ge + 1) / (iters + 1)


def one_sided_sign_test_pvalue(diffs: List[float]) -> float:
    # H1 directional test: comparator - baseline > 0 (baseline faster).
    pos = sum(1 for d in diffs if d > 0)
    neg = sum(1 for d in diffs if d < 0)
    n = pos + neg
    if n == 0:
        return 1.0

    # exact tail P(X >= pos), X~Bin(n,0.5)
    tail = 0.0
    for k in range(pos, n + 1):
        tail += math.comb(n, k)
    return tail / (2 ** n)


def holm_bonferroni(rows: List[Dict[str, float]], p_key: str, out_key: str) -> None:
    indexed = sorted(enumerate(rows), key=lambda x: x[1][p_key])
    m = len(rows)
    adjusted = [0.0] * m
    running_max = 0.0
    for rank, (idx, row) in enumerate(indexed, start=1):
        adj = min(1.0, row[p_key] * (m - rank + 1))
        running_max = max(running_max, adj)
        adjusted[idx] = running_max
    for i, row in enumerate(rows):
        row[out_key] = adjusted[i]


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Analyze hypothesis-testing benchmark run data")
    p.add_argument("--input", required=True, help="Path to raw_run_results.csv")
    p.add_argument("--output-dir", default="", help="Output directory (default: sibling analysis/)")
    p.add_argument("--baseline", default="array", help="Baseline book for pairwise tests")
    p.add_argument("--alpha", type=float, default=0.05)
    p.add_argument("--bootstrap-iters", type=int, default=10000)
    p.add_argument("--perm-iters", type=int, default=20000)
    p.add_argument("--seed", type=int, default=123)
    return p


def main() -> int:
    args = build_parser().parse_args()
    input_path = pathlib.Path(args.input).resolve()
    output_dir = pathlib.Path(args.output_dir).resolve() if args.output_dir else input_path.parent / "analysis"
    output_dir.mkdir(parents=True, exist_ok=True)

    rows: List[Dict[str, str]] = []
    with input_path.open("r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)

    rng = random.Random(args.seed)

    # Cell-level summaries: (mode, scenario, book)
    cells: Dict[Tuple[str, str, str], List[float]] = defaultdict(list)
    for r in rows:
        try:
            val = float(r["latency_ns"])
        except Exception:
            continue
        cells[(r["mode"], r["scenario"], r["book"])].append(val)

    cell_summary_path = output_dir / "cell_summary.csv"
    with cell_summary_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "mode",
                "scenario",
                "book",
                "n",
                "mean_ns",
                "sd_ns",
                "median_ns",
                "p95_ns",
                "ci95_low_ns",
                "ci95_high_ns",
            ]
        )
        for (mode, scenario, book), vals in sorted(cells.items()):
            vals_sorted = sorted(vals)
            mean_v = statistics.fmean(vals)
            sd_v = statistics.stdev(vals) if len(vals) > 1 else 0.0
            ci_lo, ci_hi = bootstrap_ci_mean(vals, args.bootstrap_iters, args.alpha, rng)
            writer.writerow(
                [
                    mode,
                    scenario,
                    book,
                    len(vals),
                    round(mean_v, 6),
                    round(sd_v, 6),
                    round(percentile(vals_sorted, 0.5), 6),
                    round(percentile(vals_sorted, 0.95), 6),
                    round(ci_lo, 6),
                    round(ci_hi, 6),
                ]
            )

    # Pairwise vs baseline with replicate pairing.
    # key -> replicate -> latency
    replicate_map: Dict[Tuple[str, str, str], Dict[int, float]] = defaultdict(dict)
    for r in rows:
        try:
            rep = int(r["replicate"])
            lat = float(r["latency_ns"])
        except Exception:
            continue
        replicate_map[(r["mode"], r["scenario"], r["book"])][rep] = lat

    pairwise: List[Dict[str, float]] = []
    pairwise_path = output_dir / "pairwise_vs_baseline.csv"
    with pairwise_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "mode",
                "scenario",
                "baseline",
                "comparator",
                "n_pairs",
                "mean_diff_ns",
                "ci95_diff_low_ns",
                "ci95_diff_high_ns",
                "baseline_faster_fraction",
                "perm_p_two_sided",
                "sign_p_one_sided",
                "perm_p_holm",
                "sign_p_holm",
            ]
        )

        keys = sorted({(m, s) for (m, s, _) in replicate_map.keys()})
        for mode, scenario in keys:
            base = replicate_map.get((mode, scenario, args.baseline), {})
            comparators = sorted({b for (m, s, b) in replicate_map.keys() if m == mode and s == scenario and b != args.baseline})
            for comp in comparators:
                cmp_vals = replicate_map.get((mode, scenario, comp), {})
                common_reps = sorted(set(base.keys()) & set(cmp_vals.keys()))
                if len(common_reps) < 3:
                    continue

                diffs = [cmp_vals[r] - base[r] for r in common_reps]
                mean_diff = statistics.fmean(diffs)
                ci_lo, ci_hi = bootstrap_ci_diff(diffs, args.bootstrap_iters, args.alpha, rng)
                faster_frac = sum(1 for d in diffs if d > 0) / len(diffs)
                perm_p = paired_sign_flip_pvalue(diffs, args.perm_iters, rng)
                sign_p = one_sided_sign_test_pvalue(diffs)

                row_obj: Dict[str, float] = {
                    "mode": mode,
                    "scenario": scenario,
                    "baseline": args.baseline,
                    "comparator": comp,
                    "n_pairs": float(len(common_reps)),
                    "mean_diff_ns": mean_diff,
                    "ci95_diff_low_ns": ci_lo,
                    "ci95_diff_high_ns": ci_hi,
                    "baseline_faster_fraction": faster_frac,
                    "perm_p_two_sided": perm_p,
                    "sign_p_one_sided": sign_p,
                }
                pairwise.append(row_obj)

        # Holm correction per mode
        for mode in sorted({r["mode"] for r in pairwise}):
            subset = [r for r in pairwise if r["mode"] == mode]
            holm_bonferroni(subset, "perm_p_two_sided", "perm_p_holm")
            holm_bonferroni(subset, "sign_p_one_sided", "sign_p_holm")

        for r in sorted(pairwise, key=lambda x: (x["mode"], x["scenario"], x["comparator"])):
            writer.writerow(
                [
                    r["mode"],
                    r["scenario"],
                    r["baseline"],
                    r["comparator"],
                    int(r["n_pairs"]),
                    round(r["mean_diff_ns"], 6),
                    round(r["ci95_diff_low_ns"], 6),
                    round(r["ci95_diff_high_ns"], 6),
                    round(r["baseline_faster_fraction"], 6),
                    round(r["perm_p_two_sided"], 8),
                    round(r["sign_p_one_sided"], 8),
                    round(r.get("perm_p_holm", 1.0), 8),
                    round(r.get("sign_p_holm", 1.0), 8),
                ]
            )

    # Human-readable markdown summary
    md_path = output_dir / "hypothesis_summary.md"
    with md_path.open("w") as f:
        f.write("# Hypothesis Testing Summary\n\n")
        f.write(f"- Input: `{input_path}`\n")
        f.write(f"- Baseline: `{args.baseline}`\n")
        f.write(f"- Alpha: {args.alpha}\n\n")

        f.write("## Cell-Level Confidence Intervals\n\n")
        f.write(f"- See `{cell_summary_path}` for 95% bootstrap confidence intervals per (mode, scenario, book).\n\n")

        f.write("## Pairwise Baseline Comparisons\n\n")
        f.write("For each (mode, scenario), comparator minus baseline latency differences were tested using paired sign-flip permutation tests and one-sided sign tests, with Holm correction per mode.\n\n")

        if not pairwise:
            f.write("No valid pairwise comparisons were available.\n")
        else:
            for mode in sorted({r["mode"] for r in pairwise}):
                f.write(f"### Mode: {mode}\n\n")
                mode_rows = [r for r in pairwise if r["mode"] == mode]
                for r in sorted(mode_rows, key=lambda x: (x["scenario"], x["comparator"])):
                    support = r.get("sign_p_holm", 1.0) < args.alpha and r["mean_diff_ns"] > 0
                    verdict = "supports baseline advantage" if support else "inconclusive"
                    f.write(
                        "- "
                        f"{r['scenario']} vs {r['comparator']}: "
                        f"mean diff={r['mean_diff_ns']:.2f} ns "
                        f"(95% CI {r['ci95_diff_low_ns']:.2f} to {r['ci95_diff_high_ns']:.2f}), "
                        f"Holm sign p={r.get('sign_p_holm', 1.0):.4g} -> {verdict}.\n"
                    )
                f.write("\n")

    print(f"Wrote: {cell_summary_path}")
    print(f"Wrote: {pairwise_path}")
    print(f"Wrote: {md_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
