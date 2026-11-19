# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonicalizer ``stream-elements``: a GDSII or OASIS stream as each cell's element sequence.

m6-lef-def-oasis design D19, OASIS-052: with ``oasis repetitions no``, oasis-write "GDS-consistent
content" makes every cell's elements the translations of its GDS element stream, in the same order.
This canonical form lists, per cell (cells in name order, ``cell <name>``), one line per element in
record order, the same for either format:

* a rectangle (an OASIS RECTANGLE, or a GDS BOUNDARY or BOX whose outline is an axis-parallel
  rectangle with area): ``rect L/D x1 y1 x2 y2``;
* another polygon: ``polygon L/D x0 y0 x1 y1 ...``, its vertices from the first one in record order
  (the closing GDS point dropped, consecutive duplicates merged);
* a path: ``path L/D w=<width> ext=<start>,<end> x0 y0 ...`` (extensions as values);
* a text: ``text L/T x y "string"``;
* a placement: ``ref <child> a b c d x y`` (the transform of ``stream-hierarchy``), or for an AREF
  or an OASIS placement with a repetition ``aref <child> a b c d`` followed by its sorted
  displacements; then its reference properties ``| <attribute>="<value>"``.

An OASIS shape repetition becomes ``... rep <displacements>``, which a GDS element never has, so a
repetition where the GDS stream has single elements is a difference. A stream that cannot be
decoded gives an ``error`` line.
"""
from __future__ import annotations

from goldenlib import gds, oasis

from .stream_geometry import _first, _xy, placement_matrix
from .stream_hierarchy import _escape, _number, _oasis_props

Point = tuple[int, int]


def _dedupe(points) -> list[Point]:
    out: list[Point] = []
    for p in points:
        if not out or out[-1] != p:
            out.append(p)
    return out


def _outline_line(layer: int, datatype: int, points) -> str:
    pts = _dedupe(points)
    if len(pts) > 1 and pts[0] == pts[-1]:
        pts.pop()
    xs, ys = sorted({x for x, _ in pts}), sorted({y for _, y in pts})
    if len(pts) == 4 and len(xs) == 2 and len(ys) == 2 and set(pts) == {(x, y) for x in xs for y in ys}:
        return f"rect {layer}/{datatype} {xs[0]} {ys[0]} {xs[1]} {ys[1]}"
    return f"polygon {layer}/{datatype} " + " ".join(f"{x} {y}" for x, y in pts)


def _transform_text(matrix) -> str:
    return " ".join(_number(v) for v in matrix[:4])


def _props_text(props) -> str:
    return "".join(f" | {attr}={_escape(value)}" for attr, value in props)


# --- GDS -------------------------------------------------------------------------------------


def _gds_element(element: dict) -> str | None:
    kind = element["kind"]
    layer = _first(element, "LAYER")
    if kind in ("BOUNDARY", "BOX"):
        datatype = _first(element, "BOXTYPE") if kind == "BOX" else _first(element, "DATATYPE")
        return _outline_line(layer, datatype, _xy(element))
    if kind == "PATH":
        width, pathtype = abs(_first(element, "WIDTH")), _first(element, "PATHTYPE")
        ext = ((_first(element, "BGNEXTN"), _first(element, "ENDEXTN")) if pathtype == 4
               else (width / 2, width / 2) if pathtype in (1, 2) else (0, 0))
        points = " ".join(f"{x} {y}" for x, y in _xy(element))
        return (f"path {layer}/{_first(element, 'DATATYPE')} w={width} ext={_number(ext[0])},{_number(ext[1])} "
                f"{points}")
    if kind == "TEXT":
        x, y = (_xy(element) or [(0, 0)])[0]
        return f"text {layer}/{_first(element, 'TEXTTYPE')} {x} {y} {_escape(_first(element, 'STRING', ''))}"
    if kind in ("SREF", "AREF"):
        return _gds_ref(element)
    return None


def _gds_ref(element: dict) -> str:
    points = _xy(element) or [(0, 0)]
    flip = bool(_first(element, "STRANS") & 0x8000)
    matrix = placement_matrix(*points[0], _first(element, "MAG", 1.0), _first(element, "ANGLE", 0.0), flip)
    head = f"{_first(element, 'SNAME', '?')} {_transform_text(matrix)}"
    props = _props_text(element.get("props", []))
    if element["kind"] == "SREF":
        return f"ref {head} {points[0][0]} {points[0][1]}{props}"
    cols, rows = (element.get("COLROW") or (1, 1))[:2]
    (ox, oy), (cx, cy), (rx, ry) = (points + [(0, 0)] * 3)[:3]
    col, row = ((cx - ox) / cols, (cy - oy) / cols), ((rx - ox) / rows, (ry - oy) / rows)
    origins = sorted((round(ox + i * col[0] + j * row[0]), round(oy + i * col[1] + j * row[1]))
                     for j in range(rows) for i in range(cols))
    return f"aref {head} " + " ".join(f"{x},{y}" for x, y in origins) + props


class _GdsReader:
    def __init__(self) -> None:
        self.cells: dict[str, list[str]] = {}
        self.current: list[str] | None = None
        self.element: dict | None = None

    def record(self, rec: gds.Record) -> None:
        if self.element is not None:
            self._element_record(rec)
        elif rec.name in ("BOUNDARY", "BOX", "PATH", "TEXT", "SREF", "AREF", "NODE"):
            self.element = {"kind": rec.name, "props": []}
        elif rec.name == "STRNAME":
            self.current = self.cells.setdefault(rec.values()[0], [])

    def _element_record(self, rec: gds.Record) -> None:
        element = self.element
        if rec.name == "PROPATTR":
            element["props"].append([rec.values()[0], ""])
        elif rec.name == "PROPVALUE" and element["props"]:
            element["props"][-1][1] = rec.values()[0]
        elif rec.name != "ENDEL":
            element[rec.name] = rec.values()
        else:
            line = _gds_element(element)
            if line and self.current is not None:
                self.current.append(line)
            self.element = None


def _from_gds(data: bytes) -> dict[str, list[str]]:
    reader = _GdsReader()
    for rec in gds.records(data):
        reader.record(rec)
    return reader.cells


# --- OASIS -----------------------------------------------------------------------------------


def _rep_suffix(repetition) -> str:
    if len(repetition) <= 1:
        return ""
    return " rep " + " ".join(f"{x},{y}" for x, y in repetition)


def _oasis_element(element) -> str:
    if isinstance(element, oasis.Text):
        x, y = element.position
        return f"text {element.layer}/{element.texttype} {x} {y} {_escape(str(element.string))}" + \
            _rep_suffix(element.repetition)
    if isinstance(element, oasis.Placement):
        return _oasis_placement(element)
    if element.kind == "path":
        points = " ".join(f"{x} {y}" for x, y in element.points)
        start, end = element.extensions
        line = f"path {element.layer}/{element.datatype} w={2 * element.halfwidth} ext={start},{end} {points}"
    elif element.kind == "circle":
        (cx, cy), radius = element.points[0], element.radius
        line = f"circle {element.layer}/{element.datatype} {cx} {cy} {radius}"
    else:
        line = _outline_line(element.layer, element.datatype, element.points)
    return line + _rep_suffix(element.repetition)


def _oasis_placement(p: oasis.Placement) -> str:
    matrix = placement_matrix(*p.position, p.mag if p.mag > 0 else 1.0, p.angle, p.flip)
    head = f"{p.cell} {_transform_text(matrix)}"
    props = _props_text(_oasis_props(p))
    if len(p.repetition) <= 1:
        return f"ref {head} {p.position[0]} {p.position[1]}{props}"
    origins = sorted((p.position[0] + dx, p.position[1] + dy) for dx, dy in p.repetition)
    return f"aref {head} " + " ".join(f"{x},{y}" for x, y in origins) + props


def _from_oasis(data: bytes) -> dict[str, list[str]]:
    lib = oasis.decode(data)
    if lib.error is not None:
        raise oasis.OasisError(lib.error)
    return {name: [_oasis_element(e) for e in cell.elements if not getattr(e, "kind", "") == "xgeometry"]
            for name, cell in lib.cells.items()}


def elements_text(data: bytes) -> str:
    try:
        is_oasis = oasis.unwrap(data)[:len(oasis.MAGIC)] == oasis.MAGIC
        cells = _from_oasis(data) if is_oasis else _from_gds(data)
    except (ValueError, ZeroDivisionError) as error:  # GdsError and OasisError are ValueErrors
        return f"error {error}\n"
    lines = []
    for name in sorted(cells):
        lines += [f"cell {name}", *(f"  {line}" for line in cells[name])]
    return "\n".join(lines) + "\n"


def canonicalize(data: bytes) -> bytes:
    return elements_text(data).encode("utf-8")
