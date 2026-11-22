# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Tests of scripts/schem_inventory.py (openspec/changes/m8-schematic design D18, task 2.3) on fixture trees.

The fixtures are Hefesto-authored files in the layout of the three xschem data sets of fetch_pdk.py."""
from __future__ import annotations

import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import fetch_pdk  # noqa: E402
import schem_inventory as inv  # noqa: E402


def write(base: Path, rel: str, text: str) -> None:
    path = base / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


def variant_dir(root: Path, set_name: str) -> Path:
    data = fetch_pdk.DATA_SETS[set_name]
    return data.version_dir(root) / data.variant


SKY_TB = """v {xschem version=3.4.8RC file_version=1.3}
G {}
K {}
C {devices/code_shown.sym} 0 0 0 0 {name=s1 value=".lib cmos.lib tt"}
C {lab_pin.sym} 10 0 0 0 {name=p1 lab=A}
C {devices/iopin} 20 0 0 0 {name=p2 lab=B}
C {sky130_fd_pr/corner.sym} 30 0 0 0 {name=CORNER corner=tt}
C {sky130_fd_pr/missing.sym} 40 0 0 0 {name=x9}
C {sky130_tests/res.tcl(@value\\)} 50 0 0 0 {name=x10 value=2}
C {launcher.sym} 60 0 0 0 {name=h1 descr="run" tclcommand="xschem netlist; xschem simulate"}
T {tcleval(id=[to_eng [ngspice::get_node \\{i(@m.x1.m0\\\\[id])\\}]])} 0 0 0 0 0.2 0.2 {}
C {ngspice_get_expr.sym} 70 0 0 0 {name=r1 node="[ngspice::get_current v3]" descr="I="}
C {devices/ngspice_get_expr.sym} 80 0 0 0 {name=r2 node="[to_eng [ngspice::get_voltage s]]W"}
C {sky130_tests/probe.sym} 90 0 0 0 {name=r3 node="[not_a_device_text]"}
"""
SKY_CORNER = """v {xschem version=3.4.8RC file_version=1.3}
K {type=netlist_commands
format="tcleval(.lib $::SKYWATER_MODELS/sky130.lib.spice @corner )"
template="name=CORNER corner=tt"}
"""
SKY_NOT = """v {xschem version=3.4.8RC file_version=1.3}
K {type=subcircuit
format="@name @pinlist @symname"
template="name=x1 m=1
+ W_N=1"}
"""
SKY_PRIM = """v {xschem version=3.4.8RC file_version=1.3}
K {type=primitive
template="name=x1
W=1"}
"""
SKY_RC = """# comment: xschem should not count here
set SKYWATER_MODELS $env(PDK_ROOT)/models
proc sky130_menu {} {
  if {[regexp {[pn]mos} $::type]} {
    xschem set netlist_type spice
  } else {
    puts [file join $::env(HOME) x]
  }
}
"""
IHP_RES = """v {xschem version=3.4.8RC file_version=1.3}
K {type=resistor
format="tcleval(@spiceprefix@name @pinlist @model w=@w A=[ev7 \\\\\\{ @w * @l \\\\\\} ] lay=[cap_cmomi_C @w])"
template="name=R1 w=1e-6 l=2e-6"}
"""
IHP_SCALE = """v {xschem version=3.4.8RC file_version=1.3}
K {type=resistor
format="tcleval(@name @pinlist @value)"
template="name=R1 value=1"}
"""
IHP_TB = """v {xschem version=3.4.8RC file_version=1.3}
C {devices/code_shown.sym} 0 0 0 0 {name=MODEL
format="tcleval( @value )"
value=".lib $::SG13G2_MODELS/cornerMOSlv.lib mos_tt"}
C {rhigh.sym} 0 0 0 0 {name=R1 w=1e-6}
C {scale.sym} 0 0 0 0 {name=R2 value="[from_eng 1k]"}
C {sg13g2_inv_1.sym} 0 0 0 0 {name=x1}
"""
IHP_RC = """set PDK sg13g2
if { [file exists ${PDK_ROOT}/${PDK}/libs.tech/xschem/sg13g2_pr/cap.tcl] } {
    source ${PDK_ROOT}/${PDK}/libs.tech/xschem/sg13g2_pr/cap.tcl
}
"""
IHP_CAP_TCL = """proc cap_cmomi_C {w} { return [expr {$w * 2}] }
"""
GF_TB = """v {xschem version=3.4.8RC file_version=1.3}
C {symbols/nfet_03v3.sym} 0 0 0 0 {name=M1}
C {gnd.sym} 0 0 0 0 {name=l1 lab=0}
"""


def make_tree(root: Path) -> None:
    sky = variant_dir(root, "sky130-xschem") / "libs.tech/xschem"
    write(sky, "sky130_tests/tb.sch", SKY_TB)
    write(sky, "sky130_fd_pr/corner.sym", SKY_CORNER)
    write(sky, "sky130_tests/not.sym", SKY_NOT)
    write(sky, "sky130_tests/prim.sym", SKY_PRIM)
    write(sky, "sky130_tests/res.tcl", "proc res {v} { return {} }\n")
    write(sky, "sky130_tests/probe.sym", "v {}\nK {type=probe}\n")
    write(sky, "xschemrc", SKY_RC)
    ihp = variant_dir(root, "ihp-xschem")
    write(ihp, "libs.tech/xschem/sg13g2_pr/rhigh.sym", IHP_RES)
    write(ihp, "libs.tech/xschem/sg13g2_pr/scale.sym", IHP_SCALE)
    write(ihp, "libs.tech/xschem/sg13g2_tests/tb.sch", IHP_TB)
    write(ihp, "libs.tech/xschem/xschemrc", IHP_RC)
    write(ihp, "libs.tech/xschem/sg13g2_pr/cap.tcl", IHP_CAP_TCL)
    write(ihp, "libs.ref/sg13g2_stdcell/sym/xschem/sg13g2_inv_1.sym", "v {}\nK {type=subcircuit}\n")
    gf = variant_dir(root, "gf180mcu-xschem") / "libs.tech/xschem"
    write(gf, "tests/tb.sch", GF_TB)
    write(gf, "symbols/nfet_03v3.sym", "v {}\nK {type=nmos}\n")


class FixtureInventory(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.tmp.name)
        make_tree(cls.root)
        cls.result = inv.build(cls.root)
        cls.sky, cls.ihp, cls.gf = (cls.result["pdks"][p] for p in ("sky130", "ihp", "gf180mcu"))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_devices_references_in_both_forms(self):
        # devices/<name>, a bare name not in the PDK directories, and a reference without an extension.
        self.assertEqual(self.sky["devices"], ["code_shown.sym", "iopin.sym", "lab_pin.sym", "launcher.sym",
                                               "ngspice_get_expr.sym"])
        self.assertEqual(self.gf["devices"], ["gnd.sym"])

    def test_bare_reference_found_in_a_pdk_directory_is_not_a_device(self):
        # rhigh.sym is in sg13g2_pr and sg13g2_inv_1.sym in the standard-cell directory, both on IHP's path.
        self.assertEqual(self.ihp["devices"], ["code_shown.sym"])

    def test_unresolved_references_and_generators(self):
        self.assertEqual(self.sky["unresolved"], ["sky130_fd_pr/missing.sym"])
        self.assertEqual(self.sky["generators"], ["sky130_tests/res.tcl"])

    def test_tcleval_commands_and_variables(self):
        # [to_eng [ngspice::get_node {...\\[id]}]]: the escaped bracket is not a command.
        self.assertEqual(self.sky["tcleval_commands"],
                         ["ngspice::get_current", "ngspice::get_node", "ngspice::get_voltage", "to_eng"])

    def test_node_of_ngspice_get_expr_is_tcl(self):
        # Regression (task 2.3 gap): the node of ngspice_get_expr.sym, in both reference forms, is a tcleval
        # value that only the symbol's own text evaluates; the node of a PDK symbol is not scanned.
        commands = self.sky["tcleval_commands"]
        self.assertIn("ngspice::get_current", commands)
        self.assertIn("ngspice::get_voltage", commands)
        self.assertNotIn("not_a_device_text", commands)
        self.assertEqual(self.sky["tcleval_variables"], ["SKYWATER_MODELS"])

    def test_attributes_substituted_into_tcleval_bodies(self):
        # An instance's own format override, and a symbol format whose tcleval body uses @value.
        self.assertIn("SG13G2_MODELS", self.ihp["tcleval_variables"])
        self.assertEqual(self.ihp["tcleval_commands"], ["cap_cmomi_C", "ev7", "from_eng"])

    def test_rc_files_and_sourced_pdk_files(self):
        self.assertEqual(self.sky["rc_commands"], ["file", "if", "proc", "puts", "regexp", "set", "xschem"])
        self.assertEqual(self.sky["rc_variables"], ["env", "type"])
        self.assertEqual(self.sky["pdk_procs"], ["sky130_menu"])
        # cap.tcl is sourced by the IHP xschemrc, so its procedure is the PDK's own.
        self.assertEqual(self.ihp["pdk_procs"], ["cap_cmomi_C"])
        self.assertIn("expr", self.ihp["rc_commands"])

    def test_xschem_subcommands_from_rc_and_launchers(self):
        self.assertEqual(self.sky["xschem_subcommands"], ["netlist", "set", "simulate"])

    def test_multiline_templates_of_subcircuits_only(self):
        self.assertEqual(self.sky["multiline_templates"], ["libs.tech/xschem/sky130_tests/not.sym"])
        self.assertEqual(self.ihp["multiline_templates"], [])

    def test_data_sets_are_the_pins(self):
        self.assertEqual(self.result["data_sets"]["ihp-xschem"],
                         {"version": fetch_pdk.IHP_PIN, "tree_sha256": fetch_pdk.DATA_SETS["ihp-xschem"].tree_sha256})

    def test_two_runs_give_identical_text(self):
        self.assertEqual(inv.render(inv.build(self.root)), inv.render(self.result))
        self.assertTrue(inv.render(self.result).endswith("}\n"))
        self.assertEqual(json.loads(inv.render(self.result)), self.result)


class CommittedInventory(unittest.TestCase):
    """The committed tests/data/schem/inventory.json (task 2.3 landmarks); CI's schem-inventory job checks
    that it still matches the data sets."""

    def test_landmarks_of_task_2_3(self):
        committed = json.loads(inv.DEFAULT_OUTPUT.read_text())
        pdks = committed["pdks"]
        self.assertEqual(set(pdks), {"sky130", "gf180mcu", "ihp"})
        self.assertIn("code_shown.sym", pdks["ihp"]["devices"])
        self.assertIn("code_shown.sym", pdks["sky130"]["devices"])
        self.assertIn("ev7", pdks["ihp"]["tcleval_commands"])
        self.assertIn("SKYWATER_MODELS", pdks["sky130"]["tcleval_variables"])
        self.assertEqual(committed["data_sets"]["sky130-xschem"]["tree_sha256"],
                         fetch_pdk.DATA_SETS["sky130-xschem"].tree_sha256)

    def test_committed_text_is_canonical(self):
        text = inv.DEFAULT_OUTPUT.read_text()
        self.assertEqual(inv.render(json.loads(text)), text)


class Grammar(unittest.TestCase):
    def test_brace_string_escapes(self):
        self.assertEqual(inv.read_brace("{a\\}b\\\\c\r\nd} rest", 0), ("a}b\\c\nd", 12))

    def test_records_keep_brace_fields_across_lines(self):
        recs = inv.records("v {x}\nC {r.sym} 1 2 0 0 {name=R1\nvalue=1}\n# C {not.sym}\nN 0 0 1 1 {lab=a}\n")
        self.assertEqual(recs, [("v", ["x"]), ("C", ["r.sym", "name=R1\nvalue=1"]), ("#", []), ("N", ["lab=a"])])

    def test_property_strings(self):
        self.assertEqual(inv.properties('name=R1 value="a b;c" k = 2\nflag; q=\\"x\\"'),
                         [("name", "R1"), ("value", "a b;c"), ("k", "2"), ("flag", ""), ("q", '"x"')])

    def test_tcleval_bodies(self):
        self.assertEqual(inv.tcleval_bodies("tcleval(a [b] \\}) "), ["a [b] }"])
        self.assertEqual(inv.tcleval_bodies("@name @tcleval([f (x)]) tail"), ["[f (x)]"])

    def test_tcl_scanner_control_bodies(self):
        use = inv.TclUse()
        use.add_script(inv.TclScanner(
            "foreach x $l { lappend r [g $x] }; while {[h]} { k }; button .b -command {xschem redraw}\n"
            "set s {data [notacommand]}").script(0)[0])
        self.assertEqual(use.commands, {"foreach", "lappend", "g", "while", "h", "k", "button", "xschem", "set"})
        self.assertEqual(use.subcommands, {"redraw"})


class CommandLine(unittest.TestCase):
    def run_main(self, *args):
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            code = inv.main(list(args))
        return code, out.getvalue(), err.getvalue()

    def test_missing_data_set_exits_2(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, _, err = self.run_main("--root", tmp, "--output", str(Path(tmp) / "inv.json"))
        self.assertEqual(code, 2)
        self.assertIn("sky130-xschem", err)

    def test_check_passes_on_the_written_file_and_fails_on_a_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            root, output = Path(tmp), Path(tmp) / "inv.json"
            make_tree(root)
            pdks = {t.set_name: fetch_pdk.DATA_SETS[t.set_name] for t in inv.PDKS}
            digests = {name: fetch_pdk.set_digest(data, root) for name, data in pdks.items()}
            saved = dict(fetch_pdk.DATA_SETS)
            try:  # pin the fixture trees' digests so that missing_sets() accepts them
                for name, data in pdks.items():
                    fetch_pdk.DATA_SETS[name] = fetch_pdk.DataSet(**{**data.__dict__, "tree_sha256": digests[name]})
                self.assertEqual(self.run_main("--root", tmp, "--output", str(output))[0], 0)
                self.assertEqual(self.run_main("--root", tmp, "--output", str(output), "--check")[0], 0)
                write(variant_dir(root, "gf180mcu-xschem") / "libs.tech/xschem", "tests/tb.sch",
                      GF_TB + "C {devices/vdd.sym} 0 0 0 0 {name=l2}\n")
                fetch_pdk.DATA_SETS["gf180mcu-xschem"] = fetch_pdk.DataSet(**{
                    **pdks["gf180mcu-xschem"].__dict__,
                    "tree_sha256": fetch_pdk.set_digest(pdks["gf180mcu-xschem"], root)})
                code, out, err = self.run_main("--root", tmp, "--output", str(output), "--check")
            finally:
                fetch_pdk.DATA_SETS.clear()
                fetch_pdk.DATA_SETS.update(saved)
        self.assertEqual(code, 1)
        self.assertIn('+    "vdd.sym"', out)
        self.assertIn("differs", err)


if __name__ == "__main__":
    unittest.main()
