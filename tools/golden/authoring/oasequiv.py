# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The GDS equivalent of an OASIS file (oasis-read "GDS-equivalence cross-check"; design D19).

The OASIS read cases compare Hefesto's ``oasis read`` of a file ``F`` with Magic's ``gds read`` of
its GDS equivalent ``G``. oasis-read defines ``G``: every OASIS element instance becomes the GDS
element that capability assigns to it. :func:`gds_equivalent` builds ``G`` from ``F`` with the
clean-room decoder ``goldenlib/oasis.py`` and the GDS writer ``gdsgen.py``:

* UNITS: one database unit of ``1e-6 / unit`` metres, ``1 / unit`` user units (micrometres);
* cells in file order, each a structure whose elements follow the cell's records;
* RECTANGLE: the five-point BOUNDARY ``(x,y) (x+w,y) (x+w,y+h) (x,y+h) (x,y)`` ("RECTANGLE elements");
* POLYGON, TRAPEZOID, CTRAPEZOID: a BOUNDARY of the decoded vertices, closed ("POLYGON, TRAPEZOID
  and CTRAPEZOID elements");
* CIRCLE: the regular polygon of "CIRCLE elements" (``circlepoints`` vertices, round half away from
  zero, the first octant mirrored), closed; radius 0 gives nothing;
* PATH: a GDS PATH of width ``2 x half-width``, PATHTYPE 0 (flush ends), 2 (half-width ends) or 4
  with BGNEXTN/ENDEXTN ("PATH elements");
* TEXT: a TEXT with no PRESENTATION, WIDTH or STRANS ("TEXT elements");
* a shape or TEXT repetition: one element per displacement, in displacement order ("Shape
  repetitions");
* PLACEMENT: an SREF (STRANS 0x8000 for the flip, ANGLE, MAG as decoded) or, for a regular
  repetition whose vectors are axis-aligned in the child ("Placement repetitions" rules 1 and 2;
  rule 2 swaps columns and rows, so gds-read takes the columns along the child's x axis),
  one AREF; otherwise one SREF per displacement ("Placement repetitions" rule 3);
* ``S_GDS_PROPERTY`` on a placement: PROPATTR/PROPVALUE pairs ("Instance properties"); other
  properties are discarded;
* XGEOMETRY, XELEMENT and every name record give nothing ("Unsupported and extension content").

A file whose decoding stops (oasis-read "Two-pass decoding" rule 2) yields only the cells whose
records were complete before the error; ``G`` is then a well-formed library of those cells.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # goldenlib

import gdsgen as g  # noqa: E402
from goldenlib import oasis  # noqa: E402

Point = tuple[int, int]
S_GDS_PROPERTY = "S_GDS_PROPERTY"
DEFAULT_CIRCLE_POINTS = 64


def _round_half_away(value: float) -> int:
    return int(math.floor(abs(value) + 0.5)) * (1 if value >= 0 else -1)


def _unit_vector(k: int, n: int) -> tuple[float, float]:
    """(cos, sin) of ``2 pi k / n`` from the first octant, so the values are mirror-exact."""
    eighth, k = n // 8, k % n
    if n % 8:
        angle = 2 * math.pi * k / n
        return math.cos(angle), math.sin(angle)
    octant, j = divmod(k, eighth)
    if octant % 2:
        j = eighth - j
    angle = 2 * math.pi * j / n
    c, s = math.cos(angle), math.sin(angle)
    # (c, s) is the vertex mirrored into the first octant; map it back.
    return [(c, s), (s, c), (-s, c), (-c, s), (-c, -s), (-s, -c), (s, -c), (c, -s)][octant]


def circle_vertices(cx: int, cy: int, radius: int, points: int = DEFAULT_CIRCLE_POINTS) -> list[Point]:
    """The closed polygon of oasis-read "CIRCLE elements" (empty for radius 0)."""
    if radius <= 0:
        return []
    out: list[Point] = []
    for k in range(points):
        c, s = _unit_vector(k, points)
        vertex = (cx + _round_half_away(radius * c), cy + _round_half_away(radius * s))
        if not out or out[-1] != vertex:
            out.append(vertex)
    while len(out) > 1 and out[0] == out[-1]:
        out.pop()
    return out + out[:1]


def _moved(points, d: Point) -> list[Point]:
    return [(x + d[0], y + d[1]) for x, y in points]


INT32 = range(-(1 << 31), 1 << 31)


def fits(points, *lengths: int) -> bool:
    """oasis-read "Coordinate conversion" rule 1: an instance with a coordinate, width or height
    outside the signed 32-bit range is dropped ("coordinate overflow")."""
    return all(x in INT32 and y in INT32 for x, y in points) and all(v in INT32 for v in lengths)


def _shape_instance(shape: oasis.Shape, d: Point, circle_points: int) -> bytes | None:
    points = _moved(shape.points, d)
    if shape.kind == "xgeometry" or not fits(points, 2 * shape.halfwidth, shape.radius):
        return None
    if shape.kind == "path":
        return g.path(shape.layer, shape.datatype, 2 * shape.halfwidth, points,
                      **_path_type(shape.halfwidth, shape.extensions))
    if shape.kind == "circle":
        vertices = circle_vertices(*points[0], shape.radius, circle_points)
        return g.boundary(shape.layer, shape.datatype, vertices, close=False) if vertices else None
    return g.boundary(shape.layer, shape.datatype, points)


def _path_type(halfwidth: int, extensions: tuple[int, int]) -> dict:
    start, end = extensions
    if start == 0 and end == 0:
        return {"pathtype": 0}
    if start == halfwidth and end == halfwidth and halfwidth > 0:
        return {"pathtype": 2}
    return {"pathtype": 4, "bgnextn": start, "endextn": end}


def _shape(shape: oasis.Shape, circle_points: int) -> list[bytes]:
    instances = (_shape_instance(shape, d, circle_points) for d in shape.repetition)
    return [element for element in instances if element is not None]


def _text(text: oasis.Text) -> list[bytes]:
    string = text.string if isinstance(text.string, str) else ""
    origins = [(text.position[0] + dx, text.position[1] + dy) for dx, dy in text.repetition]
    return [g.text(text.layer, text.texttype, p, string) for p in origins if fits([p])]


def _gds_properties(placement: oasis.Placement) -> list[tuple[int, str]]:
    """``S_GDS_PROPERTY`` pairs (integer attribute, string value) in record order."""
    pairs = []
    for prop in placement.properties:
        values = prop.values or ()
        if prop.name != S_GDS_PROPERTY or len(values) != 2:
            continue
        attribute, value = values
        if not (isinstance(attribute, tuple) and attribute[0] in ("u", "s") and isinstance(value, tuple)):
            continue
        text = value[1].decode("latin-1") if isinstance(value[1], bytes) else value[1]
        if value[0] in ("a", "b", "n", "str"):
            pairs.append((attribute[1], text))
    return pairs


def _transform(placement: oasis.Placement) -> dict:
    flags = g.REFLECT if placement.flip else 0
    mag = None if placement.mag == 1.0 else placement.mag
    angle = None if placement.angle == 0.0 else placement.angle
    return {"flags": flags, "mag": mag, "angle": angle}


def quarter_turns(angle: float) -> int:
    """gds-read "Structure reference transforms": the angle truncated to an integer, normalized to
    0..360, then rounded to the nearest quarter turn (an exact 45 goes up)."""
    return (int(angle) % 360 + 45) // 90 % 4


def _inverse(placement: oasis.Placement, v: Point) -> tuple[float, float]:
    """T^-1(v) for T = rotation (rounded to a quarter turn as gds-read rounds ANGLE) after the flip."""
    quarter = quarter_turns(placement.angle)
    x, y = v
    for _ in range(quarter):  # rotate by -90 degrees per quarter turn
        x, y = y, -x
    return (x, -y) if placement.flip else (x, y)


def _vectors(pattern: oasis.Repetition) -> tuple[Point, int, Point, int] | None:
    """(a, na, b, nb) of a regular placement repetition (oasis-read table), else None."""
    f = pattern.fields
    table = {
        1: lambda: ((f[2], 0), f[0], (0, f[3]), f[1]),
        2: lambda: ((f[1], 0), f[0], (0, 0), 1),
        3: lambda: ((0, 0), 1, (0, f[1]), f[0]),
        8: lambda: (f[2], f[0], f[3], f[1]),
        9: lambda: (f[1], f[0], (0, 0), 1),
    }
    return table[pattern.type]() if pattern.type in table else None


def _array_axes(placement: oasis.Placement, vectors) -> tuple[Point, int, Point, int] | None:
    """The AREF's column and row (vector, count) under oasis-read "Placement repetitions": rule 1 keeps
    (a, na) as the columns; rule 2 swaps them, because gds-read takes the column count along the child's
    x axis (T^-1 of the column vector, calma/CalmaRdcl.c:1476-1500). None for rule 3."""
    a, na, b, nb = vectors
    u, v = _inverse(placement, a), _inverse(placement, b)
    if (na == 1 or u[1] == 0) and (nb == 1 or v[0] == 0):
        return a, na, b, nb
    if (na == 1 or u[0] == 0) and (nb == 1 or v[1] == 0):
        return b, nb, a, na
    return None


def _placement(placement: oasis.Placement) -> list[bytes]:
    name = placement.cell if isinstance(placement.cell, str) else str(placement.cell)
    props = _gds_properties(placement)
    transform = _transform(placement)
    x, y = placement.position
    vectors = _vectors(placement.pattern) if placement.pattern is not None else None
    axes = _array_axes(placement, vectors) if vectors is not None else None
    if axes is not None:
        a, na, b, nb = axes
        col_end = (x + na * a[0], y + na * a[1])
        row_end = (x + nb * b[0], y + nb * b[1])
        return [g.aref(name, na, nb, (x, y), col_end, row_end, props=props, **transform)]
    origins = [(x + dx, y + dy) for dx, dy in placement.repetition]
    return [g.sref(name, p, props=props, **transform) for p in origins if fits([p])]


def _cell_elements(cell: oasis.Cell, circle_points: int) -> list[bytes]:
    out: list[bytes] = []
    for element in cell.elements:
        if isinstance(element, oasis.Shape):
            out += _shape(element, circle_points)
        elif isinstance(element, oasis.Text):
            out += _text(element)
        else:
            out += _placement(element)
    return out


def complete_cells(lib: oasis.Library) -> list[tuple[str, oasis.Cell]]:
    """The cells of ``lib`` that oasis-read creates: all of them, or, when decoding stopped, the
    cells before the one whose records the error interrupted (none if it stopped in the first)."""
    cells = list(lib.cells.items())
    if lib.error is None:
        return cells
    return cells[:-1] if cells else []


def _placeholder(name: str) -> str:
    """oasis-read "Two-pass decoding" rule 1: an undefined CELLNAME refnum n is the cell ``#<n>``."""
    if name.startswith("<CELLNAME ") and name.endswith(">"):
        return "#" + name[len("<CELLNAME "):-1]
    return name


def _rename_placeholders(lib: oasis.Library) -> None:
    for cell in lib.cells.values():
        for placement in cell.placements:
            if isinstance(placement.cell, str):
                placement.cell = _placeholder(placement.cell)


def gds_equivalent(data: bytes, libname: str = "lib", circle_points: int = DEFAULT_CIRCLE_POINTS) -> bytes:
    """``G`` for the OASIS bytes ``data`` (module rules). ``libname`` is the GDS library name, which
    Magic loads into the layout window when a cell has that name (the Top cell selection cases)."""
    lib = oasis.decode(data)
    _rename_placeholders(lib)
    unit = lib.unit if lib.unit and math.isfinite(lib.unit) and lib.unit > 0 else 1000.0
    structures = [g.structure(_placeholder(str(name)), _cell_elements(cell, circle_points))
                  for name, cell in complete_cells(lib)]
    units = (float(f"{1.0 / unit:.15g}"), float(f"{1e-6 / unit:.15g}"))  # 1e-9, not 9.999999999999999e-10
    return g.library(libname, structures, units=units)


__all__ = ["gds_equivalent", "circle_vertices", "complete_cells"]
