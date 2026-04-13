# Rank Table: Best Book per Scenario

| Scenario | Direct Best | Direct Latency (ns) | Gateway Best | Gateway Latency (ns) |
|---|---:|---:|---:|---:|
| tight_spread | array | 120.03 | vector | 299110.24 |
| fixed_levels | hybrid | 103.39 | vector | 367358.65 |
| mixed | array | 91.83 | vector | 357212.83 |
| high_cancellation | array | 91.86 | hybrid | 247254.16 |
| worst_case_fifo | array | 93.41 | hybrid | 285011.51 |
| sparse_extreme | array | 93.00 | array | 226784.69 |
| dense_full | array | 94.74 | map | 265689.60 |
