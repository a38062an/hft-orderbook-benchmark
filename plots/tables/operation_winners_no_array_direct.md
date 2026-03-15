# Operation Winners by Scenario (Direct Mode, Array Excluded)

| Scenario | Best Add Book | Add Latency (ns) | Best Cancel Book | Cancel Latency (ns) | Best Lookup Book | Lookup Latency (ns) |
|---|---:|---:|---:|---:|---:|---:|
| tight_spread | pool | 18.61 | n/a | n/a | pool | 14.95 |
| fixed_levels | pool | 16.06 | n/a | n/a | hybrid | 12.89 |
| mixed | pool | 17.49 | vector | 22.34 | hybrid | 12.17 |
| high_cancellation | pool | 21.36 | pool | 16.42 | pool | 12.35 |
| worst_case_fifo | pool | 22.44 | n/a | n/a | pool | 12.70 |
| sparse_extreme | pool | 19.70 | n/a | n/a | pool | 12.37 |
| dense_full | pool | 23.05 | n/a | n/a | pool | 12.46 |

Notes:
- Array is excluded to avoid fixed-range/fixed-tick structural bias in this comparison.
- `n/a` in Cancel means that scenario has no cancellation workload (Cancel_ns is zero for all eligible books).
