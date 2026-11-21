# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""M7 capture modes: outputs collected by name pattern, a fresh session per case, the external-command
case mode, and the image's extcheck build (m7-extraction design D15, D21; tasks 2.2 and 2.3).

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import contextlib
import io
import json
import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import capture
import replay
from authoring import m7_capture
from goldenlib import collect, external_command
from goldenlib.case import CaseError, load_case
from goldenlib.refrunners import ContainerRunner, RunResult

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
CONTAINER = GOLDEN / "container"

M7_COLLECT_TOML = "".join(
    f'\n[[collect]]\npattern = "{c.pattern}"\ncomparator = "{c.comparator}"\nmasks = {json.dumps(list(c.masks))}\n'
    for c in m7_capture.M7_COLLECT)

CASE_HEAD = """schema = 1
id = "{id}"
area = "NETLIST"
capability = "netlist-spice-options"
requirements = ["Command options"]
tool = "magic"
description = "test case"
"""

SCRIPT_CASE = CASE_HEAD + """
[reference]
script = "input/commands.tcl"

[hefesto]
args = ["{{input}}/commands.tcl"]

[[outputs]]
name = "stdout.txt"
source = "stdout"
comparator = "magic-console"
""" + M7_COLLECT_TOML

EXTERNAL_CASE = CASE_HEAD + """
[reference]
mode = "external-command"

[[outputs]]
name = "stdout.txt"
source = "stdout"
comparator = "bytes"
masks = ["extcheck-memory"]

[[outputs]]
name = "stderr.txt"
source = "stderr"
comparator = "bytes"
masks = ["prog-name"]

[[outputs]]
name = "exitcode.txt"
source = "exitcode"
comparator = "bytes"
"""

COMMAND_TOML = """schema = 1
role = "extcheck"
args = {args}
reads = {reads}
"""

# What the fake reference run writes: the M7 outputs of design D21, next to the .mag (input/) and in
# the work directory, plus a file no pattern names.
M7_FILES = {
    "input/top.ext": b"timestamp 1\ncap \"b\" \"a\" 3\n",
    "input/leaf.ext": b"timestamp 2\n",
    "top.spice": b"* netlist\nC0 b a 1f\nC1 c a 2f\n",
    "top.sim": b"| units: 100\nC b a 1.0\n",
    "top.al": b"= a b\n",
    "top.nodes": b"a 0 0 m1\n",
    "top.res.ext": b'resist "a" "b" 1\nresist "a" "c" 2\n',
    "top.res.lump": b"R a 10\n",
    "top.fh": b"* fasthenry\n",
    "crash.log": b"not an output\n",
}


def write_case(root: Path, case_id: str, toml: str, files: dict[str, str | bytes]) -> Path:
    case_dir = root / "NETLIST" / "cases" / case_id
    (case_dir / "input").mkdir(parents=True)
    (case_dir / "case.toml").write_text(toml.format(id=case_id), encoding="utf-8")
    for name, content in files.items():
        data = content.encode() if isinstance(content, str) else content
        (case_dir / "input" / name).write_bytes(data)
    return case_dir


class FakeRunner:
    """Stands in for the container: writes ``files`` into the work directory, records each run."""

    name = "fake"

    def __init__(self, files: dict[str, bytes], stdout: bytes = b"", stderr: bytes = b"", returncode: int = 0):
        self.files, self.stdout, self.stderr, self.returncode = files, stdout, stderr, returncode
        self.works: list[Path] = []

    def run(self, case, work: Path) -> RunResult:
        self.works.append(work)
        for name, data in self.files.items():
            (work / name).parent.mkdir(parents=True, exist_ok=True)
            (work / name).write_bytes(data)
        return RunResult(self.stdout, self.stderr, self.returncode, ["fake"], {})


def quiet(fn, *args, **kwargs):
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        return fn(*args, **kwargs)


def script_file(tmp: Path, name: str, body: str) -> Path:
    """An executable Python script ``name`` in ``tmp``."""
    path = tmp / name
    path.write_text(f'#!{sys.executable}\n{body}', encoding="utf-8")
    path.chmod(0o755)
    return path


class CollectTest(unittest.TestCase):
    def test_every_m7_pattern_is_collected_with_its_path(self):
        """[req:Golden compatibility suite] *.ext, *.spice, *.sim, *.al, *.nodes, *.res.ext, *.res.lump,
        *.fh and stdout; inputs and unmatched files are not collected."""
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(write_case(Path(tmp), "NL-T-01", SCRIPT_CASE,
                                        {"commands.tcl": "extract all\n", "top.mag": "magic\n"}))
            runner = FakeRunner(M7_FILES, stdout=b"Extracting top\n")
            _, outputs = capture.run_reference(case, runner)
        collected = sorted(name for name in outputs if name.startswith(collect.PREFIX))
        self.assertEqual(collected, sorted(collect.PREFIX + n for n in M7_FILES if n != "crash.log"))
        self.assertEqual(outputs["stdout.txt"], b"Extracting top\n")
        self.assertNotIn("collected/input/top.mag", outputs)

    def test_res_ext_goes_to_its_own_pattern_not_to_ext(self):
        entry = collect.match(m7_capture.M7_COLLECT, "top.res.ext")
        self.assertEqual((entry.pattern, entry.comparator), ("*.res.ext", "res-ext"))
        self.assertEqual(collect.match(m7_capture.M7_COLLECT, "top.ext").comparator, "ext-cap")
        self.assertEqual(collect.match(m7_capture.M7_COLLECT, "top.spice").comparator, "spice-cap")
        self.assertEqual(collect.match(m7_capture.M7_COLLECT, "top.sim").comparator, "sim-cap")

    def test_an_input_left_unchanged_is_not_collected_but_a_rewritten_one_is(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            (work / "input").mkdir()
            (work / "input" / "kept.ext").write_bytes(b"hand-written\n")
            (work / "input" / "redone.ext").write_bytes(b"old\n")
            before = collect.snapshot(work)
            (work / "input" / "redone.ext").write_bytes(b"new\n")
            got = collect.gather(work, before, m7_capture.M7_COLLECT)
        self.assertEqual(list(got), ["collected/input/redone.ext"])

    def test_capture_writes_collected_files_and_verify_checks_the_set(self):
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(write_case(Path(tmp), "NL-T-02", SCRIPT_CASE, {"commands.tcl": "x\n"}))
            result, outputs = capture.run_reference(case, FakeRunner(M7_FILES))
            capture.write_expected(case, {"outputs": {}}, outputs)
            self.assertEqual((case.expected_dir / "collected" / "input" / "top.ext").read_bytes(),
                             M7_FILES["input/top.ext"])
            self.assertEqual(capture.verify_expected(case, outputs), [])
            reordered = dict(outputs)
            reordered["collected/top.spice"] = b"* netlist\nC0 a c 2f\nC1 a b 1f\n"  # Q-NETLIST-1 order
            self.assertEqual(capture.verify_expected(case, reordered), [])
            missing = {k: v for k, v in outputs.items() if k != "collected/top.fh"}
            self.assertIn("missing collected/top.fh", " ".join(capture.verify_expected(case, missing)))
            extra = {**outputs, "collected/other.ext": b"x\n"}
            self.assertIn("extra collected/other.ext", " ".join(capture.verify_expected(case, extra)))
            changed = {**outputs, "collected/top.res.ext": b'resist "a" "b" 9\nresist "a" "c" 2\n'}
            self.assertTrue(capture.verify_expected(case, changed))

    def test_bad_collect_entries_are_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad_entries = ('pattern = "a/*.ext"\ncomparator = "bytes"', 'pattern = "*.ext"\ncomparator = "nope"')
            for index, bad in enumerate(bad_entries):
                toml = SCRIPT_CASE + f"\n[[collect]]\n{bad}\n"
                case_dir = write_case(Path(tmp), f"NL-T-9{index}", toml, {"commands.tcl": ""})
                with self.assertRaises(CaseError):
                    load_case(case_dir, capture.COMPARATORS)


class FreshSessionTest(unittest.TestCase):
    def test_each_case_is_its_own_run_in_its_own_work_directory(self):
        """Q-NETLIST-16, Q-NETLIST-46 (design D21): nothing carries from one case to the next."""
        with tempfile.TemporaryDirectory() as tmp:
            cases = [load_case(write_case(Path(tmp), f"NL-T-1{i}", SCRIPT_CASE, {"commands.tcl": ""}))
                     for i in range(2)]
            runner = FakeRunner({"top.spice": b"x\n"})
            for case in cases:
                capture.run_reference(case, runner)
        self.assertEqual(len(runner.works), 2)
        self.assertNotEqual(runner.works[0], runner.works[1])

    def test_container_runs_are_separate_containers(self):
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(write_case(Path(tmp), "NL-T-20", SCRIPT_CASE, {"commands.tcl": ""}))
            argv = ContainerRunner("img").argv(case, Path(tmp))
        self.assertEqual(argv[:4], ["docker", "run", "--rm", "-i"])


class ExternalCommandTest(unittest.TestCase):
    def external_case(self, tmp: Path, case_id: str = "NL-T-30", args=(), reads=(), files=None) -> Path:
        files = dict(files or {})
        files["command.toml"] = COMMAND_TOML.format(args=json.dumps(list(args)), reads=json.dumps(list(reads)))
        return write_case(tmp, case_id, EXTERNAL_CASE, files)

    def test_command_toml_maps_the_role_per_side(self):
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(self.external_case(Path(tmp), args=["-c", "0", "inv"], reads=["inv.ext"],
                                                files={"inv.ext": "timestamp 0\n"}))
            argv = ContainerRunner("img").argv(case, Path(tmp))
        self.assertEqual(case.command.program.capture, "extcheck")
        self.assertEqual(case.command.program.replay, "hefesto-extcheck")
        self.assertEqual(argv[argv.index("-w") + 1], "/work/input")
        self.assertEqual(argv[-7:-4], ["sh", "-c", external_command.GUARD])
        self.assertEqual(argv[-4:], ["extcheck", "-c", "0", "inv"])

    def test_bad_command_files_are_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(CaseError):
                load_case(self.external_case(Path(tmp), "NL-T-31", reads=["missing.ext"]))
            case_dir = self.external_case(Path(tmp), "NL-T-32")
            (case_dir / "input" / "command.toml").write_text('schema = 1\nrole = "magic"\n', encoding="utf-8")
            with self.assertRaises(CaseError):
                load_case(case_dir)

    def test_nonzero_exit_status_is_captured_not_a_failure(self):
        """The netlist-extcheck "Missing root" scenario: exit status 1 and the usage text."""
        usage = b"Standard arguments: [-R] [-C] [-r rthresh] [-c cthresh] [-v]\n"
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(self.external_case(Path(tmp)))
            _, outputs = capture.run_reference(case, FakeRunner({}, stderr=usage, returncode=1))
        self.assertEqual(outputs["exitcode.txt"], b"1\n")
        self.assertEqual(outputs["stderr.txt"], usage)

    def test_a_program_missing_from_the_image_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(self.external_case(Path(tmp)))
            runner = ContainerRunner("img")
            missing = mock.Mock(returncode=127, stdout=b"",
                                stderr=f"{external_command.MISSING_MARKER} extcheck\n".encode())
            with mock.patch.object(runner, "_image_facts", return_value={}), \
                    mock.patch("goldenlib.refrunners._run", return_value=missing):
                with self.assertRaises(capture.RunnerError):
                    runner.run(case, Path(tmp))

    def test_replay_runs_hefesto_extcheck_and_compares_the_exit_status(self):
        usage = "Standard arguments: [-R] [-C] [-r rthresh] [-c cthresh] [-v]\n"
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            case_dir = self.external_case(tmp / "golden")
            case = load_case(case_dir)
            capture.write_expected(case, {"reference_captured": True},
                                   {"stdout.txt": b"", "stderr.txt": usage.encode(), "exitcode.txt": b"1\n"})
            bindir = tmp / "bin"
            (bindir / "extcheck").mkdir(parents=True)
            cli = script_file(bindir, "hefesto-cli", "raise SystemExit(3)\n")
            script_file(bindir / "extcheck", "hefesto-extcheck",
                        f"import sys\nsys.stderr.write({usage!r})\nsys.exit(1)\n")
            report = replay.run_one(cli, case_dir)
            self.assertEqual(report.status, "pass", report)
            script_file(bindir / "extcheck", "hefesto-extcheck", "import sys\nsys.exit(0)\n")
            failed = replay.run_one(cli, case_dir)
            self.assertEqual(failed.status, "fail")
            self.assertIn("exitcode.txt", [o.name for o in failed.outputs if o.status == "fail"])

    def multi_run_case(self, tmp: Path, case_id: str = "NL-T-40") -> Path:
        outputs = "".join(f'\n[[outputs]]\nname = "{n}.{k}"\nsource = "{n}.{k}"\ncomparator = "bytes"\n'
                          for n in ("default", "usage") for k in external_command.RUN_OUTPUTS)
        command = ('schema = 1\nrole = "extcheck"\ncwd = "."\nreads = ["input/top.ext"]\n'
                   '\n[[run]]\nname = "default"\nargs = ["-p", "input", "top"]\n'
                   '\n[[run]]\nname = "usage"\nargs = []\n')
        toml = CASE_HEAD + '\n[reference]\nmode = "external-command"\n' + outputs
        return write_case(tmp, case_id, toml, {"command.toml": command, "top.ext": "timestamp 0\n"})

    def test_run_tables_run_the_program_once_each(self):
        """NL-CHK-01's shape: several [[run]] tables, each writing <name>.stdout/.stderr/.exit."""
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(self.multi_run_case(Path(tmp)))
            argv = ContainerRunner("img").argv(case, Path(tmp))
        self.assertEqual([r.name for r in case.command.runs], ["default", "usage"])
        self.assertEqual(argv[argv.index("-w") + 1], "/work")
        script = argv[-2]
        self.assertTrue(script.startswith(external_command.GUARD_CHECK))
        self.assertIn('"$0" -p input top >default.stdout 2>default.stderr; echo $? >default.exit', script)
        self.assertIn('"$0" >usage.stdout 2>usage.stderr; echo $? >usage.exit', script)
        self.assertEqual(argv[-1], "extcheck")

    def test_run_tables_in_a_subdirectory_write_to_the_work_directory(self):
        command = external_command.Command("extcheck", (), (), "input",
                                           (external_command.Run("a", ("x y",)),))
        self.assertEqual(external_command.multi_run_script(command),
                         "\"$0\" 'x y' >../a.stdout 2>../a.stderr; echo $? >../a.exit")

    def test_bad_run_tables_are_rejected(self):
        bad = ['schema = 1\nrole = "extcheck"\nargs = []\n[[run]]\nname = "a"\n',
               'schema = 1\nrole = "extcheck"\n[[run]]\nname = "a b"\n',
               'schema = 1\nrole = "extcheck"\n[[run]]\nname = "a"\n[[run]]\nname = "a"\n']
        with tempfile.TemporaryDirectory() as tmp:
            for i, text in enumerate(bad):
                case_dir = self.external_case(Path(tmp), f"NL-T-5{i}")
                (case_dir / "input" / "command.toml").write_text(text, encoding="utf-8")
                with self.subTest(text=text), self.assertRaises(CaseError):
                    load_case(case_dir)

    def test_replay_runs_each_run_table(self):
        usage = "Standard arguments: [-R]\n"
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            case_dir = self.multi_run_case(tmp / "golden")
            capture.write_expected(load_case(case_dir), {"reference_captured": True}, {
                "default.stdout": b"top ok\n", "default.stderr": b"", "default.exit": b"0\n",
                "usage.stdout": b"", "usage.stderr": usage.encode(), "usage.exit": b"1\n"})
            bindir = tmp / "bin"
            bindir.mkdir()
            cli = script_file(bindir, "hefesto-cli", "raise SystemExit(3)\n")
            script_file(bindir, "hefesto-extcheck",
                        "import sys\nif len(sys.argv) == 1:\n"
                        f"    sys.stderr.write({usage!r})\n    sys.exit(1)\nprint('top ok')\n")
            report = replay.run_one(cli, case_dir)
        self.assertEqual(report.status, "pass", report)

    def test_replay_skips_while_hefesto_extcheck_is_not_built(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            case_dir = self.external_case(tmp / "golden")
            capture.write_expected(load_case(case_dir), {}, {"stdout.txt": b"", "stderr.txt": b"",
                                                             "exitcode.txt": b"1\n"})
            cli = script_file(tmp, "hefesto-cli", "raise SystemExit(3)\n")
            report = replay.run_one(cli, case_dir)
        self.assertEqual(report.status, "skip")
        self.assertIn("hefesto-extcheck is not built", report.detail)


class ReplayCollectTest(unittest.TestCase):
    def test_replay_compares_collected_files_and_fails_on_a_missing_one(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            case_dir = write_case(tmp / "golden", "NL-T-40", SCRIPT_CASE, {"commands.tcl": "x\n", "top.mag": "m\n"})
            case = load_case(case_dir)
            expected = {"stdout.txt": b"", "collected/input/top.ext": b"timestamp 7\ncap \"b\" \"a\" 3\n",
                        "collected/top.spice": b"C0 b a 1f\nC1 c a 2f\n"}
            capture.write_expected(case, {"reference_captured": True}, expected)
            writes = {"input/top.ext": "timestamp 9\ncap \"a\" \"b\" 3\n", "top.spice": "C0 a c 2f\nC1 a b 1f\n"}
            body = ("import pathlib\n" + "".join(f"pathlib.Path({k!r}).write_text({v!r})\n" for k, v in writes.items()))
            cli = script_file(tmp, "hefesto-cli", body)
            report = replay.run_one(cli, case_dir)
            self.assertEqual(report.status, "pass", report)
            out_dir = tmp / "out"
            quiet(replay.run_one, cli, case_dir, replay.Options(output_dir=out_dir))
            self.assertEqual((out_dir / "NETLIST" / "NL-T-40" / "collected" / "input" / "top.ext").read_bytes(),
                             b"timestamp 0\ncap \"a\" \"b\" 3\n")  # the ext-timestamp mask applied
            del writes["top.spice"]
            body = ("import pathlib\n" + "".join(f"pathlib.Path({k!r}).write_text({v!r})\n" for k, v in writes.items()))
            cli = script_file(tmp, "hefesto-cli", body)
            failed = replay.run_one(cli, case_dir)
        self.assertEqual(failed.status, "fail")
        self.assertIn("collected/top.spice", [o.name for o in failed.outputs if o.status == "fail"])


class ContainerfileTest(unittest.TestCase):
    """Task 2.2: the image builds Magic's standalone extcheck and records glibc in its manifest."""

    def test_extcheck_step_tries_the_tcl_tree_then_a_tcl_free_configure(self):
        text = (CONTAINER / "build_extcheck.sh").read_text(encoding="utf-8")
        self.assertIn("make -C extcheck", text)
        self.assertIn("--without-tcl", text)
        tcl, notcl = text.index("try_tcl_tree"), text.index("try_tcl_free")
        self.assertLess(tcl, notcl)
        self.assertRegex(text, r"Standard arguments:")  # the usage smoke (EFargs.c:158-165)

    def test_containerfile_runs_the_extcheck_build_and_writes_the_manifest(self):
        text = (CONTAINER / "Containerfile").read_text(encoding="utf-8")
        self.assertIn("build_extcheck.sh", text)
        self.assertIn("write_manifest.sh", text)
        manifest = (CONTAINER / "write_manifest.sh").read_text(encoding="utf-8")
        self.assertIn("ldd --version", manifest)
        self.assertIn("/opt/ref/share/hefesto-golden/manifest.json", manifest)

    def test_no_new_build_arg(self):
        """test_pins.py checks every ARG against pins.toml; the extcheck build reuses MAGIC_COMMIT."""
        args = re.findall(r"^ARG (\w+)", (CONTAINER / "Containerfile").read_text(encoding="utf-8"), re.MULTILINE)
        self.assertFalse([a for a in args if "EXTCHECK" in a])


if __name__ == "__main__":
    unittest.main()
