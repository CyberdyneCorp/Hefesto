# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Pairwise comparator ``pnm-image``: PNM images of ``plot pnm``, exact.

openspec/changes/m10-plot-route design D19 (the MISC-PLOT PNM cases). The images are compared exactly:
two files match only when their bytes are equal. No tolerance is configured for any case (Hefesto's
rasteriser reproduces Magic's arithmetic, design D9). The comparator only explains a difference:

1. Both files are parsed as binary PNM (``P6`` colour or ``P5`` grey): the magic number, width, height
   and maximum value, separated by whitespace, with ``#`` comments allowed, then one whitespace byte
   and the raster. Magic writes ``P6\\n<w> <h>\\n255\\n`` (plot/plotPNM.c).
2. A header difference is reported as such (``header differs: expected P6 100x100 max 255, got ...``),
   so a different size is named before any pixel.
3. With equal headers, the rasters are compared pixel by pixel (1 or 2 bytes per sample, 3 samples per
   ``P6`` pixel): the report gives the number of differing pixels, their bounding box and the first one
   in raster order with both values; a raster of the wrong length is reported as a length difference.
4. A file that is not binary PNM is reported with the first differing byte offset.
"""
from __future__ import annotations

from dataclasses import dataclass

NAME = "pnm-image"
CHANNELS = {b"P5": 1, b"P6": 3}
WHITESPACE = b" \t\r\n\x0b\x0c"


@dataclass(frozen=True)
class Image:
    magic: bytes
    width: int
    height: int
    maxval: int
    raster: bytes

    @property
    def header(self) -> str:
        return f"{self.magic.decode()} {self.width}x{self.height} max {self.maxval}"

    @property
    def pixel_size(self) -> int:
        return CHANNELS[self.magic] * (1 if self.maxval < 256 else 2)


def _skip_space(data: bytes, pos: int) -> int:
    while pos < len(data):
        if data[pos] in WHITESPACE:
            pos += 1
        elif data[pos:pos + 1] == b"#":
            end = data.find(b"\n", pos)
            pos = len(data) if end < 0 else end + 1
        else:
            break
    return pos


def _token(data: bytes, pos: int) -> tuple[bytes, int]:
    pos = _skip_space(data, pos)
    end = pos
    while end < len(data) and data[end] not in WHITESPACE and data[end:end + 1] != b"#":
        end += 1
    return data[pos:end], end


def parse(data: bytes) -> Image | None:
    """The binary PNM image in ``data``, or None when it is not one."""
    magic, pos = _token(data, 0)
    if magic not in CHANNELS:
        return None
    fields = []
    for _ in range(3):
        token, pos = _token(data, pos)
        if not token.isdigit():
            return None
        fields.append(int(token))
    width, height, maxval = fields
    if pos >= len(data) or data[pos] not in WHITESPACE or not 0 < maxval < 65536:
        return None
    return Image(magic, width, height, maxval, data[pos + 1:])


def canonicalize(data: bytes) -> bytes:
    """Identity: the images are compared exactly (the verdict and its report come from :func:`compare`)."""
    return data


def _first_byte(expected: bytes, actual: bytes) -> str:
    offset = next((i for i, (a, b) in enumerate(zip(expected, actual)) if a != b), min(len(expected), len(actual)))
    return f"not binary PNM; first differing byte at offset {offset} ({len(expected)} vs {len(actual)} bytes)"


def _pixel_differences(exp: Image, act: Image) -> str:
    size = exp.pixel_size
    count, first = 0, None
    left, bottom, right, top = exp.width, exp.height, -1, -1
    for index in range(exp.width * exp.height):
        a, b = exp.raster[index * size:(index + 1) * size], act.raster[index * size:(index + 1) * size]
        if a == b:
            continue
        x, y = index % exp.width, index // exp.width
        count += 1
        first = first or (x, y, a.hex(), b.hex())
        left, right, bottom, top = min(left, x), max(right, x), min(bottom, y), max(top, y)
    x, y, a, b = first
    return (f"{count} pixel(s) differ in the box x {left}..{right}, y {bottom}..{top} (rows from the top); "
            f"first at ({x}, {y}): expected {a}, got {b}")


def compare(expected: bytes, actual: bytes) -> str | None:
    """None when the images are equal, else what differs (header, raster length or pixels)."""
    if expected == actual:
        return None
    exp, act = parse(expected), parse(actual)
    if exp is None or act is None:
        return _first_byte(expected, actual)
    if exp.header != act.header:
        return f"header differs: expected {exp.header}, got {act.header}"
    want = exp.width * exp.height * exp.pixel_size
    if len(exp.raster) != want or len(act.raster) != want:
        return f"raster length differs: {exp.header} needs {want} bytes, expected has {len(exp.raster)}, " \
               f"got {len(act.raster)}"
    if exp.raster == act.raster:
        return "header bytes differ (same size and maximum value; whitespace or comments)"
    return _pixel_differences(exp, act)
