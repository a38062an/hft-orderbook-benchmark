TEX=report.tex
PDF=report.pdf
BUILD_DIR=build
PDF_OUT=$(BUILD_DIR)/$(PDF)

.PHONY: pdf clean

pdf:
	@mkdir -p $(BUILD_DIR) $(BUILD_DIR)/chapters
	@if [ -f uom_logo.gif ]; then \
		echo "Converting uom_logo.gif to uom_logo.png..."; \
		sips -s format png uom_logo.gif --out uom_logo.png > /dev/null 2>&1; \
	fi
	@if command -v latexmk >/dev/null 2>&1; then \
		latexmk -pdf -interaction=nonstopmode -outdir=$(BUILD_DIR) $(TEX); \
	else \
		pdflatex -interaction=nonstopmode -output-directory=$(BUILD_DIR) $(TEX) && \
		if [ -f $(BUILD_DIR)/$(TEX:.tex=.aux) ]; then \
			if grep -r '\\citation' $(BUILD_DIR) > /dev/null 2>&1; then bibtex $(BUILD_DIR)/$(TEX:.tex=); fi; \
		fi && \
		pdflatex -interaction=nonstopmode -output-directory=$(BUILD_DIR) $(TEX) && \
		pdflatex -interaction=nonstopmode -output-directory=$(BUILD_DIR) $(TEX); \
	fi
	@cp $(BUILD_DIR)/$(PDF) .

clean:
	rm -rf $(BUILD_DIR)
	rm -f uom_logo.png report.aux report.log report.out report.lof report.lot report.toc report.pdf
	rm -f chapters/*.aux
