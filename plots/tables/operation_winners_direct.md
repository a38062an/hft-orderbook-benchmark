# Operation Winners by Scenario (Direct Mode)

| Scenario | Best Add Book | Add Latency (ns) | Best Cancel Book | Cancel Latency (ns) | Best Lookup Book | Lookup Latency (ns) |
|---|---:|---:|---:|---:|---:|---:|
| tight_spread | array | 48.01 | n/a | n/a | array | 32.32 |
| fixed_levels | array | 45.80 | n/a | n/a | array | 31.98 |
| mixed | array | 46.14 | array | 46.30 | array | 31.74 |
| high_cancellation | array | 46.44 | array | 46.38 | array | 31.69 |
| worst_case_fifo | array | 49.24 | n/a | n/a | array | 34.36 |
| sparse_extreme | array | 48.40 | n/a | n/a | array | 32.59 |
| dense_full | array | 45.06 | n/a | n/a | array | 31.60 |

Notes:
- Add corresponds to Insert_ns, Cancel to Cancel_ns, and Lookup to Lookup_ns.
- `n/a` in Cancel means that scenario has no cancellation workload (Cancel_ns is zero for all books).
