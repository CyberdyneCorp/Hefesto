# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Harness support of the M4 SCRIPT cases (m4-scripting design D16, tasks.md 12.1/12.4):
the ``[env]`` table, ``[reference] display = "xvfb"``, directory and sub-path external
inputs, and the replay's work-directory spelling.

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import shutil
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import replay
from goldenlib import external
from goldenlib.case import CaseError, load_case, substitute_env
from goldenlib.refrunners import XVFB_PREFIX, ContainerRunner, default_argv

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
CASE = REPO / "tests" / "golden" / "CORE" / "cases" / "CORE-HASH-01"


def case_with(tmp: Path, extra: str, reference: str = "") -> Path:
    root = tmp / "CORE" / "cases" / "CORE-HASH-01"
    shutil.copytree(CASE, root)
    shutil.rmtree(root / "expected", ignore_errors=True)
    text = (root / "case.toml").read_text(encoding="utf-8")
    if reference:
        text = text.replace("[reference]\n", "[reference]\n" + reference, 1)
    (root / "case.toml").write_text(text + extra, encoding="utf-8")
    return root


class EnvTableTest(unittest.TestCase):
    def test_env_is_parsed_and_substituted_per_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(case_with(Path(tmp), '\n[env]\nPDK_ROOT = "{work}/input/pdk"\nPDK = "sky130A"\n'))
            self.assertEqual(case.env, {"PDK_ROOT": "{work}/input/pdk", "PDK": "sky130A"})
            self.assertEqual(substitute_env(case.env, "/work"), {"PDK_ROOT": "/work/input/pdk", "PDK": "sky130A"})

    def test_container_receives_the_env_with_the_container_work_dir(self):
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(case_with(Path(tmp), '\n[env]\nPDK_ROOT = "{work}/input/pdk"\n'))
            argv = ContainerRunner("img").argv(case, Path(tmp))
            self.assertIn("PDK_ROOT=/work/input/pdk", argv)
            self.assertEqual(argv[argv.index("PDK_ROOT=/work/input/pdk") - 1], "-e")

    def test_bad_env_rejected(self):
        for extra in ('\n[env]\npdk_root = "x"\n', '\n[env]\nPDK_ROOT = 3\n', '\n[[env]]\nX = "1"\n'):
            with self.subTest(extra=extra), tempfile.TemporaryDirectory() as tmp:
                with self.assertRaises(CaseError):
                    load_case(case_with(Path(tmp), extra))

    def test_replay_runs_hefesto_with_the_env(self):
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(case_with(Path(tmp), '\n[env]\nPDK_ROOT = "{work}/input/pdk"\n'))
            work = Path(tmp) / "w"
            work.mkdir()
            with mock.patch.object(replay.subprocess, "run") as run:
                run.return_value = mock.Mock(stdout=b"", stderr=b"", returncode=0)
                replay.execute(Path("/bin/true"), case, work)
            self.assertEqual(run.call_args.kwargs["env"]["PDK_ROOT"], f"{work}/input/pdk")


class DisplayTest(unittest.TestCase):
    def test_xvfb_display_prefixes_the_tool(self):
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(case_with(Path(tmp), "", 'display = "xvfb"\n'))
            self.assertEqual(default_argv(case)[:len(XVFB_PREFIX)], XVFB_PREFIX)
            self.assertEqual(default_argv(case)[len(XVFB_PREFIX)], "magic-hash-harness")

    def test_display_cases_run_with_an_init_process(self):
        # Regression: xvfb-run as the container's PID 1 never returned (every GUI case hung).
        with tempfile.TemporaryDirectory() as tmp:
            gui = load_case(case_with(Path(tmp) / "a", "", 'display = "xvfb"\n'))
            plain = load_case(case_with(Path(tmp) / "b", ""))
            self.assertIn("--init", ContainerRunner("img").argv(gui, Path(tmp)))
            self.assertNotIn("--init", ContainerRunner("img").argv(plain, Path(tmp)))

    def test_display_cases_get_a_writable_fontconfig_cache(self):
        # Regression: the GUI captures' stderr began with Fontconfig's "No writable cache
        # directories" (the container user's HOME is /).
        with tempfile.TemporaryDirectory() as tmp:
            gui = load_case(case_with(Path(tmp) / "a", "", 'display = "xvfb"\n'))
            plain = load_case(case_with(Path(tmp) / "b", ""))
            self.assertIn("XDG_CACHE_HOME=/tmp/.cache", ContainerRunner("img").argv(gui, Path(tmp)))
            self.assertNotIn("XDG_CACHE_HOME=/tmp/.cache", ContainerRunner("img").argv(plain, Path(tmp)))

    def test_unknown_display_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(CaseError):
                load_case(case_with(Path(tmp), "", 'display = "wayland"\n'))


class TreeExternalTest(unittest.TestCase):
    def test_directory_input_copied_to_a_sub_path(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "root"
            magic_dir = external.pdk_set_root("sky130").relative_to(external.pdk_root())
            src = root / magic_dir / "sky130A" / "libs.tech" / "magic"
            src.mkdir(parents=True)
            (src / "sky130A.magicrc").write_text("rc", encoding="utf-8")
            (src / "sub").mkdir()
            (src / "sub" / "x.tcl").write_text("x", encoding="utf-8")
            dest = Path(tmp) / "input"
            dest.mkdir()
            ext = external.parse_external({"source": "pdk", "path": "sky130A/libs.tech/magic",
                                           "to": "pdk/sky130A/libs.tech/magic"}, 0)
            with mock.patch.dict("os.environ", {"HEFESTO_PDK_ROOT": str(root)}):
                external.stage([ext], dest)
            self.assertEqual((dest / "pdk/sky130A/libs.tech/magic/sky130A.magicrc").read_text(encoding="utf-8"), "rc")
            self.assertEqual((dest / "pdk/sky130A/libs.tech/magic/sub/x.tcl").read_text(encoding="utf-8"), "x")


class FilesReducerTest(unittest.TestCase):
    def test_every_matching_file_with_its_name(self):
        from goldenlib import derived
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            (work / "b.mag").write_bytes(b"magic\ntimestamp 5\n")
            (work / "a.mag").write_bytes(b"magic\n")
            (work / "x.txt").write_bytes(b"x")
            self.assertEqual(derived.build(work, "derived:files:*.mag"),
                             b"file a.mag\nmagic\nfile b.mag\nmagic\ntimestamp 5\n")

    def test_timestamp_mask_applies_to_each_file(self):
        from compare.masks import apply_masks
        data = b"file a.mag\ntimestamp 7\nfile b.mag\ntimestamp 9\n"
        self.assertEqual(apply_masks(data, ["mag-timestamp"]), b"file a.mag\ntimestamp 0\nfile b.mag\ntimestamp 0\n")


class WorkPathTest(unittest.TestCase):
    def test_replay_work_dir_is_written_as_the_container_work_dir(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            data = f"{work}/input/a.mag {work.resolve()}/b /workshop\n".encode()
            self.assertEqual(replay.with_container_work(data, work), b"/work/input/a.mag /work/b /workshop\n")

    def test_replay_cad_root_is_written_as_the_image_cad_root(self):
        # Regression: SCRIPT-GUI-08 lists the cell path, whose library directories are under
        # CAD_ROOT (/opt/ref/lib in the capture image, <work>/cadroot in a replay).
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            data = f"{work}/cadroot/magic/sys/current/ {work}/input/\n".encode()
            self.assertEqual(replay.with_container_work(data, work),
                             b"/opt/ref/lib/magic/sys/current/ /work/input/\n")

    def test_other_bytes_untouched(self):
        self.assertEqual(replay.with_container_work(b"/tmp/x\n", Path("/nonexistent/w")), b"/tmp/x\n")


if __name__ == "__main__":
    unittest.main()
