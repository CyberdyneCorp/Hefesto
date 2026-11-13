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
        self.assertEqual([fetch.DATA_SETS[name].cache_key() for name in ("sky130", "gf180mcu", "ihp")],
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


class GdsDataSets(unittest.TestCase):
    """The library GDS sets of openspec/changes/m5-mask-gds-cif design D19 (task 1.2)."""

    GDS_SETS = ("sky130-gds", "gf180mcu-gds", "ihp-gds", "sky130-gds-sc", "gf180mcu-gds-sc", "ihp-gds-sc")

    def test_gds_sets_use_the_m4_pins_and_their_own_pdk_root(self):
        for name in self.GDS_SETS:
            with self.subTest(name):
                data = fetch.DATA_SETS[name]
                tech = fetch.DATA_SETS[name.split("-")[0]]
                self.assertEqual((data.family, data.variant, data.version), (tech.family, tech.variant, tech.version))
                self.assertEqual(data.pdk_dir, "gds")
                self.assertEqual(data.version_dir(Path("/r")),
                                 Path("/r/gds/ciel") / tech.family / "versions" / tech.version)
                self.assertNotEqual(data.version_dir(Path("/r")), tech.version_dir(Path("/r")))

    def test_each_subset_is_the_gds_directory_of_each_library(self):
        for name in self.GDS_SETS:
            with self.subTest(name):
                data = fetch.DATA_SETS[name]
                self.assertEqual(data.subset, tuple(f"{data.variant}/libs.ref/{lib}/gds" for lib in data.libraries))

    def test_standard_cell_sets_hold_one_library_of_the_full_set(self):
        expected = {"sky130-gds-sc": "sky130_fd_sc_hd", "gf180mcu-gds-sc": "gf180mcu_fd_sc_mcu7t5v0",
                    "ihp-gds-sc": "sg13g2_stdcell"}
        for name, lib in expected.items():
            with self.subTest(name):
                self.assertEqual(fetch.DATA_SETS[name].libraries, (lib,))
                self.assertIn(lib, fetch.DATA_SETS[name.removesuffix("-sc")].libraries)

    def test_full_sets_list_the_libraries_of_the_releases(self):
        self.assertEqual(fetch.DATA_SETS["ihp-gds"].libraries,
                         ("sg13g2_io", "sg13g2_pr", "sg13g2_sram", "sg13g2_stdcell"))
        self.assertIn("sky130_sram_macros", fetch.DATA_SETS["sky130-gds"].libraries)
        self.assertIn("gf180mcu_fd_ip_sram", fetch.DATA_SETS["gf180mcu-gds"].libraries)

    def test_sky130_installs_one_library_at_a_time(self):
        sky = fetch.DATA_SETS["sky130-gds"]
        commands = sky.install_commands()
        self.assertTrue(sky.per_library)
        self.assertEqual(len(commands), len(sky.libraries))
        for lib, command in zip(sky.libraries, commands):
            self.assertIn(f" -lib {lib} -accept-license ", command)
            self.assertEqual(command.count("-only "), len(sky.subset))
        self.assertEqual(sky.install_script(), "; ".join(commands))

    def test_gf180_and_ihp_install_in_one_command(self):
        for name in ("gf180mcu-gds", "ihp-gds"):
            with self.subTest(name):
                data = fetch.DATA_SETS[name]
                self.assertFalse(data.per_library)  # common is large: download it once
                self.assertEqual(len(data.install_commands()), 1)
                self.assertEqual(data.install_script().count("-lib "), len(data.libraries))

    def test_m4_sets_keep_their_root_and_single_install(self):
        for name in ("sky130", "gf180mcu", "ihp"):
            with self.subTest(name):
                data = fetch.DATA_SETS[name]
                self.assertEqual(data.pdk_dir, "")
                self.assertEqual(data.pdk_root(Path("/r")), Path("/r"))
                self.assertEqual(len(data.install_commands()), 1)

    def test_gds_cache_keys_are_distinct_and_named_after_the_set(self):
        keys = [fetch.DATA_SETS[name].cache_key() for name in self.GDS_SETS]
        self.assertEqual(len(set(keys)), len(keys))
        for name, key in zip(self.GDS_SETS, keys):
            self.assertTrue(key.startswith(f"pdk-{name}-"), key)

    def test_cache_info_uses_underscored_output_names(self):
        info = dict(line.split("=", 1) for line in fetch.cache_info(fetch.REPO / ".cache" / "pdks"))
        self.assertEqual(info["sky130_gds_sc_key"], fetch.DATA_SETS["sky130-gds-sc"].cache_key())
        self.assertEqual(info["ihp_gds_path"], f".cache/pdks/gds/ciel/ihp-sg13/versions/{fetch.IHP_PIN}")

    def test_full_and_standard_cell_sets_share_the_version_directory(self):
        # The -sc set is a subset of the full set: installing either first leaves the other verifiable.
        for pdk in ("sky130", "gf180mcu", "ihp"):
            full, sc = fetch.DATA_SETS[f"{pdk}-gds"], fetch.DATA_SETS[f"{pdk}-gds-sc"]
            self.assertEqual(full.version_dir(Path("/r")), sc.version_dir(Path("/r")))
            self.assertTrue(set(sc.subset) <= set(full.subset))


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

    def test_gds_set_installs_under_its_pdk_root_and_a_second_run_is_a_no_op(self):
        shape = dict(pdk_dir="gds", per_library=True, libraries=("lib_a", "lib_b"),
                     subset=("toyA/libs.ref/lib_a/gds", "toyA/libs.ref/lib_b/gds"))
        scratch = Path(self.tmp.name) / "gds-scratch"
        populate(make_set(**shape), scratch)
        data = make_set(**shape, tree_sha256=fetch.set_digest(make_set(**shape), scratch))
        calls = []

        def run(argv, check):
            calls.append(argv)
            self.assertEqual(argv[2], f"--pdk-root={self.root / 'gds'}")
            self.assertEqual(argv[4], "pdk install toyA " + "ab" * 20 + " -lib lib_a -accept-license "
                             "-accept-unverified -unpack-factor 32 -only toyA/libs.ref/lib_a/gds/ "
                             "-only toyA/libs.ref/lib_b/gds/; pdk install toyA " + "ab" * 20 + " -lib lib_b "
                             "-accept-license -accept-unverified -unpack-factor 32 -only toyA/libs.ref/lib_a/gds/ "
                             "-only toyA/libs.ref/lib_b/gds/")
            populate(data, self.root)  # version_dir() adds the set's pdk_dir
            return subprocess.CompletedProcess(argv, 0)

        with mock.patch.object(fetch.subprocess, "run", side_effect=run):
            first, _, _ = quiet(fetch.fetch_set, data, self.cli, self.root)
            second, out, _ = quiet(fetch.fetch_set, data, self.cli, self.root)
        self.assertTrue(first and second)
        self.assertEqual(len(calls), 1)
        self.assertIn("already present and verified", out)
        self.assertTrue((self.root / "gds" / "ciel" / "toyfam" / "versions" / ("ab" * 20)).is_dir())

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
    def test_default_steps_are_the_tech_sets_then_magic_ref(self):
        # The GDS sets (several GB) are fetched only when named.
        self.assertEqual(fetch.selected_steps([]), ["sky130", "gf180mcu", "ihp", "magic-ref"])

    def test_gds_sets_are_steps_in_canonical_order(self):
        self.assertEqual(fetch.selected_steps(["magic-ref", "ihp-gds-sc", "sky130-gds"]),
                         ["sky130-gds", "ihp-gds-sc", "magic-ref"])
        self.assertEqual(fetch.parse_args(["--print-digest", "gf180mcu-gds"]).print_digest, "gf180mcu-gds")

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
            prefix = f"{data.pdk_dir}/" if data.pdk_dir else ""
            self.assertEqual(info[f"{data.output_name()}_key"], data.cache_key())
            self.assertEqual(info[f"{data.output_name()}_path"],
                             f".cache/pdks/{prefix}ciel/{data.family}/versions/{data.version}")
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
