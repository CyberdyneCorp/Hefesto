# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonicalizer ``stream-geometry``: a GDSII or OASIS stream compared as flat per-layer geometry.

m6-lef-def-oasis design D19 (references of the OASIS cases): Hefesto's ``oasis write`` must give
the same per-layer merged geometry as Magic's ``gds write`` of the same layout, whatever records,
order, fracturing, hierarchy encoding (SREF/AREF or PLACEMENT repetitions) and file format each
side used. Either side may be GDSII (decoded by goldenlib/gds.py) or OASIS (goldenlib/oasis.py,
gzip-wrapped or not); the format is told by the OASIS magic bytes.

Rules:

* Every top cell (a cell no other cell places) is flattened, in name order, into one layout:
  placements, SREFs and AREFs and every repetition are expanded; transforms are applied as
  flip, magnify, rotate, translate. Coordinates are database units, rounded half up to integers
  after the transform; the database unit is the ``units`` line (DB units per micrometre, 6
  significant digits), so two files that agree on coordinates but not on units differ.
* Geometry is grouped by ``layer/datatype`` (GDS: LAYER and DATATYPE, BOXTYPE for BOX; OASIS:
  layer and datatype). Rectangles, polygons, trapezoids, CTRAPEZOIDs and boxes are polygons;
  a PATH becomes one polygon per centerline segment, with the start and end extensions at the
  ends (GDS PATHTYPE 0 flush, 1 and 2 half-width, 4 BGNEXTN/ENDEXTN; OASIS its extension
  values) and a half-width extension at each joint of an axis-parallel path, so a Manhattan
  path covers exactly its mitred outline. Joints of non-Manhattan paths get no extension.
* Manhattan polygons (every edge axis-parallel) are cut into rectangles under the non-zero
  winding rule, and each layer's rectangles are merged into the canonical maximal-strip form
  of ``compare/polygon_set.py``: ``rect x1 y1 x2 y2`` lines. Their order, fracturing and
  overlaps are therefore not compared.
* Other polygons are compared as a canonical polygon list: consecutive duplicate and collinear
  vertices removed, counterclockwise, starting at the smallest (x, y) vertex, sorted:
  ``polygon x1 y1 x2 y2 ...``. Polygons with no area are dropped. They are not merged with each
  other or with rectangles; the cases with arbitrary angles also run KLayout ``strmxor`` in
  ``oasis-validate`` (design D18).
* OASIS circles stay ``circle cx cy r`` lines (GDS has no circle; ``oasis-read`` owns the
  polygonal approximation).
* Texts are ``text L/T x y "string"`` lines after the geometry, sorted; their presentation,
  orientation and size are not compared.
* A placed cell that the file does not define gives a ``missing cell <name>`` line. A stream
  that cannot be decoded gives an ``error`` line in place of the geometry.
"""
from __future__ import annotations

import math
from collections import defaultdict
from dataclasses import dataclass, field

from goldenlib import gds, oasis

from .polygon_set import canonical_rects

Point = tuple[int, int]
Matrix = tuple[float, float, float, float, float, float]  # x' = a x + b y + tx, y' = c x + d y + ty
IDENTITY: Matrix = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)
MAX_DEPTH = 256


@dataclass
class _Cell:
    polygons: list[tuple[int, int, tuple]] = field(default_factory=list)
    circles: list[tuple[int, int, Point, int]] = field(default_factory=list)
    texts: list[tuple[int, int, Point, str]] = field(default_factory=list)
    refs: list[tuple[str, Matrix]] = field(default_factory=list)


@dataclass
class _Model:
    cells: dict[str, _Cell] = field(default_factory=dict)
    dbu_per_micron: float = 0.0


# --- transforms ------------------------------------------------------------------------------


def _cos_sin(angle: float) -> tuple[float, float]:
    quarter = angle / 90.0
    if quarter.is_integer():
        return ((1.0, 0.0), (0.0, 1.0), (-1.0, 0.0), (0.0, -1.0))[int(quarter) % 4]
    radians = math.radians(angle)
    return math.cos(radians), math.sin(radians)


def placement_matrix(x: float, y: float, mag: float = 1.0, angle: float = 0.0, flip: bool = False) -> Matrix:
    """translate(x, y) . magnify(mag) . rotate(angle, counterclockwise) . flip about the x axis."""
    cos, sin = _cos_sin(angle)
    sign = -1.0 if flip else 1.0
    return (mag * cos, -mag * sin * sign, mag * sin, mag * cos * sign, float(x), float(y))


def compose(outer: Matrix, inner: Matrix) -> Matrix:
    a, b, c, d, tx, ty = outer
    e, f, g, h, ux, uy = inner
    return (a * e + b * g, a * f + b * h, c * e + d * g, c * f + d * h, a * ux + b * uy + tx, c * ux + d * uy + ty)


def _round(value: float) -> int:
    return math.floor(value + 0.5)


def apply(m: Matrix, p: tuple[float, float]) -> Point:
    return _round(m[0] * p[0] + m[1] * p[1] + m[4]), _round(m[2] * p[0] + m[3] * p[1] + m[5])


def _scale(m: Matrix) -> float:
    return math.hypot(m[0], m[2])


# --- paths -----------------------------------------------------------------------------------


def _is_manhattan(points) -> bool:
    return all(a[0] == b[0] or a[1] == b[1] for a, b in zip(points, points[1:]))


def path_polygons(centerline, halfwidth: float, start_ext: float, end_ext: float) -> list[tuple]:
    """One quadrilateral per segment of ``centerline`` (see the module rules)."""
    line = [p for i, p in enumerate(centerline) if i == 0 or p != centerline[i - 1]]
    joint = halfwidth if _is_manhattan(line) else 0.0
    out = []
    last = len(line) - 2
    for i, (a, b) in enumerate(zip(line, line[1:])):
        length = math.hypot(b[0] - a[0], b[1] - a[1])
        ux, uy = (b[0] - a[0]) / length, (b[1] - a[1]) / length
        back = start_ext if i == 0 else joint
        ahead = end_ext if i == last else joint
        p0 = (a[0] - ux * back, a[1] - uy * back)
        p1 = (b[0] + ux * ahead, b[1] + uy * ahead)
        nx, ny = -uy * halfwidth, ux * halfwidth
        out.append(((p0[0] - nx, p0[1] - ny), (p1[0] - nx, p1[1] - ny), (p1[0] + nx, p1[1] + ny),
                    (p0[0] + nx, p0[1] + ny)))
    return out


# --- GDSII -----------------------------------------------------------------------------------


def _xy(element: dict) -> list[Point]:
    values = element.get("XY", ())
    return list(zip(values[0::2], values[1::2]))


def _first(element: dict, key: str, default=0):
    values = element.get(key)
    return values[0] if values else default


def _gds_boundary(cell: _Cell, element: dict) -> None:
    points = _xy(element)
    if len(points) > 1 and points[0] == points[-1]:
        points.pop()
    datatype = _first(element, "BOXTYPE") if element["kind"] == "BOX" else _first(element, "DATATYPE")
    cell.polygons.append((_first(element, "LAYER"), datatype, tuple(points)))


def _gds_path(cell: _Cell, element: dict) -> None:
    halfwidth = abs(_first(element, "WIDTH")) / 2
    pathtype = _first(element, "PATHTYPE")
    if pathtype == 4:
        ext = (_first(element, "BGNEXTN"), _first(element, "ENDEXTN"))
    else:
        ext = (halfwidth, halfwidth) if pathtype in (1, 2) else (0, 0)
    layer, datatype = _first(element, "LAYER"), _first(element, "DATATYPE")
    for quad in path_polygons(_xy(element), halfwidth, *ext):
        cell.polygons.append((layer, datatype, quad))


def _gds_transform(element: dict, origin: Point) -> Matrix:
    flip = bool(_first(element, "STRANS") & 0x8000)
    return placement_matrix(origin[0], origin[1], _first(element, "MAG", 1.0), _first(element, "ANGLE", 0.0), flip)


def _gds_sref(cell: _Cell, element: dict) -> None:
    points = _xy(element)
    cell.refs.append((_first(element, "SNAME", "?"), _gds_transform(element, points[0] if points else (0, 0))))


def _gds_aref(cell: _Cell, element: dict) -> None:
    points = _xy(element)
    cols, rows = (element.get("COLROW") or (1, 1))[:2]
    if len(points) < 3 or cols < 1 or rows < 1:
        return
    (ox, oy), (cx, cy), (rx, ry) = points[:3]
    col = ((cx - ox) / cols, (cy - oy) / cols)
    row = ((rx - ox) / rows, (ry - oy) / rows)
    for j in range(rows):
        for i in range(cols):
            origin = (ox + i * col[0] + j * row[0], oy + i * col[1] + j * row[1])
            cell.refs.append((_first(element, "SNAME", "?"), _gds_transform(element, origin)))


def _gds_text(cell: _Cell, element: dict) -> None:
    points = _xy(element) or [(0, 0)]
    layer, texttype = _first(element, "LAYER"), _first(element, "TEXTTYPE")
    cell.texts.append((layer, texttype, points[0], _first(element, "STRING", "")))


_GDS_ELEMENTS = {"BOUNDARY": _gds_boundary, "BOX": _gds_boundary, "PATH": _gds_path, "SREF": _gds_sref,
                 "AREF": _gds_aref, "TEXT": _gds_text}


class _GdsReader:
    def __init__(self) -> None:
        self.model = _Model()
        self.cell: _Cell | None = None
        self.element: dict | None = None

    def record(self, rec: gds.Record) -> None:
        name = rec.name
        if self.element is not None:
            self._element_record(name, rec.values())
        elif name in _GDS_ELEMENTS or name == "NODE":
            self.element = {"kind": name}
        elif name == "STRNAME":
            self.cell = self.model.cells.setdefault(rec.values()[0], _Cell())
        elif name == "UNITS":
            self.model.dbu_per_micron = 1e-6 / rec.values()[1]

    def _element_record(self, name: str, values: tuple) -> None:
        if name != "ENDEL":
            self.element[name] = values
            return
        handler = _GDS_ELEMENTS.get(self.element["kind"])
        if handler is not None and self.cell is not None:
            handler(self.cell, self.element)
        self.element = None


def _from_gds(data: bytes) -> _Model:
    reader = _GdsReader()
    for rec in gds.records(data):
        reader.record(rec)
    return reader.model


# --- OASIS -----------------------------------------------------------------------------------


def _moved(points, d: Point) -> tuple:
    return tuple((x + d[0], y + d[1]) for x, y in points)


def _oasis_shape(cell: _Cell, shape: oasis.Shape) -> None:
    key = (shape.layer, shape.datatype)
    for d in shape.repetition:
        if shape.kind == "path":
            for quad in path_polygons(_moved(shape.points, d), shape.halfwidth, *shape.extensions):
                cell.polygons.append((*key, quad))
        elif shape.kind == "circle":
            cx, cy = shape.points[0]
            cell.circles.append((*key, (cx + d[0], cy + d[1]), shape.radius))
        elif shape.kind != "xgeometry":
            cell.polygons.append((*key, _moved(shape.points, d)))


def _oasis_cell(source: oasis.Cell) -> _Cell:
    cell = _Cell()
    for shape in source.shapes:
        _oasis_shape(cell, shape)
    for text in source.texts:
        for dx, dy in text.repetition:
            cell.texts.append((text.layer, text.texttype, (text.position[0] + dx, text.position[1] + dy), text.string))
    for p in source.placements:
        for dx, dy in p.repetition:
            matrix = placement_matrix(p.position[0] + dx, p.position[1] + dy, p.mag, p.angle, p.flip)
            cell.refs.append((p.cell, matrix))
    return cell


def _from_oasis(data: bytes) -> _Model:
    lib = oasis.decode(data)
    if lib.error is not None:
        raise oasis.OasisError(lib.error)
    return _Model({name: _oasis_cell(c) for name, c in lib.cells.items()}, lib.unit)


# --- flattening and canonical form -----------------------------------------------------------


@dataclass
class _Flat:
    polygons: dict[tuple[int, int], list[tuple]] = field(default_factory=lambda: defaultdict(list))
    circles: dict[tuple[int, int], list[str]] = field(default_factory=lambda: defaultdict(list))
    texts: list[str] = field(default_factory=list)
    missing: set[str] = field(default_factory=set)


def _escape(text: str) -> str:
    return '"' + text.encode("unicode_escape").decode("ascii").replace('"', '\\"') + '"'


def _flatten(model: _Model, name: str, m: Matrix, out: _Flat, stack: tuple[str, ...]) -> None:
    cell = model.cells.get(name)
    if cell is None:
        out.missing.add(name)
        return
    if name in stack or len(stack) > MAX_DEPTH:
        raise ValueError(f"recursive or too deep placement of cell {name}")
    for layer, datatype, points in cell.polygons:
        out.polygons[(layer, datatype)].append(tuple(apply(m, p) for p in points))
    for layer, datatype, centre, radius in cell.circles:
        cx, cy = apply(m, centre)
        out.circles[(layer, datatype)].append(f"circle {cx} {cy} {_round(radius * _scale(m))}")
    for layer, texttype, position, string in cell.texts:
        x, y = apply(m, position)
        out.texts.append(f"text {layer}/{texttype} {x} {y} {_escape(string)}")
    for child, inner in cell.refs:
        _flatten(model, child, compose(m, inner), out, (*stack, name))


def manhattan_rects(polygon) -> list[tuple[int, int, int, int]]:
    """The rectangles covered by an axis-parallel ``polygon`` under the non-zero winding rule."""
    closed = list(zip(polygon, (*polygon[1:], polygon[0])))
    edges = [(a[0], min(a[1], b[1]), max(a[1], b[1]), 1 if b[1] > a[1] else -1)
             for a, b in closed if a[0] == b[0] and a[1] != b[1]]
    ys = sorted({y for _, y in polygon})
    rects = []
    for y1, y2 in zip(ys, ys[1:]):
        crossings = sorted((x, w) for x, lo, hi, w in edges if lo <= y1 and hi >= y2)
        rects += [(x1, y1, x2, y2) for x1, x2 in _covered_spans(crossings)]
    return rects


def _covered_spans(crossings: list[tuple[int, int]]) -> list[tuple[int, int]]:
    """The x spans of non-zero winding along one strip, from its sorted (x, direction) edge crossings."""
    spans, winding, begin = [], 0, 0
    for x, direction in crossings:
        if winding == 0:
            begin = x
        winding += direction
        if winding == 0 and x > begin:
            spans.append((begin, x))
    return spans


def canonical_polygon(polygon) -> tuple[Point, ...] | None:
    """``polygon`` without duplicate or collinear vertices, counterclockwise, from its smallest
    vertex; ``None`` when nothing with area is left."""
    points = list(polygon)
    changed = True
    while changed and len(points) >= 3:
        changed = False
        for i in range(len(points)):
            a, b, c = points[i - 1], points[i], points[(i + 1) % len(points)]
            if (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0]) == 0:
                del points[i]
                changed = True
                break
    if len(points) < 3:
        return None
    area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(points, (*points[1:], points[0])))
    if area < 0:
        points.reverse()
    start = points.index(min(points))
    return tuple(points[start:] + points[:start])


def _layer_lines(polygons: list[tuple], circles: list[str]) -> list[str]:
    rects, others = [], set()
    for polygon in polygons:
        if len(polygon) < 3:
            continue
        if _is_manhattan((*polygon, polygon[0])):
            rects += manhattan_rects(polygon)
            continue
        canonical = canonical_polygon(polygon)
        if canonical is not None:
            others.add("polygon " + " ".join(f"{x} {y}" for x, y in canonical))
    lines = [f"rect {x1} {y1} {x2} {y2}" for x1, y1, x2, y2 in canonical_rects(rects)]
    return lines + sorted(others) + sorted(circles)


def _top_cells(model: _Model) -> list[str]:
    placed = {child for cell in model.cells.values() for child, _ in cell.refs}
    return sorted(name for name in model.cells if name not in placed)


def geometry_text(data: bytes) -> str:
    """The canonical text of a GDSII or OASIS stream (see the module rules)."""
    try:
        is_oasis = oasis.unwrap(data)[:len(oasis.MAGIC)] == oasis.MAGIC
        model = _from_oasis(data) if is_oasis else _from_gds(data)
        if model.cells and not _top_cells(model):
            raise ValueError("every cell is placed by another (recursive hierarchy)")
        flat = _Flat()
        for top in _top_cells(model):
            _flatten(model, top, IDENTITY, flat, ())
    except (ValueError, ZeroDivisionError) as error:  # GdsError and OasisError are ValueErrors
        return f"error {error}\n"
    lines = [f"units {model.dbu_per_micron:.6g}"]
    for key in sorted(set(flat.polygons) | set(flat.circles)):
        body = _layer_lines(flat.polygons.get(key, []), flat.circles.get(key, []))
        if body:
            lines += [f"layer {key[0]}/{key[1]}", *body]
    lines += sorted(flat.texts)
    lines += [f"missing cell {name}" for name in sorted(flat.missing)]
    return "\n".join(lines) + "\n"


def canonicalize(data: bytes) -> bytes:
    return geometry_text(data).encode("utf-8")
