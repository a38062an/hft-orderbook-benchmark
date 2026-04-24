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

    # Support both legacy title-case schema and newer lowercase schema.
    column_aliases = {
        "mode": "Mode",
        "book": "Book",
        "scenario": "Scenario",
        "command": "Command",
        "latency_ns": "Latency_ns",
        "latency_stddev_ns": "LatencyStdDev_ns",
        "p99_ns": "P99_ns",
        "p99_stddev_ns": "P99StdDev_ns",
        "max_ns": "Max_ns",
        "throughput": "Throughput",
        "throughput_stddev": "ThroughputStdDev",
        "network_ns": "Network_ns",
        "queue_ns": "Queue_ns",
        "engine_ns": "Engine_ns",
        "insert_ns": "Insert_ns",
        "cancel_ns": "Cancel_ns",
        "lookup_ns": "Lookup_ns",
        "match_ns": "Match_ns",
        "producers": "Producers",
        "dropped": "Dropped",
        "peakdepth": "PeakDepth",
        "mpscque_ns": "MpscQue_ns",
        "mpscquep99_ns": "MpscQueP99_ns",
        "mpsceng_ns": "MpscEng_ns",
        "mpscengp99_ns": "MpscEngP99_ns",
    }
    rename_map = {}
    for source_column, target_column in column_aliases.items():
        if source_column in results_dataframe.columns and target_column not in results_dataframe.columns:
            rename_map[source_column] = target_column
    if rename_map:
        results_dataframe = results_dataframe.rename(columns=rename_map)

    required_columns = ["Mode", "Book", "Scenario"]
    missing_required = [column for column in required_columns if column not in results_dataframe.columns]
    if missing_required:
        raise KeyError(f"Missing required columns in results CSV: {missing_required}")

    for column_name in NUMERIC_COLUMNS:
        if column_name in results_dataframe.columns:
            results_dataframe[column_name] = pd.to_numeric(
                results_dataframe[column_name], errors="coerce"
            ).fillna(0.0)

    # Normalize categorical text columns to lowercase for stable ordering.
    results_dataframe["Mode"] = results_dataframe["Mode"].astype(str).str.lower()
    results_dataframe["Book"] = results_dataframe["Book"].astype(str).str.lower()
    results_dataframe["Scenario"] = results_dataframe["Scenario"].astype(str).str.lower()

    # Newer campaign exports store producer count only in the command string.
    # Recover Producers before replicate aggregation so MPSC rows are not
    # incorrectly collapsed across 1/2/4/8 producers.
    if "Producers" not in results_dataframe.columns and "Command" in results_dataframe.columns:
        extracted = results_dataframe["Command"].astype(str).str.extract(r"--producers\s+(\d+)", expand=False)
        if extracted.notna().any():
            results_dataframe["Producers"] = pd.to_numeric(extracted, errors="coerce")

    if "Producers" in results_dataframe.columns:
        results_dataframe["Producers"] = pd.to_numeric(
            results_dataframe["Producers"], errors="coerce"
        )

    # Fallback mapping for MPSC component fields when dedicated MPSC columns
    # are absent in the CSV schema.
    if "MpscQue_ns" not in results_dataframe.columns and "Queue_ns" in results_dataframe.columns:
        results_dataframe["MpscQue_ns"] = results_dataframe["Queue_ns"]
    if "MpscEng_ns" not in results_dataframe.columns and "Engine_ns" in results_dataframe.columns:
        results_dataframe["MpscEng_ns"] = results_dataframe["Engine_ns"]
    if "MpscQueP99_ns" not in results_dataframe.columns and "P99_ns" in results_dataframe.columns:
        results_dataframe["MpscQueP99_ns"] = results_dataframe["P99_ns"]
    if "MpscEngP99_ns" not in results_dataframe.columns:
        results_dataframe["MpscEngP99_ns"] = 0.0
    if "Dropped" not in results_dataframe.columns:
        results_dataframe["Dropped"] = 0.0
    if "PeakDepth" not in results_dataframe.columns:
        results_dataframe["PeakDepth"] = 0.0

    # Replicate-level datasets can contain multiple rows for the same
    # Mode/Scenario/Book(/Producers) key; aggregate to mean values expected by
    # the plotting pipeline.
    group_columns = [column for column in ["Mode", "Scenario", "Book", "Producers"] if column in results_dataframe.columns]
    if group_columns:
        # Keep direct/gateway rows in grouped aggregation even when Producers is
        # only populated for MPSC rows.
        if "Producers" in group_columns:
            results_dataframe["Producers"] = results_dataframe["Producers"].fillna(-1)

        duplicate_mask = results_dataframe.duplicated(subset=group_columns, keep=False)
        if duplicate_mask.any():
            grouped = results_dataframe.groupby(group_columns, observed=False, as_index=False)

            # Use mean values for primary metrics, then derive variability from
            # replicate-to-replicate spread instead of averaging per-row stddev
            # columns (which are often zero when each row is a single run).
            mean_columns = [
                column
                for column in NUMERIC_COLUMNS
                if column in results_dataframe.columns
                and column not in {"LatencyStdDev_ns", "P99StdDev_ns", "ThroughputStdDev"}
            ]
            results_dataframe = grouped[mean_columns].mean(numeric_only=True)

            std_column_map = {
                "Latency_ns": "LatencyStdDev_ns",
                "P99_ns": "P99StdDev_ns",
                "Throughput": "ThroughputStdDev",
            }
            for value_column, std_column in std_column_map.items():
                if value_column in results_dataframe.columns or value_column in mean_columns:
                    if value_column in grouped.obj.columns:
                        std_dataframe = grouped[[value_column]].std(numeric_only=True).rename(
                            columns={value_column: std_column}
                        )
                        std_dataframe[std_column] = std_dataframe[std_column].fillna(0.0)
                        results_dataframe = results_dataframe.merge(std_dataframe, on=group_columns, how="left")

    if "Producers" in results_dataframe.columns:
        results_dataframe["Producers"] = results_dataframe["Producers"].replace(-1, pd.NA)

    if "Book" in results_dataframe.columns:
        results_dataframe["Book"] = pd.Categorical(
            results_dataframe["Book"], categories=BOOK_ORDER, ordered=True
        )

    if "Scenario" in results_dataframe.columns:
        results_dataframe["Scenario"] = pd.Categorical(
            results_dataframe["Scenario"], categories=SCENARIO_ORDER, ordered=True
        )

    if "Producers" in results_dataframe.columns:
        producer_numeric = pd.to_numeric(results_dataframe["Producers"], errors="coerce")
        results_dataframe["Producers"] = pd.Categorical(
            producer_numeric,
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
    for column_name in [
        "Producers",
        "MpscQue_ns",
        "MpscQueP99_ns",
        "MpscEng_ns",
        "MpscEngP99_ns",
        "Dropped",
        "PeakDepth",
    ]:
        if column_name not in mpsc_dataframe.columns:
            mpsc_dataframe[column_name] = 0.0

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
