"""Figure construction functions for dissertation benchmark analysis."""

from __future__ import annotations

from pathlib import Path
from typing import Dict, List

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

from .constants import BOOK_ORDER, BOOK_ORDER_NO_VECTOR, MIDDLE_TIER_BOOKS, REPRESENTATIVE_SCENARIOS
from .data_processing import (
    direct_latency_matrix,
    direct_slowdown_matrix,
    direct_tail_ratio_dataframe,
    filter_books,
    gateway_dense_decomposition_dataframe,
    gateway_latency_dataframe,
    mpsc_scaling_dataframe,
    slowdown_matrix,
)
from .plot_style import save_figure



def plot_direct_latency_heatmap(direct_dataframe: pd.DataFrame, output_dir: Path) -> Path:
    heatmap_dataframe = direct_latency_matrix(direct_dataframe, "Latency_ns")

    plt.figure(figsize=(8.0, 4.6))
    sns.heatmap(
        heatmap_dataframe,
        annot=True,
        fmt=".0f",
        cmap="YlGnBu",
        linewidths=0.3,
        cbar_kws={"label": "Mean Latency (ns)"},
    )
    plt.title("Direct Mode Mean Latency by Scenario and Order Book")
    plt.xlabel("Order Book")
    plt.ylabel("Scenario")

    output_path = output_dir / "direct" / "latency_mean_heatmap.png"
    save_figure(output_path)
    return output_path



def plot_direct_slowdown_heatmap(direct_dataframe: pd.DataFrame, output_dir: Path) -> Path:
    slowdown_dataframe = direct_slowdown_matrix(direct_dataframe)

    plt.figure(figsize=(8.0, 4.6))
    sns.heatmap(
        slowdown_dataframe,
        annot=True,
        fmt=".2f",
        cmap="YlOrRd",
        linewidths=0.3,
        cbar_kws={"label": "Slowdown vs Scenario Best"},
        vmin=1.0,
    )
    plt.title("Direct Mode Relative Slowdown vs Best Book per Scenario")
    plt.xlabel("Order Book")
    plt.ylabel("Scenario")

    output_path = output_dir / "direct" / "latency_slowdown_vs_best_heatmap.png"
    save_figure(output_path)
    return output_path



def plot_direct_slowdown_no_vector_heatmap(direct_dataframe: pd.DataFrame, output_dir: Path) -> Path:
    no_vector_dataframe = filter_books(direct_dataframe, BOOK_ORDER_NO_VECTOR)
    slowdown_dataframe = slowdown_matrix(no_vector_dataframe)

    plt.figure(figsize=(8.0, 4.6))
    sns.heatmap(
        slowdown_dataframe,
        annot=True,
        fmt=".2f",
        cmap="YlOrRd",
        linewidths=0.3,
        cbar_kws={"label": "Slowdown vs Scenario Best (Excl. Vector)"},
        vmin=1.0,
    )
    plt.title("Direct Mode Relative Slowdown (Excluding Vector)")
    plt.xlabel("Order Book")
    plt.ylabel("Scenario")

    output_path = output_dir / "direct" / "latency_slowdown_no_vector_heatmap.png"
    save_figure(output_path)
    return output_path



def plot_direct_tail_ratio(direct_dataframe: pd.DataFrame, output_dir: Path) -> Path:
    tail_ratio_dataframe = direct_tail_ratio_dataframe(direct_dataframe)

    plt.figure(figsize=(10.4, 4.8))
    sns.barplot(
        data=tail_ratio_dataframe,
        x="Scenario",
        y="P99MeanRatio",
        hue="Book",
        errorbar=None,
        palette="Set2",
    )
    plt.title("Direct Mode Tail Amplification (P99 / Mean)")
    plt.ylabel("P99 / Mean")
    plt.xlabel("Scenario")
    plt.xticks(rotation=25, ha="right")
    plt.legend(title="Book", ncol=3)

    output_path = output_dir / "direct" / "tail_p99_over_mean.png"
    save_figure(output_path)
    return output_path



def plot_direct_operation_breakdown(direct_dataframe: pd.DataFrame, output_dir: Path) -> Path:
    representative_dataframe = direct_dataframe[
        direct_dataframe["Scenario"].isin(REPRESENTATIVE_SCENARIOS)
    ].copy()
    # Keep only selected scenario categories so facet columns do not include empty panels.
    if pd.api.types.is_categorical_dtype(representative_dataframe["Scenario"]):
        representative_dataframe["Scenario"] = representative_dataframe[
            "Scenario"
        ].cat.remove_unused_categories()

    long_form_dataframe = representative_dataframe.melt(
        id_vars=["Scenario", "Book"],
        value_vars=["Insert_ns", "Lookup_ns", "Match_ns", "Cancel_ns"],
        var_name="Operation",
        value_name="OperationLatency_ns",
    )
    long_form_dataframe = long_form_dataframe[
        long_form_dataframe["OperationLatency_ns"] > 0
    ]

    g = sns.catplot(
        data=long_form_dataframe,
        x="Book",
        y="OperationLatency_ns",
        hue="Operation",
        col="Scenario",
        col_order=REPRESENTATIVE_SCENARIOS,
        kind="bar",
        col_wrap=3,
        height=3.4,
        aspect=1.2,
        sharey=False,
        errorbar=None,
        palette="muted",
    )
    g.fig.subplots_adjust(top=0.84)
    g.fig.suptitle("Direct Mode Operation-Level Latency Breakdown")
    g.set_axis_labels("Order Book", "Latency (ns)")
    for axis in g.axes.flatten():
        for label in axis.get_xticklabels():
            label.set_rotation(25)
            label.set_ha("right")

    output_path = output_dir / "direct" / "operation_latency_breakdown_selected_scenarios.png"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    g.savefig(output_path, bbox_inches="tight")
    plt.close(g.fig)
    return output_path



def plot_mixed_operation_breakdown(
    direct_dataframe: pd.DataFrame,
    output_dir: Path,
) -> Path:
    mixed_df = direct_dataframe[direct_dataframe["Scenario"] == "mixed"]
    mixed_df = filter_books(mixed_df, BOOK_ORDER_NO_VECTOR)

    long_form = mixed_df.melt(
        id_vars=["Book"],
        value_vars=["Insert_ns", "Lookup_ns", "Match_ns", "Cancel_ns"],
        var_name="Operation",
        value_name="SubOperationLatency_ns",
    )

    plt.figure(figsize=(7.5, 4.8))
    sns.barplot(
        data=long_form,
        x="Operation",
        y="SubOperationLatency_ns",
        hue="Book",
        hue_order=BOOK_ORDER_NO_VECTOR,
    )

    plt.title("Mixed Scenario: Operation-Level Latency Breakdown")
    plt.ylabel("Latency (ns)")
    plt.xlabel("Operation Type")
    plt.legend(title="Order Book", loc="upper right")

    output_path = output_dir / "direct" / "mixed_operation_latency_breakdown.png"
    save_figure(output_path)
    return output_path



def plot_middle_tier_tradeoff_heatmap(direct_dataframe: pd.DataFrame, output_dir: Path) -> Path:
    middle_tier_dataframe = filter_books(direct_dataframe, MIDDLE_TIER_BOOKS)
    slowdown_dataframe = slowdown_matrix(middle_tier_dataframe)

    plt.figure(figsize=(6.2, 4.4))
    sns.heatmap(
        slowdown_dataframe,
        annot=True,
        fmt=".2f",
        cmap="YlOrBr",
        linewidths=0.3,
        cbar_kws={"label": "Slowdown vs Middle-Tier Best"},
        vmin=1.0,
    )
    plt.title("Direct Mode Middle-Tier Trade-offs")
    plt.xlabel("Order Book")
    plt.ylabel("Scenario")

    output_path = output_dir / "direct" / "middle_tier_slowdown_heatmap.png"
    save_figure(output_path)
    return output_path



def plot_direct_vs_gateway_compression(
    direct_dataframe: pd.DataFrame,
    gateway_dataframe: pd.DataFrame,
    output_dir: Path,
) -> Path:
    direct_middle_tier = filter_books(direct_dataframe, MIDDLE_TIER_BOOKS)
    gateway_middle_tier = filter_books(gateway_dataframe, MIDDLE_TIER_BOOKS)

    direct_slowdown = slowdown_matrix(direct_middle_tier)
    gateway_slowdown = slowdown_matrix(gateway_middle_tier)
    shared_max = max(
        float(direct_slowdown.max().max()),
        float(gateway_slowdown.max().max()),
    )

    figure, axes = plt.subplots(1, 2, figsize=(11.2, 4.8), sharey=True)
    sns.heatmap(
        direct_slowdown,
        annot=True,
        fmt=".2f",
        cmap="YlOrRd",
        linewidths=0.3,
        cbar=False,
        vmin=1.0,
        vmax=shared_max,
        ax=axes[0],
    )
    axes[0].set_title("Direct Slowdown")
    axes[0].set_xlabel("Order Book")
    axes[0].set_ylabel("Scenario")

    sns.heatmap(
        gateway_slowdown,
        annot=True,
        fmt=".2f",
        cmap="YlOrRd",
        linewidths=0.3,
        cbar_kws={"label": "Slowdown vs Middle-Tier Best"},
        vmin=1.0,
        vmax=shared_max,
        ax=axes[1],
    )
    axes[1].set_title("Gateway Slowdown")
    axes[1].set_xlabel("Order Book")
    axes[1].set_ylabel("")

    figure.suptitle("Direct vs Gateway Compression for Middle-Tier Books")
    figure.tight_layout(rect=[0, 0, 1, 0.93])

    output_path = output_dir / "direct_vs_gateway" / "middle_tier_slowdown_direct_vs_gateway.png"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output_path, bbox_inches="tight")
    plt.close(figure)
    return output_path



def plot_gateway_dense_decomposition(gateway_dataframe: pd.DataFrame, output_dir: Path) -> Path | None:
    dense_dataframe = gateway_dense_decomposition_dataframe(gateway_dataframe)
    if dense_dataframe.empty:
        return None

    dense_dataframe = filter_books(dense_dataframe, BOOK_ORDER)
    dense_dataframe["Book"] = pd.Categorical(
        dense_dataframe["Book"], categories=BOOK_ORDER, ordered=True
    )
    dense_dataframe = dense_dataframe.sort_values("Book")
    x_positions = range(len(dense_dataframe))

    plt.figure(figsize=(8.8, 4.8))
    plt.bar(x_positions, dense_dataframe["Network_ns"], label="Network")
    plt.bar(
        x_positions,
        dense_dataframe["Queue_ns"],
        bottom=dense_dataframe["Network_ns"],
        label="Queue",
    )
    plt.bar(
        x_positions,
        dense_dataframe["Engine_ns"],
        bottom=dense_dataframe["Network_ns"] + dense_dataframe["Queue_ns"],
        label="Engine",
    )
    plt.bar(
        x_positions,
        dense_dataframe["ResidualOther_ns"],
        bottom=dense_dataframe["Network_ns"]
        + dense_dataframe["Queue_ns"]
        + dense_dataframe["Engine_ns"],
        label="Residual",
    )

    plt.xticks(x_positions, dense_dataframe["Book"], rotation=20, ha="right")
    plt.ylabel("Latency (ns)")
    plt.xlabel("Order Book")
    plt.title("Gateway Mode Dense Full Decomposition")
    plt.legend(ncol=4)

    output_path = output_dir / "gateway" / "dense_full_latency_decomposition.png"
    save_figure(output_path)
    return output_path



def plot_gateway_latency_by_scenario(gateway_dataframe: pd.DataFrame, output_dir: Path) -> Path:
    scenario_latency_dataframe = gateway_latency_dataframe(gateway_dataframe)

    plt.figure(figsize=(10.8, 4.8))
    sns.pointplot(
        data=scenario_latency_dataframe,
        x="Scenario",
        y="Latency_ns",
        hue="Book",
        errorbar=None,
        markers="o",
        linestyles="-",
    )
    plt.title("Gateway Mode End-to-End Latency by Scenario")
    plt.ylabel("Mean E2E Latency (ns)")
    plt.xlabel("Scenario")
    plt.xticks(rotation=25, ha="right")
    plt.legend(title="Book", ncol=3)

    output_path = output_dir / "gateway" / "latency_by_scenario.png"
    save_figure(output_path)
    return output_path



def plot_gateway_latency_by_scenario_no_vector(gateway_dataframe: pd.DataFrame, output_dir: Path) -> Path:
    scenario_latency_dataframe = gateway_latency_dataframe(gateway_dataframe)
    scenario_latency_dataframe = filter_books(scenario_latency_dataframe, BOOK_ORDER_NO_VECTOR)

    plt.figure(figsize=(10.2, 4.8))
    sns.pointplot(
        data=scenario_latency_dataframe,
        x="Scenario",
        y="Latency_ns",
        hue="Book",
        hue_order=BOOK_ORDER_NO_VECTOR,
        errorbar=None,
        markers="o",
        linestyles="-",
    )
    plt.title("Gateway Mode End-to-End Latency by Scenario (Excluding Vector)")
    plt.ylabel("Mean E2E Latency (ns)")
    plt.xlabel("Scenario")
    plt.xticks(rotation=25, ha="right")
    plt.legend(title="Book", ncol=2)

    output_path = output_dir / "gateway" / "latency_by_scenario_no_vector.png"
    save_figure(output_path)
    return output_path



def plot_gateway_slowdown_heatmap_no_vector(gateway_dataframe: pd.DataFrame, output_dir: Path) -> Path:
    no_vector_dataframe = filter_books(gateway_dataframe, BOOK_ORDER_NO_VECTOR)
    slowdown_dataframe = slowdown_matrix(no_vector_dataframe)

    plt.figure(figsize=(7.4, 4.6))
    sns.heatmap(
        slowdown_dataframe,
        annot=True,
        fmt=".3f",
        cmap="YlOrRd",
        linewidths=0.3,
        cbar_kws={"label": "Slowdown vs No-Vector Best"},
        vmin=1.0,
    )
    plt.title("Gateway Mode Relative Slowdown by Scenario (Excluding Vector)")
    plt.xlabel("Order Book")
    plt.ylabel("Scenario")

    output_path = output_dir / "gateway" / "latency_slowdown_no_vector_heatmap.png"
    save_figure(output_path)
    return output_path



def plot_direct_latency_stddev_heatmap_no_vector(
    direct_dataframe: pd.DataFrame,
    output_dir: Path,
) -> Path:
    direct_no_vector = filter_books(direct_dataframe, BOOK_ORDER_NO_VECTOR)
    direct_std = direct_no_vector.pivot(index="Scenario", columns="Book", values="LatencyStdDev_ns")
    plt.figure(figsize=(7.0, 4.8))
    sns.heatmap(
        direct_std,
        annot=True,
        fmt=".0f",
        cmap="Blues",
        linewidths=0.3,
        vmin=0,
        cbar_kws={"label": "Latency StdDev (ns)"},
    )
    plt.title("Direct Latency StdDev by Scenario (No-Vector)")
    plt.xlabel("Order Book")
    plt.ylabel("Scenario")

    output_path = output_dir / "direct" / "latency_stddev_no_vector_heatmap.png"
    save_figure(output_path)
    return output_path



def plot_gateway_latency_stddev_heatmap_no_vector(
    gateway_dataframe: pd.DataFrame,
    output_dir: Path,
) -> Path:
    gateway_no_vector = filter_books(gateway_dataframe, BOOK_ORDER_NO_VECTOR)
    gateway_std = gateway_no_vector.pivot(index="Scenario", columns="Book", values="LatencyStdDev_ns")

    plt.figure(figsize=(7.0, 4.8))
    sns.heatmap(
        gateway_std,
        annot=True,
        fmt=".0f",
        cmap="Blues",
        linewidths=0.3,
        vmin=0,
        cbar_kws={"label": "Latency StdDev (ns)"},
    )
    plt.title("Gateway Latency StdDev by Scenario (No-Vector)")
    plt.xlabel("Order Book")
    plt.ylabel("Scenario")

    output_path = output_dir / "gateway" / "latency_stddev_no_vector_heatmap.png"
    save_figure(output_path)
    return output_path



def plot_mixed_validity_profile(output_dir: Path) -> Path:
    scenarios = ["tight_spread", "mixed", "high_cancellation"]
    near_share = np.array([100.0, 55.0, 100.0])
    deep_share = np.array([0.0, 30.0, 0.0])
    extreme_share = np.array([0.0, 15.0, 0.0])
    cancel_ratio = np.array([0.0, 18.0, 50.0])

    figure, axes = plt.subplots(1, 2, figsize=(11.8, 4.5))

    x = np.arange(len(scenarios))
    axes[0].bar(x, near_share, label="Near-spread")
    axes[0].bar(x, deep_share, bottom=near_share, label="Deep levels")
    axes[0].bar(x, extreme_share, bottom=near_share + deep_share, label="Extreme levels")
    axes[0].set_xticks(x)
    axes[0].set_xticklabels(scenarios, rotation=20, ha="right")
    axes[0].set_ylabel("Configured Order Share (%)")
    axes[0].set_title("Price Placement Mix by Scenario")
    axes[0].legend(frameon=False)

    axes[1].bar(x, cancel_ratio, color="tab:red")
    axes[1].set_xticks(x)
    axes[1].set_xticklabels(scenarios, rotation=20, ha="right")
    axes[1].set_ylabel("Configured Cancel Branch (%)")
    axes[1].set_title("Cancellation Pressure by Scenario")

    figure.suptitle("Mixed Scenario Validity Profile (Generator-Defined)")
    figure.tight_layout(rect=[0, 0, 1, 0.93])

    output_path = output_dir / "scenario_validity" / "mixed_profile.png"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output_path, bbox_inches="tight")
    plt.close(figure)
    return output_path



def plot_mpsc_throughput_scaling(mpsc_dataframe: pd.DataFrame, output_dir: Path) -> Path | None:
    scaling_dataframe = mpsc_scaling_dataframe(mpsc_dataframe)
    if scaling_dataframe.empty:
        return None

    plt.figure(figsize=(8.8, 4.8))
    sns.lineplot(
        data=scaling_dataframe,
        x="Producers",
        y="Throughput",
        hue="Book",
        marker="o",
    )
    plt.title("MPSC Throughput Scaling")
    plt.xlabel("Producer Threads")
    plt.ylabel("Throughput (orders/sec)")
    plt.legend(title="Book", ncol=3)

    output_path = output_dir / "mpsc" / "throughput_scaling.png"
    save_figure(output_path)
    return output_path



def plot_mpsc_queue_engine_scaling(mpsc_dataframe: pd.DataFrame, output_dir: Path) -> Path | None:
    scaling_dataframe = mpsc_scaling_dataframe(mpsc_dataframe)
    if scaling_dataframe.empty:
        return None

    figure, axes = plt.subplots(1, 2, figsize=(12.0, 4.4), sharex=True)

    sns.lineplot(
        data=scaling_dataframe,
        x="Producers",
        y="MpscQue_ns",
        hue="Book",
        marker="o",
        ax=axes[0],
    )
    axes[0].set_title("Queue Mean Latency vs Producers")
    axes[0].set_xlabel("Producer Threads")
    axes[0].set_ylabel("Queue Mean (ns)")
    axes[0].legend_.remove()

    sns.lineplot(
        data=scaling_dataframe,
        x="Producers",
        y="MpscEng_ns",
        hue="Book",
        marker="o",
        ax=axes[1],
    )
    axes[1].set_title("Engine Mean Latency vs Producers")
    axes[1].set_xlabel("Producer Threads")
    axes[1].set_ylabel("Engine Mean (ns)")

    handles, labels = axes[1].get_legend_handles_labels()
    figure.legend(handles, labels, loc="upper center", ncol=5, frameon=False)
    axes[1].legend_.remove()
    figure.suptitle("MPSC Queue vs Engine Scaling")
    figure.tight_layout(rect=[0, 0, 1, 0.92])

    output_path = output_dir / "mpsc" / "queue_vs_engine_scaling.png"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output_path, bbox_inches="tight")
    plt.close(figure)
    return output_path



def build_all_figures(
    direct_dataframe: pd.DataFrame,
    gateway_dataframe: pd.DataFrame,
    mpsc_dataframe: pd.DataFrame,
    output_dir: Path,
) -> Dict[str, Path]:
    """Generate the full recommended figure set and return output paths."""
    figure_paths: Dict[str, Path] = {}

    if not direct_dataframe.empty:
        figure_paths["direct_latency_heatmap"] = plot_direct_latency_heatmap(direct_dataframe, output_dir)
        figure_paths["direct_slowdown_heatmap"] = plot_direct_slowdown_heatmap(direct_dataframe, output_dir)
        figure_paths["direct_slowdown_no_vector_heatmap"] = plot_direct_slowdown_no_vector_heatmap(
            direct_dataframe, output_dir
        )
        figure_paths["direct_tail_ratio"] = plot_direct_tail_ratio(direct_dataframe, output_dir)
        figure_paths["direct_operation_breakdown"] = plot_direct_operation_breakdown(
            direct_dataframe, output_dir
        )
        figure_paths["direct_mixed_operation_breakdown"] = plot_mixed_operation_breakdown(
            direct_dataframe, output_dir
        )
        figure_paths["direct_middle_tier_tradeoff"] = plot_middle_tier_tradeoff_heatmap(
            direct_dataframe, output_dir
        )

    if not gateway_dataframe.empty:
        dense_path = plot_gateway_dense_decomposition(gateway_dataframe, output_dir)
        if dense_path is not None:
            figure_paths["gateway_dense_decomposition"] = dense_path
        figure_paths["gateway_latency_by_scenario"] = plot_gateway_latency_by_scenario(
            gateway_dataframe, output_dir
        )
        figure_paths["gateway_latency_by_scenario_no_vector"] = plot_gateway_latency_by_scenario_no_vector(
            gateway_dataframe, output_dir
        )
        figure_paths["gateway_slowdown_heatmap_no_vector"] = plot_gateway_slowdown_heatmap_no_vector(
            gateway_dataframe, output_dir
        )

    if not direct_dataframe.empty:
        figure_paths["direct_latency_stddev_no_vector_heatmap"] = plot_direct_latency_stddev_heatmap_no_vector(
            direct_dataframe,
            output_dir,
        )

    if not gateway_dataframe.empty:
        figure_paths["gateway_latency_stddev_no_vector_heatmap"] = plot_gateway_latency_stddev_heatmap_no_vector(
            gateway_dataframe,
            output_dir,
        )

    if not direct_dataframe.empty and not gateway_dataframe.empty:
        figure_paths["direct_vs_gateway_middle_tier_compression"] = plot_direct_vs_gateway_compression(
            direct_dataframe,
            gateway_dataframe,
            output_dir,
        )

    figure_paths["mixed_scenario_validity_profile"] = plot_mixed_validity_profile(output_dir)

    if not mpsc_dataframe.empty:
        throughput_path = plot_mpsc_throughput_scaling(mpsc_dataframe, output_dir)
        if throughput_path is not None:
            figure_paths["mpsc_throughput_scaling"] = throughput_path

        queue_engine_path = plot_mpsc_queue_engine_scaling(mpsc_dataframe, output_dir)
        if queue_engine_path is not None:
            figure_paths["mpsc_queue_engine_scaling"] = queue_engine_path

    return figure_paths
