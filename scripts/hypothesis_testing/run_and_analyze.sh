#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

OUT_DIR="${1:-}" 
if [[ -z "$OUT_DIR" ]]; then
  TS="$(date +%Y%m%d_%H%M%S)"
  OUT_DIR="$REPO_ROOT/results/hypothesis_testing/$TS"
fi

python3 "$REPO_ROOT/scripts/hypothesis_testing/run_hypothesis_campaign.py" \
  --repo-root "$REPO_ROOT" \
  --output-dir "$OUT_DIR" \
  --reps-direct 30 \
  --reps-gateway 0 \
  --orders 10000 \
  --pin-core 0

python3 "$REPO_ROOT/scripts/hypothesis_testing/analyze_hypothesis_results.py" \
  --input "$OUT_DIR/raw_run_results.csv"

echo "All done. Artifacts in: $OUT_DIR"
