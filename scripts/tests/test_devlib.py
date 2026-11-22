# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The system symbol library is what scripts/gen_devlib.py writes (openspec/changes/m8-schematic design D8, task 7.5).

[cap:schematic-symbols-and-libraries][req:System library]"""
from __future__ import annotations

import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import gen_devlib as gen  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
INVENTORY = REPO / "tests/data/schem/inventory.json"


def run(argv: list[str]) -> tuple[int, str]:
    err = io.StringIO()
    with redirect_stderr(err), redirect_stdout(io.StringIO()):
        status = gen.main(argv)
    return status, err.getvalue()


class CommittedLibrary(unittest.TestCase):
    """[cap:schematic-symbols-and-libraries][req:System library]"""

    def test_check_passes_on_the_committed_library(self) -> None:
        status, err = run(["--check"])
        self.assertEqual(status, 0, err)

    def test_library_holds_exactly_the_interface_symbols(self) -> None:
        interface = set(gen.load_interface())
        present = {p.name for p in gen.OUT.iterdir()}
        self.assertEqual(present, interface)
        self.assertEqual(len(interface), 43)

    def test_interface_is_the_inventory_plus_netlist_options(self) -> None:
        inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))["pdks"]
        listed = {name for pdk in inventory.values() for name in pdk["devices"]}
        self.assertEqual(set(gen.load_interface()), listed | {"netlist_options.sym"})  # Q-SCHEM-31

    def test_every_file_carries_the_apache_notice_in_its_version_record(self) -> None:
        for path in sorted(gen.OUT.glob("*.sym")):
            with self.subTest(path.name):
                text = path.read_text(encoding="utf-8")
                self.assertTrue(text.startswith("v {xschem version=3.4.8RC file_version=1.3\n"
                                                "* SPDX-License-Identifier: Apache-2.0\n"))


class CheckMode(unittest.TestCase):
    """[cap:schematic-symbols-and-libraries][req:System library]"""

    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.out = Path(self.tmp.name) / "devices"
        self.assertEqual(run(["--out", str(self.out)])[0], 0)

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def test_fresh_output_checks_clean(self) -> None:
        self.assertEqual(run(["--check", "--out", str(self.out)]), (0, ""))

    def test_edited_file_fails(self) -> None:
        path = self.out / "res.sym"
        path.write_text(path.read_text(encoding="utf-8").replace("@value", "@VALUE"), encoding="utf-8")
        status, err = run(["--check", "--out", str(self.out)])
        self.assertEqual(status, 1)
        self.assertIn("differs: res.sym", err)

    def test_missing_and_extra_files_fail(self) -> None:
        (self.out / "gnd.sym").unlink()
        (self.out / "extra.sym").write_text("v {}\n", encoding="utf-8")
        status, err = run(["--check", "--out", str(self.out)])
        self.assertEqual(status, 1)
        self.assertIn("missing gnd.sym", err)
        self.assertIn("extra file extra.sym", err)

    def test_write_removes_stale_symbols(self) -> None:
        (self.out / "stale.sym").write_text("v {}\n", encoding="utf-8")
        self.assertEqual(run(["--out", str(self.out)])[0], 0)
        self.assertFalse((self.out / "stale.sym").exists())


class FileText(unittest.TestCase):
    """The writer's canonical form ("Canonical write order", "Numeric fields", "Brace strings")."""

    def test_numbers_print_as_percent_16g(self) -> None:
        self.assertEqual(gen.number(10.0), "10")
        self.assertEqual(gen.number(-2.5), "-2.5")
        self.assertEqual(gen.number(0.46875), "0.46875")
        self.assertEqual(gen.number(1e-5), "1e-05")

    def test_values_are_quoted_and_escaped_only_when_needed(self) -> None:
        self.assertEqual(gen.value_text("resistor"), "resistor")
        self.assertEqual(gen.value_text("a b"), '"a b"')
        self.assertEqual(gen.value_text('x="y"'), '"x=\\"y\\""')
        self.assertEqual(gen.value_text("v(@@p\\)"), '"v(@@p\\\\)"')
        self.assertEqual(gen.brace('a{b}\\'), "{a\\{b\\}\\\\}")

    def test_lines_and_boxes_are_normalised_and_grouped_by_layer(self) -> None:
        self.assertEqual(gen.record(gen.Line(20, 0, 10, 5)), "L 4 10 5 20 0 {}")
        self.assertEqual(gen.record(gen.Line(0, 10, 0, -10)), "L 4 0 -10 0 10 {}")
        self.assertEqual(gen.record(gen.Box(2.5, 2.5, -2.5, -2.5, "name=A", 5)), "B 5 -2.5 -2.5 2.5 2.5 {name=A}")
        shapes = [gen.Line(0, 0, 1, 1, layer=7), gen.Line(0, 0, 2, 2, layer=3)]
        self.assertEqual([s.layer for s in gen.by_layer(shapes, gen.Line)], [3, 7])

    def test_simulator_commands_format_follows_the_interface_note(self) -> None:
        fmt = gen.NOTE_PROPERTIES["simulator_commands.sym"]["format"]
        self.assertTrue(fmt.startswith("tcleval("))
        self.assertIn("sim_is_@simulator ", fmt)  # the token ends at the blank
        self.assertIn("{\n\n@value\n}", fmt)
        self.assertEqual(fmt.count("{"), fmt.count("}"))

    def test_every_interface_symbol_has_a_drawing(self) -> None:
        self.assertEqual(set(gen.DRAWINGS), set(gen.load_interface()))


if __name__ == "__main__":
    unittest.main()
