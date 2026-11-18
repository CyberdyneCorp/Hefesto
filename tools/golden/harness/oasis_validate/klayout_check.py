# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""KLayout side of ``oasis-validate`` (openspec/changes/m6-lef-def-oasis design D18, task 1.7).

The Debian KLayout package has no strm2gds/strmxor/strmcmp buddy programs, so their
operations run here through KLayout's documented scripting API, in batch mode:

    HEFESTO_KLAYOUT_REQUEST=req.json klayout -b -r klayout_check.py

(``python3 klayout_check.py req.json`` with the ``klayout`` Python module works too; the
unit tests use it). ``req.json`` holds one operation; the result is written as JSON to
the request's ``result`` path. Reader warnings go to stderr, where oasis_validate.py
reads them. KLayout is a reference tool only: Hefesto never links or calls it.

Operations (paths absolute or relative to the working directory):
    read      {file, expect_strict: -1|0|1|null}    LoadLayoutOptions.oasis_expect_strict_mode
    strm2gds  {file, out}                            read, then write GDS2
    strm2oas  {file, out, strict: bool}              read, then write OASIS (strict or not)
    xor       {file, against, top: name|null}        per-layer XOR of the flattened top cells
    cmp       {file, against, flags: [name...]}      LayoutDiff.compare (strmcmp)
    version   {}                                     the KLayout version
"""
from __future__ import annotations

import json
import os
import sys

try:
    import pya  # klayout -b
except ImportError:  # the klayout Python module
    import klayout.db as pya


def load(path: str, expect_strict=None):
    options = pya.LoadLayoutOptions()
    if expect_strict is not None:
        options.oasis_expect_strict_mode = int(expect_strict)
    layout = pya.Layout()
    layout.read(path, options)
    return layout


def describe(layout) -> dict:
    return {"dbu": layout.dbu, "cells": layout.cells(), "top_cells": sorted(c.name for c in layout.top_cells()),
            "layers": sorted(str(layout.get_info(i)) for i in layout.layer_indexes())}


def op_read(request: dict) -> dict:
    return describe(load(request["file"], request.get("expect_strict")))


def op_strm2gds(request: dict) -> dict:
    layout = load(request["file"])
    options = pya.SaveLayoutOptions()
    options.format = "GDS2"
    layout.write(request["out"], options)
    return describe(layout)


def op_strm2oas(request: dict) -> dict:
    layout = load(request["file"])
    options = pya.SaveLayoutOptions()
    options.format = "OASIS"
    options.oasis_strict_mode = bool(request.get("strict", True))
    layout.write(request["out"], options)
    return describe(layout)


def top_cell(layout, name):
    return layout.cell(name) if name else layout.top_cell()


def flat_region(layout, cell, info, scale: float):
    index = layout.find_layer(info)
    region = pya.Region() if index is None else pya.Region(cell.begin_shapes_rec(index))
    return region.transformed(pya.ICplxTrans(scale)) if scale != 1 else region


def op_xor(request: dict) -> dict:
    """strmxor: the per-layer XOR of the two top cells, in the first layout's units."""
    a, b = load(request["file"]), load(request["against"])
    top = request.get("top")
    cell_a, cell_b = top_cell(a, top), top_cell(b, top)
    infos = {(i.layer, i.datatype): i for lay in (a, b) for i in (lay.get_info(x) for x in lay.layer_indexes())}
    differences = {}
    for key in sorted(infos):
        info = pya.LayerInfo(*key)
        diff = flat_region(a, cell_a, info, 1) ^ flat_region(b, cell_b, info, b.dbu / a.dbu)
        if not diff.is_empty():
            differences[f"{key[0]}/{key[1]}"] = diff.count()
    return {"layers": len(infos), "differences": differences}


def op_cmp(request: dict) -> dict:
    """strmcmp: LayoutDiff.compare with the named flags (e.g. SmartCellMapping)."""
    flags = 0
    for name in request.get("flags", []):
        flags |= getattr(pya.LayoutDiff, name)
    same = pya.LayoutDiff().compare(load(request["file"]), load(request["against"]), flags)
    return {"identical": bool(same)}


def op_version(_request: dict) -> dict:
    application = getattr(pya, "Application", None)
    if application is not None and application.instance() is not None:
        return {"version": application.instance().version()}
    return {"version": f"klayout module {getattr(pya, '__version__', 'unknown')}"}


OPERATIONS = {"read": op_read, "strm2gds": op_strm2gds, "strm2oas": op_strm2oas, "xor": op_xor,
              "cmp": op_cmp, "version": op_version}


def main(request_path: str) -> int:
    with open(request_path, encoding="utf-8") as stream:
        request = json.load(stream)
    try:
        result = {"ok": True, **OPERATIONS[request["op"]](request)}
    except Exception as error:  # noqa: BLE001 (any reader or writer failure is the verdict)
        result = {"ok": False, "error": f"{type(error).__name__}: {error}"}
    with open(request["result"], "w", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
    return 0


def request_path() -> str | None:
    if os.environ.get("HEFESTO_KLAYOUT_REQUEST"):
        return os.environ["HEFESTO_KLAYOUT_REQUEST"]
    return sys.argv[1] if __name__ == "__main__" and len(sys.argv) > 1 else None


if request_path():
    main(request_path())
