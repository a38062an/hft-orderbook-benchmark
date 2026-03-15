"""Command-line entrypoint for dissertation plotting pipeline."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Sequence

from .data_processing import available_modes, load_results, split_modes
from .figures import build_all_figures
from .findings import write_analysis_tables
from .plot_style import configure_plot_theme



def _build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate modular dissertation plots from benchmark CSV results."
    )
    parser.add_argument(
        "csv",
        nargs="?",
        default="results/results.csv",
        help="Path to benchmark CSV results file.",
    )
    parser.add_argument(
        "--out",
        default="plots",
        help="Output directory for generated plots and analysis tables.",
    )
    return parser



def run_pipeline(argv: Sequence[str] | None = None) -> int:
    parser = _build_argument_parser()
    arguments = parser.parse_args(argv)

    csv_path = Path(arguments.csv)
    output_dir = Path(arguments.out)

    configure_plot_theme()
    results_dataframe = load_results(csv_path)
    mode_split = split_modes(results_dataframe)

    generated_figures = build_all_figures(
        direct_dataframe=mode_split.direct,
        gateway_dataframe=mode_split.gateway,
        mpsc_dataframe=mode_split.mpsc,
        output_dir=output_dir,
    )
    generated_tables = write_analysis_tables(
        direct_dataframe=mode_split.direct,
        gateway_dataframe=mode_split.gateway,
        output_dir=output_dir,
    )

    print(f"Detected modes: {', '.join(available_modes(results_dataframe))}")
    if generated_figures:
        print("Generated figure files:")
        for figure_name, figure_path in generated_figures.items():
            print(f"  - {figure_name}: {figure_path}")
    else:
        print("No figures generated: input data did not include supported modes.")

    if generated_tables:
        print("Generated table files:")
        for table_path in generated_tables:
            print(f"  - {table_path}")
    return 0
