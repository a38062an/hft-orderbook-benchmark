#!/bin/bash
set -u
set -o pipefail

# Configuration
SERVER_BIN="./build/src/hft_exchange_server"
CLIENT_BIN="./build/benchmarks/orderbook_benchmark"
CSV_OUT="results/results.csv"
PORT=12345
MAX_ATTEMPTS=2
# Usage: ./scripts/run_gateway_sweep.sh [scenario|all] [runs] [orders]
# Example: ./scripts/run_gateway_sweep.sh all 5 10000

# Default Values
SCENARIO="mixed"
RUNS=3
ORDERS=10000
BOOKS=""

# Parse Arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --scenario) SCENARIO="$2"; shift 2 ;;
        --runs)     RUNS="$2";     shift 2 ;;
        --orders)   ORDERS="$2";   shift 2 ;;
        --book)     BOOKS="$2";    shift 2 ;; # Allow overriding BOOKS via flag too
        *)
            # Support positional arguments if no flags are used
            if [[ "$1" != --* ]]; then
                SCENARIO=${1:-$SCENARIO}
                RUNS=${2:-$RUNS}
                ORDERS=${3:-$ORDERS}
                break
            else
                echo "Unknown option: $1"
                exit 1
            fi
            ;;
    esac
done

# Ensure results directory exists
mkdir -p results

has_gateway_row() {
    local csv_file="$1"
    local book="$2"
    local scenario="$3"

    if [[ ! -f "$csv_file" ]]; then
        return 1
    fi

    awk -F, -v b="$book" -v s="$scenario" 'NR>1 && $1=="gateway" && $2==b && $3==s {found=1} END{exit(found?0:1)}' "$csv_file"
}

# Clean previous results if desired (optional)
# rm $CSV_OUT

# Discover supported books dynamically if none specified
if [ -z "$BOOKS" ]; then
    BOOKS=$($CLIENT_BIN --list_books 2>/dev/null)
    if [ $? -ne 0 ] || [ -z "$BOOKS" ]; then
        echo "Warning: Could not discover books from $CLIENT_BIN. Using defaults."
        BOOKS="map vector array hybrid pool"
    fi
fi
BOOKS_ARRAY=($BOOKS)

# Discover scenarios if 'all' specified
if [ "$SCENARIO" == "all" ]; then
    SCENARIOS=$($CLIENT_BIN --list_scenarios 2>/dev/null)
    if [ $? -ne 0 ]; then
        SCENARIOS="dense_full fixed_levels mixed tight_spread high_cancellation sparse_extreme worst_case_fifo"
    fi
else
    SCENARIOS=$SCENARIO
fi
SCENARIOS_ARRAY=($SCENARIOS)

echo "Starting Full HFT Gateway Comparison..."
echo "========================================"
echo "Pinning: disabled for gateway sweep"

for CUR_SCENARIO in "${SCENARIOS_ARRAY[@]}"
do
    echo "Scenario: $CUR_SCENARIO"
    echo "----------------------------------------"
    
    for BOOK in "${BOOKS_ARRAY[@]}"
    do
        echo "  - Testing Implementation: $BOOK"

        success=0
        for ATTEMPT in $(seq 1 "$MAX_ATTEMPTS")
        do
            echo "    Attempt $ATTEMPT/$MAX_ATTEMPTS"

            # 1. Start Server in background
            $SERVER_BIN --book "$BOOK" --port "$PORT" > /dev/null 2>&1 &
            SERVER_PID=$!

            # 2. Wait for server to initialize
            sleep 1

            # 3. Run Client Benchmark (Specific scenario, not 'all')
            $CLIENT_BIN --mode gateway --book "$BOOK" --port "$PORT" --runs "$RUNS" --orders "$ORDERS" --scenario "$CUR_SCENARIO" --csv_out "$CSV_OUT"
            CLIENT_EXIT=$?

            # 4. Shutdown Server (best effort)
            kill -INT "$SERVER_PID" >/dev/null 2>&1 || true
            wait "$SERVER_PID" >/dev/null 2>&1 || true

            # 5. Validate this specific row exists in CSV
            if [[ "$CLIENT_EXIT" -eq 0 ]] && has_gateway_row "$CSV_OUT" "$BOOK" "$CUR_SCENARIO"; then
                success=1
                break
            fi

            echo "    Warning: gateway case missing/failed for book=$BOOK scenario=$CUR_SCENARIO (exit=$CLIENT_EXIT)"
            sleep 1
        done

        if [[ "$success" -ne 1 ]]; then
            echo "Error: gateway case failed after $MAX_ATTEMPTS attempts: book=$BOOK scenario=$CUR_SCENARIO"
            exit 1
        fi

        # Small gap between cases for port reuse
        sleep 1
    done
    echo "----------------------------------------"
done

echo "Benchmark Complete. Results saved to $CSV_OUT"
echo "Run 'python3 scripts/plot_benchmark.py $CSV_OUT' to visualize."
