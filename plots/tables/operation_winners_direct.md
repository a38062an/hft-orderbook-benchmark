# Operation Winners by Scenario (Direct Mode)

| Scenario | Best Add Book | Add Latency (ns) | Best Cancel Book | Cancel Latency (ns) | Best Lookup Book | Lookup Latency (ns) |
|---|---:|---:|---:|---:|---:|---:|
| tight_spread | pool | 18.61 | n/a | n/a | pool | 14.95 |
| fixed_levels | pool | 16.06 | n/a | n/a | hybrid | 12.89 |
| mixed | array | 13.93 | array | 12.20 | hybrid | 12.17 |
| high_cancellation | array | 14.26 | array | 12.07 | array | 12.28 |
| worst_case_fifo | array | 14.29 | n/a | n/a | array | 12.41 |
| sparse_extreme | array | 14.15 | n/a | n/a | pool | 12.37 |
| dense_full | array | 14.47 | n/a | n/a | pool | 12.46 |

Notes:
- Add corresponds to Insert_ns, Cancel to Cancel_ns, and Lookup to Lookup_ns.
- `n/a` in Cancel means that scenario has no cancellation workload (Cancel_ns is zero for all books).
