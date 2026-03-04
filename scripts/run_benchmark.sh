#!/bin/bash

# High-Frequency Trading OrderBook Benchmark Runner
# Runs benchmarks multiple times with process pinning for fairness

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
RESULTS_DIR="$PROJECT_ROOT/results"
NUM_RUNS=5

echo "=========================================="
echo "OrderBook Benchmark Runner (${NUM_RUNS} runs)"
echo "=========================================="
echo ""

# Check if benchmark is built
if [ ! -f "$BUILD_DIR/benchmarks/orderbook_benchmark" ]; then
    echo "Error: Benchmark executable not found!"
    echo "Please build the project first:"
    echo "  cd build && cmake .. && make orderbook_benchmark"
    exit 1
fi

# Create results directory
mkdir -p "$RESULTS_DIR"

# Run benchmark multiple times
for run_num in $(seq 1 $NUM_RUNS); do
    echo "=========================================="
    echo "Run ${run_num}/${NUM_RUNS}"
    echo "=========================================="
    echo ""
    
    cd "$BUILD_DIR/benchmarks"
    
    # Run benchmark
    echo "Running benchmark..."
    ./orderbook_benchmark
    
    echo ""
    echo "Run ${run_num}/${NUM_RUNS} complete!"
    echo ""
    
    # Small delay between runs to let system settle
    if [ $run_num -lt $NUM_RUNS ]; then
        echo "Waiting 5 seconds before next run..."
        sleep 5
    fi
done

# Find all CSV files from this session
CSV_FILES=$(ls -t "$RESULTS_DIR"/benchmark_breakdown_*.csv 2>/dev/null | head -$NUM_RUNS)

if [ -z "$CSV_FILES" ]; then
    echo ""
    echo "Warning: No results CSV found in $RESULTS_DIR"
    echo "Results may have been saved to a different location."
    exit 1
fi

echo ""
echo "=========================================="
echo "All Runs Complete!"
echo "=========================================="
echo ""
echo "Results saved:"
for csv in $CSV_FILES; do
    echo "  - $csv"
done
echo ""
echo "Total runs: $NUM_RUNS"
echo "Average the results for final analysis"
echo ""
