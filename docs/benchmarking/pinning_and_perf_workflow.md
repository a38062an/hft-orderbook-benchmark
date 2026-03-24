# Pinning And Perf Workflow (Linux/WSL2)

This is the canonical benchmark execution workflow for performance measurements. It enforces the following policy:

- **Direct mode**: pinned to one allowed CPU core.
- **Gateway mode**: unpinned.
- **Perf counters**: captured for direct mode and written to disk.

**Environment**: This workflow requires Linux or WSL2 (Windows Subsystem for Linux 2) for CPU affinity controls and perf event collection. It is not suitable for macOS (which has restricted thread pinning). For correctness testing on macOS, see `docs/testing/testing_strategy.md`.

## Command

```bash
./scripts/run_direct_gateway_with_perf.sh --scenario all --runs 5 --orders 10000 --with-mpsc --mpsc-producers all
```

For a clean, fresh dataset:

```bash
rm -f results/results.csv
./scripts/run_direct_gateway_with_perf.sh --scenario all --runs 5 --orders 10000 --with-mpsc --mpsc-producers all
```

If perf is unavailable, run with:

```bash
./scripts/run_direct_gateway_with_perf.sh --scenario all --runs 5 --orders 10000 --with-mpsc --mpsc-producers all --skip-perf
```

## What Gets Recorded

- Perf counters: `results/perf/direct_perf_<timestamp>.txt`
- Run metadata manifest: `results/perf/manifest.csv`
- Benchmark aggregates: `results/results.csv`

`results/results.csv` is key-upserted by mode/book/scenario (and by producerCount for MPSC), so reruns update existing keys and append missing keys without creating duplicates.

The manifest appends one row per execution with timestamp, pinned core ID, scenario, run count, order count, and perf log path. This ledger enables reproducibility audits and cross-run comparisons.

## Perf Events

The runner captures:

- `task-clock`
- `cycles`
- `instructions`
- `cache-references`
- `cache-misses`
- `branches`
- `branch-misses`

## Pinning Evidence Checklist

Use this checklist to show, in a report/dissertation, that pinning policy was actually enforced at runtime.

### 1) Direct and MPSC pinned, Gateway unpinned

Run the integrated workflow and capture console output:

```bash
./scripts/run_direct_gateway_with_perf.sh --scenario all --runs 5 --orders 10000 --with-mpsc --mpsc-producers all | tee results/pinning_evidence.log
```

Expected evidence in `results/pinning_evidence.log`:

- Direct/MPSC pinning line from benchmark process:
	- `Thread successfully pinned to core <id>`
- Gateway unpinned policy line from sweep script:
	- `Pinning: disabled for gateway sweep`

Quick grep checks:

```bash
grep -n "Thread successfully pinned to core" results/pinning_evidence.log
grep -n "Pinning: disabled for gateway sweep" results/pinning_evidence.log
```

### 2) Core selection evidence

The integrated runner prints the selected allowed core set and chosen pin target:

- `Using allowed core list: ...`
- `Direct mode pin target: core ...`

This demonstrates pinning stayed inside the scheduler-allowed CPU mask.

### 3) Reproducibility and Audit Artifacts

Persisted artifacts that fully document and support reproducibility claims:

- `results/perf/manifest.csv` records execution timestamp, pinned core ID, scenario name, run count, order count, output CSV file, and perf log path. This allows reconstruction of the exact conditions for any row in the results.
- `results/perf/direct_perf_<timestamp>.txt` records Linux perf counter data for direct-mode runs, including task-clock, cycles, instructions, cache behavior, and branch statistics.
- `results/results.csv` records aggregated benchmark rows (Direct/Gateway/MPSC) for the same execution, with means and standard deviations.

### 4) Completeness check for full sweep

For a full `all` run, verify complete matrix population:

```bash
awk -F, 'NR>1{m[$1]++} END{print "direct="m["direct"]", gateway="m["gateway"]", mpsc="m["mpsc"]", total="m["direct"]+m["gateway"]+m["mpsc"]}' results/results.csv
```

Expected:

- `direct=35`
- `gateway=35`
- `mpsc=140`
- `total=210`
