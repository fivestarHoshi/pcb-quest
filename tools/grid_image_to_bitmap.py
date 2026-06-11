#!/usr/bin/env python3
"""Convert a black/white grid image into #/. bitmap text.

The script samples the middle of each cell, avoiding grid lines at the edges.
It uses macOS `sips` to convert common image formats to BMP, then parses BMP
with the Python standard library so no extra packages are required.
"""

from __future__ import annotations

import argparse
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


def read_bmp(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError("converted file is not BMP")

    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    if dib_size < 40:
        raise ValueError("unsupported BMP header")

    width = struct.unpack_from("<i", data, 18)[0]
    height_raw = struct.unpack_from("<i", data, 22)[0]
    planes = struct.unpack_from("<H", data, 26)[0]
    bpp = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]

    if planes != 1 or bpp not in (24, 32) or compression not in (0, 3):
        raise ValueError(f"unsupported BMP format: planes={planes}, bpp={bpp}, compression={compression}")
    if compression == 3 and bpp != 32:
        raise ValueError(f"unsupported BMP bitfields format: bpp={bpp}")

    height = abs(height_raw)
    top_down = height_raw < 0
    bytes_per_px = bpp // 8
    stride = ((width * bpp + 31) // 32) * 4

    masks = None
    if compression == 3:
        masks = [struct.unpack_from("<I", data, 54 + i * 4)[0] for i in range(3)]

    def channel(value: int, mask: int) -> int:
        if mask == 0:
            return 0
        shift = 0
        while ((mask >> shift) & 1) == 0:
            shift += 1
        raw = (value & mask) >> shift
        max_raw = mask >> shift
        return (raw * 255 + max_raw // 2) // max_raw

    pixels: list[tuple[int, int, int]] = [(0, 0, 0)] * (width * height)
    for y in range(height):
      src_y = y if top_down else (height - 1 - y)
      row = pixel_offset + src_y * stride
      for x in range(width):
          i = row + x * bytes_per_px
          if masks:
              value = struct.unpack_from("<I", data, i)[0]
              r, g, b = (channel(value, mask) for mask in masks)
          else:
              b, g, r = data[i], data[i + 1], data[i + 2]
          pixels[y * width + x] = (r, g, b)

    return width, height, pixels


def load_image(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    if path.suffix.lower() == ".bmp":
        try:
            return read_bmp(path)
        except ValueError:
            pass

    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "converted.bmp"
        subprocess.run(
            ["sips", "-s", "format", "bmp", str(path), "--out", str(out)],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
        return read_bmp(out)


def luminance(rgb: tuple[int, int, int]) -> float:
    r, g, b = rgb
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def cell_sample_rect(
    width: int,
    height: int,
    x0: float,
    y0: float,
    cell_w: float,
    cell_h: float,
    inset: float,
) -> tuple[int, int, int, int]:
    sx0 = int(round(x0 + cell_w * inset))
    sx1 = int(round(x0 + cell_w * (1.0 - inset)))
    sy0 = int(round(y0 + cell_h * inset))
    sy1 = int(round(y0 + cell_h * (1.0 - inset)))

    sx0 = max(0, min(width - 1, sx0))
    sx1 = max(sx0 + 1, min(width, sx1))
    sy0 = max(0, min(height - 1, sy0))
    sy1 = max(sy0 + 1, min(height, sy1))
    return sx0, sx1, sy0, sy1


def cell_luma(
    pixels: list[tuple[int, int, int]],
    width: int,
    height: int,
    x0: float,
    y0: float,
    cell_w: float,
    cell_h: float,
    inset: float,
) -> float:
    sx0, sx1, sy0, sy1 = cell_sample_rect(width, height, x0, y0, cell_w, cell_h, inset)

    total = 0.0
    count = 0
    for y in range(sy0, sy1):
        row = y * width
        for x in range(sx0, sx1):
            total += luminance(pixels[row + x])
            count += 1
    return total / count


def cell_black_ratio(
    pixels: list[tuple[int, int, int]],
    width: int,
    height: int,
    x0: float,
    y0: float,
    cell_w: float,
    cell_h: float,
    inset: float,
    threshold: float,
) -> float:
    sx0, sx1, sy0, sy1 = cell_sample_rect(width, height, x0, y0, cell_w, cell_h, inset)

    black = 0
    count = 0
    for y in range(sy0, sy1):
        row = y * width
        for x in range(sx0, sx1):
            if luminance(pixels[row + x]) < threshold:
                black += 1
            count += 1
    return black / count


def write_preview_ppm(path: Path, bits: list[list[bool]], scale: int) -> None:
    rows = len(bits)
    cols = len(bits[0])
    grid = 1
    w = cols * scale + (cols + 1) * grid
    h = rows * scale + (rows + 1) * grid
    white = (255, 255, 255)
    black = (0, 0, 0)
    gray = (120, 120, 120)
    canvas = [white] * (w * h)

    for y in range(h):
        for x in range(w):
            if x % (scale + grid) == 0 or y % (scale + grid) == 0:
                canvas[y * w + x] = gray

    for row in range(rows):
        for col in range(cols):
            color = black if bits[row][col] else white
            px0 = grid + col * (scale + grid)
            py0 = grid + row * (scale + grid)
            for y in range(py0, py0 + scale):
                for x in range(px0, px0 + scale):
                    canvas[y * w + x] = color

    with path.open("w", encoding="ascii") as f:
        f.write(f"P3\n{w} {h}\n255\n")
        for i, (r, g, b) in enumerate(canvas):
            f.write(f"{r} {g} {b} ")
            if (i + 1) % w == 0:
                f.write("\n")


def write_preview(path: Path, bits: list[list[bool]], scale: int) -> None:
    if path.suffix.lower() in (".ppm", ".pnm"):
        write_preview_ppm(path, bits, scale)
        return

    with tempfile.TemporaryDirectory() as td:
        ppm = Path(td) / "preview.ppm"
        write_preview_ppm(ppm, bits, scale)
        fmt = path.suffix.lower().lstrip(".") or "png"
        subprocess.run(
            ["sips", "-s", "format", fmt, str(ppm), "--out", str(path)],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("image", type=Path)
    p.add_argument("--cols", type=int, default=16)
    p.add_argument("--rows", type=int, default=16)
    p.add_argument("--crop", type=int, nargs=4, metavar=("X", "Y", "W", "H"))
    p.add_argument("--threshold", type=float, help="0-255 threshold. Auto if omitted.")
    p.add_argument("--inset", type=float, default=0.30, help="cell-edge inset ratio for sampling")
    p.add_argument("--mode", choices=("average", "coverage"), default="average")
    p.add_argument("--coverage", type=float, default=0.08, help="black pixel ratio needed in coverage mode")
    p.add_argument("--black", default="#")
    p.add_argument("--white", default=".")
    p.add_argument("--out", type=Path)
    p.add_argument("--preview", type=Path)
    p.add_argument("--preview-scale", type=int, default=12)
    return p.parse_args()


def main() -> int:
    args = parse_args()
    width, height, pixels = load_image(args.image)

    if args.crop:
        crop_x, crop_y, crop_w, crop_h = args.crop
    else:
        crop_x, crop_y, crop_w, crop_h = 0, 0, width, height

    cell_w = crop_w / args.cols
    cell_h = crop_h / args.rows
    lumas: list[list[float]] = []
    flat: list[float] = []
    for row in range(args.rows):
        row_lumas = []
        for col in range(args.cols):
            lum = cell_luma(
                pixels,
                width,
                height,
                crop_x + col * cell_w,
                crop_y + row * cell_h,
                cell_w,
                cell_h,
                args.inset,
            )
            row_lumas.append(lum)
            flat.append(lum)
        lumas.append(row_lumas)

    threshold = args.threshold
    if threshold is None:
        threshold = (min(flat) + max(flat)) / 2.0

    if args.mode == "coverage":
        bits = []
        for row in range(args.rows):
            bit_row = []
            for col in range(args.cols):
                ratio = cell_black_ratio(
                    pixels,
                    width,
                    height,
                    crop_x + col * cell_w,
                    crop_y + row * cell_h,
                    cell_w,
                    cell_h,
                    args.inset,
                    threshold,
                )
                bit_row.append(ratio >= args.coverage)
            bits.append(bit_row)
    else:
        bits = [[lum < threshold for lum in row] for row in lumas]
    lines = [
        "".join(args.black if bit else args.white for bit in row)
        for row in bits
    ]
    text = "\n".join(lines) + "\n"

    if args.out:
        args.out.write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)

    print(
        f"image={width}x{height} grid={args.cols}x{args.rows} threshold={threshold:.1f}",
        file=sys.stderr,
    )

    if args.preview:
        write_preview(args.preview, bits, args.preview_scale)
        print(f"preview={args.preview}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
