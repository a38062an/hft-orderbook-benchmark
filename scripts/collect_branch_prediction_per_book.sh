#!/bin/bash
set -euo pipefail

BENCH_BIN="./build/benchmarks/orderbook_benchmark"
OUT_DIR="results/perf/validation_real"
RUNS=10
ORDERS=10000
BOOKS_CSV="array,map,vector,pool"
SCENARIOS_CSV="mixed,dense_full"
PIN_MODE="auto"
PIN_CORE=""
PERF_BIN=""

usage() {
    cat <<'EOF'
Usage: scripts/collect_branch_prediction_per_book.sh [options]

Collect per-orderbook branch predictor counters using Linux perf and write:
- per-case raw perf files: results/perf/validation_real/perf_branch_<book>_<scenario>.txt
- summary CSV:           results/perf/validation_real/branch_prediction_summary.csv

Options:
  --bench-bin PATH        Benchmark binary (default: ./build/benchmarks/orderbook_benchmark)
  --out-dir PATH          Output directory (default: results/perf/validation_real)
  --runs N                Runs per case (default: 10)
  --orders N              Orders per run (default: 10000)
  --books CSV             Comma-separated books (default: array,map,vector,pool)
  --scenarios CSV         Comma-separated scenarios (default: mixed,dense_full)
  --perf-bin PATH         Explicit perf binary path
  --pin-core N            Pin benchmark to core N
  --no-pin                Disable CPU pinning
  --help                  Show this help

Examples:
  scripts/collect_branch_prediction_per_book.sh
  scripts/collect_branch_prediction_per_book.sh --books array,map,vector,pool --scenarios mixed,dense_full
  scripts/collect_branch_prediction_per_book.sh --bench-bin ./build/benchmarks/orderbook_benchmark --perf-bin /usr/lib/linux-tools-6.8.0-106/perf
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --bench-bin) BENCH_BIN="$2"; shift 2 ;;
        --out-dir) OUT_DIR="$2"; shift 2 ;;
        --runs) RUNS="$2"; shift 2 ;;
        --orders) ORDERS="$2"; shift 2 ;;
        --books) BOOKS_CSV="$2"; shift 2 ;;
        --scenarios) SCENARIOS_CSV="$2"; shift 2 ;;
        --perf-bin) PERF_BIN="$2"; shift 2 ;;
        --pin-core) PIN_MODE="fixed"; PIN_CORE="$2"; shift 2 ;;
        --no-pin) PIN_MODE="off"; shift 1 ;;
        --help|-h) usage; exit 0 ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

if [[ ! -x "$BENCH_BIN" ]]; then
    echo "Error: benchmark binary not found or not executable: $BENCH_BIN"
    exit 1
fi

resolve_perf() {
    if [[ -n "$PERF_BIN" ]]; then
        if [[ ! -x "$PERF_BIN" ]]; then
            echo "Error: --perf-bin is not executable: $PERF_BIN"
            exit 1
        fi
        if ! "$PERF_BIN" --version >/dev/null 2>&1; then
            echo "Error: --perf-bin failed '--version': $PERF_BIN"
            exit 1
        fi
        return
    fi

    if command -v perf >/dev/null 2>&1 && perf --version >/dev/null 2>&1; then
        PERF_BIN="perf"
        return
    fi

    local fallback
    fallback=$(ls -1 /usr/lib/linux-tools-*/perf 2>/dev/null | sort -V | tail -n1 || true)
    if [[ -n "$fallback" && -x "$fallback" ]] && "$fallback" --version >/dev/null 2>&1; then
        PERF_BIN="$fallback"
        return
    fi

    echo "Error: unable to find a usable perf binary."
    echo "Hint: install linux-tools for your kernel or pass --perf-bin /full/path/to/perf"
    exit 1
}

choose_pin_core() {
    if [[ "$PIN_MODE" == "off" ]]; then
        PIN_CORE=""
        return
    fi

    if [[ "$PIN_MODE" == "fixed" ]]; then
        return
    fi

    local allowed
    allowed=$(taskset -pc $$ 2>/dev/null | sed -E 's/.*: *//' || true)
    if [[ -z "$allowed" ]]; then
        PIN_MODE="off"
        PIN_CORE=""
        return
    fi

    PIN_CORE=$(echo "$allowed" | cut -d, -f1 | cut -d- -f1)
    if [[ -z "$PIN_CORE" ]]; then
        PIN_MODE="off"
    fi
}

extract_event_value() {
    local file="$1"
    local event="$2"
    awk -F, -v ev="$event" '
        $3 == ev {
            gsub(/[[:space:]]/, "", $1)
            print $1
            exit
        }
    ' "$file"
}

extract_task_clock_ms() {
    local file="$1"
    awk -F, '
        $3 == "task-clock" {
            v = $1
            u = $2
            gsub(/[[:space:]]/, "", v)
            gsub(/[[:space:]]/, "", u)
            if (v == "" || v == "<notsupported>" || v == "<notcounted>") {
                print "NA"
                exit
            }
            if (u == "msec") {
                printf "%.2f\n", v + 0
                exit
            }
            if (u == "sec") {
                printf "%.2f\n", (v + 0) * 1000
                exit
            }
            if (u == "usec") {
                printf "%.2f\n", (v + 0) / 1000
                exit
            }
            if (u == "nsec") {
                printf "%.2f\n", (v + 0) / 1000000
                exit
            }
            printf "%.2f\n", v + 0
            exit
        }
    ' "$file"
}

is_number() {
    [[ "$1" =~ ^[0-9]+([.][0-9]+)?$ ]]
}

safe_value() {
    local v="$1"
    if [[ -z "$v" ]]; then
        echo "NA"
        return
    fi
    if [[ "$v" == "<not\ supported>" || "$v" == "<not\ counted>" ]]; then
        echo "NA"
        return
    fi
    if is_number "$v"; then
        echo "$v"
        return
    fi
    echo "NA"
}

calc_ratio() {
    local numerator="$1"
    local denominator="$2"
    if is_number "$numerator" && is_number "$denominator"; then
        awk -v n="$numerator" -v d="$denominator" 'BEGIN { if (d > 0) printf "%.6f", n / d; else print "NA" }'
    else
        echo "NA"
    fi
}

calc_one_minus() {
    local ratio="$1"
    if is_number "$ratio"; then
        awk -v r="$ratio" 'BEGIN { printf "%.6f", 1 - r }'
    else
        echo "NA"
    fi
}

calc_pct() {
    local ratio="$1"
    if is_number "$ratio"; then
        awk -v r="$ratio" 'BEGIN { printf "%.3f", r * 100 }'
    else
        echo "NA"
    fi
}

calc_ipc() {
    local instructions="$1"
    local cycles="$2"
    if is_number "$instructions" && is_number "$cycles"; then
        awk -v i="$instructions" -v c="$cycles" 'BEGIN { if (c > 0) printf "%.4f", i / c; else print "NA" }'
    else
        echo "NA"
    fi
}

calc_mpki() {
    local misses="$1"
    local instructions="$2"
    if is_number "$misses" && is_number "$instructions"; then
        awk -v m="$misses" -v i="$instructions" 'BEGIN { if (i > 0) printf "%.4f", (m * 1000) / i; else print "NA" }'
    else
        echo "NA"
    fi
}

resolve_perf
choose_pin_core

mkdir -p "$OUT_DIR"
SUMMARY_CSV="$OUT_DIR/branch_prediction_summary.csv"
HOST_FILE="$OUT_DIR/host_provenance_branch.txt"

{
    echo "uname:"
    uname -a
    echo
    echo "os-release:"
    cat /etc/os-release 2>/dev/null | sed -n '1,6p' || true
    echo
    echo "perf:"
    "$PERF_BIN" --version || true
    echo
    echo "cpu:"
    lscpu 2>/dev/null | grep -E 'Model name|Architecture|CPU\(s\)|Vendor ID' || true
} > "$HOST_FILE"

echo "book,scenario,branches,branch_misses,branch_miss_rate,branch_hit_rate,branch_miss_pct,instructions,cycles,ipc,branch_mpki,task_clock_ms,perf_file" > "$SUMMARY_CSV"

IFS=',' read -r -a BOOKS <<< "$BOOKS_CSV"
IFS=',' read -r -a SCENARIOS <<< "$SCENARIOS_CSV"

EVENTS="branches,branch-misses,instructions,cycles,task-clock"

echo "Using perf binary: $PERF_BIN"
if [[ "$PIN_MODE" == "off" ]]; then
    echo "CPU pinning: disabled"
else
    echo "CPU pinning: core $PIN_CORE"
fi

echo "Collecting branch metrics into: $OUT_DIR"

for book in "${BOOKS[@]}"; do
    for scenario in "${SCENARIOS[@]}"; do
        perf_file="$OUT_DIR/perf_branch_${book}_${scenario}.txt"
        cmd=("$BENCH_BIN" --mode direct --book "$book" --scenario "$scenario" --runs "$RUNS" --orders "$ORDERS")

        echo "Running: book=$book scenario=$scenario"
        if [[ "$PIN_MODE" == "off" ]]; then
            "$PERF_BIN" stat -x, -e "$EVENTS" -o "$perf_file" -- "${cmd[@]}"
        else
            taskset -c "$PIN_CORE" "$PERF_BIN" stat -x, -e "$EVENTS" -o "$perf_file" -- "${cmd[@]}"
        fi

        branches=$(safe_value "$(extract_event_value "$perf_file" "branches")")
        branch_misses=$(safe_value "$(extract_event_value "$perf_file" "branch-misses")")
        instructions=$(safe_value "$(extract_event_value "$perf_file" "instructions")")
        cycles=$(safe_value "$(extract_event_value "$perf_file" "cycles")")
        task_clock_ms=$(extract_task_clock_ms "$perf_file")
        if [[ -z "$task_clock_ms" ]]; then
            task_clock_ms="NA"
        fi

        miss_rate=$(calc_ratio "$branch_misses" "$branches")
        hit_rate=$(calc_one_minus "$miss_rate")
        miss_pct=$(calc_pct "$miss_rate")
        ipc=$(calc_ipc "$instructions" "$cycles")
        mpki=$(calc_mpki "$branch_misses" "$instructions")

        echo "$book,$scenario,$branches,$branch_misses,$miss_rate,$hit_rate,$miss_pct,$instructions,$cycles,$ipc,$mpki,$task_clock_ms,$perf_file" >> "$SUMMARY_CSV"
    done
done

echo "Done."
echo "Summary CSV: $SUMMARY_CSV"
echo "Host provenance: $HOST_FILE"
