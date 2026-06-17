# AGENTS.md

## Cursor Cloud specific instructions

This repo is **embedded firmware** for TinyJoypad / ATtiny85 (Arduino C/C++ sketch in
`firmware/pcb_quest/`), with Python asset generators in `tools/` and flashing scripts in
`scripts/`. There is **no server, database, or web UI** — the "app" is firmware, and the
runnable verification in a VM is compiling it.

### Toolchain (provided by the startup update script)
- `arduino-cli` is installed at `~/.local/bin/arduino-cli` and is on `PATH` via `~/.bashrc`.
- Board cores: `arduino:avr` (AVR toolchain) and `attiny:avr` (from the damellis
  board-manager URL) are installed.
- The OLED driver library `ssd1306xled` is installed at
  `~/Arduino/libraries/ssd1306xled`, pinned to **v1.0.0** from
  `github.com/tejashwikalptaru/ssd1306xled`.

### Non-obvious gotchas
- **Library version matters.** The sketch calls `ssd1306_display_off()` /
  `ssd1306_display_on()` plus the custom `ssd1306_draw_bmp_px()` /
  `ssd1306_clear_area_px()` methods. These only exist in `ssd1306xled` **v1.0.0+**. The
  version published in the Arduino Library index tops out at `0.0.4` (no display on/off) and
  will **fail to compile** — do not `arduino-cli lib install ssd1306xled`. Use the pinned
  v1.0.0 from GitHub (the update script handles this).
- **Do not use `arduino-cli lib install --git-url` for this library.** The repo contains a
  broken symlink under `simulation/` that makes the git-url installer fail mid-copy. The
  update script downloads the release tarball and copies only the root library files instead.
- **Flash is ~96% full** (7868/8192 bytes). Code changes can easily overflow the ATtiny85's
  8 KB flash; always re-run the build to check the `Sketch uses ... bytes` line.

### Build / verify (standard commands, see `Makefile` and `README.md`)
- `make gen` — regenerate `kana_glyphs.h` / `title_intro.h` from `tools/*.py` (Python 3,
  stdlib only; output should match the checked-in headers, i.e. no git diff).
- `make build` — compile via `arduino-cli compile --fqbn attiny:avr:ATtinyX5:...`.
- `make flash` / `make fuses` — **require physical hardware** (ATtiny85 + ISP programmer +
  serial port) and **cannot run in the cloud VM**.

### Lint / test
- There is no lint config and no automated test suite in this repo; `make build` (a clean
  compile) is the primary correctness check available in the VM.
