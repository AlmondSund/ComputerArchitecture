# ADC Temperature Monitor Documentation

This folder contains the professional documentation package for the STM32L476RG
ADC/DMA internal temperature monitor.

## Files

- `adc-temperature-monitor.tex`: source for the PDF report.
- `adc-temperature-monitor.pdf`: generated report.
- `assets/screenshots/*.png`: rendered official-document screenshots embedded in the report.
- `figures/*.mmd`: editable source versions of the system diagrams.
- `source-of-truth/*.pdf`: official STMicroelectronics PDFs used as hardware evidence.
- `Makefile`: local PDF build wrapper using `pdflatex`.

## Build

```sh
make -C docs
```

The report cites the official STMicroelectronics documents used as hardware
source material:

- UM1724, *STM32 Nucleo-64 boards (MB1136)*
- MB1136-DEFAULT-C03 board schematic
- STM32L476xx datasheet

The PDF includes rendered screenshots from the staged official PDFs. The custom
system diagrams are rendered in the report, with editable source copies in
`figures/*.mmd`.
