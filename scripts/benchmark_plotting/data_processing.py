"""Data loading and transformation helpers for benchmark plots."""

from __future__ import annotations

from pathlib import Path
from typing import Iterable

import pandas as pd

from .constants import BOOK_ORDER, MPSC_PRODUCER_ORDER, SCENARIO_ORDER


NUMERIC_COLUMNS = [
    "Latency_ns",
    "LatencyStdDev_ns",
    "P99_ns",
    "P99StdDev_ns",
    "Max_ns",
    "Throughput",
    "ThroughputStdDev",
    "Network_ns",
    "Queue_ns",
    "Engine_ns",
    "Insert_ns",
    "Cancel_ns",
    "Lookup_ns",
    "Match_ns",
    "Producers",
    "Dropped",
    "PeakDepth",
    "MpscQue_ns",
    "MpscQueP99_ns",
    "MpscEng_ns",
    "MpscEngP99_ns",
]


class DatasetSplit:
    """Container for direct, gateway, and mpsc benchmark subsets."""

    def __init__(self, full: pd.DataFrame) -> None:
        self.full = full
        self.direct = full[full["Mode"] == "direct"].copy()
        self.gateway = full[full["Mode"] == "gateway"].copy()
        self.mpsc = full[full["Mode"] == "mpsc"].copy()



def load_results(csv_path: Path) -> pd.DataFrame:
    """Load benchmark CSV and normalize datatypes/orderings."""
    if not csv_path.exists():
        raise FileNotFoundError(f"Results file not found: {csv_path}")

    results_dataframe = pd.read_csv(csv_path)
    for column_name in NUMERIC_COLUMNS:
        if column_name in results_dataframe.columns:
            results_dataframe[column_name] = pd.to_numeric(
                results_dataframe[column_name], errors="coerce"
            ).fillna(0.0)

    if "Book" in results_dataframe.columns:
        results_dataframe["Book"] = pd.Categorical(
            results_dataframe["Book"], categories=BOOK_ORDER, ordered=True
        )

    if "Scenario" in results_dataframe.columns:
        results_dataframe["Scenario"] = pd.Categorical(
            results_dataframe["Scenario"], categories=SCENARIO_ORDER, ordered=True
        )

    if "Producers" in results_dataframe.columns:
        results_dataframe["Producers"] = pd.Categorical(
            results_dataframe["Producers"].astype(int),
            categories=MPSC_PRODUCER_ORDER,
            ordered=True,
        )

    return results_dataframe.sort_values(["Mode", "Scenario", "Book"]).reset_index(drop=True)



def split_modes(results_dataframe: pd.DataFrame) -> DatasetSplit:
    """Split loaded results by benchmarking mode."""
    return DatasetSplit(results_dataframe)



def direct_latency_matrix(direct_dataframe: pd.DataFrame, value_column: str) -> pd.DataFrame:
    """Build a scenario x book matrix for heatmap visualizations."""
    return direct_dataframe.pivot(index="Scenario", columns="Book", values=value_column)



def direct_slowdown_matrix(direct_dataframe: pd.DataFrame) -> pd.DataFrame:
    """Compute per-scenario slowdown relative to best implementation."""
    direct_copy = direct_dataframe.copy()
    direct_copy["ScenarioBest"] = direct_copy.groupby("Scenario", observed=False)["Latency_ns"].transform("min")
    direct_copy["SlowdownVsBest"] = direct_copy["Latency_ns"] / direct_copy["ScenarioBest"]
    return direct_copy.pivot(index="Scenario", columns="Book", values="SlowdownVsBest")



def filter_books(results_dataframe: pd.DataFrame, books: Iterable[str]) -> pd.DataFrame:
    """Return rows restricted to a chosen book subset while preserving order."""
    filtered_dataframe = results_dataframe[results_dataframe["Book"].isin(list(books))].copy()
    if "Book" in filtered_dataframe.columns:
        filtered_dataframe["Book"] = pd.Categorical(
            filtered_dataframe["Book"].astype(str), categories=list(books), ordered=True
        )
    return filtered_dataframe.sort_values(["Scenario", "Book"]).reset_index(drop=True)



def slowdown_matrix(results_dataframe: pd.DataFrame) -> pd.DataFrame:
    """Compute per-scenario slowdown relative to the best book in the provided subset."""
    slowdown_dataframe = results_dataframe.copy()
    slowdown_dataframe["ScenarioBest"] = slowdown_dataframe.groupby(
        "Scenario", observed=False
    )["Latency_ns"].transform("min")
    slowdown_dataframe["SlowdownVsBest"] = (
        slowdown_dataframe["Latency_ns"] / slowdown_dataframe["ScenarioBest"]
    )
    return slowdown_dataframe.pivot(index="Scenario", columns="Book", values="SlowdownVsBest")



def direct_tail_ratio_dataframe(direct_dataframe: pd.DataFrame) -> pd.DataFrame:
    """Compute tail ratio P99/mean for each direct scenario/book point."""
    tail_ratio_dataframe = direct_dataframe.copy()
    tail_ratio_dataframe["P99MeanRatio"] = (
        tail_ratio_dataframe["P99_ns"] / tail_ratio_dataframe["Latency_ns"].replace(0, pd.NA)
    )
    return tail_ratio_dataframe



def gateway_dense_decomposition_dataframe(gateway_dataframe: pd.DataFrame) -> pd.DataFrame:
    """Prepare dense_full decomposition columns for stacked bar plotting."""
    dense_dataframe = gateway_dataframe[gateway_dataframe["Scenario"] == "dense_full"].copy()
    if dense_dataframe.empty:
        return dense_dataframe

    dense_dataframe["ResidualOther_ns"] = (
        dense_dataframe["Latency_ns"]
        - dense_dataframe["Network_ns"]
        - dense_dataframe["Queue_ns"]
        - dense_dataframe["Engine_ns"]
    ).clip(lower=0.0)
    return dense_dataframe



def gateway_latency_dataframe(gateway_dataframe: pd.DataFrame) -> pd.DataFrame:
    """Return clean gateway rows for scenario-wise comparisons."""
    return gateway_dataframe[["Scenario", "Book", "Latency_ns", "LatencyStdDev_ns"]].copy()



def mpsc_scaling_dataframe(mpsc_dataframe: pd.DataFrame) -> pd.DataFrame:
    """Return key columns used by MPSC scaling plots."""
    return mpsc_dataframe[
        [
            "Book",
            "Producers",
            "Throughput",
            "MpscQue_ns",
            "MpscQueP99_ns",
            "MpscEng_ns",
            "MpscEngP99_ns",
            "Dropped",
            "PeakDepth",
        ]
    ].copy()



def available_modes(results_dataframe: pd.DataFrame) -> Iterable[str]:
    """List present benchmark modes in results file."""
    return sorted(results_dataframe["Mode"].dropna().unique())
