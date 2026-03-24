# Operation Winners by Scenario (Direct Mode, Array Excluded)

| Scenario | Best Add Book | Add Latency (ns) | Best Cancel Book | Cancel Latency (ns) | Best Lookup Book | Lookup Latency (ns) |
|---|---:|---:|---:|---:|---:|---:|
| tight_spread | pool | 106.50 | n/a | n/a | pool | 57.00 |
| fixed_levels | pool | 109.73 | n/a | n/a | map | 55.88 |
| mixed | pool | 148.20 | pool | 279.82 | pool | 58.32 |
| high_cancellation | pool | 380.18 | pool | 127.09 | pool | 65.88 |
| worst_case_fifo | pool | 357.20 | n/a | n/a | map | 64.18 |
| sparse_extreme | pool | 293.78 | n/a | n/a | pool | 66.42 |
| dense_full | pool | 421.11 | n/a | n/a | pool | 69.51 |

Notes:
- Array is excluded to avoid fixed-range/fixed-tick structural bias in this comparison.
- `n/a` in Cancel means that scenario has no cancellation workload (Cancel_ns is zero for all eligible books).
