#!/usr/bin/env python3
"""
Verify the custom statistical functions in analyze_hypothesis_results.py
against scipy equivalents.  Run this from the project root:

    python3 scripts/hypothesis_testing/verify_stats.py

Pass/fail is printed per test; a zero exit code means all checks passed.
"""

import csv
import math
import pathlib
import random
import statistics
import sys

import numpy as np
from scipy import stats as sp

# ── import the functions we want to verify ─────────────────────────────────
sys.path.insert(0, str(pathlib.Path(__file__).parent))
from analyze_hypothesis_results import (
    bootstrap_ci_mean,
    paired_sign_flip_pvalue,
    percentile,
)

RESULTS_CSV = pathlib.Path("results/results.csv")
ALPHA = 0.05
SEED = 123
BOOTSTRAP_ITERS = 10_000
PERM_ITERS = 20_000
TOL_CI = 3.0      # ns — acceptable difference between CI bounds
TOL_P = 0.02      # acceptable absolute difference in p-value


# ── helpers ────────────────────────────────────────────────────────────────

def load_cell(mode: str, scenario: str, book: str) -> list[float]:
    vals = []
    with RESULTS_CSV.open() as f:
        for row in csv.DictReader(f):
            if row["mode"] == mode and row["scenario"] == scenario and row["book"] == book:
                try:
                    vals.append(float(row["latency_ns"]))
                except ValueError:
                    pass
    return vals


def check(label: str, custom: float, reference: float, tol: float, unit: str = "") -> bool:
    diff = abs(custom - reference)
    ok = diff <= tol
    mark = "✅ PASS" if ok else "❌ FAIL"
    print(f"  {mark}  {label}: custom={custom:.4f}{unit}  scipy={reference:.4f}{unit}  diff={diff:.4f}{unit}  (tol={tol}{unit})")
    return ok


# ── test 1 : bootstrap CI on array/mixed latency ───────────────────────────

def test_bootstrap_ci() -> bool:
    print("\n── Test 1: 95% Bootstrap CI (array, direct, mixed) ──")
    vals = load_cell("direct", "mixed", "array")
    if not vals:
        print("  SKIP — no data found")
        return True

    rng = random.Random(SEED)
    lo_c, hi_c = bootstrap_ci_mean(vals, BOOTSTRAP_ITERS, ALPHA, rng)

    # scipy bootstrap (percentile method, same seed via numpy)
    rng_np = np.random.default_rng(SEED)
    result = sp.bootstrap(
        (np.array(vals),),
        np.mean,
        n_resamples=BOOTSTRAP_ITERS,
        confidence_level=1 - ALPHA,
        method="percentile",
        random_state=rng_np,
    )
    lo_s, hi_s = result.confidence_interval.low, result.confidence_interval.high

    ok1 = check("CI lower bound", lo_c, lo_s, TOL_CI, " ns")
    ok2 = check("CI upper bound", hi_c, hi_s, TOL_CI, " ns")
    return ok1 and ok2


# ── test 2 : permutation p-value on array vs map (direct, mixed) ───────────

def test_permutation_pvalue() -> bool:
    print("\n── Test 2: Paired sign-flip permutation p-value (array vs map, direct, mixed) ──")

    def load_rep(mode, scenario, book) -> dict[int, float]:
        m = {}
        with RESULTS_CSV.open() as f:
            for row in csv.DictReader(f):
                if row["mode"] == mode and row["scenario"] == scenario and row["book"] == book:
                    try:
                        m[int(row["replicate"])] = float(row["latency_ns"])
                    except ValueError:
                        pass
        return m

    base = load_rep("direct", "mixed", "array")
    comp = load_rep("direct", "mixed", "map")
    common = sorted(set(base) & set(comp))
    if len(common) < 3:
        print("  SKIP — insufficient paired data")
        return True

    diffs = [comp[r] - base[r] for r in common]

    rng = random.Random(SEED)
    p_custom = paired_sign_flip_pvalue(diffs, PERM_ITERS, rng)

    # scipy permutation_test (two-sided, using mean of differences)
    result = sp.permutation_test(
        (np.array(diffs),),
        lambda x: np.mean(x),
        permutation_type="samples",
        n_resamples=PERM_ITERS,
        alternative="two-sided",
        random_state=SEED,
    )
    p_scipy = result.pvalue

    return check("p-value", p_custom, p_scipy, TOL_P)


# ── test 3 : percentile helper ─────────────────────────────────────────────

def test_percentile() -> bool:
    print("\n── Test 3: percentile() helper vs numpy ──")
    data = sorted([12.5, 7.1, 19.3, 44.2, 3.8, 101.0, 55.5, 22.1])
    ok = True
    for q in [0.025, 0.25, 0.5, 0.75, 0.975]:
        c = percentile(data, q)
        n = float(np.percentile(data, q * 100, method="linear"))
        ok &= check(f"q={q}", c, n, 1e-9)
    return ok


# ── main ───────────────────────────────────────────────────────────────────

def main() -> int:
    results = [
        test_percentile(),
        test_bootstrap_ci(),
        test_permutation_pvalue(),
    ]
    print()
    if all(results):
        print("All checks passed ✅")
        return 0
    else:
        failed = results.count(False)
        print(f"{failed} check(s) FAILED ❌")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
