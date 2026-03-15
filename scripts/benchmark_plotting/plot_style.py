"""Matplotlib and seaborn style helpers for consistent publication plots."""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import seaborn as sns


def configure_plot_theme() -> None:
    """Configure a clean and readable visual style for dissertation figures."""
    sns.set_theme(
        context="paper",
        style="whitegrid",
        font="DejaVu Sans",
        rc={
            "figure.dpi": 140,
            "savefig.dpi": 220,
            "axes.titlesize": 12,
            "axes.labelsize": 10,
            "xtick.labelsize": 9,
            "ytick.labelsize": 9,
            "legend.fontsize": 9,
        },
    )


def save_figure(output_path: Path) -> None:
    """Save the active figure with consistent layout and file options."""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(output_path, bbox_inches="tight")
    plt.close()
