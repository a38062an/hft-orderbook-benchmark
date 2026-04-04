TEX=report.tex
PDF=report.pdf
BUILD_DIR=build
PDF_OUT=$(BUILD_DIR)/$(PDF)

.PHONY: pdf clean

pdf:
	@mkdir -p $(BUILD_DIR) $(BUILD_DIR)/chapters
	@if command -v latexmk >/dev/null 2>&1; then \
		latexmk -pdf -interaction=nonstopmode -outdir=$(BUILD_DIR) $(TEX); \
	else \
		pdflatex -interaction=nonstopmode -output-directory=$(BUILD_DIR) $(TEX) && \
		if grep -q '\\citation' $(BUILD_DIR)/report.aux; then bibtex $(BUILD_DIR)/report; else echo 'No citations found; skipping bibtex'; fi && \
		pdflatex -interaction=nonstopmode -output-directory=$(BUILD_DIR) $(TEX) && \
		pdflatex -interaction=nonstopmode -output-directory=$(BUILD_DIR) $(TEX); \
	fi

clean:
	rm -rf $(BUILD_DIR)
