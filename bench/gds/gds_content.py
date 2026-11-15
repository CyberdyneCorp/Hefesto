# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Order-insensitive comparison of two GDS libraries by decoded content (m5-mask-gds-cif design D19).

The equivalence report of the library round trip compares two writes of the same
library by what they contain, not by their bytes or record order:

* structures as a set, by name;
* per structure, SREF and AREF elements as a multiset (name, transformation, points, columns and rows);
* per structure, TEXT elements as a multiset (layer, texttype, transformation, point, string);
* per structure and layer/datatype, BOUNDARY and BOX geometry as a canonical polygon set: first the
  multiset of polygons, each ring normalised (closing point dropped, counter-clockwise, starting at its
  smallest point); when the polygons differ and every polygon is Manhattan, the covered area, cut into
  maximal vertical slabs, so that two tilings of the same area compare equal;
* PATH and the other elements as a multiset of their records.

Dates, LIBNAME, UNITS and the record order are not compared. Used by bench/gds/gds_equivalence.py.
"""
from __future__ import annotations

import gzip
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "golden"))
from goldenlib import gds  # noqa: E402

Point = tuple[int, int]
Ring = tuple[Point, ...]
# Above this many vertical edges in one layer of one structure the area is not recomputed: the
# slab sweep is quadratic in the worst case and the report is information only.
MAX_AREA_EDGES = 20000
ELEMENTS = {"BOUNDARY", "BOX", "PATH", "SREF", "AREF", "TEXT", "NODE"}


@dataclass
class Structure:
    refs: Counter = field(default_factory=Counter)
    texts: Counter = field(default_factory=Counter)
    others: Counter = field(default_factory=Counter)
    polygons: dict[tuple[int, int], list[Ring]] = field(default_factory=lambda: defaultdict(list))


def read_bytes(path: Path) -> bytes:
    data = path.read_bytes()
    return gzip.decompress(data) if data[:2] == b"\x1f\x8b" else data


def _points(values: tuple) -> Ring:
    return tuple(zip(values[0::2], values[1::2]))


def _signed_area2(ring: Ring) -> int:
    return sum(x1 * y2 - x2 * y1 for (x1, y1), (x2, y2) in zip(ring, ring[1:] + ring[:1]))


def normalise_ring(points: Ring) -> Ring:
    """Closing point dropped, repeated points dropped, counter-clockwise, starting at the smallest point."""
    ring = [p for i, p in enumerate(points) if i == 0 or p != points[i - 1]]
    if len(ring) > 1 and ring[0] == ring[-1]:
        ring.pop()
    if _signed_area2(tuple(ring)) < 0:
        ring.reverse()
    if not ring:
        return ()
    start = ring.index(min(ring))
    return tuple(ring[start:] + ring[:start])


def _box_ring(points: Ring) -> Ring:
    xs, ys = [p[0] for p in points], [p[1] for p in points]
    return normalise_ring(((min(xs), min(ys)), (max(xs), min(ys)), (max(xs), max(ys)), (min(xs), max(ys))))


def _add_element(structure: Structure, kind: str, fields: dict[str, tuple], body: tuple) -> None:
    layer = fields.get("LAYER", (0,))[0]
    points = _points(fields.get("XY", ()))
    if kind == "BOUNDARY" and points:
        structure.polygons[(layer, fields.get("DATATYPE", (0,))[0])].append(normalise_ring(points))
    elif kind == "BOX" and points:
        structure.polygons[(layer, fields.get("BOXTYPE", (0,))[0])].append(_box_ring(points))
    elif kind in ("SREF", "AREF"):
        structure.refs[body] += 1
    elif kind == "TEXT":
        structure.texts[body] += 1
    else:
        structure.others[body] += 1


def parse(data: bytes) -> dict[str, Structure]:
    """The structures of a GDS stream by name."""
    structures: dict[str, Structure] = {}
    current: Structure | None = None
    kind, fields, body = "", {}, []
    for record in gds.records(data):
        name = record.name
        if name == "STRNAME":
            current = structures.setdefault(record.values()[0], Structure())
        elif name == "ENDSTR":
            current = None
        elif current is not None and name in ELEMENTS:
            kind, fields, body = name, {}, [name]
        elif current is not None and name == "ENDEL" and kind:
            _add_element(current, kind, fields, tuple(body))
            kind = ""
        elif kind:
            fields[name] = record.values()
            body.append((name, record.values()))
    return structures


def _is_manhattan(ring: Ring) -> bool:
    return all(a[0] == b[0] or a[1] == b[1] for a, b in zip(ring, ring[1:] + ring[:1]))


def _vertical_edges(rings: list[Ring]) -> list[tuple[int, int, int, int]]:
    """(x, y_low, y_high, winding) of every vertical edge; rings are counter-clockwise."""
    edges = []
    for ring in rings:
        for (x1, y1), (x2, y2) in zip(ring, ring[1:] + ring[:1]):
            if x1 == x2 and y1 != y2:
                edges.append((x1, min(y1, y2), max(y1, y2), 1 if y2 < y1 else -1))
    return edges


def _covered(ys: list[int], counts: list[int]) -> tuple[tuple[int, int], ...]:
    spans: list[tuple[int, int]] = []
    for i, count in enumerate(counts):
        if count <= 0:
            continue
        if spans and spans[-1][1] == ys[i]:
            spans[-1] = (spans[-1][0], ys[i + 1])
        else:
            spans.append((ys[i], ys[i + 1]))
    return tuple(spans)


def canonical_area(rings: list[Ring]) -> tuple | None:
    """The union of Manhattan rings as maximal vertical slabs ((x1, x2, y spans), ...), or None
    when a ring is not Manhattan or the layer is too large to sweep."""
    if not all(_is_manhattan(r) for r in rings):
        return None
    edges = sorted(_vertical_edges(rings))
    if len(edges) > MAX_AREA_EDGES:
        return None
    return tuple((x1, x2, spans) for x1, x2, spans in _sweep(edges) if spans)


def _coverage_steps(edges: list[tuple[int, int, int, int]]):
    """(x, covered y spans to the right of x) after all the edges at each distinct x, left to right."""
    ys = sorted({y for e in edges for y in (e[1], e[2])})
    index = {y: i for i, y in enumerate(ys)}
    counts = [0] * max(len(ys) - 1, 0)
    for i, (x, low, high, winding) in enumerate(edges):
        for j in range(index[low], index[high]):
            counts[j] += winding
        if i + 1 == len(edges) or edges[i + 1][0] != x:
            yield x, _covered(ys, counts)


def _sweep(edges: list[tuple[int, int, int, int]]) -> list[list]:
    """Maximal slabs [x1, x2, spans]: a new slab starts wherever the coverage changes."""
    slabs: list[list] = []
    for x, spans in _coverage_steps(edges):
        if slabs and slabs[-1][2] == spans:
            continue  # the area to the right of x is the same as the open slab's
        if slabs:
            slabs[-1][1] = x
        slabs.append([x, None, spans])
    return slabs


def compare_layer(first: list[Ring], second: list[Ring]) -> str:
    """'' when equal; otherwise why the geometry of one layer differs."""
    if Counter(first) == Counter(second):
        return ""
    area_a, area_b = canonical_area(first), canonical_area(second)
    if area_a is not None and area_a == area_b:
        return ""
    detail = "area differs" if area_a is not None and area_b is not None else "polygons differ (area not compared)"
    return f"{len(first)} vs {len(second)} polygons, {detail}"


def _multiset_diff(label: str, first: Counter, second: Counter) -> list[str]:
    if first == second:
        return []
    only_a, only_b = sum((first - second).values()), sum((second - first).values())
    return [f"{label}: {only_a} only in the first write, {only_b} only in the second"]


def compare_structure(a: Structure, b: Structure) -> list[str]:
    diffs = _multiset_diff("references", a.refs, b.refs)
    diffs += _multiset_diff("labels", a.texts, b.texts)
    diffs += _multiset_diff("other elements", a.others, b.others)
    for key in sorted(set(a.polygons) | set(b.polygons)):
        why = compare_layer(a.polygons.get(key, []), b.polygons.get(key, []))
        if why:
            diffs.append(f"layer {key[0]}/{key[1]}: {why}")
    return diffs


def compare(first: dict[str, Structure], second: dict[str, Structure]) -> list[str]:
    """Differences between two parsed libraries, one line each ([] when equivalent)."""
    lines = [f"structure {name}: only in the first write" for name in sorted(set(first) - set(second))]
    lines += [f"structure {name}: only in the second write" for name in sorted(set(second) - set(first))]
    for name in sorted(set(first) & set(second)):
        lines += [f"structure {name}: {d}" for d in compare_structure(first[name], second[name])]
    return lines
