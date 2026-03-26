TEX=report.tex
PDF=report.pdf

.PHONY: pdf clean

pdf:
	@if command -v latexmk >/dev/null 2>&1; then \
		latexmk -pdf -interaction=nonstopmode $(TEX); \
	else \
		pdflatex -interaction=nonstopmode $(TEX) && bibtex report && pdflatex -interaction=nonstopmode $(TEX) && pdflatex -interaction=nonstopmode $(TEX); \
	fi

clean:
	rm -f *.aux *.bbl *.bcf *.blg *.fdb_latexmk *.fls *.log *.out *.run.xml *.toc *.lof *.lot
