#!/bin/bash
set -euo pipefail

BENCH_BIN="./build/benchmarks/orderbook_benchmark"
GATEWAY_SCRIPT="./scripts/run_gateway_sweep.sh"
CSV_OUT="results/results.csv"
SCENARIO="all"
RUNS=5
ORDERS=10000
BOOKS=""
WITH_MPSC=0
MPSC_PRODUCERS="all"
SKIP_PERF=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --scenario) SCENARIO="$2"; shift 2 ;;
        --runs) RUNS="$2"; shift 2 ;;
        --orders) ORDERS="$2"; shift 2 ;;
        --csv_out) CSV_OUT="$2"; shift 2 ;;
        --book) BOOKS="$2"; shift 2 ;;
        --with-mpsc) WITH_MPSC=1; shift 1 ;;
        --mpsc-producers) MPSC_PRODUCERS="$2"; shift 2 ;;
        --skip-perf) SKIP_PERF=1; shift 1 ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

if [[ ! -x "$BENCH_BIN" ]]; then
    echo "Error: benchmark binary not found or not executable: $BENCH_BIN"
    exit 1
fi

if [[ ! -x "$GATEWAY_SCRIPT" ]]; then
    echo "Error: gateway script not found or not executable: $GATEWAY_SCRIPT"
    exit 1
fi

HAVE_PERF=0
PERF_CMD=""

# Prefer system perf when it is directly usable.
if command -v perf >/dev/null 2>&1 && perf --version >/dev/null 2>&1; then
    HAVE_PERF=1
    PERF_CMD="perf"
else
    # WSL can have a kernel-mismatched /usr/bin/perf wrapper; try real binaries.
    PERF_FALLBACK=$(ls -1 /usr/lib/linux-tools-*/perf 2>/dev/null | sort -V | tail -n1 || true)
    if [[ -n "$PERF_FALLBACK" && -x "$PERF_FALLBACK" ]] && "$PERF_FALLBACK" --version >/dev/null 2>&1; then
        HAVE_PERF=1
        PERF_CMD="$PERF_FALLBACK"
    fi
fi

if [[ "$HAVE_PERF" -eq 0 && "$SKIP_PERF" -eq 0 ]]; then
    echo "Error: 'perf' is not installed or not in PATH."
    echo "Hint: rerun with --skip-perf to execute benchmarks without perf counters."
    exit 1
fi

mkdir -p results/perf

ALLOWED_CORES=$(taskset -pc $$ | sed -E 's/.*: *//')
PIN_CORE=$(echo "$ALLOWED_CORES" | cut -d, -f1 | cut -d- -f1)
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
PERF_LOG="results/perf/direct_perf_${TIMESTAMP}.txt"
MANIFEST="results/perf/manifest.csv"

if [[ -n "$BOOKS" ]]; then
    DIRECT_BOOK_ARGS=(--book "$BOOKS")
    GATEWAY_BOOK_ARGS=(--book "$BOOKS")
else
    DIRECT_BOOK_ARGS=(--book all)
    GATEWAY_BOOK_ARGS=()
fi

echo "Using allowed core list: $ALLOWED_CORES"
echo "Direct mode pin target: core $PIN_CORE"
if [[ "$HAVE_PERF" -eq 1 && "$SKIP_PERF" -eq 0 ]]; then
    echo "Perf command: $PERF_CMD"
    echo "Perf log: $PERF_LOG"
else
    echo "Perf recording: skipped"
fi

if [[ "$HAVE_PERF" -eq 1 && "$SKIP_PERF" -eq 0 ]]; then
    echo "=== Step 1: Direct benchmark (pinned) with perf ==="
    taskset -c "$PIN_CORE" "$PERF_CMD" stat \
        -e task-clock,cycles,instructions,cache-references,cache-misses,branches,branch-misses \
        -o "$PERF_LOG" \
        -- \
        "$BENCH_BIN" \
        --mode direct \
        "${DIRECT_BOOK_ARGS[@]}" \
        --scenario "$SCENARIO" \
        --runs "$RUNS" \
        --orders "$ORDERS" \
        --pin-core "$PIN_CORE" \
        --csv_out "$CSV_OUT"
else
    echo "=== Step 1: Direct benchmark (pinned) without perf ==="
    taskset -c "$PIN_CORE" \
        "$BENCH_BIN" \
        --mode direct \
        "${DIRECT_BOOK_ARGS[@]}" \
        --scenario "$SCENARIO" \
        --runs "$RUNS" \
        --orders "$ORDERS" \
        --pin-core "$PIN_CORE" \
        --csv_out "$CSV_OUT"
fi

echo "=== Step 2: Gateway benchmark (unpinned) ==="
"$GATEWAY_SCRIPT" --scenario "$SCENARIO" --runs "$RUNS" --orders "$ORDERS" "${GATEWAY_BOOK_ARGS[@]}"

if [[ "$WITH_MPSC" -eq 1 ]]; then
    echo "=== Step 3: MPSC benchmark (pinned single core) ==="
    taskset -c "$PIN_CORE" \
        "$BENCH_BIN" \
        --mode mpsc \
        "${DIRECT_BOOK_ARGS[@]}" \
        --scenario "$SCENARIO" \
        --runs "$RUNS" \
        --orders "$ORDERS" \
        --producers "$MPSC_PRODUCERS" \
        --pin-core "$PIN_CORE" \
        --csv_out "$CSV_OUT"
fi

if [[ ! -f "$MANIFEST" ]]; then
    echo "timestamp,step,mode,pinning,core,scenario,runs,orders,books,csv_out,perf_log" > "$MANIFEST"
fi

if [[ "$HAVE_PERF" -eq 1 && "$SKIP_PERF" -eq 0 ]]; then
    echo "${TIMESTAMP},direct+gateway,direct,pinned,${PIN_CORE},${SCENARIO},${RUNS},${ORDERS},${BOOKS:-all},${CSV_OUT},${PERF_LOG}" >> "$MANIFEST"
else
    echo "${TIMESTAMP},direct+gateway,direct,pinned,${PIN_CORE},${SCENARIO},${RUNS},${ORDERS},${BOOKS:-all},${CSV_OUT}," >> "$MANIFEST"
fi

if [[ "$WITH_MPSC" -eq 1 ]]; then
    echo "${TIMESTAMP},mpsc,mpsc,pinned,${PIN_CORE},${SCENARIO},${RUNS},${ORDERS},${BOOKS:-all},${CSV_OUT}," >> "$MANIFEST"
fi

echo "Done."
if [[ "$HAVE_PERF" -eq 1 && "$SKIP_PERF" -eq 0 ]]; then
    echo "Recorded perf output: $PERF_LOG"
fi
echo "Recorded run manifest: $MANIFEST"
