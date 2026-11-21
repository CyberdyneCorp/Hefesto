# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Reduce a 3D-view polygon log to the compared face form (m10-3d-view design D13, task 12.4).

Input: a ``gl-capture 1`` log, as written by the LD_PRELOAD shim ``gl_capture.c`` from Magic's
3D window or, with the same format, by hefesto-cli from its reduced mesh (both write the file
named by ``HEFESTO_GL_CAPTURE``)::

    gl-capture 1
    poly <style> <n> <x1> <y1> <z1> ... <xn> <yn> <zn>

Magic emits faces per tile and per cell (GrBoxOutline works in each cell's own plane, so tiles
abutting across cells get inner side faces, and a type in several display styles is drawn once
per style), while Hefesto's mesh walks the flattened boundaries. A face multiset would therefore
differ where the geometry does not, so both logs are reduced to:

* ``face <style> <z> <+z|-z> <xa> <xb> <ya_lo> <yb_lo> <ya_hi> <yb_hi>``: per (style, z, facing),
  the union of the horizontal polygons, as the canonical trapezoids of its maximal vertical slabs
  (between consecutive x where the union's boundary changes). The facing is the sign of the
  polygon's area in xy: Magic draws top faces counter-clockwise and bottom faces clockwise.
* ``side <style> <z_hi> <z_lo> <x1> <y1> <x2> <y2>``: per (style, z range), the union of the
  vertical polygons as segments in xy, keeping only the parts on the boundary of the prisms of
  that range: a part with prism on both sides (the union of the ``+z`` faces at z_hi intersected
  with the ``-z`` faces at z_lo of the same style) is an inner face and is dropped.
* ``skew <style> <n> <coordinates>``: a polygon that is neither horizontal nor vertical (none is
  expected; it is listed as is).

x and y are exact (the logged decimals as fractions, printed as integers or ``p/q``); z is
rounded to six significant digits, so float and double z arithmetic compare equal.

    python3 -m gl_capture.faces gl.log        # print the reduced form
"""
from __future__ import annotations

import sys
from collections import defaultdict
from dataclasses import dataclass
from fractions import Fraction
from itertools import combinations
from pathlib import Path

HEADER = "gl-capture 1"
OUTPUT_HEADER = "gl-faces 1"
EPSILON = Fraction(1, 10**12)  # side probe offset; far below any distance between integer features

Point = tuple[Fraction, Fraction]
Line = tuple[Fraction, Fraction]  # y = slope * x + intercept


class LogError(ValueError):
    """The polygon log is malformed."""


@dataclass(frozen=True)
class Polygon:
    style: str
    points: tuple[tuple[Fraction, Fraction, float], ...]


def _z_key(value: float) -> str:
    value = 0.0 if value == 0 else value  # -0 from -h * s_z with h = 0
    return format(value, ".6g")


def _num(value: Fraction) -> str:
    return str(value.numerator) if value.denominator == 1 else f"{value.numerator}/{value.denominator}"


def parse_log(text: str) -> list[Polygon]:
    """The polygons of a ``gl-capture 1`` log; LogError when malformed."""
    lines = text.splitlines()
    if not lines or lines[0].strip() != HEADER:
        raise LogError(f"missing header '{HEADER}'")
    return [_parse_poly(line, i + 2) for i, line in enumerate(lines[1:]) if line.strip()]


def _parse_poly(line: str, number: int) -> Polygon:
    words = line.split()
    try:
        if words[0] != "poly":
            raise ValueError(words[0])
        count = int(words[2])
        values = words[3:]
        if count < 0 or len(values) != 3 * count:
            raise ValueError("vertex count")
        points = tuple((Fraction(values[i]), Fraction(values[i + 1]), float(values[i + 2]))
                       for i in range(0, len(values), 3))
    except (IndexError, ValueError, ZeroDivisionError) as error:
        raise LogError(f"line {number}: bad polygon ({error})") from None
    return Polygon(words[1], points)


def _area2(points: list[Point]) -> Fraction:
    return sum((a[0] * b[1] - b[0] * a[1] for a, b in zip(points, points[1:] + points[:1])), Fraction(0))


def _collinear(points: list[Point]) -> bool:
    base = points[0]
    far = max(points, key=lambda p: abs(p[0] - base[0]) + abs(p[1] - base[1]))
    dx, dy = far[0] - base[0], far[1] - base[1]
    return all((p[0] - base[0]) * dy - (p[1] - base[1]) * dx == 0 for p in points)


def _edges(points: list[Point]) -> list[tuple[Point, Point]]:
    return list(zip(points, points[1:] + points[:1]))


# --- horizontal faces: slab trapezoids ------------------------------------------------------------

def _line(a: Point, b: Point) -> Line:
    slope = (b[1] - a[1]) / (b[0] - a[0])
    return slope, a[1] - slope * a[0]


def _at(line: Line, x: Fraction) -> Fraction:
    return line[0] * x + line[1]


def _breakpoints(polys: list[list[Point]]) -> list[Fraction]:
    xs = {p[0] for poly in polys for p in poly}
    edges = [e for poly in polys for e in _edges(poly) if e[0][0] != e[1][0]]
    for e, f in combinations(edges, 2):
        x = _intersection_x(e, f)
        if x is not None:
            xs.add(x)
    return sorted(xs)


def _intersection_x(e: tuple[Point, Point], f: tuple[Point, Point]) -> Fraction | None:
    le, lf = _line(*e), _line(*f)
    if le[0] == lf[0]:
        return None
    x = (lf[1] - le[1]) / (le[0] - lf[0])
    inside = all(min(s[0][0], s[1][0]) <= x <= max(s[0][0], s[1][0]) for s in (e, f))
    return x if inside else None


def _intervals_at(poly: list[Point], xm: Fraction) -> list[tuple[Line, Line]]:
    lines = [_line(a, b) for a, b in _edges(poly) if min(a[0], b[0]) < xm < max(a[0], b[0])]
    lines.sort(key=lambda line: _at(line, xm))
    return list(zip(lines[0::2], lines[1::2]))


def _union_at(polys: list[list[Point]], xm: Fraction) -> tuple[tuple[Line, Line], ...]:
    intervals = sorted((iv for poly in polys for iv in _intervals_at(poly, xm)), key=lambda iv: _at(iv[0], xm))
    merged: list[list[Line]] = []
    for lo, hi in intervals:
        if merged and _at(lo, xm) <= _at(merged[-1][1], xm):
            if _at(hi, xm) > _at(merged[-1][1], xm):
                merged[-1][1] = hi
        else:
            merged.append([lo, hi])
    return tuple((lo, hi) for lo, hi in merged)


def union_slabs(polys: list[list[Point]]) -> list[tuple[Fraction, Fraction, tuple[tuple[Line, Line], ...]]]:
    """The union of ``polys`` as maximal slabs (xa, xb, ((lower, upper) line pairs bottom to top))."""
    xs = _breakpoints(polys)
    slabs: list[tuple[Fraction, Fraction, tuple[tuple[Line, Line], ...]]] = []
    for xa, xb in zip(xs, xs[1:]):
        pairs = _union_at(polys, (xa + xb) / 2)
        if not pairs:
            continue
        if slabs and slabs[-1][1] == xa and slabs[-1][2] == pairs:
            slabs[-1] = (slabs[-1][0], xb, pairs)
        else:
            slabs.append((xa, xb, pairs))
    return slabs


def _face_lines(style: str, z: str, facing: str, polys: list[list[Point]]) -> list[str]:
    out = []
    for xa, xb, pairs in union_slabs(polys):
        for lo, hi in pairs:
            values = (xa, xb, _at(lo, xa), _at(lo, xb), _at(hi, xa), _at(hi, xb))
            out.append(f"face {style} {z} {facing} " + " ".join(_num(v) for v in values))
    return out


# --- vertical faces: boundary segments ------------------------------------------------------------

def _inside(point: Point, polys: list[list[Point]]) -> bool:
    return any(_in_polygon(point, poly) for poly in polys)


def _in_polygon(point: Point, poly: list[Point]) -> bool:
    px, py = point
    inside = False
    for (x1, y1), (x2, y2) in _edges(poly):
        if (y1 > py) != (y2 > py) and px < x1 + (py - y1) * (x2 - x1) / (y2 - y1):
            inside = not inside
    return inside


@dataclass(frozen=True)
class _Carrier:
    """The line of a side segment: direction (1, m) or (0, 1), offset, parameter = x or y."""
    direction: tuple[Fraction, Fraction]
    offset: Fraction

    @staticmethod
    def of(a: Point, b: Point) -> "_Carrier":
        if a[0] == b[0]:
            return _Carrier((Fraction(0), Fraction(1)), a[0])
        slope = (b[1] - a[1]) / (b[0] - a[0])
        return _Carrier((Fraction(1), slope), a[1] - slope * a[0])

    def param(self, p: Point) -> Fraction:
        return p[0] if self.direction[0] else p[1]

    def point(self, t: Fraction) -> Point:
        if self.direction[0]:
            return t, self.direction[1] * t + self.offset
        return self.offset, t

    def side(self, p: Point) -> Fraction:
        """Signed distance-like value of p from the line (0 on it)."""
        (ox, oy), (dx, dy) = self.point(Fraction(0)), self.direction
        return dy * (p[0] - ox) - dx * (p[1] - oy)

    def crossing(self, a: Point, b: Point) -> list[Fraction]:
        """Parameters where segment a-b meets this line (its ends that lie on it, or the crossing)."""
        sa, sb = self.side(a), self.side(b)
        if sa == 0 or sb == 0:
            return [self.param(p) for p, s in ((a, sa), (b, sb)) if s == 0]
        if (sa > 0) == (sb > 0):
            return []
        s = sa / (sa - sb)
        return [self.param((a[0] + s * (b[0] - a[0]), a[1] + s * (b[1] - a[1])))]


def _merge(intervals: list[tuple[Fraction, Fraction]]) -> list[tuple[Fraction, Fraction]]:
    merged: list[tuple[Fraction, Fraction]] = []
    for lo, hi in sorted(intervals):
        if merged and lo <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(hi, merged[-1][1]))
        else:
            merged.append((lo, hi))
    return merged


def _is_inner(carrier: _Carrier, t: Fraction, tops: list[list[Point]], bottoms: list[list[Point]]) -> bool:
    (mx, my), (dx, dy) = carrier.point(t), carrier.direction
    probes = ((mx - EPSILON * dy, my + EPSILON * dx), (mx + EPSILON * dy, my - EPSILON * dx))
    return all(_inside(p, tops) and _inside(p, bottoms) for p in probes)


def _boundary_parts(carrier: _Carrier, lo: Fraction, hi: Fraction,
                    tops: list[list[Point]], bottoms: list[list[Point]]) -> list[tuple[Fraction, Fraction]]:
    cuts = {lo, hi}
    for a, b in (e for poly in tops + bottoms for e in _edges(poly)):
        cuts.update(t for t in carrier.crossing(a, b) if lo < t < hi)
    ts = sorted(cuts)
    return [(a, b) for a, b in zip(ts, ts[1:]) if not _is_inner(carrier, (a + b) / 2, tops, bottoms)]


def _side_lines(style: str, z_hi: str, z_lo: str, segments: list[tuple[Point, Point]],
                faces: dict) -> list[str]:
    tops, bottoms = faces.get((style, z_hi, "+z"), []), faces.get((style, z_lo, "-z"), [])
    by_line: dict[_Carrier, list[tuple[Fraction, Fraction]]] = defaultdict(list)
    for a, b in segments:
        carrier = _Carrier.of(a, b)
        by_line[carrier].append(tuple(sorted((carrier.param(a), carrier.param(b)))))
    out = []
    for carrier, intervals in by_line.items():
        parts = [part for lo, hi in _merge(intervals) for part in _boundary_parts(carrier, lo, hi, tops, bottoms)]
        for lo, hi in _merge(parts):
            (x1, y1), (x2, y2) = carrier.point(lo), carrier.point(hi)
            out.append(f"side {style} {z_hi} {z_lo} " + " ".join(_num(v) for v in (x1, y1, x2, y2)))
    return out


# --- classification and output ---------------------------------------------------------------------

def _segment(points: list[Point]) -> tuple[Point, Point]:
    carrier = _Carrier.of(points[0], max(points, key=lambda p: abs(p[0] - points[0][0]) + abs(p[1] - points[0][1])))
    ordered = sorted(points, key=carrier.param)
    return ordered[0], ordered[-1]


class _Groups:
    """The polygons of a log sorted into horizontal faces, side segments and skew polygons."""

    def __init__(self) -> None:
        self.faces: dict[tuple[str, str, str], list[list[Point]]] = defaultdict(list)
        self.sides: dict[tuple[str, str, str], list[tuple[Point, Point]]] = defaultdict(list)
        self.skew: list[str] = []

    def add(self, poly: Polygon) -> None:
        xy = [(x, y) for x, y, _ in poly.points]
        if len(xy) < 3:
            return
        zs = sorted((z for _, _, z in poly.points), reverse=True)
        if _z_key(zs[0]) == _z_key(zs[-1]):
            self._add_face(poly.style, _z_key(zs[0]), xy)
        elif _collinear(xy):
            self._add_side(poly.style, _z_key(zs[0]), _z_key(zs[-1]), xy)
        else:
            coords = " ".join(f"{_num(x)} {_num(y)} {_z_key(z)}" for x, y, z in poly.points)
            self.skew.append(f"skew {poly.style} {len(poly.points)} {coords}")

    def _add_face(self, style: str, z: str, xy: list[Point]) -> None:
        area = _area2(xy)
        if area != 0:
            self.faces[(style, z, "+z" if area > 0 else "-z")].append(xy)

    def _add_side(self, style: str, z_hi: str, z_lo: str, xy: list[Point]) -> None:
        a, b = _segment(xy)
        if a != b:
            self.sides[(style, z_hi, z_lo)].append((a, b))


def _classify(polys: list[Polygon]) -> tuple[dict, dict, list[str]]:
    groups = _Groups()
    for poly in polys:
        groups.add(poly)
    return groups.faces, groups.sides, groups.skew


def reduce_polygons(polys: list[Polygon]) -> str:
    """The reduced form (``gl-faces 1``) of a polygon list, sorted, one record per line."""
    faces, sides, skew = _classify(polys)
    out = [line for key, group in faces.items() for line in _face_lines(*key, group)]
    out += [line for key, group in sides.items() for line in _side_lines(*key, group, faces)]
    return "\n".join([OUTPUT_HEADER, *sorted(out), *sorted(skew)]) + "\n"


def reduce_log(text: str) -> str:
    return reduce_polygons(parse_log(text))


def main(argv: list[str]) -> int:
    if len(argv) != 1:
        print("usage: python3 -m gl_capture.faces <gl.log>", file=sys.stderr)
        return 2
    try:
        sys.stdout.write(reduce_log(Path(argv[0]).read_text(encoding="utf-8")))
    except LogError as error:
        print(f"{argv[0]}: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
