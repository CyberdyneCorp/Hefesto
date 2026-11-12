# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The SCRIPT golden cases of tests/golden/SCRIPT/CASES.md (m4-scripting design D16).

The id, capability and requirements of each case come from its CASES.md row (the
requirement column, without the ``(Q-...)`` notes; a requirement of another capability
than the row's is kept as a note); this module adds the inputs and the Tcl body that
Magic and hefesto-cli both run (``tclcase.py``). Bodies probe each command with
``probe {cmd}`` (``[list <code> <result>]`` per line), the form of the CASES.md preamble.

* PDK cases start from the PDK's rc file with ``PDK_ROOT`` set (``--trust-pdk`` on the
  Hefesto side) and run ``random seed 1`` first. A row that names several PDKs runs them
  in one session: the first through ``-rcfile``, the others by resetting the technology
  (``tech load <tech> -noprompt``) and sourcing their rc file, because a case is one run.
* GUI cases (SCRIPT-GUI-*, SCRIPT-API-12, SCRIPT-BOX-04/08, SCRIPT-PCELL-13/14,
  SCRIPT-PDK-08/16/17) run Magic's Tk wrapper under Xvfb and compare what the script
  prints (widget names, menu entries, issued commands); Hefesto replays them through
  its view models (no display, design D16).
* SCRIPT-API-11 is the Q-SCRIPT-3 case: an unknown ``%`` escape makes Magic loop
  forever, so it is captured with a timeout and listed in deviations.toml.

Never touches expected/. Re-run ``m4_cases.py``, then ``capture.py --check``.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

from casegen import GOLDEN
from common import gzip_bytes, mag
from tclcase import TclCase, lines, p

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from goldenlib import external  # noqa: E402

CASES_MD = GOLDEN / "SCRIPT" / "CASES.md"
ROW = re.compile(r"^\| (SCRIPT-[A-Z]+-\d+) \| ([a-z-]+) \| (.*) \| (.*) \| (.*) \|$", re.MULTILINE)
QUALIFIER = re.compile(r"\s*\(Q-[A-Z]+-\d+\)$")
SPECS = GOLDEN.parents[1] / "openspec" / "specs"



def rows() -> dict[str, tuple[str, list[str], str]]:
    """``{id: (capability, requirements, input description)}`` of CASES.md."""
    out = {}
    for cid, cap, inp, _what, reqs in ROW.findall(CASES_MD.read_text(encoding="utf-8")):
        out[cid] = (cap, [QUALIFIER.sub("", r.strip()) for r in reqs.split(";")], inp)
    return out


def headings(capability: str) -> set[str]:
    text = (SPECS / capability / "spec.md").read_text(encoding="utf-8")
    return set(re.findall(r"^### Requirement: (.+?)\s*$", text, re.MULTILINE))


# --- shared inputs (scmos) -------------------------------------------------------------

def cell(body: str) -> str:
    return mag("scmos", body)


INV = cell("<< metal1 >>\nrect 0 0 10 40\n<< polysilicon >>\nrect 20 0 22 40\n<< labels >>\n"
           "rlabel metal1 0 0 0 0 0 Y\nport 1 nsew\nrlabel polysilicon 20 0 20 0 0 A\nport 2 nsew\n"
           "rlabel metal1 0 40 0 40 0 VDD\nport 3 nsew\nrlabel metal1 10 0 10 0 0 VSS\nport 4 nsew\n")
LEAF = cell("<< metal1 >>\nrect 0 0 4 4\n")


def top_with(*uses: str) -> str:
    """A scmos cell with the given ``use`` blocks (``use <cell> <id>`` + transform)."""
    body = "".join(f"use {u.split()[0]} {u.split()[1]}\ntimestamp 100\ntransform 1 0 {i * 50} 0 1 0\n"
                   f"box 0 0 10 40\n" for i, u in enumerate(uses))
    return cell(body)


HIER = {"top.mag": top_with("a a_0"), "a.mag": top_with("b b_0"), "b.mag": LEAF}

TOY_LIB = """\
# toy: a device library of the pcell-toolkit contract (Library and device procedure contract)
namespace eval toy {}
proc toy::nfet_defaults {} { return {w 1 l 0.15 nf 1} }
proc toy::nfet_convert {parameters} { return $parameters }
proc toy::nfet_dialog {parameters} {
    magic::add_entry w "Width (um)" $parameters
    magic::add_entry l "Length (um)" $parameters
    magic::add_entry nf "Fingers" $parameters
    magic::add_dependency toy::nfet_dep nfet w l
}
proc toy::nfet_dep {parameters {target {}}} { return $parameters }
proc toy::nfet_check {parameters} { return $parameters }
proc toy::nfet_draw {parameters} {
    set w [dict get $parameters w]
    set l [dict get $parameters l]
    set snap [snap]
    snap internal
    box 0 0 [expr {int($l * 100)}] [expr {int($w * 100)}]
    paint poly
    box 0 0 0 0
    label G c poly
    snap $snap
}
proc toy::bad_defaults {} { return {w 1} }
proc toy::bad_convert {parameters} { return $parameters }
proc toy::bad_check {parameters} { return $parameters }
proc toy::bad_draw {parameters} { error "bad layer" }
proc toy::fixed_defaults {} { return {nocell 1 nx 2 ny 3 pitchx 10 pitchy 12} }
proc toy::fixed_convert {parameters} { return $parameters }
proc toy::fixed_check {parameters} { return $parameters }
proc toy::fixed_draw {parameters} {
    set nx [dict get $parameters nx]
    set ny [dict get $parameters ny]
    set use [getcell fixedcap]
    box 0 0 0 0
    array $nx $ny
    return $use
}
"""
FIXEDCAP = cell("<< metal1 >>\nrect 0 0 8 10\n")
TOY_FILES = {"toy.tcl": TOY_LIB, "fixedcap.mag": FIXEDCAP}
TOY_START = "load top\nsource input/toy.tcl"

# The tech of SCRIPT-API-16: tiny.tech with an extract section whose substrate node is $SUB.
# The empty compose section is required: without it Magic fc5daa9e loads the tech (only noting
# "Section compose was missing") but crashes with SIGSEGV in DBCellWriteFile on `save`.
SUB_TECH = """\
tech
    format 35
    subtech
end

planes
    well,w
    active,a
    metal1,m1
end

types
    well    pwell,pw
    active  ndiffusion,ndiff
    active  polysilicon,poly
    active  ntransistor,nfet
    metal1  metal1,m1
    active  ndcontact,ndc
end

contact
    ndc ndiff metal1
end

styles
    styletype mos
end

compose
end

connect
    ndiff ndc
    metal1 ndc
end

cifoutput
end

cifinput
end

drc
end

extract
    style default
    lambda 100
    step 100
    sidehalo 0
    planeorder well 0
    planeorder active 1
    planeorder metal1 2
    substrate *pw,space well $SUB
    resist ndiff,ndc 1000
    resist metal1 50
end
"""


# --- bodies ---------------------------------------------------------------------------

def api_bodies() -> dict[str, dict]:
    return {
        "SCRIPT-API-01": dict(body=p("namespace eval magic {lsort [info commands ::magic::*]}", "info commands paint",
                                     "namespace origin paint")),
        "SCRIPT-API-02": dict(body=p("magic::bo values")),
        "SCRIPT-API-03": dict(files={"child.mag": LEAF}, body=lines(
            p("array set a {x 1}", "array get a"), "load top", "box 0 0 0 0", "getcell child", "array 1 4",
            "select top cell", "select cell child_0", p("array -list count"))),
        "SCRIPT-API-04": dict(files={"lib.v2/inv.mag": INV, "cells/inv2.mag.gz": gzip_bytes(
            INV.encode())}, body=p("load /nonexistent/libfoo.so", "load input/lib.v2/inv",
                                             "cellname list top", "load input/cells/inv2.mag.gz",
                                             "cellname list top", "info commands tcl_load")),
        "SCRIPT-API-05": dict(body=lines("load top", p("cellname list top", "units MICRONS", "units",
                                                       "tech layer *", "tech lock poly", "tech locked",
                                                       "cif scale out"))),
        "SCRIPT-API-06": dict(body=p("box frobnicate", "load nosuch -fail -silent", "magic::shownet")),
        "SCRIPT-API-07": dict(body=p("tag select", "magic::tag add select {puts A}", "magic::tag add select {puts B}",
                                     "tag select", "select top cell", "magic::tag select {}", "tag select")),
        "SCRIPT-API-08": dict(body=p('magic::tag box {puts "W=%W 1=%1 2=%2 r=%r"}', "box", "box values",
                                     "box 0 0 10 10")),
        "SCRIPT-API-09": dict(body=lines("load top", "box 0 0 10 10", p("magic::tag paint {error boom}", "paint m1",
                                                                         "select area", "what -list"))),
        "SCRIPT-API-10": dict(body=lines("load top", p("magic::tag cellname {set x 1}", "cellname list top",
                                                       "magic::tag cellname {set x %R}", "cellname list top"))),
        "SCRIPT-API-11": dict(timeout=20, files={"a b.mag": LEAF, "x[0].mag": LEAF}, body=lines(
            "load {a b}", 'magic::tag cellname {puts "r=%r"}', p("cellname list top"), "load {x[0]}",
            p("cellname list top"), 'magic::tag cellname {puts "pct=%% d=%d"}', p("cellname list top")),
            notes=["Q-SCRIPT-3: the %% / %d escape makes Magic loop (tclmagic.c:250-264, default case); "
                   "captured with a 20 s timeout, recorded in deviations.toml."]),
        "SCRIPT-API-12": dict(gui=True, body=p(".layout1.pane.top.magic box values", ".layout1.pane.top.magic bo values")),
        "SCRIPT-API-13": dict(body=lines("set VDD vdd!", p("cellname create a$abc", "cellname create b$VDD",
                                                           "cellname list allcells", "echo {x$abc y$VDD}"))),
        "SCRIPT-API-14": dict(stdin=True, prelude=False, body="box 0 0 1 1\nbox values\nquit -noprompt\n"),
        "SCRIPT-API-15": dict(files={"lib/inv.mag": INV}, body=lines(
            "load input/lib/inv", p("cellname list filepath inv"), "file mkdir out", "cd out",
            p("cellname list filepath inv", "save inv", "pwd"), "cd ..", p("file exists out/inv.mag",
                                                                            "file exists input/lib/inv.mag"))),
        "SCRIPT-API-16": dict(files={"subtech.tech": SUB_TECH}, outputs=["top.ext", "top2.ext"], prelude_tech=False,
                              body=lines("tech load input/subtech -noprompt", "snap internal", "load top",
                                         "box 0 0 20 20", "paint m1", "box 0 0 4 4", "paint ndc", "save top", "set SUB VSUBS",
                                         p("extract all"), "unset SUB", "load top2", "box 0 0 20 20",
                                         "paint m1", "box 0 0 4 4", "paint ndc", "save top2", p("extract all"))),
        "SCRIPT-API-17": dict(body=p("set PDKPATH /pdk/x", "addpath $PDKPATH/lib", "path search",
                                     "set PDKPATH /other", "addpath $PDKPATH/lib2", "path search")),
        "SCRIPT-API-18": dict(pdk="sky130A", rc=False, body=lines(
            "tech load input/pdk/sky130A/libs.tech/magic/sky130A.tech -noprompt", "scalegrid 1 2", "snap internal",
            "box 0 0 30 100", "grid 10 20",
            *(p(f"units {u}", "box values", f"units {u} print", "box values", f"units {u} noprint", "box values")
              for u in ("default", "internal", "lambda", "microns", "grid")))),
        "SCRIPT-API-19": dict(body=p("units", "units lambda print", "units", "set u [units]", "units internal",
                                     "units {*}$u", "units")),
        "SCRIPT-API-20": dict(files={"inv.mag": INV, "top.mag": top_with("inv x1", "inv x2")}, body=lines(
            "load top", "select cell x2", "array 2 1", "select clear", "select cell x1",
            p("box values", "box position", "box size", "box width", "box height", "box exists",
              "snap", "cellname list exists inv", "cellname list exists nand2", "cellname list self",
              "cellname list children top", "cellname list parents inv", "cellname list instances inv",
              "cellname list topcells", "cellname list allcells", "cellname list modified",
              "cellname list window", "cellname list flags inv", "cellname list property inv nosuch",
              "cellname list writeable inv", "cellname list filepath inv",
              "instance list celldef x1", "instance list exists x1", "instance list self",
              "instance list celldef nosuch", "instance list children x1", "instance list orientation",
              "instance list orientation -def", "instance list abutment", "what -list",
              "array -list count", "array -list pitch", "array -list position", "goto VDD", "goto nosuch",
              "port first", "port last", "property", "tech name", "tech version", "tech lambda",
              "extract list style", "extract listall style", "drc list style", "drc listall style",
              "drc status", "macro list", "windownames", "windowcaption", "view get", "view bbox",
              "path search", "path cell", "path sys"))),
        "SCRIPT-API-21": dict(files={"lab.mag": cell("<< metal1 >>\nrect 0 0 10 10\n<< labels >>\n"
                                                     "rlabel metal1 0 0 0 0 0 A\n")}, body=lines(
            "load lab", p("port first", "port last"), "box 0 0 0 0", "label P3 c m1", "port make 3",
            "box 2 2 2 2", "label P7 c m1", "port make 7", p("port 3 next", "port 7 next", "port first",
                                                                "port last"))),
        "SCRIPT-API-22": dict(files={"ver.tech": SUB_TECH.replace("    subtech\nend", "    vertech\nend\n\nversion\n"
                                                                    "    version 1.0 beta 2\n    description \"a test\"\nend", 1)},
                              prelude_tech=False, body=lines(
            "tech load input/ver -noprompt", p("tech version", "tech lambda", "drc list count",
                                               "drc list count total", "drc listall why"))),
        "SCRIPT-API-23": dict(body=lines("box 0 0 10 10", "set n0 [macro list]", p("*bypass box values"),
                                         "select top cell", p("*bypass select", "select"),
                                         "set f [open cmdlog.txt w]", "close $f", p("history info", "history"))),
        "SCRIPT-API-24": dict(body=lines(p("info commands unknown", "info commands tcl_unknown"),
                                         "proc tcl_foo {args} {return [list tcl_foo {*}$args]}", p("foo a b"))),
        "SCRIPT-API-25": dict(pdk="sky130A", rc=False, body=lines(
            p("info commands magic::spice2float"), "set PDKPATH input/pdk/sky130A",
            "source input/pdk/sky130A/libs.tech/magic/sky130A.tcl",
            p("magic::3digitpastdecimal 0.12345", "3digitpastdecimal 1"))),
    }


def _pdk_values() -> str:
    return 'puts [list $VDD $GND $SUB [tech name] [tech lambda] [snap] [path search] $PDKPATH ' \
           '[info exists PDKNAMESPACE] [array get Opts callback] [tech locked]]'


def _switch(variant: str) -> str:
    """Reset the technology and source another PDK's rc file in the same session."""
    return (f"tech load input/pdk/{variant}/libs.tech/magic/{variant}.tech -noprompt\n"
            f"source input/pdk/{variant}/libs.tech/magic/{variant}.magicrc\nrandom seed 1")


def _devices(variant: str) -> str:
    """The devices of a PDK's menus, read from its device script (the gencell lines)."""
    return (f"set f [open input/pdk/{variant}/libs.tech/magic/{variant}.tcl]\n"
            "set devices [regexp -all -inline {magic::gencell ([^\\s\"]+)} [read $f]]\nclose $f\n"
            "set devices [dict values $devices]")


GENERATE_ALL = """\
set i 0
foreach dev $devices {
    load top_[incr i]
    probe [list magic::gencell $dev X$i]
    probe [list cellname list children top_$i]
    foreach c [cellname list children top_$i] { probe [list cellname list property $c] ; probe [list save $c] }
}"""


def pdk_bodies() -> dict[str, dict]:
    sky_cell = ["libs.ref/sky130_fd_sc_hd/mag/sky130_fd_sc_hd__inv_1.mag"]
    return {
        "SCRIPT-PDK-01": dict(pdk="sky130A", also=["gf180mcuD", "ihp-sg13g2"], body=lines(
            _pdk_values(), _switch("gf180mcuD"), _pdk_values(), "unset env(PDK_ROOT)",
            _switch("ihp-sg13g2"), _pdk_values())),
        "SCRIPT-PDK-02": dict(files={"bad.magicrc": "puts line1\nputs line2\nfrobnicate\nputs line4\n"},
                              magic_args=["-rcfile", "input/bad.magicrc"], body=p("info exists x")),
        "SCRIPT-PDK-03": dict(script_args=["a.gds", "b.lef", "c.def", "d.cif", "e.tcl", "f.mag", "g", "g.mag.gz",
                                           "h.xyz", "-foo"],
                              files={"e.tcl": "puts sourced-e\n", "f.mag": LEAF, "g.mag": LEAF},
                              body=lines(p("set argv", "cellname list allcells"))),
        "SCRIPT-PDK-04": dict(prelude=False, magic_args=["--version"], body="# Magic exits at --version.",
                              notes=["One run: --version (Magic exits there); --commit and --prefix print "
                                     "build facts that differ by design and are unit-tested."]),
        "SCRIPT-PDK-05": dict(pdk="sky130A", rc=False, pdk_cells=sky_cell, body=lines(
            p("tech name"), "load input/pdk/sky130A/libs.ref/sky130_fd_sc_hd/mag/sky130_fd_sc_hd__inv_1",
            p("tech name", "cellname list filepath sky130_fd_sc_hd__inv_1"))),
        "SCRIPT-PDK-06": dict(pdk="sky130A", pdk_cells=sky_cell, outputs=["parent.mag"], body=lines(
            "load parent", "box 0 0 0 0", p("getcell sky130_fd_sc_hd__inv_1"), p("save parent"))),
        "SCRIPT-PDK-07": dict(pdk="sky130A", also=["gf180mcuD", "ihp-sg13g2"], fonts=["FreeSans"], body=lines(
            _devices("sky130A"), GENERATE_ALL, _switch("gf180mcuD"), _devices("gf180mcuD"),
            GENERATE_ALL.replace("top_", "gtop_"), _switch("ihp-sg13g2"), _devices("ihp-sg13g2"),
            GENERATE_ALL.replace("top_", "itop_"))),
        "SCRIPT-PDK-08": dict(pdk="sky130A", gui=True, body=lines(
            "openwrapper", "foreach w [lsort [windownames layout]] {",
            "    set f [winfo toplevel $w]",
            "    foreach m [lsort [info commands $f.*mbuttons.*]] { catch {puts [list $m [$m cget -text]]} }",
            "    foreach m {pdk1 pdk2} { catch {set mm $f.titlebar.mbuttons.$m.toolmenu; "
            "for {set i 0} {$i <= [$mm index end]} {incr i} { catch {puts [list $m $i [$mm entrycget $i -label]]} } } }",
            "}")),
        "SCRIPT-PDK-09": dict(pdk="sky130A", rc=False, also=["gf180mcuD", "ihp-sg13g2"], body=lines(
            "set inventory {}",
            "proc record {cmd op} { global inventory; set w [lindex $cmd 0]; set w [string trimleft $w :];"
            " if {[string match magic::* $w]} {set w [string range $w 7 end]}; "
            "if {[info commands ::magic::$w] ne {}} { dict incr inventory $w } }",
            "foreach c [info commands ::magic::*] { catch {trace add execution $c enter record} }",
            "source input/pdk/sky130A/libs.tech/magic/sky130A.magicrc", _devices("sky130A"), GENERATE_ALL,
            "puts [lsort -stride 2 $inventory]")),
        "SCRIPT-PDK-10": dict(pdk="sky130A", rc=False, body=lines(
            "set f [open input/pdk/sky130A/libs.tech/magic/sky130A.magicrc]",
            "set rc [string map {sky130A.tech missing.tech} [read $f]]", "close $f",
            "set f [open missing.magicrc w]", "puts -nonewline $f $rc", "close $f",
            "proc ::sourced? {} { puts [list device-script [llength [info procs ::sky130::*]]] }",
            p("tech name"), "puts before-rc", "flush stdout", "source missing.magicrc", "puts after-rc",
            "sourced?"),
            notes=["The rc file with its tech load pointing to a missing file is written by the script "
                   "(the PDK file is not committed); it is sourced, so Magic's quit in it ends the run."]),
        "SCRIPT-PDK-11": dict(pdk="sky130A", also=["ihp-sg13g2"], body=lines(
            p("magic::u2i 0.17", "magic::i2u 34", "magic::u2l 0.17", "magic::l2u 1", "magic::float2spice 4.2e-7",
              "magic::3digitpastdecimal 0.12345"), _switch("ihp-sg13g2"),
            p("magic::u2i 0.17", "magic::i2u 34", "magic::u2l 0.17", "magic::l2u 1", "magic::float2spice 4.2e-7",
              "magic::3digitpastdecimal 0.12345"))),
        "SCRIPT-PDK-12": dict(pdk="sky130A", body=p(*(f"magic::spice2float {v}" for v in
                                                    ("2k", "1.5", "0.42u", "10n", "1nf", "10pf", "3meg", "5a", "abc")))),
        "SCRIPT-PDK-13": dict(pdk="sky130A", body=lines(
            "load top", "snap lambda", "set boxstack {}",
            p("snap", "tech unlocked", "llength $boxstack"),
            p("magic::gencell sky130::sky130_fd_pr__cap_mim_m3_1 C1"),
            p("snap", "tech unlocked", "llength $boxstack"),
            "load empty", "box 0 0 0 0", p("label B c li", "select area label", "port make", "port first",
                                           "setlabel text", "setlabel layer", "port B index"))),
        "SCRIPT-PDK-14": dict(pdk="ihp-sg13g2", fonts=["FreeSans"], body=lines(
            "load top", p("magic::gencell sg13g2::npn13g2 Q1", "magic::gencell sg13g2::diodevdd_2kv D1",
                          "cellname list window", "cellname list self", "cellname list children top"),
            "foreach c [cellname list allcells] { probe [list cellname list property $c] }")),
        "SCRIPT-PDK-15": dict(pdk="ihp-sg13g2", body=lines(
            "load top", p("magic::gencell sg13g2::npn13g2 Q1"),
            "foreach c [lsort [cellname list children top]] { load $c; select top cell; "
            "probe {port first}; probe {port last}; probe {what -list} }")),
        "SCRIPT-PDK-16": dict(pdk="sky130A", gui=True, body=lines(
            "load top", p("magic::gencell sky130::sky130_fd_pr__npn_05v5_W1p00L1p00 Q1 nx 2 ny 3 deltax 1"),
            "select cell Q1", p("array -list count", "array -list pitch"),
            "magic::gencell {}", p("magic::gencell_getparams"),
            "catch {.params.buttons.apply invoke}", "select cell Q1", p("array -list count", "array -list pitch"))),
        "SCRIPT-PDK-17": dict(pdk="sky130A", gui=True, also=["gf180mcuD", "ihp-sg13g2"], body=lines(
            "proc menus {} { foreach m {file drc pdk1 pdk2} { set mm .layout1.titlebar.mbuttons.$m.toolmenu;"
            " if {[catch {$mm index end} n] || $n eq {none}} { puts [list $m none]; continue };"
            " for {set i 0} {$i <= $n} {incr i} { catch {puts [list $m $i [$mm entrycget $i -label]]} } } }",
            "menus", _switch("gf180mcuD"), "menus", _switch("ihp-sg13g2"), "menus")),
        "SCRIPT-PDK-18": dict(pdk="sky130A", rc=False, also=["ihp-sg13g2"],
                              files={"old/sky130A.magicrc": None, "old/ihp-sg13g2.magicrc": None},
                              body=lines("source input/old/sky130A.magicrc", _pdk_values(), _switch_old(),
                                         _pdk_values()),
                              notes=["Older rc smoke (design D15): the 0fe599b2 sky130A .magicrc and the ddb601a4 "
                                     "ihp-sg13g2.magicrc (Apache-2.0, THIRD_PARTY_NOTICES.md) against the new-pin tech "
                                     "files."]),
    }


def _switch_old() -> str:
    return ("tech load input/pdk/ihp-sg13g2/libs.tech/magic/ihp-sg13g2.tech -noprompt\n"
            "source input/old/ihp-sg13g2.magicrc\nrandom seed 1")


def pcell_bodies() -> dict[str, dict]:
    toy = dict(files=dict(TOY_FILES))
    return {
        "SCRIPT-PCELL-01": dict(body=p(*(f"magic::normalize_value {v}" for v in
                                         ("2", "2.0", "2e0", "0.1500", "1.23456", "-0.5", "0.0004", "10.5", "0x10",
                                          '" 3 "', "abc", "inf")))),
        "SCRIPT-PCELL-02": dict(body=p(*(f"magic::murmur3_32 {v}" for v in
                                         ("{}", "a", "ab", "abc", "abcd", "hello", "{0.15;1;1;}", "é;")))),
        "SCRIPT-PCELL-03": dict(body=lines(
            "expr {srand(1)}", "set out {}",
            "for {set i 0} {$i < 200} {incr i} {",
            "    set d [dict create w [expr {int(rand()*100)/10.0}] l [expr {int(rand()*50)/100.0}] "
            "nf [expr {1 + int(rand()*8)}] model nfet_[expr {int(rand()*3)}]]",
            "    puts [list $d [magic::get_gencell_hash $d]]", "}",
            p("magic::get_gencell_hash {w 1 note éè l 0.15}"))),
        "SCRIPT-PCELL-04": dict(**toy, outputs=["top.mag"], body=lines(
            TOY_START, p("box values", "magic::gencell toy::nfet M1 w 2", "box values",
                         "magic::gencell toy::nfet M2 -spice w 2.0", "box values",
                         "cellname list children top", "instance list celldef M1", "instance list celldef M2"),
            "foreach c [lsort [cellname list children top]] { probe [list cellname list property $c] }",
            "save top")),
        "SCRIPT-PCELL-05": dict(**toy, body=lines(
            TOY_START, p("magic::gencell toy::nfet M1 w 2", "magic::gencell toy::nfet M2 w 2", "allcells"),
            "select cell M1", "rotate 90", p("cellname list allcells"),
            p("magic::gencell toy::nfet M1 w 3", "cellname list allcells", "instance list orientation"),
            p("magic::gencell toy::nfet M2 w 3", "cellname list allcells"),
            "foreach c [lsort [cellname list allcells]] { probe [list cellname list parents $c] }")),
        "SCRIPT-PCELL-06": dict(**toy, body=lines(
            TOY_START, p("magic::gencell toy::nfet M1 w 2", "magic::gencell toy::nfet M2 w 2"),
            "foreach c [lsort [cellname list children top]] { load $c; probe {select top cell}; "
            "probe {llength [lindex [what -list] 1]} }")),
        "SCRIPT-PCELL-07": dict(**toy, body=lines(
            TOY_START, p("magic::gencell toy::nfet M1 w 2", "select cell M1", "magic::gencell M1",
                         "magic::gencell toy::nfet M1 w 3", "info exists Opts(suspend)"))),
        "SCRIPT-PCELL-08": dict(**toy, body=lines(
            TOY_START, p("magic::gencell toy::bad B1", "cellname list allcells", "instance list exists B1"))),
        "SCRIPT-PCELL-09": dict(**toy, body=lines(
            TOY_START, p("magic::gencell toy::nosuch x", "select clear", "magic::gencell {}",
                         "magic::gencell toy::nfet M1 w"))),
        "SCRIPT-PCELL-10": dict(**toy, body=lines(
            TOY_START, p("magic::gencell_makecell toy::nfet w 1 l 0.15 nf 1", "cellname list children top",
                         "cellname list allcells"))),
        "SCRIPT-PCELL-11": dict(**toy, gui=True, body=lines(
            TOY_START, p("magic::gencell toy::fixed F1", "cellname list children top", "select cell F1",
                         "array -list count", "array -list pitch"),
            "magic::gencell {}", p("magic::gencell_getparams"))),
        "SCRIPT-PCELL-12": dict(files={**TOY_FILES, "top.spice": NETLIST, "sub2.mag": LEAF},
                                outputs=["sub1.mag", "top.mag"], fonts=["FreeSans"], body=lines(
            "source input/toy.tcl", p("magic::netlist_to_layout input/top.spice toy", "cellname list allcells"),
            "foreach c [lsort [cellname list allcells]] { load $c; select top cell; probe {port first}; "
            "probe {port last}; probe {box values} }", p("save sub1", "save top"))),
        "SCRIPT-PCELL-13": dict(**toy, gui=True, body=lines(
            TOY_START, "rename toy::nfet_dep toy::_dep",
            "proc toy::nfet_dep {parameters {target {}}} { puts [list dep $target]; return $parameters }",
            "magic::gencell toy::nfet", "proc tree {w} { puts $w; foreach c [winfo children $w] { tree $c } }",
            "tree .params", p("magic::gencell_getparams"),
            "catch {.params.body.area.w.entry delete 0 end; .params.body.area.w.entry insert 0 3}",
            "catch {event generate .params.body.area.w.entry <Return>}", p("magic::gencell_getparams"))),
        "SCRIPT-PCELL-14": dict(**toy, gui=True, body=lines(
            TOY_START, p("magic::gencell toy::nfet M1 w 2", "magic::gencell toy::nfet M2 w 3"),
            "select cell M1", "magic::gencell {}", p("wm title .params"), "select cell M2", "update",
            p("wm title .params", "magic::gencell_getparams"))),
        "SCRIPT-PCELL-15": dict(pdk="ihp-sg13g2", body=lines(
            "load top", p("magic::gencell sg13g2::rm6 R1", "cellname list children top"),
            "foreach c [lsort [cellname list children top]] { probe [list cellname list property $c] }")),
        "SCRIPT-PCELL-16": dict(pdk="sky130A", body=lines(
            "set d [sky130::sky130_fd_pr__nfet_01v8_defaults]",
            p("magic::normalize_value [dict get $d compatible]", "magic::get_gencell_hash $d"))),
    }


NETLIST = """\
* netlist_to_layout input (SCRIPT-PCELL-12)
.subckt sub1 a vout[0] b
C1 a b 1p
R1 a vout[0] 1k
L1 b
+ 0 1n
.ends
.subckt sub2 x y
.ends
.subckt top in out vdd
X1 in out vdd sub1 M=2
X2 in out sub2
.ends
.control
op
.endc
"""


def box_bodies() -> dict[str, dict]:
    return {
        "SCRIPT-BOX-01": dict(body=lines("scalegrid 1 2", "units microns", "box 0 0 1um 2um",
                                         p("box values", "pushbox", "set boxstack", "units"))),
        "SCRIPT-BOX-02": dict(body=lines(p("popbox", "peekbox"), "box 0 0 5 5",
                                         p("pushbox {10 10 30 50}", "box values", "pushbox", "box 1 1 2 2",
                                           "popbox size", "box values", "popbox", "box values", "peekbox"))),
        "SCRIPT-BOX-03": dict(files=dict(HIER), body=lines(
            "load top", p("pushstack a_0", "cellname list window", "view bbox", "set editstack",
                          "pushstack b_0", "cellname list window", "popstack", "cellname list window",
                          "popstack", "cellname list window", "popstack", "set editstack"))),
        "SCRIPT-BOX-04": dict(gui=True, body=lines(
            "rename updatedisplay _ud", "proc updatedisplay {args} { puts [list updatedisplay {*}$args]; "
            "_ud {*}$args }", p("suspendall", "suspendall", "resumeall", "resumeall", "resumeall"),
            "puts [lsort [array names Winopts]]")),
        "SCRIPT-BOX-05": dict(body=lines("snap internal", "box 0 0 100 20", p("ruler hello"), "box 0 0 20 100",
                                         p("ruler"), "box 0 0 200 200",
                                         "foreach e [lsort [element inbox]] { probe [list element configure $e] }",
                                         p("element inbox"))),
        "SCRIPT-BOX-06": dict(body=lines("snap internal", "box 0 0 250 10", p("measure", "measure vertical"),
                                         "box -100 -100 400 400", p("lsort [element inbox]", "unmeasure",
                                                                    "lsort [element inbox]"),
                                         "units microns", "box 0 0 250 10", p("measure", "unmeasure",
                                                                              "lsort [element inbox]"))),
        "SCRIPT-BOX-07": dict(outputs=["genkey.mag"], body=lines(
            "load genkey", "box 0 0 0 0", p("genkey poly", "box 0 -100 0 -100", "genkey m1 6", "what -list"),
            "box -500 -500 500 500", "foreach e [lsort [element inbox]] { probe [list element configure $e] }",
            "save genkey")),
        "SCRIPT-BOX-08": dict(gui=True, body=lines(
            "load top", "box 0 0 10 10", "paint m1", "box 5 5 5 5", p("label A12 c m1"),
            "event generate .layout1.pane.top.magic <Motion> -x 100 -y 100",
            "event generate .layout1.pane.top.magic <KeyPress> -keysym plus",
            "event generate .layout1.pane.top.magic <KeyPress> -keysym plus", "update",
            "select area label", p("what -list", "macro list ."))),
    }


# The display styles of scmos's styletype `mos` that hefesto-cli reads from
# $CAD_ROOT/magic/sys in wrapper mode, as Magic under X11 does (`feedback add`
# with a long style name, SCRIPT-GUI-09; the OpenGL variant, X-5).
MOS_STYLES = [{"source": "magic-sys", "path": path, "to": path.split("/")[1]}
              for path in ("scmos/mos.OpenGL.dstyle", "scmos/mos.OpenGL.std.cmap", "graphics/color.glyphs")]


def gui_bodies() -> dict[str, dict]:
    return {
        "SCRIPT-GUI-01": dict(gui=True, body=p("winfo children .layout1.pane.top", "windownames layout",
                                               "winfo children .layout1")),
        "SCRIPT-GUI-02": dict(gui=True, body=lines(
            'set Opts(callback) {puts "cb $framename $layoutframe"}', p("openwrapper"),
            'set Winopts(.layout3,callback) {puts "wcb .layout3"}', p("openwrapper"), p("windownames layout"))),
        "SCRIPT-GUI-03": dict(gui=True, files={"inv.mag": INV}, body=lines(
            "load inv", "rename magic::tkcon _tkcon", "proc magic::tkcon {args} {}",
            "foreach mb [lsort [winfo children .layout1.titlebar.mbuttons]] {",
            "    set m $mb.toolmenu", "    if {[catch {$m index end} n] || $n eq {none}} continue",
            "    for {set i 0} {$i <= $n} {incr i} {",
            "        if {[catch {$m entrycget $i -command} c] || $c eq {}} continue",
            "        if {[string match *quit* $c] || [string match *closewrapper* $c] || [string match *tk_get* $c]} continue",
            "        puts [list $mb $i [$m entrycget $i -label] $c]",
            "        puts [list [catch {$m invoke $i} r] $r]", "    }", "}")),
        "SCRIPT-GUI-04": dict(gui=True, body=lines(
            "load top", p("box 0 0 1.5um 2um"), "update", p(".layout1.titlebar.pos cget -text",
                                                            ".layout1.titlebar.caption cget -text", "box remove"),
            "update", p(".layout1.titlebar.pos cget -text"),
            "event generate .layout1.pane.top.magic <Motion> -x 50 -y 60", "update",
            p(".layout1.titlebar.pos cget -text"))),
        "SCRIPT-GUI-05": dict(gui=True, env={"PDK_ROOT": "{work}/input/pdkroot"},
                              files={"pdkroot/x/libs.tech/magic/x.tech": "tech\n    format 35\n    x\nend\n"},
                              body=lines(p("techmanager create"),
                                         "set m .techmgr.title.tname.menu",
                                         "catch {for {set i 0} {$i <= [$m index end]} {incr i} "
                                         "{ puts [list $i [$m entrycget $i -label] [$m entrycget $i -command]] }}")),
        "SCRIPT-GUI-06": dict(gui=True, files={"scmos_toolbars.tcl": "set toolbar_order {metal1 poly ndiff}\n"},
                              outputs=["scmos_toolbars.tcl"], body=lines(
            "file copy input/scmos_toolbars.tcl scmos_toolbars.tcl",
            "proc trace_cmd {cmd op} { puts [list issued $cmd] }",
            "foreach c {magic::paint magic::erase magic::see magic::select} { trace add execution $c enter trace_cmd }",
            p("magic::maketoolbar .layout1"), "update",
            "foreach w [lsort [info commands .layout1*toolbar*]] { puts $w }",
            "foreach ev {<ButtonPress-1> <ButtonPress-2> <ButtonPress-3>} "
            "{ catch {event generate .layout1.toolbar.b_poly $ev} }", "update")),
        "SCRIPT-GUI-07": dict(gui=True, files=dict(HIER), body=lines(
            "load top", p("cellmanager create"), "update",
            "proc walk {t n} { foreach c [$t children $n] { puts [list $c [$t item $c -text]]; walk $t $c } }",
            "catch {walk .cellmgr.box.view {}}",
            "proc trace_cmd {cmd op} { puts [list issued $cmd] }",
            "foreach c {magic::load magic::select magic::see magic::expand} { trace add execution $c enter trace_cmd }",
            "foreach b [winfo children .cellmgr.target] { catch {$b invoke} }")),
        "SCRIPT-GUI-08": dict(gui=True, files={"lib/a.mag": LEAF, "lib/b.mag": mag("othertech", ""),
                                               "lib/c.mag": "not a magic file\n"}, body=lines(
            p("libmanager create"), "catch {.libmgr.title.path delete 0 end; .libmgr.title.path insert 0 input/lib}",
            "update", "catch {magic::libmanager update}",
            "catch {foreach c [.libmgr.box.view children {}] { puts [.libmgr.box.view item $c -text] }}")),
        "SCRIPT-GUI-09": dict(gui=True, outputs=["drc.out"], external=list(MOS_STYLES), files={"err.mag": cell(
            "<< metal1 >>\nrect 0 0 2 10\nrect 4 0 10 10\n<< polysilicon >>\nrect 20 0 21 10\n")}, body=lines(
            "load err", "select top cell", "drc check", "drc catchup",
            p("drc listall why", "drc_save_report err drc.out", "feedback clear", "drc_load_report drc.out",
              "feedback count"))),
        "SCRIPT-GUI-10": dict(gui=True, body=lines(
            "load top", "box 0 0 10 10", "paint m1",
            "proc trace_cmd {cmd op} { puts [list issued $cmd] }",
            "foreach c {magic::label magic::setlabel magic::port} { trace add execution $c enter trace_cmd }",
            p("magic::make_texthelper"), "catch {.texthelper.text.tentry insert 0 NEW}",
            "catch {.texthelper.buttonbar.apply invoke}", "catch {.texthelper.layer.tentry insert 0 m1}",
            "catch {.texthelper.buttonbar.apply invoke}", "select area label", p("magic::make_texthelper"),
            "catch {.texthelper.buttonbar.apply invoke}")),
        "SCRIPT-GUI-11": dict(gui=True, files={"one.gds": b"", "inv.mag": INV}, body=lines(
            "proc tk_getOpenFile {args} { puts [list open {*}$args]; return [file normalize input/inv.mag] }",
            "proc tk_getSaveFile {args} { puts [list save {*}$args]; return [file normalize saved.mag] }",
            p("promptload magic", "cellname list window", "promptsave magic", "file exists saved.mag"))),
        "SCRIPT-GUI-12": dict(body=lines(
            "load top", "box 0 0 10 10", "paint m1", "set Opts(backupinterval) 1000",
            p("crashbackups start", "llength [after info]"), "after 2500 {set done 1}", "vwait done",
            p("crashbackups stop", "llength [after info]", "crashbackups disable", "crashbackups resume",
              "llength [after info]"))),
        "SCRIPT-GUI-13": dict(gui=True, body=lines(
            p("magic::enable_tools", "tool"),
            "foreach t {box wiring netlist pick} { probe [list tool $t]; probe {tool type}; "
            "probe {llength [macro list]}; probe {tool info} }")),
    }


def rsp_bodies() -> dict[str, dict]:
    reg = cell("<< metal1 >>\nrect 0 0 30 10\n<< labels >>\nrlabel metal1 0 0 0 0 0 D<0>\nport 1 nsew\n"
               "rlabel metal1 10 0 10 0 0 d<1>\nport 2 nsew\nrlabel metal1 20 0 20 0 0 CLK\n")
    inv_body = INV.replace("<< end >>\n", "rlabel metal1 5 5 5 5 0 BODY\nport 5 nsew\n<< end >>\n")
    ports = "foreach l {A Y VDD VSS BODY} { probe [list port $l index] }"
    return {
        "SCRIPT-RSP-01": dict(files={"inv.mag": INV, "inv.spice": ".subckt inv A Y VDD VSS\n.ends\n"}, body=lines(
            "load inv", p("readspice input/inv.spice"), ports)),
        "SCRIPT-RSP-02": dict(files={"inv.mag": inv_body, "inv.spice": ".subckt inv Y A VDD VSS\n.ends\n"},
                              body=lines("load inv", p("readspice input/inv.spice"), ports)),
        "SCRIPT-RSP-03": dict(files={"reg.mag": reg, "reg.spice": ".subckt reg D[0] D[1] CLK\n.ends\n"}, body=lines(
            "load reg", p("readspice input/reg.spice"),
            "foreach l {D<0> d<1> CLK} { probe [list port $l index] }")),
        "SCRIPT-RSP-04": dict(files={"inv.mag": INV, "cells.cdl": ".subckt inv A Y VDD VSS\n"
                                     "*.PININFO A:I Y:O VDD:B VSS:B\n.ends\n"}, body=lines(
            "load inv", "port Y class output", "cd input", p("readspice cells"), "cd ..",
            "foreach l {A Y VDD VSS} { probe [list port $l class] }")),
        "SCRIPT-RSP-05": dict(files={"inv.mag": INV, "inv.scs": "// spectre netlist\n"
                                     "inline subckt inv (A\t Y\n* comment\n+  VDD VSS) W=1\nends inv\n"
                                     "subckt notloaded (a b)\nends notloaded\n"}, body=lines(
            "load inv", p("readspice input/inv.scs"), ports)),
        "SCRIPT-RSP-06": dict(body=p("readspice nofile.sp")),
    }


def aux_bodies() -> dict[str, dict]:
    gds_cell = cell("<< metal1 >>\nrect 0 0 4 4\n<< properties >>\nstring GDS_START 100\n"
                    "string GDS_END 200\nstring GDS_FILE /x/y.gds\nstring FIXED_BBOX 0 0 4 4\n")
    return {
        "SCRIPT-AUX-01": dict(files={"lib/a.mag": gds_cell, "lib/b.mag": gds_cell}, outputs=["input/lib/a.mag",
                                                                                            "input/lib/b.mag"],
                              output_masks={"input/lib/a.mag": [], "input/lib/b.mag": []},
                              external=[magic_tcl("strip_reflibs.tcl")],
                              body=lines("cd input/lib", p("source $CAD_ROOT/magic/tcl/strip_reflibs.tcl"), "cd ../..")),
        "SCRIPT-AUX-02": dict(files={"f.net": " Netlist File\nnet1\n a/1\n b/1\n\nnet2\n a/2\n b/2\n"}, body=lines(
            "proc magic::iroute {args} { puts [list iroute {*}$args]; return {} }",
            p("mazeroute input/f.net"))),
        "SCRIPT-AUX-03": dict(body=p("info commands magic::add_menu", "info exists batch_mode",
                                     "set batch_mode", "catch {socket localhost 12946} e")),
        "SCRIPT-AUX-04": dict(files={"rev0.tcl": REV0}, external=[magic_tcl("toolkit_rev0.tcl")], body=lines(
            "load top", "source $CAD_ROOT/magic/tcl/toolkit_rev0.tcl", "source input/rev0.tcl",
            p("magic::gencell old::dev D1 w 2", "magic::gencell old::dev D2 w 3", "cellname list children top"),
            "foreach c [lsort [cellname list children top]] { probe [list cellname list property $c] }")),
    }


REV0 = """\
# A rev-0 library (toolkit_rev0.tcl): <lib>::<dev>_defaults, _draw
namespace eval old {}
proc old::dev_defaults {} { return {w 1 l 1} }
proc old::dev_convert {parameters} { return $parameters }
proc old::dev_check {parameters} { return $parameters }
proc old::dev_draw {parameters} {
    set w [dict get $parameters w]
    box 0 0 [expr {int($w * 10)}] 10
    paint m1
}
"""


def bodies() -> dict[str, dict]:
    return {**api_bodies(), **pdk_bodies(), **pcell_bodies(), **box_bodies(), **gui_bodies(), **rsp_bodies(),
            **aux_bodies()}


# Inputs that are files of a pinned PDK release other than the case's own pin: the older
# rc files of SCRIPT-PDK-18 (design D15). None in a spec's files means "copied here".
OLD_RC = {
    "old/sky130A.magicrc": ("sky130", "0fe599b2afb6708d281543108caf8310912f54af", "sky130A/libs.tech/magic/sky130A.magicrc"),
    "old/ihp-sg13g2.magicrc": ("ihp-sg13", "ddb601a4a4473163e1ed6df416b885df18b4ac03",
                               "ihp-sg13g2/libs.tech/magic/ihp-sg13g2.magicrc"),
}


def old_rc(name: str) -> str:
    """An older rc file: the committed case input, else the old pin in the PDK cache
    (scripts/fetch_pdk.py kept the 0fe599b2/ddb601a4 trees; Apache-2.0)."""
    committed = GOLDEN / "SCRIPT" / "cases" / "SCRIPT-PDK-18" / "input" / name
    if committed.is_file():
        return committed.read_text(encoding="utf-8")
    family, pin, path = OLD_RC[name]
    return (external.pdk_root() / "ciel" / family / "versions" / pin / path).read_text(encoding="utf-8")


def magic_tcl(name: str) -> dict:
    """An auxiliary script Magic installs in $CAD_ROOT/magic/tcl (from tcltk/ of the pinned checkout)."""
    return {"source": "magic-tcl", "path": f"tcltk/{name}", "to": name}


def build(cid: str, row: tuple[str, list[str], str], spec: dict) -> TclCase:
    cap, reqs, inp = row
    own = headings(cap)
    notes = [f"Also covers {r} (another capability's requirement)." for r in reqs if r not in own]
    files = {}
    for name, content in spec.pop("files", {}).items():
        files[name] = old_rc(name) if content is None else content
    also = spec.pop("also", [])
    tech_first = spec.pop("prelude_tech", True)
    stdin = spec.pop("stdin", False)
    case = TclCase(cid, cap, [r for r in reqs if r in own], _description(inp), files=files,
                   notes=notes + spec.pop("notes", []), driver="m4_cases.py", **spec)
    if case.pdk is None and tech_first and case.prelude and cid != "SCRIPT-PDK-04":
        case.tech = "scmos"
    case.also_pdks = also
    case.stdin = stdin
    return case


def _description(inp: str) -> str:
    text = inp.replace("`", "")
    return text if len(text) <= 200 else text[:197] + "..."


def specs() -> list[TclCase]:
    table = rows()
    body = bodies()
    missing = sorted(set(table) - set(body))
    extra = sorted(set(body) - set(table))
    if missing or extra:
        raise SystemExit(f"SCRIPT bodies out of step with CASES.md: missing {missing}, extra {extra}")
    return [build(cid, table[cid], dict(body[cid])) for cid in sorted(table)]


NOT_AUTHORED: dict[str, str] = {}
