#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Writes the netlisting test symbols tests/data/schem/net/devices/*.sym.

Each symbol is built from the system-library interface table
(tests/data/schem/devices_interface.json, generated from the spec's
"System library" requirement): its symbol property and its pin boxes, no
graphics. They stand in for the system library in the netlisting tests until
DEVLIB's library exists. Owner: NETLIST.

Usage: python3 tests/unit/schem/net/gen_test_devices.py
"""
from __future__ import annotations

import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[4]
TABLE = ROOT / "tests/data/schem/devices_interface.json"
OUT = ROOT / "tests/data/schem/net/devices"
SPECIAL = set(' \t\n"\\;{}')
PROPERTY_ORDER = ("type", "format", "lvs_format", "template")


def value_text(value: str) -> str:
    """A property-string value: bare when it has no separator or quote, else quoted."""
    if value and not (set(value) & SPECIAL):
        return value
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def property_string(properties: dict[str, str]) -> str:
    keys = [k for k in PROPERTY_ORDER if k in properties]
    keys += sorted(k for k in properties if k not in PROPERTY_ORDER)
    return "\n".join(f"{k}={value_text(properties[k])}" for k in keys)


def brace(text: str) -> str:
    return "{" + text.replace("\\", "\\\\").replace("{", "\\{").replace("}", "\\}") + "}"


def number(value: float) -> str:
    return f"{value:g}"


def symbol_text(entry: dict) -> str:
    lines = [
        "v {xschem version=3.4.8RC file_version=1.2",
        "* SPDX-License-Identifier: Apache-2.0",
        "* Netlisting test symbol written by tests/unit/schem/net/gen_test_devices.py",
        "* from tests/data/schem/devices_interface.json.}",
        "G {}",
        "K " + brace(property_string(entry["properties"])),
        "V {}",
        "S {}",
        "E {}",
    ]
    for pin in entry["pins"]:
        attributes = {"name": pin["name"], "dir": pin["dir"], **pin["properties"]}
        box = " ".join(number(v) for v in pin["box"])
        lines.append(f"B 5 {box} " + brace(" ".join(f"{k}={value_text(v)}" for k, v in attributes.items())))
    return "\n".join(lines) + "\n"


def main() -> None:
    table = json.loads(TABLE.read_text(encoding="utf-8"))["symbols"]
    OUT.mkdir(parents=True, exist_ok=True)
    for name, entry in sorted(table.items()):
        (OUT / name).write_text(symbol_text(entry), encoding="utf-8")


if __name__ == "__main__":
    main()
