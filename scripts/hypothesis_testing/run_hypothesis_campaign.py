#!/usr/bin/env python3
"""Run repeat benchmark campaigns for inferential hypothesis testing.

This script keeps all generated artifacts separate from canonical results by
writing into results/hypothesis_testing/<timestamp>/ by default.
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import os
import pathlib
import random
import shlex
import signal
import subprocess
import sys
import time
from typing import Dict, List, Optional


def now_utc_iso() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat()


def run_cmd(cmd: List[str], cwd: pathlib.Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=str(cwd), text=True, capture_output=True)


def resolve_bin(primary: pathlib.Path, fallback: Optional[pathlib.Path] = None) -> pathlib.Path:
    if primary.exists() and os.access(primary, os.X_OK):
        return primary
    if fallback and fallback.exists() and os.access(fallback, os.X_OK):
        return fallback
    raise FileNotFoundError(f"Executable not found: {primary}{' or ' + str(fallback) if fallback else ''}")


def read_single_row(csv_path: pathlib.Path, mode: str, book: str, scenario: str) -> Dict[str, str]:
    with csv_path.open("r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row.get("Mode") == mode and row.get("Book") == book and row.get("Scenario") == scenario:
                return row
    raise RuntimeError(f"Expected row not found in {csv_path}: mode={mode}, book={book}, scenario={scenario}")


def safe_float(v: str) -> float:
    if v is None or v == "":
        return 0.0
    return float(v)


def safe_int(v: str) -> int:
    if v is None or v == "":
        return 0
    return int(float(v))


def is_invalid_gateway_row(row: Dict[str, str]) -> bool:
    lat = safe_float(row.get("Latency_ns", "0"))
    net = safe_float(row.get("Network_ns", "0"))
    que = safe_float(row.get("Queue_ns", "0"))
    eng = safe_float(row.get("Engine_ns", "0"))
    return lat <= 0.0 and net <= 0.0 and que <= 0.0 and eng <= 0.0


def start_gateway_server(server_bin: pathlib.Path, book: str, port: int, cwd: pathlib.Path) -> subprocess.Popen[str]:
    cmd = [str(server_bin), "--book", book, "--port", str(port)]
    return subprocess.Popen(cmd, cwd=str(cwd), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, text=True)


def stop_process(proc: subprocess.Popen[str]) -> None:
    if proc.poll() is not None:
        return
    try:
        proc.send_signal(signal.SIGINT)
    except ProcessLookupError:
        return
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=2)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Repeat benchmark campaigns for hypothesis testing")
    p.add_argument("--repo-root", default=".", help="Repository root path")
    p.add_argument("--output-dir", default="", help="Output directory (default: results/hypothesis_testing/<timestamp>)")
    p.add_argument("--books", default="array,map,vector,hybrid,pool", help="Comma-separated books")
    p.add_argument(
        "--scenarios",
        default="tight_spread,fixed_levels,mixed,high_cancellation,sparse_extreme,dense_full,worst_case_fifo",
        help="Comma-separated scenarios",
    )
    p.add_argument("--orders", type=int, default=10000)
    p.add_argument("--reps-direct", type=int, default=30, help="Replicates per scenario for direct mode")
    p.add_argument("--reps-gateway", type=int, default=0, help="Replicates per scenario for gateway mode")
    p.add_argument("--replicate-offset", type=int, default=0, help="Offset added to recorded replicate IDs")
    p.add_argument("--gateway-inner-runs", type=int, default=5, help="Internal --runs value for each gateway replicate")
    p.add_argument("--pin-core", type=int, default=0, help="Core for direct mode pinning")
    p.add_argument("--gateway-port", type=int, default=12345)
    p.add_argument("--gateway-port-span", type=int, default=200, help="Port range span for rotating gateway ports")
    p.add_argument("--gateway-max-attempts", type=int, default=3, help="Retry count per gateway case")
    p.add_argument("--gateway-start-delay", type=float, default=1.0, help="Seconds to wait after server start")
    p.add_argument("--gateway-case-delay", type=float, default=0.3, help="Seconds to wait between gateway cases")
    p.add_argument("--random-seed", type=int, default=42)
    p.add_argument("--continue-on-error", action="store_true")
    return p


def main() -> int:
    args = build_parser().parse_args()

    repo_root = pathlib.Path(args.repo_root).resolve()
    ts = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    output_dir = pathlib.Path(args.output_dir) if args.output_dir else repo_root / "results" / "hypothesis_testing" / ts
    output_dir.mkdir(parents=True, exist_ok=True)
    tmp_dir = output_dir / "tmp"
    tmp_dir.mkdir(parents=True, exist_ok=True)

    bench_bin = resolve_bin(repo_root / "build" / "benchmarks" / "orderbook_benchmark")
    server_bin = resolve_bin(
        repo_root / "build" / "src" / "hft_exchange_server",
        repo_root / "build" / "bin" / "hft_exchange_server",
    )

    books = [b.strip() for b in args.books.split(",") if b.strip()]
    scenarios = [s.strip() for s in args.scenarios.split(",") if s.strip()]

    rng = random.Random(args.random_seed)
    campaign_id = f"hypothesis_campaign_{ts}"

    manifest = {
        "campaign_id": campaign_id,
        "created_utc": now_utc_iso(),
        "repo_root": str(repo_root),
        "benchmark_binary": str(bench_bin),
        "server_binary": str(server_bin),
        "books": books,
        "scenarios": scenarios,
        "orders": args.orders,
        "reps_direct": args.reps_direct,
        "reps_gateway": args.reps_gateway,
        "replicate_offset": args.replicate_offset,
        "gateway_inner_runs": args.gateway_inner_runs,
        "pin_core": args.pin_core,
        "gateway_port": args.gateway_port,
        "gateway_port_span": args.gateway_port_span,
        "gateway_max_attempts": args.gateway_max_attempts,
        "gateway_start_delay": args.gateway_start_delay,
        "gateway_case_delay": args.gateway_case_delay,
        "random_seed": args.random_seed,
    }

    raw_csv = output_dir / "raw_run_results.csv"
    failures_csv = output_dir / "failures.csv"

    raw_header = [
        "campaign_id",
        "captured_utc",
        "mode",
        "replicate",
        "scenario",
        "book",
        "orders",
        "pin_core",
        "latency_ns",
        "latency_stddev_ns",
        "p99_ns",
        "p99_stddev_ns",
        "max_ns",
        "throughput",
        "throughput_stddev",
        "network_ns",
        "queue_ns",
        "engine_ns",
        "insert_ns",
        "cancel_ns",
        "lookup_ns",
        "match_ns",
        "duration_ms",
        "command",
    ]
    failure_header = ["captured_utc", "mode", "replicate", "scenario", "book", "returncode", "command", "stderr_tail"]

    with raw_csv.open("w", newline="") as f:
        csv.writer(f).writerow(raw_header)
    with failures_csv.open("w", newline="") as f:
        csv.writer(f).writerow(failure_header)

    def record_failure(mode: str, rep: int, scenario: str, book: str, cp: subprocess.CompletedProcess[str], cmd: List[str]) -> None:
        tail = (cp.stderr or "")[-2000:]
        with failures_csv.open("a", newline="") as f:
            csv.writer(f).writerow([now_utc_iso(), mode, rep, scenario, book, cp.returncode, " ".join(map(shlex.quote, cmd)), tail])

    def record_row(mode: str, rep: int, scenario: str, book: str, row: Dict[str, str], duration_ms: float, cmd: List[str]) -> None:
        with raw_csv.open("a", newline="") as f:
            csv.writer(f).writerow(
                [
                    campaign_id,
                    now_utc_iso(),
                    mode,
                    rep,
                    scenario,
                    book,
                    args.orders,
                    args.pin_core if mode == "direct" else "",
                    safe_float(row.get("Latency_ns", "0")),
                    safe_float(row.get("LatencyStdDev_ns", "0")),
                    safe_float(row.get("P99_ns", "0")),
                    safe_float(row.get("P99StdDev_ns", "0")),
                    safe_float(row.get("Max_ns", "0")),
                    safe_float(row.get("Throughput", "0")),
                    safe_float(row.get("ThroughputStdDev", "0")),
                    safe_float(row.get("Network_ns", "0")),
                    safe_float(row.get("Queue_ns", "0")),
                    safe_float(row.get("Engine_ns", "0")),
                    safe_float(row.get("Insert_ns", "0")),
                    safe_float(row.get("Cancel_ns", "0")),
                    safe_float(row.get("Lookup_ns", "0")),
                    safe_float(row.get("Match_ns", "0")),
                    round(duration_ms, 3),
                    " ".join(map(shlex.quote, cmd)),
                ]
            )

    # Direct mode campaign
    for rep in range(1, args.reps_direct + 1):
        rep_id = rep + args.replicate_offset
        order = books[:]
        rng.shuffle(order)
        for scenario in scenarios:
            for book in order:
                tmp_csv = tmp_dir / f"direct_rep{rep}_{scenario}_{book}.csv"
                cmd = [
                    str(bench_bin),
                    "--mode",
                    "direct",
                    "--book",
                    book,
                    "--scenario",
                    scenario,
                    "--runs",
                    "1",
                    "--orders",
                    str(args.orders),
                    "--pin-core",
                    str(args.pin_core),
                    "--csv_out",
                    str(tmp_csv),
                ]
                t0 = time.perf_counter()
                cp = run_cmd(cmd, repo_root)
                dt_ms = (time.perf_counter() - t0) * 1000.0
                if cp.returncode != 0:
                    record_failure("direct", rep_id, scenario, book, cp, cmd)
                    if not args.continue_on_error:
                        print("Direct benchmark failure; stopping campaign.", file=sys.stderr)
                        return 2
                    continue
                try:
                    row = read_single_row(tmp_csv, "direct", book, scenario)
                    if safe_float(row.get("Latency_ns", "0")) <= 0.0:
                        raise RuntimeError("Invalid direct row (non-positive latency)")
                    record_row("direct", rep_id, scenario, book, row, dt_ms, cmd)
                except Exception as exc:  # pylint: disable=broad-except
                    cp2 = subprocess.CompletedProcess(cmd, 99, cp.stdout, str(exc))
                    record_failure("direct", rep_id, scenario, book, cp2, cmd)
                    if not args.continue_on_error:
                        print(f"Failed to parse direct output: {exc}", file=sys.stderr)
                        return 3

    # Gateway mode campaign (optional)
    gateway_case_counter = 0
    for rep in range(1, args.reps_gateway + 1):
        rep_id = rep + args.replicate_offset
        order = books[:]
        rng.shuffle(order)
        for scenario in scenarios:
            for book in order:
                gateway_case_counter += 1
                case_ok = False
                for attempt in range(1, args.gateway_max_attempts + 1):
                    # Rotate ports per case+attempt to avoid bind/TIME_WAIT collisions.
                    offset = (gateway_case_counter + attempt - 1) % max(1, args.gateway_port_span)
                    gateway_port = args.gateway_port + offset

                    server = start_gateway_server(server_bin, book, gateway_port, repo_root)
                    time.sleep(args.gateway_start_delay)
                    if server.poll() is not None:
                        cp = subprocess.CompletedProcess(
                            [],
                            98,
                            "",
                            f"Gateway server failed to start (attempt {attempt}/{args.gateway_max_attempts})",
                        )
                        record_failure("gateway", rep_id, scenario, book, cp, [str(server_bin), "--book", book, "--port", str(gateway_port)])
                        continue

                    tmp_csv = tmp_dir / f"gateway_rep{rep}_{scenario}_{book}.csv"
                    cmd = [
                        str(bench_bin),
                        "--mode",
                        "gateway",
                        "--book",
                        book,
                        "--scenario",
                        scenario,
                        "--runs",
                        str(args.gateway_inner_runs),
                        "--orders",
                        str(args.orders),
                        "--port",
                        str(gateway_port),
                        "--csv_out",
                        str(tmp_csv),
                    ]
                    t0 = time.perf_counter()
                    cp = run_cmd(cmd, repo_root)
                    dt_ms = (time.perf_counter() - t0) * 1000.0
                    stop_process(server)

                    if cp.returncode != 0:
                        record_failure("gateway", rep_id, scenario, book, cp, cmd)
                        time.sleep(args.gateway_case_delay)
                        continue
                    try:
                        row = read_single_row(tmp_csv, "gateway", book, scenario)
                        if is_invalid_gateway_row(row):
                            raise RuntimeError("Invalid gateway row (all latency components are zero)")
                        record_row("gateway", rep_id, scenario, book, row, dt_ms, cmd)
                        case_ok = True
                        break
                    except Exception as exc:  # pylint: disable=broad-except
                        cp2 = subprocess.CompletedProcess(cmd, 99, cp.stdout, str(exc))
                        record_failure("gateway", rep_id, scenario, book, cp2, cmd)
                        time.sleep(args.gateway_case_delay)

                if not case_ok:
                    if not args.continue_on_error:
                        print("Gateway benchmark failure; stopping campaign.", file=sys.stderr)
                        return 5

                time.sleep(args.gateway_case_delay)

    manifest["completed_utc"] = now_utc_iso()
    manifest["raw_csv"] = str(raw_csv)
    manifest["failures_csv"] = str(failures_csv)

    with (output_dir / "manifest.json").open("w") as f:
        json.dump(manifest, f, indent=2)

    print(f"Campaign complete. Raw run data: {raw_csv}")
    print(f"Manifest: {output_dir / 'manifest.json'}")
    print(f"Failures (if any): {failures_csv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
