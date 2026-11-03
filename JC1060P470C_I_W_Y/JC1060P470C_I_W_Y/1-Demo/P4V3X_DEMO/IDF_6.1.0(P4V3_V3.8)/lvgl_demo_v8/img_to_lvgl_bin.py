#!/usr/bin/env python3
"""Convert a JPEG/PNG image to an LVGL-native RGB565 raw binary (for EMBED_FILES).

Usage:
    python3 img_to_lvgl_bin.py <input_image> <output.bin> [--w 1024] [--h 600]

Output pixels are 16-bit RGB565 stored little-endian, matching
lv_color16_t / LV_COLOR_DEPTH 16 on ESP32 targets.
"""
import argparse
import struct

from PIL import Image


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--w", type=int, default=1024)
    ap.add_argument("--h", type=int, default=600)
    args = ap.parse_args()

    im = Image.open(args.input).convert("RGB")
    if im.size != (args.w, args.h):
        im = im.resize((args.w, args.h), Image.LANCZOS)
    if im.size != (args.w, args.h):
        raise SystemExit(f"unexpected size {im.size}")

    px = im.tobytes()  # RGB triplets
    n = args.w * args.h
    with open(args.output, "wb") as f:
        buf = bytearray(n * 2)
        p = 0
        for i in range(n):
            r, g, b = px[p], px[p + 1], px[p + 2]
            p += 3
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            struct.pack_into("<H", buf, i * 2, v)
        f.write(buf)
    print(f"ok: {im.size[0]}x{im.size[1]} RGB565 -> {args.output} ({len(buf)} bytes)")


if __name__ == "__main__":
    main()
