#!/usr/bin/env python3
"""Generate 128x64 title bitmap for SSD1306 (page-major, column bytes)."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "firmware" / "pcb_quest" / "title_intro.h"

LOGO_16 = {
    "電": (
        "################",
        "#..............#",
        "#..............#",
        "#######..#######",
        "#..............#",
        "#..............#",
        "#..####..####..#",
        "#..#..#..#..#..#",
        "####..####..####",
        "#..............#",
        "#..###...####..#",
        "#..............#",
        "#..####...###..#",
        "#..............#",
        "########...#####",
        "........####....",
    ),
    "子": (
        ".......########.",
        "......#.......#.",
        ".....#.......#..",
        "....#.......#...",
        "...#.......#....",
        "..#.......######",
        ".#.............#",
        "#.............#.",
        "#######......#..",
        "......#.....#...",
        "......#....#....",
        "......#...#.....",
        "......#..#......",
        "......#.#.......",
        "......##........",
        "......#.........",
    ),
    "基": (
        "...####..####...",
        "...#..#..#..#...",
        "####..####..####",
        "#..............#",
        "#..............#",
        "####........####",
        "...#..####..#...",
        "...#........#...",
        "...#..####..#...",
        "...#........#...",
        "####..####..####",
        "#.....#..#.....#",
        "#..####..####..#",
        "#.##........##.#",
        "##.#........#.##",
        "#..##########..#",
    ),
    "板": (
        "..###..#########",
        "..#.#..#.......#",
        "###.####.......#",
        "#.....##..######",
        "#.....##..#.....",
        "###.####..#.....",
        "..#.#..#.#######",
        "..#.#..#.#.....#",
        ".#...#.#.#.....#",
        ".#...#.#.#.....#",
        "#.....##..#....#",
        "#.....##..##...#",
        "#.#.#.##..#.#..#",
        "###.####.#...#.#",
        "..#.#..##.....##",
        "..###..#.......#",
    ),
}

LOGO_32X16 = {
    "クエ": (
        "....######...........###########",
        "...#......########...#.........#",
        "...#.............#..#..........#",
        "..#..............#..#..........#",
        "..#.............#..#...........#",
        ".#..............#..#####....####",
        ".#....####.....#.......#....#...",
        "#.....#..#.....#......#....#....",
        "#....#..#.....#.......#....#....",
        "######..#.....#......#....#.....",
        ".......#.....#..######....######",
        ".......#.....#..#..............#",
        "......#.....#..#...............#",
        "......#.....#..#...............#",
        ".....#.....#..#................#",
        ".....#######..##################",
    ),
    "スト": (
        ".######################.........",
        ".#.............#.#....#.........",
        ".#............#..#....#.........",
        ".#...........#...#....#.........",
        ".#..........#....#....#.........",
        ".#####.....#.....#....######....",
        "....#.....#......#....##....#...",
        "...#.....##......#....##....#...",
        "..#.....##.##....#....#.#....#..",
        ".#.....#.#...##..#....#.#....#..",
        ".#....#..#.....###....#..#....#.",
        ".#...#...#......##....#..#....#.",
        ".#..#....##.....##....#...#....#",
        ".#.#.......##...##....#...######",
        ".##..........##.##....#.........",
        ".#.............########.........",
    ),
}

SMALL_KANA = {
    "デ": (0x04, 0x05, 0x45, 0x3D, 0x05, 0x04, 0x05, 0x00),
    "ン": (0x00, 0x41, 0x42, 0x40, 0x20, 0x10, 0x0C, 0x00),
    "シ": (0x00, 0x45, 0x4A, 0x40, 0x20, 0x10, 0x0C, 0x00),
    "キ": (0x00, 0x12, 0x12, 0x1F, 0x72, 0x12, 0x10, 0x00),
    "バ": (0x40, 0x30, 0x0E, 0x00, 0x02, 0x0D, 0x71, 0x00),
}


def make_buffer() -> list[list[int]]:
    return [[0] * 128 for _ in range(8)]


def set_pixel(buf: list[list[int]], x: int, y: int, on: bool = True) -> None:
    if 0 <= x < 128 and 0 <= y < 64:
        page = y // 8
        bit = y % 8
        if on:
            buf[page][x] |= 1 << bit
        else:
            buf[page][x] &= ~(1 << bit)


def draw_logo_char(buf: list[list[int]], x: int, y: int, ch: str) -> None:
    glyph = LOGO_16[ch]
    for row, line in enumerate(glyph):
        if len(line) != 16:
            raise RuntimeError(f"{ch}: expected 16 columns, got {len(line)}")
        for col, px in enumerate(line):
            if px == "#":
                set_pixel(buf, x + col, y + row)


def draw_logo(buf: list[list[int]], x: int, y: int, key: str) -> None:
    glyph = LOGO_32X16[key]
    for row, line in enumerate(glyph):
        if len(line) != 32:
            raise RuntimeError(f"{key}: expected 32 columns, got {len(line)}")
        for col, px in enumerate(line):
            if px == "#":
                set_pixel(buf, x + col, y + row)


def draw_small_text(buf: list[list[int]], x: int, y: int, text: str) -> None:
    for i, ch in enumerate(text):
        glyph = SMALL_KANA[ch]
        for col, bits in enumerate(glyph):
            for row in range(8):
                if bits & (1 << row):
                    set_pixel(buf, x + i * 8 + col, y + row)


def flatten(buf: list[list[int]]) -> list[int]:
    out: list[int] = []
    for page in range(8):
        out.extend(buf[page])
    return out


def main() -> None:
    buf = make_buffer()

    small = "デンシキバン"
    draw_small_text(buf, (128 - len(small) * 8) // 2, 16, small)

    kanji = "電子基板"
    x = (128 - len(kanji) * 16) // 2
    for i, ch in enumerate(kanji):
        draw_logo_char(buf, x + i * 16, 24, ch)

    draw_logo(buf, 32, 40, "クエ")
    draw_logo(buf, 64, 40, "スト")

    data = flatten(buf)
    if len(data) != 1024:
        raise RuntimeError(f"expected 1024 bytes, got {len(data)}")
    if any(data[:128]) or any(data[7 * 128 : 8 * 128]):
        raise RuntimeError("title must keep page 0 and page 7 blank")

    lines = []
    for page in range(8):
        row = data[page * 128 : (page + 1) * 128]
        hexes = ", ".join(f"0x{b:02X}" for b in row)
        lines.append(f"  {hexes},")

    content = """// Auto-generated by tools/gen_title_intro.py
// 128x64 title bitmap. Replace by re-running the generator or editing bytes.
#pragma once

#include <Arduino.h>

static const uint8_t TITLE_INTRO[1024] PROGMEM = {
"""
    content += "\n".join(lines)
    content += "\n};\n"
    OUT.write_text(content, encoding="utf-8")
    print(f"Wrote {OUT} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
