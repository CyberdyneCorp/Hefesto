# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""pins.toml and container/Containerfile must agree.

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import re
import unittest
from pathlib import Path

from goldenlib.pins import base_image_ref, load_pins

CONTAINERFILE = Path(__file__).resolve().parents[1] / "container" / "Containerfile"
FULL_SHA = re.compile(r"^[0-9a-f]{40}$")


def containerfile_args() -> dict[str, str]:
    text = CONTAINERFILE.read_text(encoding="utf-8")
    return dict(re.findall(r"^ARG (\w+)=(\S+)$", text, re.MULTILINE))


class PinsTest(unittest.TestCase):
    def setUp(self):
        self.pins, self.args = load_pins(), containerfile_args()

    def test_tool_commits_are_full_shas_and_match(self):
        for tool, pin in self.pins["tools"].items():
            with self.subTest(tool=tool):
                self.assertRegex(pin["commit"], FULL_SHA)
                self.assertEqual(self.args[f"{tool.upper()}_COMMIT"], pin["commit"])
                self.assertEqual(self.args[f"{tool.upper()}_REPO"], pin["repo"])

    def test_roadmap_short_shas(self):
        tools = self.pins["tools"]
        for tool, short in (("magic", "fc5daa9e"), ("netgen", "93dcda14"), ("ngspice", "7aa47164"),
                            ("xschem", "4142e6d6")):
            self.assertTrue(tools[tool]["commit"].startswith(short), tool)

    def test_base_image_pinned_by_digest(self):
        self.assertEqual(self.args["BASE_IMAGE"], base_image_ref(self.pins))
        self.assertRegex(self.pins["base_image"]["digest"], r"^sha256:[0-9a-f]{64}$")
        self.assertEqual(self.args["APT_SNAPSHOT"], self.pins["base_image"]["apt_snapshot"])
        self.assertEqual(self.args["NGSPICE_BRANCH"], self.pins["tools"]["ngspice"]["branch"])


if __name__ == "__main__":
    unittest.main()
