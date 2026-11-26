# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""pins.toml and container/Containerfile must agree.

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
import unittest
from pathlib import Path

from goldenlib.pins import base_image_ref, load_pins

CONTAINERFILE = Path(__file__).resolve().parents[1] / "container" / "Containerfile"
FULL_SHA = re.compile(r"^[0-9a-f]{40}$")
SHA256 = r"^[0-9a-f]{64}$"
# Tools installed from a package, a wheel (m6-lef-def-oasis design D18) or a binary release
# (OpenVAF, m9-lvs-simulation design D19) instead of built.
INSTALLED = ("klayout", "gdstk", "openvaf")
NGSPICE_DIR = Path(__file__).resolve().parents[3] / "third_party" / "ngspice"
NGSPICE_COPY = CONTAINERFILE.parent / "ngspice"


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
        for tool, short in (("magic", "fc5daa9e"), ("netgen", "93dcda14"), ("ngspice", "a80f6e3e"),
                            ("ngspice_stock", "7aa47164"), ("xschem", "4142e6d6")):
            self.assertTrue(tools[tool]["commit"].startswith(short), tool)

    def test_base_image_pinned_by_digest(self):
        self.assertEqual(self.args["BASE_IMAGE"], base_image_ref(self.pins))
        self.assertRegex(self.pins["base_image"]["digest"], r"^sha256:[0-9a-f]{64}$")
        self.assertEqual(self.args["APT_SNAPSHOT"], self.pins["base_image"]["apt_snapshot"])
        self.assertEqual(self.args["NGSPICE_STOCK_BRANCH"], self.pins["tools"]["ngspice_stock"]["branch"])

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

    def test_image_variants_are_build_args_of_the_containerfile(self):
        """magic-gl (m10-3d-view design D13): a build argument whose default (0) is the plain image."""
        variants = self.pins["image_variants"]
        self.assertEqual(sorted(variants), ["magic-cairo", "magic-gl"])
        for name, variant in variants.items():
            with self.subTest(variant=name):
                for arg in variant["build_args"]:
                    self.assertEqual(self.args[arg], "0", arg)
        self.assertEqual(variants["magic-gl"]["build_args"], {"WITH_OPENGL": "1"})
        text = CONTAINERFILE.read_text(encoding="utf-8")
        self.assertIn("--with-opengl --with-cairo=no", text)
        self.assertIn("container/magic_gl_smoke.tcl", text)

    def test_cairo_variant_builds_the_same_commit_with_cairo(self):
        """magic-cairo (m10-plot-route design D19, Q-MISC-52 (a)): the main configure line plus Cairo."""
        variant = self.pins["image_variants"]["magic-cairo"]
        self.assertEqual(variant["build_args"], {"WITH_CAIRO": "1"})
        self.assertEqual(variant["display_type"], "XR")
        text = CONTAINERFILE.read_text(encoding="utf-8")
        self.assertIn("--with-opengl=no --with-cairo; \\", text)
        self.assertIn("container/magic_cairo_smoke.tcl", text)
        self.assertLess(text.index("ARG WITH_OPENGL=0"), text.index("ARG WITH_CAIRO=0"))

    def test_main_image_checks_the_plot_files(self):
        """Task 2.1 (Q-MISC-44): magicps.pro and the mos.7bit files are where Magic's SysLibPath finds them."""
        text = CONTAINERFILE.read_text(encoding="utf-8")
        for name in ("magicps.pro", "mos.7bit.dstyle", "mos.7bit.std.cmap", "mos.7bit.mraster_dstyle",
                     "mos.7bit.mraster.cmap"):
            self.assertIn(name, text)
        self.assertIn('test -s "${PREFIX}/lib/magic/sys/${f}"', text)

    def test_ngspice_is_the_engine_pin_with_the_patch_series(self):
        """Q-SIM-23 (a): the image builds the engine's pin (third_party/ngspice/pin.json, Q-SIM-7) with its
        patch series, from the build context's copy of third_party/ngspice (m9-lvs-simulation task 3.2)."""
        pin = self.pins["tools"]["ngspice"]
        engine = json.loads((NGSPICE_DIR / "pin.json").read_text(encoding="utf-8"))
        self.assertEqual(pin["commit"], engine["commit"])
        self.assertEqual(pin["tag"], engine["tag"])
        self.assertEqual(pin["repo"], engine["repo"])
        self.assertEqual(pin["archive_sha256"], engine["archive"]["sha256"])
        self.assertIn(pin["archive_url"], engine["archive"]["urls"])
        self.assertEqual(pin["version"], f"{engine['release']}-hefesto{engine['hefesto_level']}")
        self.assertEqual(self.args["NGSPICE_VERSION"], pin["version"])
        self.assertTrue((NGSPICE_DIR.parents[1] / pin["patch_series"]).is_file())
        sync = subprocess.run([sys.executable, "-I", str(NGSPICE_DIR / "tools" / "sync_image_copy.py"), "--check"],
                              capture_output=True, text=True, check=False)
        self.assertEqual(sync.returncode, 0, sync.stderr)
        text = CONTAINERFILE.read_text(encoding="utf-8")
        self.assertIn("COPY container/ngspice /opt/src/hefesto-ngspice", text)
        self.assertIn("build_ngspice.sh", text)

    def test_openvaf_binary_pinned_and_matches(self):
        """OpenVAF-reloaded: a build-time tool of the image, pinned by URL and SHA-256 (Q-SIM-23 (a))."""
        pin = self.pins["tools"]["openvaf"]
        self.assertEqual(pin["source"], "binary")
        self.assertRegex(pin["sha256"], SHA256)
        self.assertRegex(pin["upstream_commit"], FULL_SHA)
        self.assertIn(pin["version"], pin["url"])
        self.assertEqual(self.args["OPENVAF_URL"], pin["url"])
        self.assertEqual(self.args["OPENVAF_SHA256"], pin["sha256"])

    def test_every_tool_has_a_repo_and_version_for_provenance(self):
        for tool, pin in self.pins["tools"].items():
            with self.subTest(tool=tool):
                self.assertTrue(pin["repo"].startswith("https://"))
                if tool in INSTALLED:
                    self.assertNotIn("commit", pin)  # nothing is built from a commit
                    self.assertTrue(pin["version"])


if __name__ == "__main__":
    unittest.main()
