#!/usr/bin/env python3
"""Combine multiple raw_run_results CSV files into one dataset."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Combine hypothesis campaign CSV files")
    p.add_argument("--inputs", nargs="+", required=True, help="Input raw_run_results.csv paths")
    p.add_argument("--output", required=True, help="Output merged CSV path")
    return p


def main() -> int:
    args = build_parser().parse_args()
    in_paths = [Path(p).resolve() for p in args.inputs]
    out_path = Path(args.output).resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)

    header = None
    rows_written = 0
    with out_path.open("w", newline="") as out_f:
        writer = None
        for p in in_paths:
            with p.open("r", newline="") as in_f:
                reader = csv.reader(in_f)
                cur_header = next(reader)
                if header is None:
                    header = cur_header
                    writer = csv.writer(out_f)
                    writer.writerow(header)
                elif cur_header != header:
                    raise RuntimeError(f"Header mismatch in input: {p}")

                for row in reader:
                    writer.writerow(row)
                    rows_written += 1

    print(f"Wrote merged dataset: {out_path}")
    print(f"Rows: {rows_written}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
