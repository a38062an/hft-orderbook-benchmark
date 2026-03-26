# Dissertation Workspace

This directory is the working area for dissertation writing and evidence notes in this repository.

## What is included

- `report.tex`: main LaTeX template
- `chapters/`: one file per chapter
- `references.bib`: starter bibliography entries
- `Makefile`: PDF build helpers (`make pdf`, `make clean`)
- `initial_essay.md`: practical writing starter using your current findings

## Evidence And Viva Resources (Start Here)

- `ERROR_LOG_2026-03-26.md`: master list of issues, fixes, and remaining sub-gaps
- `results_analysis_2026-03-26.md`: canonical findings and mechanism addendum
- `MEASUREMENT_COOKBOOK_2026-03-26.md`: reproducible measurement workflow and caveats
- `../results/perf/validation_real/cache_hit_summary.csv`: derived L1 hit-rate evidence table (WSL2)
- `captions_canonical_2026-03-26.md`: figure captions with evidence anchors
- `hypotheses_refined.tex`: final hypothesis wording (viva-safe, evidence-bounded)

## Remaining Gaps (Easy To Find)

Current non-closed gaps are explicitly tracked in `ERROR_LOG_2026-03-26.md` under:

- `PARTIALLY_RESOLVED` items for cache/allocator causality limits
- `OUT_OF_SCOPE` item for ablation variants (`hybrid_pool`, `array_pool`)

This keeps unresolved risks visible and reviewable from one place.

## Build PDF

```bash
cd disseartation
make pdf
```

If `latexmk` is installed it will be used; otherwise it falls back to `pdflatex` + `bibtex`.
