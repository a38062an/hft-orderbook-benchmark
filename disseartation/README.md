# Dissertation Workspace (Local, Git-Ignored)

This directory is intentionally git-ignored so you can draft quickly inside the repo with full code context.

## What is included

- `report.tex`: main LaTeX template
- `chapters/`: one file per chapter
- `references.bib`: starter bibliography entries
- `Makefile`: PDF build helpers (`make pdf`, `make clean`)
- `initial_essay.md`: practical writing starter using your current findings

## Build PDF

```bash
cd disseartation
make pdf
```

If `latexmk` is installed it will be used; otherwise it falls back to `pdflatex` + `bibtex`.
