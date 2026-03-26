# Measurement Cookbook: Validating Cache & Allocator Hypotheses

This file documents how to extend measurements to validate H1 (cache locality) and H2 (allocator behavior).

## Error #2: Cache Profiling for H1 Validation

**Hypothesis H1 Claim:** "Cache-local direct indexing yields materially lower direct-mode latency"

**Missing Evidence:** L1/L2/L3 cache hit rates and miss penalties

### Measurement Approach

Use Linux `perf` to record cache events during direct-mode benchmarks:

```bash
# Direct mode with cache profiling (example: 1 scenario only)
perf stat -e cache-references,cache-misses,LLC-loads,LLC-load-misses,L1-dcache-load-misses \
  ./build/benchmarks/orderbook_benchmark --mode direct --book array --scenario mixed --runs 10 --orders 10000

# Repeat for other books for comparison:
for book in array map vector hybrid pool; do
  echo "=== $book ==="
  perf stat -e cache-references,cache-misses,LLC-loads,LLC-load-misses,L1-dcache-load-misses \
    ./build/benchmarks/orderbook_benchmark --mode direct --book "$book" --scenario mixed --runs 10 --orders 10000
done
```

**Expected Result Interpretation:**
- If array has significantly lower cache-miss ratio than map/vector, supports "cache locality" claim
- If vector has elevated L1-dcache-load-misses in dense_full, explains 75.3x slowdown mechanistically
- If vector miss rate ≈ array miss rate, suggests cache is NOT the primary differentiator

### Integration with Dissertation

Add to methods/evidence section:

```
H1 validation: Direct-mode benchmarks were re-run with `perf stat` recording cache events.
On the 2026-03-26 dataset, array mixed cache-miss ratio was 14.47% vs map 12.06%.
In dense_full, vector's generic cache-miss ratio was low (1.25%) but absolute L1-dcache-load-misses (148,441,143) and task-clock (1,919.02 ms) were extreme, consistent with the observed 75.3x latency slowdown.
```

**Status in this repo:** ✅ PARTIAL - perf stat comparisons collected and documented; LLC remains unsupported on this host

---

## Error #3: Allocator Behavior Measurements for H2 Validation

**Hypothesis H2 Claim:** "Memory pooling improves latency only when allocator overhead is significant"

**Missing Evidence:** Allocation count, allocator CPU time, page faults

### Measurement Approach A: Event Counting via perf

```bash
# Record page faults and heap allocations
perf stat -e page-faults,minor-faults,task-clock \
  ./build/benchmarks/orderbook_benchmark --mode direct --book pool --scenario dense_full --runs 1 --orders 10000

perf stat -e page-faults,minor-faults,task-clock \
  ./build/benchmarks/orderbook_benchmark --mode direct --book map --scenario dense_full --runs 1 --orders 10000
```

**Interpretation:**
- If pool has fewer page-faults than map in dense_full, supports pooling hypothesis
- If page-fault counts are similar, suggests page-fault rate is not the differentiator

### Measurement Approach B: Instrumented Allocator Tracking

Add manual allocation counter to `src/core/order_book.hpp`:

```cpp
// Top of file
static thread_local struct {
  size_t allocCount = 0;
  size_t deallocCount = 0;
  size_t bytesAllocated = 0;
} allocStats;

// In MapOrderBook on each insertion:
allocStats.allocCount++;
allocStats.bytesAllocated += sizeof(OrderNode);

// At benchmark completion:
printf("Allocations: %zu, Deallocations: %zu, Total bytes: %zu\n",
       allocStats.allocCount, allocStats.deallocCount, allocStats.bytesAllocated);
```

### Measurement Approach C: LD_PRELOAD Allocator Wrapper

Use an external allocator profiler (e.g., `google-perftools`):

```bash
# Install (Ubuntu)
apt-get install google-perftools libgoogle-perftools-dev

# Run with profiling
HEAPPROFILE=/tmp/heap_profile \
  ./build/benchmarks/orderbook_benchmark --mode direct --book pool --scenario mixed --runs 1 --orders 10000

# View results
pprof --text ./build/benchmarks/orderbook_benchmark /tmp/heap_profile.0*.heap
```

### Integration with Dissertation

Findings from both approaches:

```
H2 validation (current state): Explicit allocation counters are not yet instrumented, so allocator-causality remains partial.
Proxy evidence from page-fault and task-clock counters does not support a universal pooling benefit in this environment:
pool dense_full task-clock 231.28 ms vs map dense_full 155.48 ms, with higher page-fault activity in pool runs.
This supports a conditional interpretation: allocation strategy may matter, but ordering structure and execution path dominate observed latency outcomes here.
```

**Status in this repo:** ⚠️ PARTIAL - proxy measurements collected; explicit allocation counters still pending

---

## Why These Are Deferred

- **Perf stat** requires Linux kernel debug symbols and stable pinning; adds 10-20 min per scenario
- **Instrumented counters** require code changes and recompilation; risk affecting timing via measurement overhead
- **LD_PRELOAD profiler** adds runtime overhead that may distort results

## Recommendation

Run these measurements in a separate branch (`feature/cache-allocator-validation`) to keep dissertation-critical data separate from extended validation work. Use results to strengthen H1/H2 acceptance criteria (not to weaken them).

---

## 2026-03-26 Run Snapshot (Collected)

The targeted perf sweep was executed for `array/map/vector` on `mixed` and `dense_full` using:

- `/usr/lib/linux-tools-6.8.0-106/perf` (required on this WSL host)
- `--mode direct --runs 10 --orders 10000`

Observed highlights:

- mixed task-clock: array 77.28 ms, map 96.05 ms, vector 100.62 ms
- dense_full task-clock: array 73.75 ms, map 155.48 ms, vector 1919.02 ms
- dense_full L1-dcache-load-misses: array 674,099; map 1,188,878; vector 148,441,143
- mixed pool: task-clock 196.62 ms, page-faults 22,677, L1 misses 6,189,447
- dense_full pool: task-clock 231.28 ms, page-faults 22,662, L1 misses 5,745,192

Important caveat:

- Generic `cache-misses` ratio alone is not sufficient to explain ranking in this dataset.
- Absolute L1 miss volume plus task-clock inflation is more informative here.
- Pool/page-fault proxy behavior does not by itself support a universal pooling benefit claim.
- Allocator causality remains open until explicit allocation counters are instrumented.

### Practical WSL2 hit-rate method used

Because this WSL2 host does not expose all LLC events, cache hit-rate evidence was derived from L1 counters:

```bash
PERF_BIN=/usr/lib/linux-tools-6.8.0-106/perf
$PERF_BIN stat -x, -e L1-dcache-loads,L1-dcache-load-misses,cache-references,cache-misses,dTLB-load-misses,task-clock -- \
  ./build/benchmarks/orderbook_benchmark --mode direct --book array --scenario mixed --runs 10 --orders 10000
```

Derived metric used in analysis:

```text
L1_hit_rate = 1 - (L1-dcache-load-misses / L1-dcache-loads)
```

Generated evidence artifact:

- `results/perf/validation_real/cache_hit_summary.csv`

This gives defensible L1 hit-rate evidence on WSL2, while explicitly documenting that LLC-level hit-rate remains host-limited.

### Exact provenance commands used in this repository

```bash
# Host provenance capture
cd /root/projects/hft-orderbook-benchmark
{
  echo 'uname:'; uname -a
  echo; echo 'os-release:'; cat /etc/os-release | sed -n '1,6p'
  echo; echo 'perf:'; /usr/lib/linux-tools-6.8.0-106/perf --version
  echo; echo 'cpu:'; lscpu | grep -E 'Model name|Architecture|CPU\(s\)|Vendor ID'
} > results/perf/validation_real/host_provenance.txt

# L1 + generic cache hit-rate collection
PERF_BIN=/usr/lib/linux-tools-6.8.0-106/perf
$PERF_BIN stat -x, -e L1-dcache-loads,L1-dcache-load-misses,cache-references,cache-misses,dTLB-load-misses,task-clock -- \
  ./build/benchmarks/orderbook_benchmark --mode direct --book array --scenario mixed --runs 10 --orders 10000

# L2 feasibility probe
$PERF_BIN stat -x, -e l2_cache_req_stat.all,l2_cache_req_stat.dc_hit_in_l2,l2_cache_req_stat.ic_dc_miss_in_l2,task-clock -- \
  ./build/benchmarks/orderbook_benchmark --mode direct --book array --scenario mixed --runs 5 --orders 10000
```

Generated artifacts used by dissertation text:

- `results/perf/validation_real/cache_hit_summary.csv`
- `results/perf/validation_real/l2_summary.csv`
- `results/perf/validation_real/host_provenance.txt`
