#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Generate tests/data/schem/devices_interface.json from the system-library interface tables.

openspec/changes/m8-schematic design D8 and task 4.2. The source is the Requirement "System library" of
openspec/specs/schematic-symbols-and-libraries/spec.md, which holds four tables: symbols (reference PDKs,
bounding box), properties (decoded values as JSON string literals), pins (in pin order) and notes. The JSON is
the machine-readable copy that the DEVLIB library test checks its .sym files against.

Usage:
    scripts/gen_devices_interface.py [--spec FILE] [--out FILE]   write the JSON
    scripts/gen_devices_interface.py --check                       exit 1 when the committed JSON differs
Exit status: 0 success, 1 the JSON differs (--check) or the tables are malformed, 2 usage error.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SPEC = REPO / "openspec/specs/schematic-symbols-and-libraries/spec.md"
OUT = REPO / "tests/data/schem/devices_interface.json"
REQUIREMENT = "### Requirement: System library"
TABLES = {
    "symbols": "**Interface table: symbols**",
    "properties": "**Interface table: properties**",
    "pins": "**Interface table: pins**",
    "notes": "**Interface table: notes**",
}
CODE = re.compile(r"^`(.*)`$")


class TableError(ValueError):
    """A malformed interface table."""


def requirement_text(spec: str) -> list[str]:
    """The lines of the Requirement "System library", up to the next requirement."""
    lines = spec.splitlines()
    try:
        start = lines.index(REQUIREMENT)
    except ValueError as exc:
        raise TableError(f"no '{REQUIREMENT}' in the spec") from exc
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("### ")), len(lines))
    return lines[start:end]


def split_row(line: str) -> list[str]:
    """The cells of a Markdown table row (values never hold a bare '|': the tables write it as \\u007c)."""
    return [cell.strip() for cell in line.strip().strip("|").split("|")]


def table_rows(lines: list[str], title: str, width: int) -> list[list[str]]:
    """The body rows of the table that follows the line ``title``."""
    try:
        i = lines.index(title) + 1
    except ValueError as exc:
        raise TableError(f"no table '{title}'") from exc
    while i < len(lines) and not lines[i].startswith("|"):
        i += 1
    rows = []
    for line in lines[i + 2:]:  # skip the header and the separator
        if not line.startswith("|"):
            break
        cells = split_row(line)
        if len(cells) != width:
            raise TableError(f"{title}: expected {width} cells: {line}")
        rows.append(cells)
    return rows


def code(cell: str) -> str:
    """The text inside a backtick code span."""
    match = CODE.match(cell)
    if not match:
        raise TableError(f"expected a code span: {cell}")
    return match.group(1)


def json_value(cell: str) -> str:
    """A JSON string literal written as a code span."""
    value = json.loads(code(cell))
    if not isinstance(value, str):
        raise TableError(f"expected a JSON string: {cell}")
    return value


def numbers(cell: str, count: int) -> list[float]:
    """``count`` numbers of a code span such as `-2.5 -32.5 2.5 -27.5`."""
    parts = code(cell).split()
    if len(parts) != count:
        raise TableError(f"expected {count} numbers: {cell}")
    return [float(p) for p in parts]


def pin_properties(cell: str) -> dict[str, str]:
    """``key=value`` pairs of the "Other pin properties" cell, or none for ``-``."""
    if cell == "-":
        return {}
    pairs = (item.split("=", 1) for item in code(cell).split())
    return {key: value for key, value in pairs}


def symbol_rows(lines: list[str]) -> dict[str, dict]:
    """The symbols table: one entry per symbol with its referencing PDKs and bounding box."""
    symbols: dict[str, dict] = {}
    for name, refs, bbox in table_rows(lines, TABLES["symbols"], 3):
        symbol = code(name)
        if symbol in symbols:
            raise TableError(f"duplicate symbol row: {symbol}")
        symbols[symbol] = {"referenced_by": [] if refs == "-" else refs.split(), "bbox": numbers(bbox, 4),
                           "properties": {}, "pins": [], "notes": []}
    return symbols


def entry(symbols: dict[str, dict], cell: str) -> dict:
    """The symbol entry that a row of another table names."""
    symbol = code(cell)
    if symbol not in symbols:
        raise TableError(f"row for a symbol missing from the symbols table: {symbol}")
    return symbols[symbol]


def add_properties(symbols: dict[str, dict], lines: list[str]) -> None:
    for name, key, value in table_rows(lines, TABLES["properties"], 3):
        props = entry(symbols, name)["properties"]
        if code(key) in props:
            raise TableError(f"duplicate property {code(key)} of {code(name)}")
        props[code(key)] = json_value(value)


def add_pins(symbols: dict[str, dict], lines: list[str]) -> None:
    for name, index, pin, direction, box, other in table_rows(lines, TABLES["pins"], 6):
        pins = entry(symbols, name)["pins"]
        if int(index) != len(pins):
            raise TableError(f"pin {index} of {code(name)} is out of order")
        pins.append({"name": code(pin), "dir": code(direction), "box": numbers(box, 4),
                     "properties": pin_properties(other)})


def add_notes(symbols: dict[str, dict], lines: list[str]) -> None:
    for name, note in table_rows(lines, TABLES["notes"], 2):
        entry(symbols, name)["notes"].append(json_value(note))


def interface(spec: str) -> dict:
    """The JSON document generated from the spec text."""
    lines = requirement_text(spec)
    symbols = symbol_rows(lines)
    add_properties(symbols, lines)
    add_pins(symbols, lines)
    add_notes(symbols, lines)
    return {
        "source": "openspec/specs/schematic-symbols-and-libraries/spec.md, Requirement \"System library\"",
        "generator": "scripts/gen_devices_interface.py",
        "symbols": dict(sorted(symbols.items())),
    }


def render(doc: dict) -> str:
    return json.dumps(doc, indent=2, sort_keys=True, ensure_ascii=False) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--spec", type=Path, default=SPEC)
    parser.add_argument("--out", type=Path, default=OUT)
    parser.add_argument("--check", action="store_true", help="compare with --out instead of writing it")
    args = parser.parse_args(argv)
    try:
        text = render(interface(args.spec.read_text(encoding="utf-8")))
    except (OSError, TableError, json.JSONDecodeError, ValueError) as exc:
        print(f"gen_devices_interface: {exc}", file=sys.stderr)
        return 1
    if args.check:
        current = args.out.read_text(encoding="utf-8") if args.out.exists() else ""
        if current != text:
            print(f"gen_devices_interface: {args.out} is out of date; run scripts/gen_devices_interface.py",
                  file=sys.stderr)
            return 1
        return 0
    args.out.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
