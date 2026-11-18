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
SHA256 = r"^[0-9a-f]{64}$"
# Tools installed from a package or a wheel (m6-lef-def-oasis design D18) instead of built.
INSTALLED = ("klayout", "gdstk")


def containerfile_args() -> dict[str, str]:
    text = CONTAINERFILE.read_text(encoding="utf-8")
    return dict(re.findall(r"^ARG (\w+)=(\S+)$", text, re.MULTILINE))


class PinsTest(unittest.TestCase):
    def setUp(self):
        self.pins, self.args = load_pins(), containerfile_args()

    def test_tool_commits_are_full_shas_and_match(self):
        for tool, pin in self.pins["tools"].items():
            if tool in INSTALLED:
                continue
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

    def test_klayout_debian_package_pinned_and_matches(self):
        """KLayout: the Debian package at the APT snapshot, its .deb SHA-256 checked (D18)."""
        pin = self.pins["tools"]["klayout"]
        self.assertEqual(pin["source"], "debian")
        self.assertEqual(pin["package"], "klayout")
        self.assertTrue(pin["package_version"].startswith(pin["version"] + "-"), pin["package_version"])
        self.assertRegex(pin["sha256"], SHA256)
        self.assertRegex(pin["upstream_commit"], FULL_SHA)
        self.assertEqual(self.args["KLAYOUT_DEB_VERSION"], pin["package_version"])
        self.assertEqual(self.args["KLAYOUT_DEB_SHA256"], pin["sha256"])

    def test_gdstk_wheel_pinned_and_matches(self):
        """gdstk: a PyPI wheel for the image's CPython 3.13, pinned by URL and SHA-256 (D18)."""
        pin = self.pins["tools"]["gdstk"]
        self.assertEqual(pin["source"], "pypi-wheel")
        wheel = pin["url"].rsplit("/", 1)[1]
        self.assertTrue(wheel.startswith(f"gdstk-{pin['version']}-cp313-cp313-"), wheel)
        self.assertTrue(wheel.endswith("_x86_64.whl"), wheel)
        self.assertTrue(pin["url"].startswith("https://files.pythonhosted.org/"), pin["url"])
        self.assertRegex(pin["sha256"], SHA256)
        self.assertRegex(pin["upstream_commit"], FULL_SHA)
        self.assertEqual(self.args["GDSTK_VERSION"], pin["version"])
        self.assertEqual(self.args["GDSTK_WHEEL_URL"], pin["url"])
        self.assertEqual(self.args["GDSTK_WHEEL_SHA256"], pin["sha256"])

    def test_every_tool_has_a_repo_and_version_for_provenance(self):
        for tool, pin in self.pins["tools"].items():
            with self.subTest(tool=tool):
                self.assertTrue(pin["repo"].startswith("https://"))
                if tool in INSTALLED:
                    self.assertNotIn("commit", pin)  # nothing is built from a commit
                    self.assertTrue(pin["version"])


if __name__ == "__main__":
    unittest.main()
