"""Automated analysis table generation for dissertation narrative drafting."""

from __future__ import annotations

from pathlib import Path
from typing import List

import pandas as pd



def _best_book_by_scenario(results_dataframe: pd.DataFrame) -> pd.DataFrame:
    """Return best-latency row per scenario for a given mode dataframe."""
    if results_dataframe.empty:
        return pd.DataFrame(columns=["Scenario", "Book", "Latency_ns"])

    best_rows = (
        results_dataframe.sort_values(["Scenario", "Latency_ns", "Book"])
        .groupby("Scenario", observed=False)
        .first()
        .reset_index()[["Scenario", "Book", "Latency_ns"]]
    )
    return best_rows



def _appendix_best_book_table(
    direct_dataframe: pd.DataFrame,
    gateway_dataframe: pd.DataFrame,
) -> List[str]:
    """Build markdown table lines for direct vs gateway best book per scenario."""
    direct_best = _best_book_by_scenario(direct_dataframe).rename(
        columns={"Book": "DirectBestBook", "Latency_ns": "DirectLatency_ns"}
    )
    gateway_best = _best_book_by_scenario(gateway_dataframe).rename(
        columns={"Book": "GatewayBestBook", "Latency_ns": "GatewayLatency_ns"}
    )

    merged = direct_best.merge(gateway_best, on="Scenario", how="outer")
    merged = merged.sort_values("Scenario")

    table_lines: List[str] = [
        "| Scenario | Direct Best | Direct Latency (ns) | Gateway Best | Gateway Latency (ns) |",
        "|---|---:|---:|---:|---:|",
    ]

    for _, row in merged.iterrows():
        direct_book = row["DirectBestBook"] if pd.notna(row.get("DirectBestBook")) else "n/a"
        direct_latency = (
            f"{row['DirectLatency_ns']:.2f}" if pd.notna(row.get("DirectLatency_ns")) else "n/a"
        )
        gateway_book = row["GatewayBestBook"] if pd.notna(row.get("GatewayBestBook")) else "n/a"
        gateway_latency = (
            f"{row['GatewayLatency_ns']:.2f}" if pd.notna(row.get("GatewayLatency_ns")) else "n/a"
        )
        table_lines.append(
            f"| {row['Scenario']} | {direct_book} | {direct_latency} | {gateway_book} | {gateway_latency} |"
        )

    return table_lines



def _write_rank_table_file(
    direct_dataframe: pd.DataFrame,
    gateway_dataframe: pd.DataFrame,
    output_dir: Path,
) -> Path:
    """Write a standalone rank table artifact for thesis appendix insertion."""
    output_path = output_dir / "tables" / "rank_direct_vs_gateway.md"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    lines: List[str] = [
        "# Rank Table: Best Book per Scenario",
        "",
        *(_appendix_best_book_table(direct_dataframe, gateway_dataframe)),
        "",
    ]
    output_path.write_text("\n".join(lines), encoding="utf-8")
    return output_path



def _write_mixed_validity_table(output_dir: Path) -> Path:
    """Write a compact scenario-validity table documenting mixed configuration."""
    output_path = output_dir / "tables" / "mixed_scenario_validity.md"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Mixed Scenario Validity Table (Generator-Defined)",
        "",
        "| Scenario | Near-Spread Share (%) | Deep Share (%) | Extreme Share (%) | Cancel Branch (%) |",
        "|---|---:|---:|---:|---:|",
        "| tight_spread | 100 | 0 | 0 | 0 |",
        "| mixed | 55 | 30 | 15 | 18 |",
        "| high_cancellation | 100 | 0 | 0 | 50 |",
        "",
        "Notes:",
        "- Shares are based on generator branch configuration, not post-match occupancy.",
        "- Cancel branch percentages are conditional generator probabilities.",
        "",
    ]
    output_path.write_text("\n".join(lines), encoding="utf-8")
    return output_path



def _best_book_for_metric(
    results_dataframe: pd.DataFrame,
    scenario: str,
    metric_column: str,
) -> tuple[str, str]:
    """Return best book and metric value text for one scenario/metric pair."""
    scenario_rows = results_dataframe[results_dataframe["Scenario"] == scenario].copy()
    if scenario_rows.empty or metric_column not in scenario_rows.columns:
        return ("n/a", "n/a")

    valid_rows = scenario_rows[scenario_rows[metric_column] > 0]
    if valid_rows.empty:
        return ("n/a", "n/a")

    best_row = valid_rows.loc[valid_rows[metric_column].idxmin()]
    return (str(best_row["Book"]), f"{best_row[metric_column]:.2f}")



def _top_books_for_metric(
    results_dataframe: pd.DataFrame,
    scenario: str,
    metric_column: str,
    books: List[str],
    top_n: int = 2,
) -> List[tuple[str, str]]:
    """Return top-N books and metric values for one scenario/metric pair."""
    scenario_rows = results_dataframe[
        (results_dataframe["Scenario"] == scenario) & (results_dataframe["Book"].isin(books))
    ].copy()
    if scenario_rows.empty or metric_column not in scenario_rows.columns:
        return []

    valid_rows = scenario_rows[scenario_rows[metric_column] > 0].sort_values(
        [metric_column, "Book"]
    )
    if valid_rows.empty:
        return []

    top_rows = valid_rows.head(top_n)
    return [(str(row["Book"]), f"{row[metric_column]:.2f}") for _, row in top_rows.iterrows()]



def _write_operation_winner_table(direct_dataframe: pd.DataFrame, output_dir: Path) -> Path:
    """Write best order book per scenario for Add/Cancel/Lookup operation latency."""
    output_path = output_dir / "tables" / "operation_winners_direct.md"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    scenarios = list(direct_dataframe["Scenario"].dropna().unique())

    lines = [
        "# Operation Winners by Scenario (Direct Mode)",
        "",
        "| Scenario | Best Add Book | Add Latency (ns) | Best Cancel Book | Cancel Latency (ns) | Best Lookup Book | Lookup Latency (ns) |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]

    for scenario in scenarios:
        add_book, add_latency = _best_book_for_metric(direct_dataframe, scenario, "Insert_ns")
        cancel_book, cancel_latency = _best_book_for_metric(direct_dataframe, scenario, "Cancel_ns")
        lookup_book, lookup_latency = _best_book_for_metric(direct_dataframe, scenario, "Lookup_ns")
        lines.append(
            f"| {scenario} | {add_book} | {add_latency} | {cancel_book} | {cancel_latency} | {lookup_book} | {lookup_latency} |"
        )

    lines.extend(
        [
            "",
            "Notes:",
            "- Add corresponds to Insert_ns, Cancel to Cancel_ns, and Lookup to Lookup_ns.",
            "- `n/a` in Cancel means that scenario has no cancellation workload (Cancel_ns is zero for all books).",
            "",
        ]
    )
    output_path.write_text("\n".join(lines), encoding="utf-8")
    return output_path



def _write_operation_winner_table_no_array(
    direct_dataframe: pd.DataFrame,
    output_dir: Path,
) -> Path:
    """Write best operation winners by scenario with array excluded."""
    output_path = output_dir / "tables" / "operation_winners_no_array_direct.md"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    scenarios = list(direct_dataframe["Scenario"].dropna().unique())
    eligible_books = ["hybrid", "pool", "map", "vector"]

    filtered_direct = direct_dataframe[direct_dataframe["Book"].isin(eligible_books)].copy()

    lines = [
        "# Operation Winners by Scenario (Direct Mode, Array Excluded)",
        "",
        "| Scenario | Best Add Book | Add Latency (ns) | Best Cancel Book | Cancel Latency (ns) | Best Lookup Book | Lookup Latency (ns) |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]

    for scenario in scenarios:
        add_book, add_latency = _best_book_for_metric(filtered_direct, scenario, "Insert_ns")
        cancel_book, cancel_latency = _best_book_for_metric(filtered_direct, scenario, "Cancel_ns")
        lookup_book, lookup_latency = _best_book_for_metric(filtered_direct, scenario, "Lookup_ns")
        lines.append(
            f"| {scenario} | {add_book} | {add_latency} | {cancel_book} | {cancel_latency} | {lookup_book} | {lookup_latency} |"
        )

    lines.extend(
        [
            "",
            "Notes:",
            "- Array is excluded to avoid fixed-range/fixed-tick structural bias in this comparison.",
            "- `n/a` in Cancel means that scenario has no cancellation workload (Cancel_ns is zero for all eligible books).",
            "",
        ]
    )
    output_path.write_text("\n".join(lines), encoding="utf-8")
    return output_path



def _write_cancel_winner_table(direct_dataframe: pd.DataFrame, output_dir: Path) -> Path:
    """Write cancel-specific winners for scenarios that actually contain cancellation events."""
    output_path = output_dir / "tables" / "cancel_winners_direct.md"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    scenarios = list(direct_dataframe["Scenario"].dropna().unique())

    lines = [
        "# Cancel Winners by Scenario (Direct Mode)",
        "",
        "| Scenario | Best Cancel Book | Cancel Latency (ns) | Second-Best Cancel Book | Second-Best Latency (ns) |",
        "|---|---:|---:|---:|---:|",
    ]

    has_rows = False
    for scenario in scenarios:
        top_two = _top_books_for_metric(
            direct_dataframe,
            scenario,
            "Cancel_ns",
            books=["array", "hybrid", "pool", "map", "vector"],
            top_n=2,
        )
        if not top_two:
            continue

        has_rows = True
        best_book, best_latency = top_two[0]
        second_book, second_latency = top_two[1] if len(top_two) > 1 else ("n/a", "n/a")
        lines.append(
            f"| {scenario} | {best_book} | {best_latency} | {second_book} | {second_latency} |"
        )

    if not has_rows:
        lines.append("| n/a | n/a | n/a | n/a | n/a |")

    lines.extend(
        [
            "",
            "Notes:",
            "- Only scenarios with at least one cancellation event are listed.",
            "",
        ]
    )
    output_path.write_text("\n".join(lines), encoding="utf-8")
    return output_path



def _write_midtier_winner_table(
    direct_dataframe: pd.DataFrame,
    gateway_dataframe: pd.DataFrame,
    output_dir: Path,
) -> Path:
    """Write best book per scenario for middle-tier competition (no array)."""
    output_path = output_dir / "tables" / "mid_tier_winners_no_array.md"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    books = ["hybrid", "pool", "map"]

    direct_subset = direct_dataframe[direct_dataframe["Book"].isin(books)].copy()
    gateway_subset = gateway_dataframe[gateway_dataframe["Book"].isin(books)].copy()

    direct_best = _best_book_by_scenario(direct_subset).rename(
        columns={"Book": "DirectMidtierBest", "Latency_ns": "DirectMidtierLatency_ns"}
    )
    gateway_best = _best_book_by_scenario(gateway_subset).rename(
        columns={"Book": "GatewayMidtierBest", "Latency_ns": "GatewayMidtierLatency_ns"}
    )
    merged = direct_best.merge(gateway_best, on="Scenario", how="outer").sort_values("Scenario")

    lines: List[str] = [
        "# Mid-Tier Winners by Scenario (Array Excluded)",
        "",
        "| Scenario | Direct Mid-Tier Best | Direct Latency (ns) | Direct Mid-Tier Second | Direct Second Latency (ns) | Gateway Mid-Tier Best | Gateway Latency (ns) | Gateway Mid-Tier Second | Gateway Second Latency (ns) |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]

    for _, row in merged.iterrows():
        scenario = row["Scenario"]

        direct_top_two = _top_books_for_metric(
            direct_subset,
            scenario,
            "Latency_ns",
            books=books,
            top_n=2,
        )
        gateway_top_two = _top_books_for_metric(
            gateway_subset,
            scenario,
            "Latency_ns",
            books=books,
            top_n=2,
        )

        direct_book, direct_latency = direct_top_two[0] if len(direct_top_two) > 0 else ("n/a", "n/a")
        direct_second_book, direct_second_latency = (
            direct_top_two[1] if len(direct_top_two) > 1 else ("n/a", "n/a")
        )
        gateway_book, gateway_latency = gateway_top_two[0] if len(gateway_top_two) > 0 else ("n/a", "n/a")
        gateway_second_book, gateway_second_latency = (
            gateway_top_two[1] if len(gateway_top_two) > 1 else ("n/a", "n/a")
        )

        lines.append(
            f"| {scenario} | {direct_book} | {direct_latency} | {direct_second_book} | {direct_second_latency} | {gateway_book} | {gateway_latency} | {gateway_second_book} | {gateway_second_latency} |"
        )

    lines.append("")
    output_path.write_text("\n".join(lines), encoding="utf-8")
    return output_path



def write_analysis_tables(
    direct_dataframe: pd.DataFrame,
    gateway_dataframe: pd.DataFrame,
    output_dir: Path,
) -> List[Path]:
    """Write analysis tables used for dissertation narrative and appendix material."""
    output_dir.mkdir(parents=True, exist_ok=True)

    generated_tables = [
        _write_rank_table_file(direct_dataframe, gateway_dataframe, output_dir),
        _write_mixed_validity_table(output_dir),
        _write_operation_winner_table(direct_dataframe, output_dir),
        _write_operation_winner_table_no_array(direct_dataframe, output_dir),
        _write_cancel_winner_table(direct_dataframe, output_dir),
        _write_midtier_winner_table(direct_dataframe, gateway_dataframe, output_dir),
    ]
    return generated_tables
