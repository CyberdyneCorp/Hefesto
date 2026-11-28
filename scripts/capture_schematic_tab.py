#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Captures of the desktop Schematic tab for docs/design/m8-ui-conformance.md.

openspec/changes/m8-schematic task 13.2. Runs the product app (`hefesto`) once
per conformance row, each time with `--run` commands and an `--input` script,
and keeps a PPM capture and the app's output per run:

  tab        the Schematic tab of the test bench (Linux: a click on the view
             tab segment; macOS, whose 1024 px screen hides the view tabs:
             `tab schematic`); gating, as since round 6/7;
  selection  a netlist run (writes the wires' `lab`), `hilight net IN`,
             `hilight instance C1`, `select instance R1`: the selection, its
             properties in the field view and the net highlighting; gating;
  field      a click on the `value` row of R1 (place printed by the
             selection run), the value typed in the field editor, Return:
             one `schematic setprop` line; reported;
  pan        a middle-drag and a pinch on the canvas: pan and zoom; reported;
  netlist    the `n` binding on a schematic whose resistor value takes 4 s of
             Tcl: the capture is taken in the frame the Interrupt card first
             shows (--capture-running); reported;
  keys       the key-binding sheet (`panel keys show`, on the Layout view:
             the sheet lives over the layout canvas); reported.

After each `--input` script the app prints "hefesto: schematic tab ..." and
"hefesto: schematic state ..." (SchematicView::status_line and state_line).
The gating runs fail the step; the others print a warning and are recorded
in summary.txt for the conformance review.

usage: capture_schematic_tab.py --app build/<preset>/src/shell/desktop/hefesto --leg <name> --out schematic-captures
"""

from __future__ import annotations

import argparse
import os
import platform
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

TESTBENCH = "tests/data/schem/devlib/tb/ihp_style_tb.sch"
RC_FILE = "tests/data/schem/devlib/tb/xschemrc"
TAB_LINE = re.compile(r"^hefesto: schematic tab (?P<crumb>\S+), (?P<n>\d+) instances, (?P<pending>\d+) pending, "
                      r"canvas (?P<w>\d+) x (?P<h>\d+)$", re.M)
STATE_LINE = re.compile(r"^hefesto: schematic state (?P<state>.*)$", re.M)
CANVAS_AT = re.compile(r"canvas at (?P<x>-?\d+),(?P<y>-?\d+)")
ROWS_AT = re.compile(r"rows at (?P<x>-?\d+),(?P<y>-?\d+)")
ZOOM = re.compile(r"zoom (?P<z>[0-9.e+-]+) (?P<mode>user view|fitted)")
ROW_HEIGHT = 24  # schematic_view.cpp kRowHeight

# A resistor whose value takes 4 s of Tcl when netlisted, so the netlist run
# lasts past kLongRunning (2 s) and the netlist bar shows the Interrupt card.
SLOW_SCHEMATIC = """v {xschem version=3.4.8RC file_version=1.3
* SPDX-License-Identifier: Apache-2.0
* Copyright 2026 The Hefesto Authors
}
G {}
K {}
V {}
S {}
F {}
E {}
N 0 -60 0 -30 {}
N 0 30 0 60 {}
C {devices/res.sym} 0 0 0 0 {name=R1
value="tcleval([set e [expr {[clock milliseconds] + 4000}]; while {[clock milliseconds] < $e} {}]10k)"
m=1}
C {devices/gnd.sym} 0 60 0 0 {name=l1 lab=GND}
C {lab_pin.sym} 0 -60 0 0 {name=p1 sig_type=std_logic lab=A}
"""


@dataclass
class Run:
    name: str
    output: str = ""
    ok: bool = True
    notes: list[str] = field(default_factory=list)

    def state(self) -> str:
        m = STATE_LINE.search(self.output)
        return m.group("state") if m else ""

    def check(self, condition: bool, what: str) -> None:
        self.notes.append(("ok: " if condition else "FAILED: ") + what)
        self.ok = self.ok and condition


class App:
    def __init__(self, app: str, leg: str, out: Path) -> None:
        self.app = app
        self.leg = leg
        self.out = out
        self.linux = platform.system() == "Linux"

    def run(self, name: str, commands: str, steps: str, extra: list[str] | None = None) -> Run:
        capture = self.out / f"schematic-{name}-{self.leg}.ppm"
        if name == "tab":  # the round-6 artifact name, kept for the earlier conformance records
            capture = self.out / f"schematic-tab-{self.leg}.ppm"
        args = [self.app, "--width", "1280", "--height", "800", "--input", steps,
                "--capture", str(capture), "--no-vsync"]
        if commands:
            args[1:1] = ["--run", commands]
        args += extra if extra is not None else ["--capture-frames", "60"]
        env = dict(os.environ)
        if self.linux:
            env["RAVEN_WINDOW_SYSTEM"] = "x11"
            args = ["xvfb-run", "-a", "-s", "-screen 0 1920x1080x24", *args]
        print(f"== {name}: {' '.join(args)}", flush=True)
        done = subprocess.run(args, env=env, capture_output=True, text=True, timeout=300, check=False)
        run = Run(name, done.stdout + done.stderr)
        (self.out / f"schematic-{name}-{self.leg}.log").write_text(run.output)
        print(run.output, flush=True)
        run.check(done.returncode == 0, f"exit status {done.returncode}")
        run.check(capture.is_file() and capture.stat().st_size > 0, f"capture {capture.name}")
        return run


def point(pattern: re.Pattern[str], state: str) -> tuple[int, int] | None:
    m = pattern.search(state)
    return (int(m.group("x")), int(m.group("y"))) if m else None


def tab_run(app: App) -> tuple[Run, tuple[int, int, int, int] | None]:
    opened = f"schematic open {TESTBENCH}"
    if app.linux:  # the Schematic segment of the view tabs at 1280 x 800 (x 320-405)
        run = app.run("tab", opened, "wait 500; down 362,24; up 362,24; wait 3000")
    else:
        run = app.run("tab", f"{opened}; tab schematic", "wait 500; wait 3000")
    tab = TAB_LINE.search(run.output)
    run.check(tab is not None and tab.group("crumb") == "ihp_style_tb.sch" and int(tab.group("n")) > 0
              and tab.group("pending") == "0", "the Schematic tab shows ihp_style_tb.sch, all symbols resolved")
    run.check("\nhefesto: panel " not in "\n" + run.output, "no layout dock around the Schematic tab")
    width, height = (int(tab.group("w")), int(tab.group("h"))) if tab else (0, 0)
    run.check(width >= 400 and height >= 250, f"canvas {width} x {height} px (at least 400 x 250)")
    origin = point(CANVAS_AT, run.state())
    return run, (origin[0], origin[1], width, height) if origin else None


def selection_run(app: App) -> Run:
    commands = (f"schematic open {TESTBENCH}; tab schematic; schematic netlist; schematic hilight net IN; "
                "schematic hilight instance C1; schematic select instance R1")
    run = app.run("selection", commands, "wait 500; wait 2000")
    state = run.state()
    run.check("properties R1 3 rows name=R1 value=10k m=1" in state, "the field view shows R1's three attributes")
    lit = re.search(r"highlights (\d+) nets (\d+) instances", state)
    run.check(lit is not None and int(lit.group(1)) >= 1 and int(lit.group(2)) >= 1,
              "net IN and instance C1 highlighted")
    return run


def field_run(app: App, rows: tuple[int, int]) -> Run:
    x, y = rows[0] + 40, rows[1] + ROW_HEIGHT + ROW_HEIGHT // 2  # the second row: value
    commands = f"schematic open {TESTBENCH}; tab schematic; schematic select instance R1"
    steps = (f"wait 800; down {x},{y}; up {x},{y}; wait 300; key end; key backspace; key backspace; "
             "key backspace; text 47k; key return; wait 1500")
    run = app.run("field", commands, steps)
    run.check("value=47k" in run.state(), "the field editor ran schematic setprop instance R1 value 47k")
    return run


def pan_run(app: App, canvas: tuple[int, int, int, int], fitted_zoom: str) -> Run:
    cx, cy = canvas[0] + canvas[2] // 2, canvas[1] + canvas[3] // 2
    steps = (f"wait 800; down {cx},{cy} middle; move {cx + 40},{cy + 20}; move {cx + 80},{cy + 40}; "
             f"up {cx + 80},{cy + 40} middle; wait 1500")
    run = app.run("pan", f"schematic open {TESTBENCH}; tab schematic", steps,
                  ["--capture-frames", "60", "--pinch", f"{cx},{cy},1.6"])
    zoom = ZOOM.search(run.state())
    run.check(zoom is not None and zoom.group("mode") == "user view", "the view left the fit (user view)")
    run.check(zoom is not None and zoom.group("z") != fitted_zoom, f"zoom changed from the fitted {fitted_zoom}")
    return run


def netlist_run(app: App, canvas: tuple[int, int, int, int]) -> Run:
    work = app.out / "work"
    work.mkdir(parents=True, exist_ok=True)
    (work / "slow_netlist.sch").write_text(SLOW_SCHEMATIC)
    (work / "xschemrc").write_text(Path(RC_FILE).read_text())
    x, y = canvas[0] + 8, canvas[1] + 8  # a click on an empty corner gives the canvas the key focus
    steps = f"wait 800; down {x},{y}; up {x},{y}; wait 200; key n; wait 1000"
    run = app.run("netlist", f"schematic open {work / 'slow_netlist.sch'}; tab schematic", steps,
                  ["--capture-running"])
    state = run.state()
    run.check("netlist done" in state, "the netlist action ran to the end")
    run.check("card seen" in state, "the netlist bar drew the Interrupt card (captured in that frame)")
    return run


def keys_run(app: App) -> Run:
    return app.run("keys", "panel keys show", "wait 500; wait 1500")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--app", required=True)
    parser.add_argument("--leg", required=True)
    parser.add_argument("--out", required=True, type=Path)
    a = parser.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    app = App(a.app, a.leg, a.out)

    tab, canvas = tab_run(app)
    fitted = ZOOM.search(tab.state())
    selection = selection_run(app)
    runs = [tab, selection]
    gating = [tab, selection]
    rows = point(ROWS_AT, selection.state())
    if rows is not None:
        runs.append(field_run(app, rows))
    if canvas is not None:
        runs.append(pan_run(app, canvas, fitted.group("z") if fitted else "?"))
        runs.append(netlist_run(app, canvas))
    runs.append(keys_run(app))

    lines = []
    for run in runs:
        verdict = "ok" if run.ok else ("FAILED" if run in gating else "not reached")
        lines.append(f"{run.name}: {verdict}")
        lines += [f"  {note}" for note in run.notes]
        if run.state():
            lines.append(f"  state: {run.state()}")
        if not run.ok and run not in gating:
            print(f"::warning::Schematic capture '{run.name}' ({a.leg}): " +
                  "; ".join(n for n in run.notes if n.startswith("FAILED")))
    summary = "\n".join(lines) + "\n"
    (a.out / "summary.txt").write_text(summary)
    print(summary)
    return 0 if all(run.ok for run in gating) else 1


if __name__ == "__main__":
    sys.exit(main())
