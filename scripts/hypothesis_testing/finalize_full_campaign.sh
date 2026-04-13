#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BASE_DIR="${1:?Usage: finalize_full_campaign.sh <results/hypothesis_testing/full_xxx>}"

DIRECT="$REPO_ROOT/$BASE_DIR/direct_full/raw_run_results.csv"
G1="$REPO_ROOT/$BASE_DIR/gateway_chunk1/raw_run_results.csv"
G2="$REPO_ROOT/$BASE_DIR/gateway_chunk2/raw_run_results.csv"
G3="$REPO_ROOT/$BASE_DIR/gateway_chunk3/raw_run_results.csv"
G4="$REPO_ROOT/$BASE_DIR/gateway_chunk4/raw_run_results.csv"

MERGED_DIR="$REPO_ROOT/$BASE_DIR/final_combined"
mkdir -p "$MERGED_DIR"
MERGED_RAW="$MERGED_DIR/raw_run_results.csv"

python3 "$REPO_ROOT/scripts/hypothesis_testing/combine_campaigns.py" \
  --inputs "$DIRECT" "$G1" "$G2" "$G3" "$G4" \
  --output "$MERGED_RAW"

python3 "$REPO_ROOT/scripts/hypothesis_testing/analyze_hypothesis_results.py" \
  --input "$MERGED_RAW" \
  --output-dir "$MERGED_DIR/analysis"

echo "Final combined outputs: $MERGED_DIR"
