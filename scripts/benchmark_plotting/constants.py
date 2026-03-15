"""Static plot ordering and naming constants."""

BOOK_ORDER = ["array", "hybrid", "pool", "map", "vector"]
BOOK_ORDER_NO_VECTOR = ["array", "hybrid", "pool", "map"]
MIDDLE_TIER_BOOKS = ["hybrid", "pool", "map"]
SCENARIO_ORDER = [
    "tight_spread",
    "fixed_levels",
    "mixed",
    "high_cancellation",
    "worst_case_fifo",
    "sparse_extreme",
    "dense_full",
]
MPSC_PRODUCER_ORDER = [1, 2, 4, 8]
REPRESENTATIVE_SCENARIOS = ["mixed", "dense_full", "worst_case_fifo"]
