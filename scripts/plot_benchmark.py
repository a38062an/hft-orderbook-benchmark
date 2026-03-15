"""Compatibility entrypoint for modular benchmark plotting pipeline."""

from __future__ import annotations

import sys

from benchmark_plotting.cli import run_pipeline


if __name__ == "__main__":
    raise SystemExit(run_pipeline(sys.argv[1:]))
