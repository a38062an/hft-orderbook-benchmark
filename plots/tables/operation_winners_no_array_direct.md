# Operation Winners by Scenario (Direct Mode, Array Excluded)

| Scenario | Best Add Book | Add Latency (ns) | Best Cancel Book | Cancel Latency (ns) | Best Lookup Book | Lookup Latency (ns) |
|---|---:|---:|---:|---:|---:|---:|
| tight_spread | pool | 280.13 | n/a | n/a | map | 63.19 |
| fixed_levels | pool | 293.70 | n/a | n/a | map | 63.97 |
| mixed | pool | 298.81 | vector | 325.80 | map | 63.31 |
| high_cancellation | pool | 357.41 | pool | 112.14 | map | 66.08 |
| worst_case_fifo | pool | 339.69 | n/a | n/a | map | 58.61 |
| sparse_extreme | pool | 320.86 | n/a | n/a | map | 67.29 |
| dense_full | pool | 467.00 | n/a | n/a | map | 67.71 |

Notes:
- Array is excluded to avoid fixed-range/fixed-tick structural bias in this comparison.
- `n/a` in Cancel means that scenario has no cancellation workload (Cancel_ns is zero for all eligible books).
