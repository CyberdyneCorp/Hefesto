#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Hefesto's equivalence report of the library round trips (m5-mask-gds-cif design D19, task 13.7).

The MASK-RT-* cases gate on Hefesto's ``out.gds`` equalling Magic's. There is no read/write
fixed-point gate: a GDS read reverses SREF order for small cells, and regenerating masks from
read-back layers need not be idempotent. Instead, for each case this tool runs the case's
first pass with hefesto-cli (as replay.py does), then a second pass in a fresh hefesto-cli
session that reads ``out.gds`` back with the same round trip
(``tools/golden/harness/gds_rt/gds_rt.tcl`` ``rt::roundtrip``) and writes ``out2.gds``, and
compares the two writes by decoded content, insensitive to order:

* structures as a set of names;
* per structure, SREF and AREF records as a multiset (name, transform, array);
* per structure and (layer, datatype), the area of the BOUNDARY and BOX elements whose edges
  are all horizontal or vertical as one canonical rectangle set (their union, cut as
  compare/polygon_set.py does), any other polygon as a multiset of normalized rings, and
  PATH and TEXT elements as multisets.

The differences are listed (Markdown, also appended to ``$GITHUB_STEP_SUMMARY``) for review.
They never fail the run; ``--strict`` fails it only when a pass writes no output.

    gds_rt_report.py --hefesto-cli build/linux-release/src/cli/hefesto-cli [--output report.md]
                     [--strict] [CASE_DIR...]    (default: every MASK-RT-* case under tests/golden)
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

from compare.polygon_set import canonical_rects
from goldenlib import gds
from goldenlib.case import CaseError, load_case
from goldenlib.external import ExternalUnavailable
from replay import REPO_ROOT, execute, run_hefesto

SECOND_PASS = "input/gds_rt_second_pass.tcl"
FIRST_SCRIPT = "input/commands.tcl"
MAX_LINES = 40  # differences listed per case

Point = tuple[int, int]


# --- decoding ------------------------------------------------------------------------------------

@dataclass
class Structure:
    refs: Counter = field(default_factory=Counter)
    rects: dict = field(default_factory=dict)          # (layer, datatype) -> list of rectangles
    rings: dict = field(default_factory=dict)          # (layer, datatype) -> Counter of rings
    other: Counter = field(default_factory=Counter)    # PATH and TEXT elements


@dataclass
class _Element:
    kind: str
    fields: dict = field(default_factory=dict)


ELEMENTS = ("BOUNDARY", "BOX", "PATH", "TEXT", "SREF", "AREF", "NODE")


def _points(values: tuple) -> list[Point]:
    return [(values[i], values[i + 1]) for i in range(0, len(values) - 1, 2)]


def decode(data: bytes) -> dict[str, Structure]:
    """The structures of a GDS stream, by name (a repeated name merges into the first)."""
    out: dict[str, Structure] = {}
    current: Structure | None = None
    element: _Element | None = None
    for record in gds.records(data):
        name, values = record.name, record.values()
        if name == "STRNAME":
            current = out.setdefault(values[0] if values else "?", Structure())
        elif name in ELEMENTS:
            element = _Element(name)
        elif name == "ENDEL" and element is not None and current is not None:
            add_element(current, element)
            element = None
        elif element is not None:
            element.fields[name] = values
    return out


def add_element(structure: Structure, element: _Element) -> None:
    f = element.fields
    if element.kind in ("SREF", "AREF"):
        key = (element.kind, f.get("SNAME"), f.get("STRANS"), f.get("MAG"), f.get("ANGLE"), f.get("COLROW"),
               f.get("XY"))
        structure.refs[key] += 1
    elif element.kind in ("BOUNDARY", "BOX"):
        kind = f.get("DATATYPE", f.get("BOXTYPE", (0,)))
        add_polygon(structure, (f.get("LAYER", (0,))[0], kind[0]), _points(f.get("XY", ())))
    else:
        structure.other[(element.kind, tuple(sorted(f.items())))] += 1


def add_polygon(structure: Structure, layer: tuple[int, int], points: list[Point]) -> None:
    ring = points[:-1] if len(points) > 1 and points[0] == points[-1] else points
    if is_manhattan(ring):
        structure.rects.setdefault(layer, []).extend(manhattan_rects(ring))
    else:
        structure.rings.setdefault(layer, Counter())[normal_ring(ring)] += 1


def is_manhattan(ring: list[Point]) -> bool:
    edges = zip(ring, ring[1:] + ring[:1])
    return len(ring) >= 4 and all(a[0] == b[0] or a[1] == b[1] for a, b in edges)


def manhattan_rects(ring: list[Point]) -> list[tuple[int, int, int, int]]:
    """The area of a Manhattan ring (even-odd rule) as rectangles, one row per slab."""
    verticals = [(a[0], min(a[1], b[1]), max(a[1], b[1])) for a, b in zip(ring, ring[1:] + ring[:1])
                 if a[0] == b[0] and a[1] != b[1]]
    ys = sorted({y for _, y1, y2 in verticals for y in (y1, y2)})
    rects = []
    for y1, y2 in zip(ys, ys[1:]):
        xs = sorted(x for x, lo, hi in verticals if lo <= y1 and hi >= y2)
        rects += [(xs[i], y1, xs[i + 1], y2) for i in range(0, len(xs) - 1, 2) if xs[i] < xs[i + 1]]
    return rects


def normal_ring(ring: list[Point]) -> tuple[Point, ...]:
    """A ring independent of its start point and direction."""
    if not ring:
        return ()
    candidates = []
    for seq in (ring, ring[::-1]):
        start = seq.index(min(seq))
        candidates.append(tuple(seq[start:] + seq[:start]))
    return min(candidates)


# --- comparison ----------------------------------------------------------------------------------

def _counter_diff(label: str, first: Counter, second: Counter) -> list[str]:
    lines = []
    for key in sorted(set(first) | set(second), key=repr):
        if first[key] != second[key]:
            lines.append(f"{label}: {key!r} x{first[key]} in the first write, x{second[key]} in the second")
    return lines


def _layer_diff(name: str, first: Structure, second: Structure) -> list[str]:
    lines = []
    for layer in sorted(set(first.rects) | set(second.rects)):
        if canonical_rects(first.rects.get(layer, [])) != canonical_rects(second.rects.get(layer, [])):
            lines.append(f"{name}: layer {layer[0]}/{layer[1]} covers a different area")
    for layer in sorted(set(first.rings) | set(second.rings)):
        lines += _counter_diff(f"{name}: layer {layer[0]}/{layer[1]} polygon", first.rings.get(layer, Counter()),
                               second.rings.get(layer, Counter()))
    return lines


def compare(first: bytes, second: bytes) -> list[str]:
    """The order-insensitive differences between two writes (empty when equivalent)."""
    a, b = decode(first), decode(second)
    lines = [f"structure {n} only in the first write" for n in sorted(set(a) - set(b))]
    lines += [f"structure {n} only in the second write" for n in sorted(set(b) - set(a))]
    for name in sorted(set(a) & set(b)):
        lines += _counter_diff(f"{name}: reference", a[name].refs, b[name].refs)
        lines += _layer_diff(name, a[name], b[name])
        lines += _counter_diff(f"{name}: element", a[name].other, b[name].other)
    return lines


# --- running ------------------------------------------------------------------------------------

SECOND_PASS_TCL = """\
# Second pass of the MASK-RT equivalence report (tools/golden/gds_rt_report.py): Hefesto reads its own
# first write back with the same round trip and writes out2.gds.
addpath input
random seed 1
drc off
source input/gds_rt.tcl
rt::roundtrip out.gds out2
quit -noprompt
"""


@dataclass
class CaseResult:
    id: str
    status: str            # equivalent | different | error
    structures: int = 0
    lines: list[str] = field(default_factory=list)


def second_pass_args(case) -> list[str]:
    return [SECOND_PASS if arg == FIRST_SCRIPT else arg for arg in case.hefesto.get("args", [])]


def report_case(cli: Path, case_dir: Path) -> CaseResult:
    case = load_case(case_dir)
    result = CaseResult(case.id, "error")
    with tempfile.TemporaryDirectory(prefix=f"gds-rt-{case.id}-") as tmp:
        work = Path(tmp)
        try:
            execute(cli, case, work)
        except ExternalUnavailable as error:
            result.lines.append(f"external input unavailable: {error}")
            return result
        first = work / "out.gds"
        if not first.is_file():
            result.lines.append("the first pass wrote no out.gds")
            return result
        (work / SECOND_PASS).write_text(SECOND_PASS_TCL, encoding="utf-8")
        run_hefesto(cli, case, work, second_pass_args(case))
        second = work / "out2.gds"
        if not second.is_file():
            result.lines.append("the second pass wrote no out2.gds")
            return result
        lines = compare(first.read_bytes(), second.read_bytes())
        result.structures = len(decode(first.read_bytes()))
    result.status = "different" if lines else "equivalent"
    result.lines = lines
    return result


def markdown(results: list[CaseResult]) -> str:
    out = ["### MASK-RT equivalence report (second Hefesto read and write; not gating)", "",
           "| Case | Structures | Result |", "|---|---|---|"]
    out += [f"| {r.id} | {r.structures} | {r.status} ({len(r.lines)} line(s)) |" for r in results]
    for r in results:
        if r.lines:
            shown = r.lines[:MAX_LINES]
            more = [f"- ... {len(r.lines) - MAX_LINES} more"] if len(r.lines) > MAX_LINES else []
            out += ["", f"#### {r.id}", "", *(f"- {line}" for line in shown), *more]
    return "\n".join(out) + "\n"


def default_cases() -> list[Path]:
    return sorted(p for p in (REPO_ROOT / "tests" / "golden").glob("*/cases/MASK-RT-*") if p.is_dir())


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("cases", nargs="*", type=Path, help="MASK-RT case directories (default: all)")
    parser.add_argument("--hefesto-cli", type=Path, default=os.environ.get("HEFESTO_CLI"))
    parser.add_argument("--output", type=Path, help="also write the Markdown report here")
    parser.add_argument("--strict", action="store_true", help="exit 1 when a pass writes no output")
    args = parser.parse_args(argv)
    if args.hefesto_cli is None:
        parser.error("hefesto-cli not given (use --hefesto-cli or set HEFESTO_CLI)")
    results = []
    for case_dir in args.cases or default_cases():
        try:
            results.append(report_case(args.hefesto_cli.resolve(), case_dir))
        except (CaseError, OSError, ValueError, gds.GdsError, subprocess.TimeoutExpired) as error:
            results.append(CaseResult(case_dir.name, "error", lines=[str(error)]))
    text = markdown(results)
    print(text, end="")
    if args.output is not None:
        args.output.write_text(text, encoding="utf-8")
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as out:
            out.write(text)
    return 1 if args.strict and any(r.status == "error" for r in results) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
