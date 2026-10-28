#!/usr/bin/env python3
"""Draw 128x64 monochrome screens as an OLED panel would show them.

Input: PBM files (P1, as tests/test_screens.c writes them) or EmbSIM run
reports (--json), whose SSD1306 output `pixels` is an SVG path of the lit
pixels. Output: one PNG per screen, or a contact sheet of all of them.

    tools/oled_png.py build/host/tests/screen-*.pbm --sheet docs/images/screens.png
    tools/oled_png.py --embsim report.json --component OLED -o docs/images/live.png

Standard library only.
"""
import argparse
import json
import re
import struct
import sys
import zlib
from pathlib import Path

W, H = 128, 64
SCALE = 4                   # each OLED pixel is a 4x4 block, 3x3 lit
BEZEL = 14
BG = (12, 14, 18)           # the panel, off
GLASS = (22, 26, 32)        # unlit pixels, barely visible as on the real glass
LIT = (196, 230, 255)       # a white-blue SSD1306
FRAME = (40, 44, 52)
LABEL_GAP = 0


def read_pbm(path):
    tokens = re.sub(r"#.*", "", Path(path).read_text()).split()
    if tokens[0] != "P1":
        raise ValueError(f"{path}: not a plain PBM (P1)")
    w, h = int(tokens[1]), int(tokens[2])
    bits = [t == "1" for t in "".join(tokens[3:])]
    return [[bits[y * w + x] for x in range(w)] for y in range(h)]


def from_svg_path(d):
    """EmbSIM's `pixels`: an SVG path of unit squares, 'M x y h1v1h-1z'
    runs (any h/v widths), one unit per pixel."""
    img = [[False] * W for _ in range(H)]
    for m in re.finditer(r"M\s*(\d+)[ ,](\d+)\s*h\s*(\d+)\s*v\s*(\d+)", d):
        x, y, w, h = map(int, m.groups())
        for j in range(y, min(y + h, H)):
            for i in range(x, min(x + w, W)):
                img[j][i] = True
    return img


def render(img):
    """RGB rows of the panel with its bezel."""
    pw, ph = W * SCALE + 2 * BEZEL, H * SCALE + 2 * BEZEL
    rows = [[FRAME] * pw for _ in range(ph)]
    for y in range(BEZEL - 4, ph - BEZEL + 4):
        for x in range(BEZEL - 4, pw - BEZEL + 4):
            rows[y][x] = BG
    for y in range(H):
        for x in range(W):
            colour = LIT if img[y][x] else GLASS
            for j in range(SCALE - 1):
                row = rows[BEZEL + y * SCALE + j]
                for i in range(SCALE - 1):
                    row[BEZEL + x * SCALE + i] = colour
    return rows


def sheet(panels, columns=3, gap=24, bg=(255, 255, 255)):
    pw, ph = len(panels[0][0]), len(panels[0])
    nrows = (len(panels) + columns - 1) // columns
    cols = min(columns, len(panels))
    out = [[bg] * (cols * pw + (cols + 1) * gap) for _ in range(nrows * ph + (nrows + 1) * gap)]
    for n, p in enumerate(panels):
        ox = gap + (n % columns) * (pw + gap)
        oy = gap + (n // columns) * (ph + gap)
        for y, row in enumerate(p):
            out[oy + y][ox:ox + pw] = row
    return out


def write_png(path, rows):
    h, w = len(rows), len(rows[0])
    raw = b"".join(b"\x00" + bytes(c for px in row for c in px) for row in rows)

    def chunk(kind, data):
        c = kind + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(png)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pbm", nargs="*", help="PBM screens")
    ap.add_argument("--embsim", help="an `embsim run --json` report")
    ap.add_argument("--component", default="OLED", help="the SSD1306's id in the project")
    ap.add_argument("-o", "--out-dir", default=".", help="where the PNGs go (one per input)")
    ap.add_argument("--sheet", help="one contact sheet of every input instead")
    ap.add_argument("--columns", type=int, default=3)
    args = ap.parse_args()

    screens = []
    for p in args.pbm:
        screens.append((Path(p).stem, read_pbm(p)))
    if args.embsim:
        report = json.loads(Path(args.embsim).read_text())
        outputs = report["snapshot"]["outputs"][args.component]
        screens.append((args.component.lower(), from_svg_path(outputs.get("pixels", ""))))
    if not screens:
        ap.error("nothing to draw")

    panels = [(name, render(img)) for name, img in screens]
    if args.sheet:
        write_png(args.sheet, sheet([p for _, p in panels], args.columns))
        print(args.sheet)
    else:
        for name, p in panels:
            out = Path(args.out_dir) / f"{name}.png"
            write_png(out, p)
            print(out)


if __name__ == "__main__":
    sys.exit(main())
