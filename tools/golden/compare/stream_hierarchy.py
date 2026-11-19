# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonicalizer ``stream-hierarchy``: a GDSII or OASIS stream compared cell by cell, unflattened.

m6-lef-def-oasis design D19: ``stream-geometry`` flattens the top cells, so it cannot see cell
names, how a layout is split into cells, or the reference properties. oasis-write "GDS-consistent
content" requires the OASIS file to hold "the same structures ... and the same references with the
same properties" as Magic's GDS file. This canonical form shows exactly that, for either format on
either side (the format is told by the OASIS magic bytes):

* ``units <DB units per micrometre>``, then one block per cell in name order: ``cell <name>``;
* the cell's own geometry, per ``layer/datatype``, in the canonical form of ``stream-geometry``
  (Manhattan areas merged into maximal strips, other polygons as canonical polygon lists, OASIS
  circles as ``circle`` lines, paths as their outlines);
* its texts: ``text L/T x y "string"`` (presentation, orientation and size are not compared);
* its references, one line per placed instance: an AREF and an OASIS PLACEMENT repetition are
  expanded into their elements, so ``AREF 3x2`` and ``PLACEMENT`` with repetition type 1 give the
  same six lines. ``ref <child> <a> <b> <c> <d> <x> <y>`` is the instance transform (x' = a x + b y
  + x), followed by its properties ``| <attribute>=<value>`` in record order: GDS PROPATTR/PROPVALUE
  pairs, OASIS ``S_GDS_PROPERTY`` values (attribute, string). Lines are sorted.

A stream that cannot be decoded gives an ``error`` line.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field

from goldenlib import gds, oasis

from .stream_geometry import (_GDS_ELEMENTS, _Cell, _escape, _first, _layer_lines, _oasis_shape, _xy,
                              placement_matrix)

S_GDS_PROPERTY = "S_GDS_PROPERTY"


@dataclass
class _HCell:
    geometry: _Cell = field(default_factory=_Cell)
    refs: list[str] = field(default_factory=list)


def _number(value: float) -> str:
    rounded = round(value, 6)
    return str(int(rounded)) if float(rounded).is_integer() else f"{rounded:g}"


def ref_line(child: str, matrix, props: list[tuple[int, str]]) -> str:
    a, b, c, d, x, y = matrix
    shown = " ".join(_number(v) for v in (a, b, c, d, x, y))
    tail = "".join(f" | {attr}={_escape(value)}" for attr, value in props)
    return f"ref {child} {shown}{tail}"


# --- GDS -------------------------------------------------------------------------------------


def _gds_props(element: dict) -> list[tuple[int, str]]:
    return list(element.get("props", []))


def _gds_ref_lines(element: dict) -> list[str]:
    points = _xy(element)
    child = _first(element, "SNAME", "?")
    flip = bool(_first(element, "STRANS") & 0x8000)
    mag, angle = _first(element, "MAG", 1.0), _first(element, "ANGLE", 0.0)
    if element["kind"] == "SREF":
        origin = points[0] if points else (0, 0)
        return [ref_line(child, placement_matrix(*origin, mag, angle, flip), _gds_props(element))]
    cols, rows = (element.get("COLROW") or (1, 1))[:2]
    if len(points) < 3 or cols < 1 or rows < 1:
        return []
    (ox, oy), (cx, cy), (rx, ry) = points[:3]
    col, row = ((cx - ox) / cols, (cy - oy) / cols), ((rx - ox) / rows, (ry - oy) / rows)
    return [ref_line(child, placement_matrix(ox + i * col[0] + j * row[0], oy + i * col[1] + j * row[1],
                                             mag, angle, flip), _gds_props(element))
            for j in range(rows) for i in range(cols)]


class _GdsReader:
    def __init__(self) -> None:
        self.cells: dict[str, _HCell] = {}
        self.units = 0.0
        self.cell: _HCell | None = None
        self.element: dict | None = None

    def record(self, rec: gds.Record) -> None:
        if self.element is not None:
            self._element_record(rec)
        elif rec.name in _GDS_ELEMENTS:
            self.element = {"kind": rec.name, "props": []}
        elif rec.name == "NODE":
            self.element = {"kind": "NODE", "props": []}
        elif rec.name == "STRNAME":
            self.cell = self.cells.setdefault(rec.values()[0], _HCell())
        elif rec.name == "UNITS":
            self.units = 1e-6 / rec.values()[1]

    def _element_record(self, rec: gds.Record) -> None:
        element = self.element
        if rec.name == "PROPATTR":
            element["props"].append([rec.values()[0], ""])
        elif rec.name == "PROPVALUE" and element["props"]:
            element["props"][-1][1] = rec.values()[0]
        elif rec.name != "ENDEL":
            element[rec.name] = rec.values()
        else:
            self._finish(element)

    def _finish(self, element: dict) -> None:
        self.element = None
        element["props"] = [tuple(p) for p in element["props"]]
        if self.cell is None:
            return
        if element["kind"] in ("SREF", "AREF"):
            self.cell.refs += _gds_ref_lines(element)
        elif element["kind"] in _GDS_ELEMENTS:
            _GDS_ELEMENTS[element["kind"]](self.cell.geometry, element)


def _from_gds(data: bytes) -> tuple[dict[str, _HCell], float]:
    reader = _GdsReader()
    for rec in gds.records(data):
        reader.record(rec)
    return reader.cells, reader.units


# --- OASIS -----------------------------------------------------------------------------------


def _string(value) -> str | None:
    if not isinstance(value, tuple) or value[0] not in ("a", "b", "n", "str"):
        return None
    return value[1].decode("latin-1") if isinstance(value[1], bytes) else value[1]


def _oasis_props(placement: oasis.Placement) -> list[tuple[int, str]]:
    out = []
    for prop in placement.properties:
        values = prop.values or ()
        if prop.name != S_GDS_PROPERTY or len(values) != 2:
            continue
        attribute, text = values[0], _string(values[1])
        if isinstance(attribute, tuple) and attribute[0] in ("u", "s") and text is not None:
            out.append((attribute[1], text))
    return out


def _oasis_cell(source: oasis.Cell) -> _HCell:
    cell = _HCell()
    for shape in source.shapes:
        _oasis_shape(cell.geometry, shape)
    for text in source.texts:
        for dx, dy in text.repetition:
            position = (text.position[0] + dx, text.position[1] + dy)
            cell.geometry.texts.append((text.layer, text.texttype, position, text.string))
    for p in source.placements:
        mag = p.mag if math.isfinite(p.mag) and p.mag > 0 else 1.0
        props = _oasis_props(p)
        for dx, dy in p.repetition:
            matrix = placement_matrix(p.position[0] + dx, p.position[1] + dy, mag, p.angle, p.flip)
            cell.refs.append(ref_line(str(p.cell), matrix, props))
    return cell


def _from_oasis(data: bytes) -> tuple[dict[str, _HCell], float]:
    lib = oasis.decode(data)
    if lib.error is not None:
        raise oasis.OasisError(lib.error)
    return {name: _oasis_cell(c) for name, c in lib.cells.items()}, lib.unit


# --- canonical form --------------------------------------------------------------------------


def _cell_lines(cell: _HCell) -> list[str]:
    geometry = cell.geometry
    polygons: dict[tuple[int, int], list[tuple]] = {}
    for layer, datatype, points in geometry.polygons:
        polygons.setdefault((layer, datatype), []).append(points)
    circles: dict[tuple[int, int], list[str]] = {}
    for layer, datatype, (cx, cy), radius in geometry.circles:
        circles.setdefault((layer, datatype), []).append(f"circle {cx} {cy} {radius}")
    lines = []
    for key in sorted(set(polygons) | set(circles)):
        body = _layer_lines(polygons.get(key, []), circles.get(key, []))
        if body:
            lines += [f"  layer {key[0]}/{key[1]}", *(f"    {line}" for line in body)]
    lines += sorted(f"  text {layer}/{texttype} {x} {y} {_escape(string)}"
                    for layer, texttype, (x, y), string in geometry.texts)
    return lines + sorted(f"  {line}" for line in cell.refs)


def hierarchy_text(data: bytes) -> str:
    """The canonical text of a GDSII or OASIS stream (see the module rules)."""
    try:
        is_oasis = oasis.unwrap(data)[:len(oasis.MAGIC)] == oasis.MAGIC
        cells, units = _from_oasis(data) if is_oasis else _from_gds(data)
    except (ValueError, ZeroDivisionError) as error:  # GdsError and OasisError are ValueErrors
        return f"error {error}\n"
    lines = [f"units {units:.6g}"]
    for name in sorted(cells):
        lines += [f"cell {name}", *_cell_lines(cells[name])]
    return "\n".join(lines) + "\n"


def canonicalize(data: bytes) -> bytes:
    return hierarchy_text(data).encode("utf-8")
