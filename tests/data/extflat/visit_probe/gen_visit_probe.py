#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Writes the capture-only probes NL-P01 and NL-P02 (m7-extraction task 10.4).

NL-P01 prints Magic's EFVisitNodes order on the 17 DEF-write `.ext` inputs of
tests/data/extflat/LEFDEF-*: for each target, `ext2sim labels on` and
`ext2sim alias on` write `<cell>.nodes` and `<cell>.al` in visit order, renamed
`<target>.nodes` and `<target>.al`. tests/unit/netlist/sim/visit_order_test.cpp
runs Hefesto's ext2sim on the same files and compares.

NL-P02 shows that `ext2sim` builds the short devices `ext2spice short` asks
for (EFOutputFlags is shared) and how it writes them, and that `ext2sim
default` clears the mode (tests/unit/netlist/sim/short_ports_test.cpp).

Both live outside tests/golden, so replay.py never runs them. Run from the
repository root: python3 tests/data/extflat/visit_probe/gen_visit_probe.py
Capture: gh workflow run golden-capture.yml -f mode=capture -f cases="<the two case dirs>"
"""

from __future__ import annotations

import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent
CASES = HERE / "NETLIST" / "cases"
TARGETS = ["317", "318", "319", "320", "321", "322", "323", "324", "325", "326", "327", "328",
           "330", "331", "332", "333", "334"]

HEADER = """# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors

#
# {id}: capture-only probe (m7-extraction task 10.4), written by
# tests/data/extflat/visit_probe/gen_visit_probe.py; do not edit by hand.
# {what}
# It lives outside tests/golden, so replay.py never runs it.

schema = 1
id = "{id}"
area = "NETLIST"
capability = "{cap}"
requirements = ["{req}"]
tool = "magic"
description = "{desc}"

[reference]
script = "input/commands.tcl"
argv = ["magic", "-dnull", "-noconsole", "-norc", "input/commands.tcl"]

[hefesto]
args = ["--no-log", "--diagnostics=magic", "--magic", "-dnull", "-noconsole", "-norc", "input/commands.tcl"]
requires_commands = []

[[outputs]]
name = "stdout.txt"
source = "stdout"
comparator = "magic-console"
masks = ["magic-stamp-note", "magic-tech-sections"]

[[outputs]]
name = "stderr.txt"
source = "stderr"
comparator = "magic-console"
masks = ["magic-stamp-note", "magic-tech-sections"]
"""

COLLECT = """
[[collect]]
pattern = "{pattern}"
comparator = "bytes"
masks = []
"""

PROLOGUE = """# {id}: {desc}
proc probe {{script}} {{
    set code [catch {{uplevel #0 $script}} result]
    puts [list $code $result]
    flush stdout
}}
if {{[catch {{
"""

EPILOGUE = """}} scriptError]}} {{
    puts [list script-error $scriptError]
    flush stdout
}}
quit -noprompt
"""


def root_of(target: str) -> str:
    run = (ROOT / "tests/data/extflat" / f"LEFDEF-{target}" / "run.tcl").read_text()
    match = re.search(r"extprobe::run (\S+) (\S+)", run)
    assert match, target
    return match.group(2)


def fresh(case: Path) -> Path:
    if (case / "input").exists():
        shutil.rmtree(case / "input")
    (case / "input").mkdir(parents=True)
    return case / "input"


def write_p01() -> None:
    case = CASES / "NL-P01"
    inputs = fresh(case)
    desc = "Magic's node visit order on the 17 DEF-write .ext inputs: ext2sim .nodes and .al per target"
    body = ["drc off", "probe {ext2sim labels on}", "probe {ext2sim alias on}"]
    for target in TARGETS:
        name = f"LEFDEF-{target}"
        src = ROOT / "tests/data/extflat" / name
        dst = inputs / name
        dst.mkdir()
        for ext in sorted(src.glob("*.ext")):
            shutil.copyfile(ext, dst / ext.name)
        shutil.copyfile(ROOT / "tests/golden/LEFDEF/cases" / name / "input/lefdef.tech", dst / "lefdef.tech")
        root = root_of(target)
        body += [
            f"tech load input/{name}/lefdef -noprompt",
            f"probe {{ext2sim -p input/{name} {root}}}",
            f"probe {{file rename -force {root}.nodes {name}.nodes}}",
            f"probe {{file rename -force {root}.al {name}.al}}",
            f"probe {{file delete {root}.sim}}",
        ]
    (inputs / "commands.tcl").write_text(PROLOGUE.format(id="NL-P01", desc=desc) + "\n".join(body) + "\n" + EPILOGUE.format())
    toml = HEADER.format(id="NL-P01", cap="ext-flattening-hierarchy", req="Visit orders for output", desc=desc,
                         what="ext2sim writes .nodes and .al in EFVisitNodes order.")
    toml += COLLECT.format(pattern="*.nodes") + COLLECT.format(pattern="*.al")
    (case / "case.toml").write_text(toml)


def write_p02() -> None:
    case = CASES / "NL-P02"
    inputs = fresh(case)
    desc = "ext2sim after ext2spice short resistor/voltage and after ext2sim default: one cell with two shorted port pairs"
    shutil.copyfile(ROOT / "tests/golden/NETLIST/cases/NL-FLAT-06/input/nltest.tech", inputs / "nltest.tech")
    (inputs / "top.ext").write_text(
        "timestamp 0\nversion 8.3\ntech nltest\nstyle nltest\nscale 1000 1000 5\nresistclasses 1000 5000 100\n"
        'port "A" 0 0 0 1 1 metal1\nport "B" 1 10 0 11 1 metal1\n'
        'port "C" 2 20 0 21 1 metal1\nport "D" 3 30 0 31 1 metal1\n'
        'node "A" 0 2 0 0 metal1 0 0 0 0 0 0\nnode "B" 0 2 10 0 metal1 0 0 0 0 0 0\n'
        'node "C" 0 2 20 0 metal1 0 0 0 0 0 0\nnode "D" 0 2 30 0 metal1 0 0 0 0 0 0\n'
        'equiv "A" "B"\nequiv "C" "D"\n')
    body = ["addpath input", "tech load input/nltest -noprompt", "drc off", "probe {ext2sim labels on}"]
    for step, setup in (("none", None), ("resistor", "ext2spice short resistor"),
                        ("voltage", "ext2spice short voltage"), ("default", "ext2sim default")):
        if setup:
            body.append(f"probe {{{setup}}}")
        if step == "default":
            body.append("probe {ext2sim labels on}")
        body += ["probe {ext2sim top}", f"probe {{file rename -force top.sim top.{step}.sim}}",
                 f"probe {{file rename -force top.nodes top.{step}.nodes}}"]
    (inputs / "commands.tcl").write_text(PROLOGUE.format(id="NL-P02", desc=desc) + "\n".join(body) + "\n" + EPILOGUE.format())
    toml = HEADER.format(id="NL-P02", cap="netlist-sim", req="Command options and defaults", desc=desc,
                         what="Does ext2sim read the short mode of ext2spice short (shared EFOutputFlags)?")
    toml += COLLECT.format(pattern="*.sim") + COLLECT.format(pattern="*.nodes")
    (case / "case.toml").write_text(toml)


if __name__ == "__main__":
    write_p01()
    write_p02()
