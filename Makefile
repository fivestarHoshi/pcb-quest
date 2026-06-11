FQBN := attiny:avr:ATtinyX5:cpu=attiny85,clock=internal16
SKETCH := firmware/pcb_quest
PROGRAMMER ?= usbasp
PORT ?=

.PHONY: gen build flash fuses

gen:
	python3 tools/gen_kana_glyphs.py
	python3 tools/gen_title_intro.py

build:
	arduino-cli compile --fqbn $(FQBN) $(SKETCH)

flash:
	bash scripts/flash_isp.sh $(PROGRAMMER) $(PORT)

fuses:
	bash scripts/burn_fuses_16mhz_isp.sh $(PROGRAMMER) $(PORT)
