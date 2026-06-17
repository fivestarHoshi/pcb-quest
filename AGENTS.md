# AGENTS.md

## Cursor Cloud specific instructions

This repo is a single **embedded firmware** product (ATtiny85 / Arduino sketch) — the
「解読せよ！電子基板クエスト」 cipher mini-game. There are **no servers, databases, or
long-running services**; "running the app" means compiling firmware with `arduino-cli`
and (on real hardware only) flashing it. Standard commands live in the `README.md`
("ビルド" / "書き込み" sections) and the `Makefile`.

### Build / generate / lint
- Build (compile firmware): `make build` (= `arduino-cli compile --fqbn attiny:avr:ATtinyX5:cpu=attiny85,clock=internal16 firmware/pcb_quest`).
- Regenerate assets: `make gen` (runs the `tools/gen_*.py` scripts). Output is deterministic and the generated headers are already committed, so a normal `make gen` produces no git diff.
- Lint: there is **no dedicated linter**. Use `arduino-cli compile --warnings all ... firmware/pcb_quest` as the lint-style check (the firmware currently compiles warning-free).
- Tests: there is **no automated test suite**. Validation = a successful compile; full end-to-end behavior can only be confirmed by flashing real hardware (see below).

### Non-obvious gotchas
- **`ssd1306xled` library must be v1.0.0 from upstream git, not the Arduino registry.** The
  firmware calls `SSD1306.ssd1306_display_off()` / `ssd1306_display_on()`, which only exist in
  upstream `tejashwikalptaru/ssd1306xled` >= v1.0.0. The Arduino Library Manager only publishes
  up to 0.0.4 (no display on/off), so `arduino-cli lib install ssd1306xled` yields a build error
  (`'class SSD1306Device' has no member named 'ssd1306_display_off'`). The startup update script
  vendors v1.0.0 from git into `~/Arduino/libraries/ssd1306xled`. Installing `--git-url` directly
  fails on a broken symlink in the repo's `simulation/` dir, so only the flat source files are copied.
- **Toolchain:** the `attiny:avr` core (damellis board package URL) needs the `arduino:avr` core
  installed too, since it provides the `avr-gcc` toolchain.
- **`make flash` / `make fuses` cannot run in the cloud VM** — they require a physical ATtiny85,
  an ISP programmer, and a serial `PORT` (e.g. `/dev/cu.usbserial-*`). The `PORT` is a USB serial
  device, not a TCP port. Build-only validation is the maximum possible here.
- **Flash budget is tight:** the sketch uses ~96% of the ATtiny85's 8 KB program space. New code
  can easily overflow flash, so check the size line printed by `make build` after changes.
