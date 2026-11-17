#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Bisect a MASK-RT difference from the raw outputs of Magic and Hefesto (``--keep-raw``).

Takes the two ``<DIR>/<case-id>/`` directories written by ``capture.py --keep-raw`` (Magic) and
``replay.py --keep-raw`` (Hefesto) and prints, Magic first:

* the structure order of ``files/out.gds``: the first position where the STRNAME sequences differ;
* per structure in both files, the first differing element in write order (record by record);
* the order-insensitive differences of tools/golden/gds_rt_report.py's ``compare``;
* with ``--feedback-probe`` runs, the feedback entries (``files/out.feedback.tcl``) that only one
  side has, grouped by cell, layer and message.

    raw_diff.py MAGIC_DIR HEFESTO_DIR [--max N]
"""
from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from pathlib import Path

from gds_rt_report import compare
from goldenlib import gds
from goldenlib.rawkeep import FEEDBACK_FILE

GDS_FILE = "out.gds"
CIF_ERROR = re.compile(r'CIF error in cell (?P<cell>.*?), layer (?P<layer>.*?): (?P<message>[^"]*)')


def structures(data: bytes) -> list[tuple[str, list[str]]]:
    """(name, element texts) per structure in write order; an element is its records joined."""
    out: list[tuple[str, list[str]]] = []
    element: list[str] = []
    for record in gds.records(data):
        text = gds.format_record(record)
        if record.name == "STRNAME":
            out.append((record.values()[0] if record.values() else "?", []))
        elif record.name == "ENDEL" and out:
            out[-1][1].append(" ".join(element))
            element = []
        elif out and record.name not in ("BGNSTR", "ENDSTR"):
            element.append(text)
    return out


def first_order_difference(magic: list[str], hefesto: list[str]) -> str | None:
    for index, (a, b) in enumerate(zip(magic, hefesto)):
        if a != b:
            return f"structure order differs at position {index}: Magic {a}, Hefesto {b}"
    if len(magic) != len(hefesto):
        return f"structure count: Magic {len(magic)}, Hefesto {len(hefesto)}"
    return None


def first_element_difference(name: str, magic: list[str], hefesto: list[str]) -> str | None:
    for index, (a, b) in enumerate(zip(magic, hefesto)):
        if a != b:
            return f"{name}: element {index} differs\n    Magic   {a}\n    Hefesto {b}"
    if len(magic) != len(hefesto):
        return f"{name}: Magic has {len(magic)} elements, Hefesto {len(hefesto)} (equal up to the shorter)"
    return None


def gds_report(magic: bytes, hefesto: bytes, limit: int) -> list[str]:
    a, b = structures(magic), structures(hefesto)
    lines = [line for line in [first_order_difference([n for n, _ in a], [n for n, _ in b])] if line]
    by_name = dict(b)
    elements = [first_element_difference(name, els, by_name[name]) for name, els in a if name in by_name]
    lines += [line for line in elements if line][:limit]
    lines += compare(magic, hefesto)[:limit]
    return lines


def feedback_entries(text: str) -> Counter:
    """(box, text) of each entry of a ``feedback save`` file."""
    entries: Counter = Counter()
    box = ""
    for line in text.splitlines():
        if line.startswith("box "):
            box = line[4:]
        elif line.startswith("feedback add "):
            entries[(box, line[len("feedback add "):])] += 1
    return entries


def feedback_group(entry_text: str) -> str:
    match = CIF_ERROR.search(entry_text)
    return f"{match['cell']} / {match['layer']} / {match['message']}" if match else entry_text


def feedback_report(magic: str, hefesto: str, limit: int) -> list[str]:
    a, b = feedback_entries(magic), feedback_entries(hefesto)
    lines = [f"feedback entries: Magic {sum(a.values())}, Hefesto {sum(b.values())}"]
    for label, only in (("only in Magic", a - b), ("only in Hefesto", b - a)):
        groups = Counter(feedback_group(text) for (_, text), n in only.items() for _ in range(n))
        lines += [f"{label}: {n} x {group}" for group, n in sorted(groups.items())]
        lines += [f"  {label}: box {box} {text}" for (box, text) in sorted(only)[:limit]]
    return lines


def read(path: Path) -> bytes | None:
    return path.read_bytes() if path.is_file() else None


def report(magic_dir: Path, hefesto_dir: Path, limit: int) -> list[str]:
    lines = []
    magic_gds, hefesto_gds = read(magic_dir / "files" / GDS_FILE), read(hefesto_dir / "files" / GDS_FILE)
    if magic_gds is None or hefesto_gds is None:
        lines.append(f"{GDS_FILE}: missing on {'Magic' if magic_gds is None else 'Hefesto'}'s side")
    else:
        lines += gds_report(magic_gds, hefesto_gds, limit) or [f"{GDS_FILE}: no difference"]
    magic_fb, hefesto_fb = read(magic_dir / "files" / FEEDBACK_FILE), read(hefesto_dir / "files" / FEEDBACK_FILE)
    if magic_fb is not None and hefesto_fb is not None:
        lines += feedback_report(magic_fb.decode(errors="replace"), hefesto_fb.decode(errors="replace"), limit)
    return lines


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("magic", type=Path, help="capture.py --keep-raw directory of the case")
    parser.add_argument("hefesto", type=Path, help="replay.py --keep-raw directory of the case")
    parser.add_argument("--max", type=int, default=40, help="lines listed per section")
    args = parser.parse_args(argv)
    print("\n".join(report(args.magic, args.hefesto, args.max)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
