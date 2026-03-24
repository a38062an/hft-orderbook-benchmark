# Operation Winners by Scenario (Direct Mode)

| Scenario | Best Add Book | Add Latency (ns) | Best Cancel Book | Cancel Latency (ns) | Best Lookup Book | Lookup Latency (ns) |
|---|---:|---:|---:|---:|---:|---:|
| tight_spread | array | 79.01 | n/a | n/a | array | 49.71 |
| fixed_levels | array | 47.04 | n/a | n/a | array | 31.45 |
| mixed | array | 46.79 | array | 46.04 | array | 31.79 |
| high_cancellation | array | 45.99 | array | 44.79 | array | 31.58 |
| worst_case_fifo | array | 46.29 | n/a | n/a | array | 31.81 |
| sparse_extreme | array | 47.03 | n/a | n/a | array | 32.09 |
| dense_full | array | 49.89 | n/a | n/a | array | 32.31 |

Notes:
- Add corresponds to Insert_ns, Cancel to Cancel_ns, and Lookup to Lookup_ns.
- `n/a` in Cancel means that scenario has no cancellation workload (Cancel_ns is zero for all books).
