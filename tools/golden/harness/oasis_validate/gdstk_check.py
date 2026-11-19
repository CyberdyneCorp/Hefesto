# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""gdstk side of ``oasis-validate`` (openspec/changes/m6-lef-def-oasis design D18, task 1.7).

Run with the gdstk environment of the capture image:

    $HEFESTO_GDSTK_PYTHON gdstk_check.py req.json

``req.json`` holds one operation; the result is written as JSON to the request's
``result`` path. gdstk is a reference tool only: Hefesto never links or calls it.

Operations:
    read        {file}        gdstk.read_oas: cells, top cells, polygons, labels
    validate    {file}        gdstk.oas_validate: (valid, signature); valid is null without one
    make-smoke  {dir}         the smoke library: smoke.gds, smoke_gdstk_crc32.oas,
                              smoke_gdstk_checksum32.oas and a copy of the CRC32 file with one
                              byte changed (smoke_gdstk_crc32_corrupt.oas)
    make-validation-set {dir} OASIS-012's inputs: <set>.gds and <set>.oas for crc32, checksum32,
                              crc32_bad and checksum32_bad (one END padding byte changed)
    version     {}            the gdstk version
"""
from __future__ import annotations

import json
import math
import sys
from importlib import metadata
from pathlib import Path

import gdstk


def op_read(request: dict) -> dict:
    library = gdstk.read_oas(request["file"])
    cells = [c for c in library.cells if isinstance(c, gdstk.Cell)]
    return {"unit": library.unit, "precision": library.precision, "cells": len(library.cells),
            "top_cells": sorted(c.name for c in library.top_level()),
            "polygons": sum(len(c.polygons) for c in cells), "labels": sum(len(c.labels) for c in cells)}


def op_validate(request: dict) -> dict:
    valid, signature = gdstk.oas_validate(request["file"])
    return {"valid": valid, "signature": signature}


def smoke_library() -> gdstk.Library:
    """Two cells: rectangles, an L-shaped polygon, a path, a label, a rotated reference and
    a 3 x 2 array (layers 1-3, text type 5), in micrometres with 1 nm precision."""
    library = gdstk.Library("SMOKE")
    child = library.new_cell("CHILD")
    child.add(gdstk.rectangle((0, 0), (2, 1), layer=1))
    child.add(gdstk.Label("A", (0.5, 0.5), layer=1, texttype=5))
    top = library.new_cell("TOP")
    top.add(gdstk.Reference(child, (5, 0), rotation=math.pi / 2))
    top.add(gdstk.Reference(child, (10, 0), columns=3, rows=2, spacing=(3, 2)))
    top.add(gdstk.FlexPath([(0, 5), (4, 5), (4, 8)], 0.2, layer=3))
    top.add(gdstk.Polygon([(0, 10), (3, 10), (3, 11), (1, 11), (1, 12), (0, 12)], layer=2))
    top.add(gdstk.rectangle((20, 20), (21, 23), layer=2))
    return library


def op_make_smoke(request: dict) -> dict:
    out = Path(request["dir"])
    out.mkdir(parents=True, exist_ok=True)
    library = smoke_library()
    library.write_gds(out / "smoke.gds")
    for validation in ("crc32", "checksum32"):
        library.write_oas(out / f"smoke_gdstk_{validation}.oas", validation=validation)
    data = bytearray((out / "smoke_gdstk_crc32.oas").read_bytes())
    data[len(data) // 2] ^= 0x01
    (out / "smoke_gdstk_crc32_corrupt.oas").write_bytes(bytes(data))
    return {"files": sorted(p.name for p in out.iterdir())}


# OASIS-012 (m6-lef-def-oasis task 15.6): one small library per signature case, each with its own
# cell names so that the case's reads do not collide; the *_bad copies get their last END padding
# byte changed, so only the signature fails (padding content is ignored on read).
VALIDATION_SETS = (("crc32", "crc32"), ("checksum32", "checksum32"), ("crc32_bad", "crc32"),
                   ("checksum32_bad", "checksum32"))
PADDING_BYTE = -6  # the END record ends with the padding, the scheme byte and 4 signature bytes


def validation_library(prefix: str) -> gdstk.Library:
    """A leaf (rectangle, label) placed once and as a 2 x 2 array, an L polygon and a rectangle,
    on 0.1 um coordinates (1 nm precision)."""
    library = gdstk.Library(prefix.upper())
    leaf = library.new_cell(f"{prefix}_leaf")
    leaf.add(gdstk.rectangle((0, 0), (1, 0.5), layer=49))
    leaf.add(gdstk.Label("A", (0.1, 0.1), layer=49, texttype=0))
    top = library.new_cell(f"{prefix}_top")
    top.add(gdstk.Reference(leaf, (2, 0)))
    top.add(gdstk.Reference(leaf, (5, 0), columns=2, rows=2, spacing=(2, 1)))
    top.add(gdstk.Polygon([(0, 2), (1, 2), (1, 3), (0.5, 3), (0.5, 2.5), (0, 2.5)], layer=51))
    top.add(gdstk.rectangle((0, 0), (1, 1), layer=51))
    return library


def op_make_validation_set(request: dict) -> dict:
    out = Path(request["dir"])
    out.mkdir(parents=True, exist_ok=True)
    for prefix, scheme in VALIDATION_SETS:
        library = validation_library(prefix)
        library.write_gds(out / f"{prefix}.gds")
        library.write_oas(out / f"{prefix}.oas", validation=scheme)
        if prefix.endswith("_bad"):
            data = bytearray((out / f"{prefix}.oas").read_bytes())
            if data[PADDING_BYTE] != 0:
                raise ValueError(f"{prefix}.oas: byte {PADDING_BYTE} is not END padding")
            data[PADDING_BYTE] = 1
            (out / f"{prefix}.oas").write_bytes(bytes(data))
    return {"files": sorted(p.name for p in out.iterdir())}


def op_version(_request: dict) -> dict:
    return {"version": metadata.version("gdstk")}


OPERATIONS = {"read": op_read, "validate": op_validate, "make-smoke": op_make_smoke,
              "make-validation-set": op_make_validation_set, "version": op_version}


def main(request_path: str) -> int:
    request = json.loads(Path(request_path).read_text(encoding="utf-8"))
    try:
        result = {"ok": True, **OPERATIONS[request["op"]](request)}
    except Exception as error:  # noqa: BLE001 (any reader failure is the verdict)
        result = {"ok": False, "error": f"{type(error).__name__}: {error}"}
    Path(request["result"]).write_text(json.dumps(result, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
