PDF = adc-temperature-monitor.pdf
TEX = adc-temperature-monitor.tex
DIAGRAMS = figures/product-context.mmd figures/adc-dma-pipeline.mmd figures/firmware-timing.mmd
SCREENSHOTS = \
	assets/screenshots/um1724-table23-arduino-connectors-53.png \
	assets/screenshots/um1724-table23-arduino-connectors-54.png \
	assets/screenshots/mb1136-sheet3-mcu-3.png \
	assets/screenshots/stm32l476-datasheet-temperature-sensor-041.png \
	assets/screenshots/stm32l476-datasheet-adc-internal-042.png

.PHONY: all clean

all: $(PDF)

$(PDF): $(TEX) $(DIAGRAMS) $(SCREENSHOTS)
	pdflatex -interaction=nonstopmode -halt-on-error $(TEX)
	pdflatex -interaction=nonstopmode -halt-on-error $(TEX)

clean:
	rm -f *.aux *.log *.out *.toc *.lof *.lot missfont.log $(PDF)
