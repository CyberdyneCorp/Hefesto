# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""VIEW golden case specs of M2 (openspec/changes/m2-interactive-editing, design D9;
tasks 8.2-8.4): the feedback, color window and wizard rows of tests/golden/UI/CASES.md
and CMDEDIT-MISC-03 of tests/golden/CMDEDIT/CASES.md.

Registered in the M2 driver (m2_cases.py, task 11.2), which writes the case directories.

Like ui_cases.py, the cases load tiny.tech (lambda 1, like scmos) and a small cell so
that a layout window and the box exist.
"""
from __future__ import annotations

from casegen import Spec
from common import TINY, mag

FEEDBACK = "ui-feedback-and-highlights"
COLOR = "ui-color-window"
WIZARD = "ui-wizard-and-debug"

NOT_AUTHORED = {
    "UI-G-061": "the red bar midpoint pixel for setpoint must be derived from the color window's view mapping "
                "in Magic's 512x484 null screen (VIEW)",
    "UI-G-062": "pushbutton on the Hue + pump needs the pump's pixel position; same as UI-G-061 (VIEW)",
    "UI-G-068": "needs extraction (M7)",
    "UI-G-082": "X11 screenshots (no virtual display in the capture container, design Open Questions)",
}

CELL = mag("tiny", "<< metal1 >>\nrect 0 0 100 50\n")

# The display styles of tiny.tech's styletype `mos` that Magic's startup reads from
# $CAD_ROOT/magic/sys (hefesto-cli reads the same files there; the OpenGL variant, X-5).
MOS_STYLES = [{"source": "magic-sys", "path": path, "to": path.split("/")[1]}
              for path in ("scmos/mos.OpenGL.dstyle", "scmos/mos.OpenGL.std.cmap", "graphics/color.glyphs")]


def view(cid: str, capability: str, reqs: list[str], desc: str, cmds: list[str],
         outputs: list[str] | None = None, notes: list[str] | None = None,
         stream_masks: list[str] | None = None, external: list[dict] | None = None) -> Spec:
    prefix = ["tech load input/tiny -noprompt", "load input/c"]
    return Spec(cid, capability, reqs, desc, prefix + cmds, files={"tiny.tech": TINY, "c.mag": CELL},
                outputs=outputs or [], notes=notes or [], stream_masks=stream_masks or [],
                external=external or [])


def specs() -> list[Spec]:
    return [
        view("UI-G-060", COLOR, ["Color window creation and layout", "Color window commands"],
             "specialopen color 47; color; color get; color rgb; color next; color 0x1ff; color last.",
             ["specialopen color 47", "color", "color get", "color rgb", "color next", "color 0x1ff", "color last",
              "color get"]),
        view("UI-G-063", FEEDBACK, ["feedback command", "Feedback area model"],
             "feedback add over the box, a line and a point; count; find three times and find 5; why.",
             ["box 0 0 4 4", 'feedback add "a"', 'feedback add "b" medium 0 0 10 10', 'feedback add "c" outline 1 1',
              "feedback count", "feedback find", "box values", "feedback find", "box values", "feedback find 5",
              "box values", "feedback why"]),
        view("UI-G-064", FEEDBACK, ["feedback command"],
             "feedback add with a square and a triangle polygon; count; areas through find.",
             ["box 0 0 4 4", 'feedback add "p" solid 0 0 10 0 10 10 0 10', 'feedback add "t" pale 0 0 10 0 0 10',
              "feedback count", "feedback find", "box values", "feedback find", "box values", "feedback find",
              "box values"]),
        view("UI-G-065", FEEDBACK, ["feedback command"],
             "The UI-G-063/064 areas plus a quoted text, then feedback save f.txt.",
             ["box 0 0 4 4", 'feedback add "a"', 'feedback add "b" medium 0 0 10 10', 'feedback add "c" outline 1 1',
              'feedback add "p" solid 0 0 10 0 10 10 0 10', 'feedback add "t" pale 0 0 10 0 0 10',
              'feedback add {say "hi"} dotted', "feedback save f.txt"],
             outputs=["f.txt"],
             notes=["Q-UI-24 (fix): Hefesto writes the box coordinates with an `i` suffix; the deviation is",
                    "recorded in tests/golden/deviations.toml by INTEGRATION."]),
        view("UI-G-066", FEEDBACK, ["Clearing feedback"],
             "feedback clear b; count; clear; count.",
             ["box 0 0 4 4", 'feedback add "a"', 'feedback add "b"', 'feedback add "ab"', 'feedback add "c"',
              "feedback clear b", "feedback count", "feedback find", "feedback clear", "feedback count"]),
        view("UI-G-067", FEEDBACK, ["feedback command"],
             "feedback add with a numeric third argument, an unknown style and a style long name.",
             ["box 0 0 4 4", "feedback add x 5 1 2", "feedback count", "feedback add x bogus", "feedback count",
              "feedback add x solid_highlights", "feedback count", "feedback bogus", "feedback"],
             notes=["solid_highlights is a long name of the mos display styles, which both tools read at startup."],
             external=MOS_STYLES),
        view("UI-G-081", WIZARD, ["Layout wizard commands", "Window-client wizard commands"],
             "*coord with a box set; *winddebug twice; *files.",
             ["box 0 0 10 10", "*coord", "*winddebug", "*winddebug", "*files"],
             notes=["*files prints each descriptor's inode number, which changes from run to run, and the",
                    "type of descriptor 0, which is the runner's standard input: both masked."],
             stream_masks=[r"regex:(?<=inode number )\d+", r"regex:(?<=file descriptor 0: open  \(type: \x27)[^\x27]+"]),
        view("CMDEDIT-MISC-03", "cmd-misc-edit", ["Feedback command"],
             "feedback add over the box and a triangle; count; why; save; clear x.",
             ["box 0 0 4 4", 'feedback add "x"', 'feedback add "p" solid 0 0 10 0 0 10', "feedback count",
              "feedback why", "feedback save f.tcl", "feedback clear x", "feedback count"],
             outputs=["f.tcl"],
             notes=["Q-UI-24 (fix): `i`-suffixed box coordinates in f.tcl (deviation recorded by INTEGRATION)."]),
    ]
