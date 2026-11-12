# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Unit tests for scripts/fetch_pdk.py (no network: hefesto-cli is faked).

Run with: python3 -m unittest discover -s scripts/tests
"""
from __future__ import annotations

import hashlib
import importlib.util
import io
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest import mock

SCRIPTS = Path(__file__).resolve().parent.parent


def load():
    spec = importlib.util.spec_from_file_location("fetch_pdk", SCRIPTS / "fetch_pdk.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules["fetch_pdk"] = module  # dataclasses resolve annotations through sys.modules
    spec.loader.exec_module(module)
    return module


fetch = load()


def make_set(**changes):
    """A small data set for tests, with a digest filled in by populate()."""
    fields = dict(name="toy", family="toyfam", variant="toyA", version="ab" * 20, libraries=("toy_lib",),
                  subset=("toyA/libs.tech/magic", "toyA/libs.ref/toy_lib/mag"), tree_sha256="0" * 64)
    fields.update(changes)
    return fetch.DataSet(**fields)


def populate(data, root: Path) -> None:
    """Writes the files a real install of ``data`` would leave."""
    base = data.version_dir(root)
    for i, sub in enumerate(data.subset):
        (base / sub / "nested").mkdir(parents=True, exist_ok=True)
        (base / sub / f"file{i}.txt").write_bytes(f"contents {i}\n".encode())
        (base / sub / "nested" / "deep.mag").write_bytes(b"magic\n")
    (base / data.variant / "SOURCES").write_text("not part of the subset\n")


def quiet(fn, *args):
    out, err = io.StringIO(), io.StringIO()
    with redirect_stdout(out), redirect_stderr(err):
        result = fn(*args)
    return result, out.getvalue(), err.getvalue()


class DataSetTable(unittest.TestCase):
    def test_sky130_pin_and_digest_of_m4_design_d15(self):
        sky = fetch.DATA_SETS["sky130"]
        self.assertEqual(sky.version, "5740c4a81c42f9fe4af5ef2ee258a99151053d28")
        self.assertEqual(sky.tree_sha256, "8aedf059155765efbb926f5098dc552834c2de243f094c3d25b4b5d94e4b4b71")
        # sky130_fd_pr holds the fixed-layout devices of the native PCell generators (M4 round 2).
        self.assertEqual(sky.subset, ("sky130A/libs.tech/magic", "sky130A/libs.ref/sky130_fd_sc_hd/mag",
                                      "sky130A/libs.ref/sky130_fd_pr/mag"))
        self.assertEqual(sky.libraries, ("sky130_fd_sc_hd", "sky130_fd_pr"))
        # The M1 names stay (tools/golden/goldenlib/external.py reads PDK_VERSION).
        self.assertEqual(fetch.PDK_VERSION, sky.version)
        self.assertEqual(fetch.PDK_TREE_SHA256, sky.tree_sha256)

    def test_ciel_release_stands_in_for_the_spec_pin(self):
        # Q-SCRIPT-9: the specs pin open_pdks c6df28c1; ciel has no release of it, 5740c4a8 is the stand-in.
        self.assertEqual(fetch.OPEN_PDKS_SPEC_PIN, "c6df28c1fccb05c286aa0a15e9c4c7dcd8502607")
        self.assertNotEqual(fetch.OPEN_PDKS_PIN, fetch.OPEN_PDKS_SPEC_PIN)
        self.assertIn("c6df28c1", fetch.__doc__)

    def test_gf180_and_ihp_releases_of_design_d15(self):
        gf = fetch.DATA_SETS["gf180mcu"]
        ihp = fetch.DATA_SETS["ihp"]
        self.assertEqual(gf.version, fetch.DATA_SETS["sky130"].version)  # same open_pdks snapshot
        self.assertEqual(gf.tree_sha256, "354c55418060a14c5891c3b5b8af338012097440167d3d69fac0032022919637")
        self.assertEqual(gf.subset, ("gf180mcuD/libs.tech/magic", "gf180mcuD/libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag",
                                     "gf180mcuD/libs.ref/gf180mcu_fd_pr/mag"))
        self.assertEqual(gf.libraries, ("gf180mcu_fd_sc_mcu7t5v0", "gf180mcu_fd_pr"))
        self.assertEqual(ihp.version, "5e6d592e4002946a4616f798c357f0f3c06cf3b6")
        self.assertEqual(ihp.tree_sha256, "e01cd6e2db86f2d0a189f006f93f7f3098a2839f8034013f36037ec7a1d4a1d3")
        self.assertEqual(ihp.subset, ("ihp-sg13g2/libs.tech/magic",))  # sg13g2_pr has no mag/ directory
        self.assertEqual(ihp.family, "ihp-sg13")  # Hefesto's family directory for ihp-sg13g2

    def test_cache_keys_of_the_m4_pins(self):
        self.assertEqual([data.cache_key() for data in fetch.DATA_SETS.values()],
                         ["pdk-sky130-5740c4a81c42-8aedf059155765ef",
                          "pdk-gf180mcu-5740c4a81c42-354c55418060a14c",
                          "pdk-ihp-5e6d592e4002-e01cd6e2db86f2d0"])

    def test_every_set_has_a_full_digest_and_a_subset_inside_its_variant(self):
        for data in fetch.DATA_SETS.values():
            with self.subTest(data.name):
                self.assertRegex(data.tree_sha256, r"^[0-9a-f]{64}$")
                self.assertTrue(all(sub.startswith(data.variant + "/") for sub in data.subset))

    def test_cache_keys_are_distinct_and_follow_the_pins(self):
        keys = {data.cache_key() for data in fetch.DATA_SETS.values()}
        self.assertEqual(len(keys), len(fetch.DATA_SETS))
        gf = fetch.DATA_SETS["gf180mcu"]
        self.assertEqual(gf.cache_key(), f"pdk-gf180mcu-{gf.version[:12]}-{gf.tree_sha256[:16]}")

    def test_install_script_names_variant_pin_library_and_each_subset(self):
        script = fetch.DATA_SETS["ihp"].install_script()
        self.assertTrue(script.startswith("pdk install ihp-sg13g2 5e6d592e4002946a4616f798c357f0f3c06cf3b6 "
                                          "-lib sg13g2_pr "))
        self.assertIn("-only ihp-sg13g2/libs.tech/magic/", script)
        gf_script = fetch.DATA_SETS["gf180mcu"].install_script()
        self.assertEqual(gf_script.count("-only "), 3)
        self.assertIn("-lib gf180mcu_fd_sc_mcu7t5v0 -lib gf180mcu_fd_pr ", gf_script)


class TreeDigest(unittest.TestCase):
    def test_digest_covers_only_the_subset_and_is_order_independent(self):
        data = make_set()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            populate(data, root)
            first = fetch.set_digest(data, root)
            (data.version_dir(root) / "toyA" / "SOURCES").write_text("changed\n")
            self.assertEqual(fetch.set_digest(data, root), first)
            reordered = make_set(subset=tuple(reversed(data.subset)))
            self.assertEqual(fetch.set_digest(reordered, root), first)

    def test_digest_matches_the_documented_record_format(self):
        data = make_set(subset=("toyA/libs.tech/magic",))
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            populate(data, root)
            records = sorted(
                f"{rel}\0{hashlib.sha256(body).hexdigest()}\n".encode()
                for rel, body in (("toyA/libs.tech/magic/file0.txt", b"contents 0\n"),
                                  ("toyA/libs.tech/magic/nested/deep.mag", b"magic\n")))
            self.assertEqual(fetch.set_digest(data, root), hashlib.sha256(b"".join(records)).hexdigest())

    def test_content_change_changes_the_digest(self):
        data = make_set()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            populate(data, root)
            before = fetch.set_digest(data, root)
            (data.version_dir(root) / data.subset[1] / "nested" / "deep.mag").write_bytes(b"other\n")
            self.assertNotEqual(fetch.set_digest(data, root), before)

    def test_missing_subset_directory_gives_none(self):
        data = make_set()
        with tempfile.TemporaryDirectory() as tmp:
            self.assertIsNone(fetch.set_digest(data, Path(tmp)))


class FetchSet(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name) / "pdks"
        self.cli = Path(self.tmp.name) / "hefesto-cli"
        self.cli.write_text("")
        scratch = Path(self.tmp.name) / "scratch"
        populate(make_set(), scratch)
        self.data = make_set(tree_sha256=fetch.set_digest(make_set(), scratch))

    def tearDown(self):
        self.tmp.cleanup()

    def fake_install(self, returncode=0, corrupt=False):
        def run(argv, check):
            self.assertFalse(check)
            self.assertEqual(argv[:3], [str(self.cli), "--no-log", f"--pdk-root={self.root}"])
            self.assertEqual(argv[3:], ["-c", self.data.install_script()])
            if returncode == 0:
                populate(self.data, self.root)
                if corrupt:
                    (self.data.version_dir(self.root) / self.data.subset[0] / "file0.txt").write_text("bad")
            return subprocess.CompletedProcess(argv, returncode)
        return mock.patch.object(fetch.subprocess, "run", side_effect=run)

    def test_fresh_install_verifies_and_a_second_run_is_a_no_op(self):
        with self.fake_install() as run:
            ok, out, _ = quiet(fetch.fetch_set, self.data, self.cli, self.root)
            self.assertTrue(ok)
            self.assertIn("verified", out)
            self.assertEqual(run.call_count, 1)
            ok, out, _ = quiet(fetch.fetch_set, self.data, self.cli, self.root)
            self.assertTrue(ok)
            self.assertIn("already present and verified", out)
            self.assertEqual(run.call_count, 1)

    def test_verified_set_needs_no_cli(self):
        populate(self.data, self.root)
        with mock.patch.object(fetch.subprocess, "run") as run:
            ok, _, _ = quiet(fetch.fetch_set, self.data, Path("/nonexistent/hefesto-cli"), self.root)
        self.assertTrue(ok)
        run.assert_not_called()

    def test_missing_cli_fails(self):
        ok, _, err = quiet(fetch.fetch_set, self.data, Path(self.tmp.name) / "absent", self.root)
        self.assertFalse(ok)
        self.assertIn("hefesto-cli not found", err)

    def test_failed_install_fails(self):
        with self.fake_install(returncode=3):
            ok, _, err = quiet(fetch.fetch_set, self.data, self.cli, self.root)
        self.assertFalse(ok)
        self.assertIn("install failed (exit 3)", err)

    def test_digest_mismatch_after_install_fails(self):
        with self.fake_install(corrupt=True):
            ok, _, err = quiet(fetch.fetch_set, self.data, self.cli, self.root)
        self.assertFalse(ok)
        self.assertIn("does not match the pin", err)


class CommandLine(unittest.TestCase):
    def test_default_steps_are_every_set_then_magic_ref(self):
        self.assertEqual(fetch.selected_steps([]), ["sky130", "gf180mcu", "ihp", "magic-ref"])

    def test_steps_keep_canonical_order_and_pdk_aliases_sky130(self):
        self.assertEqual(fetch.selected_steps(["magic-ref", "ihp", "pdk", "sky130"]), ["sky130", "ihp", "magic-ref"])

    def test_unknown_step_is_a_usage_error(self):
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as raised:
            fetch.parse_args(["gf180mcuD"])
        self.assertEqual(raised.exception.code, 2)

    def test_print_digest_takes_a_set_and_defaults_to_sky130(self):
        self.assertEqual(fetch.parse_args(["--print-digest"]).print_digest, "sky130")
        self.assertEqual(fetch.parse_args(["--print-digest", "ihp"]).print_digest, "ihp")
        with tempfile.TemporaryDirectory() as tmp:
            code, out, _ = quiet(fetch.main, ["--root", tmp, "--print-digest", "gf180mcu"])
        self.assertEqual(code, 0)
        self.assertEqual(out.strip(), "subset not installed")

    def test_print_digest_of_an_installed_set(self):
        with tempfile.TemporaryDirectory() as tmp:
            data = fetch.DATA_SETS["ihp"]
            (data.version_dir(Path(tmp)) / data.subset[0]).mkdir(parents=True)
            (data.version_dir(Path(tmp)) / data.subset[0] / "x.tech").write_text("tech\n")
            code, out, _ = quiet(fetch.main, ["--root", tmp, "--print-digest", "ihp"])
            expected = fetch.tree_digest(data.version_dir(Path(tmp).resolve()), data.subset)
        self.assertEqual(code, 0)
        self.assertRegex(out.strip(), r"^[0-9a-f]{64}$")
        self.assertEqual(out.strip(), expected)

    def test_cache_info_lists_a_key_and_path_per_set_and_magic_ref(self):
        code, out, _ = quiet(fetch.main, ["--cache-info"])
        self.assertEqual(code, 0)
        info = dict(line.split("=", 1) for line in out.splitlines())
        for data in fetch.DATA_SETS.values():
            self.assertEqual(info[f"{data.name}_key"], data.cache_key())
            self.assertEqual(info[f"{data.name}_path"],
                             f".cache/pdks/ciel/{data.family}/versions/{data.version}")
        self.assertEqual(info["magic_ref_key"], f"magic-ref-{fetch.MAGIC_COMMIT[:12]}-scmos-graphics-tcltk")
        self.assertEqual(info["magic_ref_path"], "build/ref/magic")

    def test_run_steps_continues_after_a_failure_and_reports_it(self):
        calls = []

        def fake_fetch_set(data, cli, root):
            calls.append(data.name)
            return data.name != "sky130"

        with mock.patch.object(fetch, "fetch_set", side_effect=fake_fetch_set):
            self.assertFalse(fetch.run_steps(["sky130", "ihp"], Path("cli"), Path("root")))
        self.assertEqual(calls, ["sky130", "ihp"])


if __name__ == "__main__":
    unittest.main()
