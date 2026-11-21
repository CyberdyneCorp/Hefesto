# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The system-library interface JSON agrees with the spec (openspec/changes/m8-schematic design D8, task 4.2).

[cap:schematic-symbols-and-libraries][req:System library]"""
from __future__ import annotations

import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import gen_devices_interface as gen  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
INVENTORY = REPO / "tests/data/schem/inventory.json"
# Symbols outside the inventory that a golden case places (Q-SCHEM-31, proposed).
OUTSIDE_INVENTORY = {"netlist_options.sym"}

MINI_SPEC = """### Requirement: System library
Text.

**Interface table: symbols**

| Symbol | Referenced by | Bounding box |
|---|---|---|
| `a.sym` | ihp | `-1 -2 1 2` |
| `b.sym` | - | `0 0 0 0` |

**Interface table: properties**

| Symbol | Key | Value |
|---|---|---|
| `a.sym` | `type` | `"label"` |
| `a.sym` | `template` | `"name=l1\\nlab=\\"x y\\""` |

**Interface table: pins**

| Symbol | Pin | Name | dir | Box | Other pin properties |
|---|---|---|---|---|---|
| `a.sym` | 0 | `p` | `inout` | `-1 -1 1 1` | `goto=0` |

**Interface table: notes**

| Symbol | Note |
|---|---|
| `b.sym` | `"no pins"` |

### Requirement: Next
"""


class SpecAgreement(unittest.TestCase):
    """[cap:schematic-symbols-and-libraries][req:System library]"""

    def setUp(self) -> None:
        self.doc = gen.interface(gen.SPEC.read_text(encoding="utf-8"))

    def test_committed_json_equals_the_spec_tables(self) -> None:
        """[cap:schematic-symbols-and-libraries][req:System library] the committed JSON is regenerated."""
        self.assertEqual(gen.OUT.read_text(encoding="utf-8"), gen.render(self.doc))
        self.assertEqual(gen.main(["--check"]), 0)

    def test_every_inventoried_symbol_has_a_row(self) -> None:
        """[cap:schematic-symbols-and-libraries][req:System library] rows equal the inventory's devices lists."""
        inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
        refs: dict[str, list[str]] = {}
        for pdk, data in sorted(inventory["pdks"].items()):
            for symbol in data["devices"]:
                refs.setdefault(symbol, []).append(pdk)
        symbols = self.doc["symbols"]
        self.assertEqual(set(symbols) - OUTSIDE_INVENTORY, set(refs))
        for symbol, pdks in refs.items():
            self.assertEqual(symbols[symbol]["referenced_by"], pdks, symbol)
        for symbol in OUTSIDE_INVENTORY:
            self.assertEqual(symbols[symbol]["referenced_by"], [], symbol)

    def test_rows_are_well_formed(self) -> None:
        """[cap:schematic-symbols-and-libraries][req:System library] each symbol has a type, sorted boxes."""
        for symbol, data in self.doc["symbols"].items():
            self.assertIn("type", data["properties"], symbol)
            x1, y1, x2, y2 = data["bbox"]
            self.assertTrue(x1 <= x2 and y1 <= y2, symbol)
            for pin in data["pins"]:
                px1, py1, px2, py2 = pin["box"]
                self.assertTrue(px1 < px2 and py1 < py2, f"{symbol} {pin['name']}")
                self.assertTrue(x1 <= px1 and px2 <= x2 and y1 <= py1 and py2 <= y2, f"{symbol} {pin['name']}")


class Parser(unittest.TestCase):
    """[cap:schematic-symbols-and-libraries][req:System library]"""

    def test_mini_spec(self) -> None:
        """[cap:schematic-symbols-and-libraries][req:System library] tables become the JSON document."""
        symbols = gen.interface(MINI_SPEC)["symbols"]
        self.assertEqual(list(symbols), ["a.sym", "b.sym"])
        a = symbols["a.sym"]
        self.assertEqual(a["referenced_by"], ["ihp"])
        self.assertEqual(a["bbox"], [-1.0, -2.0, 1.0, 2.0])
        self.assertEqual(a["properties"], {"type": "label", "template": 'name=l1\nlab="x y"'})
        self.assertEqual(a["pins"], [{"name": "p", "dir": "inout", "box": [-1.0, -1.0, 1.0, 1.0],
                                      "properties": {"goto": "0"}}])
        self.assertEqual(symbols["b.sym"]["referenced_by"], [])
        self.assertEqual(symbols["b.sym"]["notes"], ["no pins"])

    def test_malformed_tables_are_rejected(self) -> None:
        """[cap:schematic-symbols-and-libraries][req:System library] errors name the bad row."""
        bad = {
            "unknown symbol": MINI_SPEC.replace("| `b.sym` | `\"no pins\"` |", "| `c.sym` | `\"x\"` |"),
            "pin order": MINI_SPEC.replace("| `a.sym` | 0 | `p`", "| `a.sym` | 1 | `p`"),
            "not JSON": MINI_SPEC.replace('`"label"`', "`label`"),
            "duplicate": MINI_SPEC.replace("| `b.sym` | - |", "| `a.sym` | - |"),
            "no requirement": MINI_SPEC.replace("System library", "Other"),
            "bbox": MINI_SPEC.replace("`-1 -2 1 2`", "`-1 -2 1`"),
        }
        for label, text in bad.items():
            with self.subTest(label), self.assertRaises((gen.TableError, ValueError)):
                gen.interface(text)

    def test_check_reports_a_stale_file(self) -> None:
        """[cap:schematic-symbols-and-libraries][req:System library] --check fails on a stale JSON."""
        with tempfile.TemporaryDirectory() as tmp:
            spec, out = Path(tmp, "spec.md"), Path(tmp, "out.json")
            spec.write_text(MINI_SPEC, encoding="utf-8")
            self.assertEqual(gen.main(["--spec", str(spec), "--out", str(out)]), 0)
            self.assertEqual(gen.main(["--spec", str(spec), "--out", str(out), "--check"]), 0)
            out.write_text("{}\n", encoding="utf-8")
            with redirect_stderr(io.StringIO()):
                self.assertEqual(gen.main(["--spec", str(spec), "--out", str(out), "--check"]), 1)


if __name__ == "__main__":
    unittest.main()
