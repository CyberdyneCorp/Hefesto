# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""UI command case specs of M2 (tests/golden/UI/CASES.md; m2-interactive-editing design D9):
ui-command-dispatch and ui-tools-and-input rows that Magic's Tcl build runs headless.

Like ui_cases.py the cases load tiny.tech and a small cell so that a layout window and
the box exist. The feedback, color window and wizard rows are VIEW's (view_cases.py).
"""
from __future__ import annotations

from casegen import Spec
from common import TINY, mag

DISPATCH = "ui-command-dispatch"
TOOLS = "ui-tools-and-input"

NOT_AUTHORED = {
    "UI-G-006": "non-Tcl build (the Tcl build is normative, D-7; Q-UI resolutions made it informative)",
    "UI-G-007": "non-Tcl build (D-7)", "UI-G-008": "non-Tcl build (D-7)",
    "UI-G-015": "X11-interactive keystrokes (no virtual display in the capture container; unit-tested)",
    "UI-G-020": "non-Tcl build (D-7)", "UI-G-021": "non-Tcl build (D-7)", "UI-G-022": "non-Tcl build (D-7)",
    "UI-G-047": "X11-interactive pointer clicks (unit tests and the pushbutton case UI-G-019 cover it)",
}

CELL = mag("tiny", "<< metal1 >>\nrect 0 0 100 50\n")


# The command log header carries the wall-clock date (textio/txCommands.c TxLogStart).
LOG_DATE_MASK = {"log.txt": [r"regex:(?<=# Date: )[^\n]*"]}


def ui(cid: str, capability: str, reqs: list[str], desc: str, cmds: list[str],
       outputs: list[str] | None = None) -> Spec:
    prefix = ["tech load input/tiny -noprompt", "load input/c"]
    return Spec(cid, capability, reqs, desc, prefix + cmds, files={"tiny.tech": TINY, "c.mag": CELL},
                outputs=outputs or [], output_masks=LOG_DATE_MASK if outputs == ["log.txt"] else {})


def dispatch_specs() -> list[Spec]:
    return [
        ui("UI-G-001", DISPATCH, ["Command name lookup"], "getc nonexist and getcell nonexist.",
           ["getc nonexist", "getcell nonexist"]),
        ui("UI-G-002", DISPATCH, ["Command name lookup", "Command dispatch across global and client tables"],
           "The prefix e and the unique prefix extr are not command names in a script (Q-UI-41); the "
           "registered alias id is (ext, the alias of extract, waits for the extractor).",
           ["e", "extr help", "id", "id x"]),
        ui("UI-G-003", DISPATCH, ["Command dispatch across global and client tables"], "foo in a layout window.",
           ["box values", "foo"]),
        ui("UI-G-004", DISPATCH, ["Command name lookup"], "ZoOm 2, then view get.", ["view get", "ZoOm 2", "view get"]),
        ui("UI-G-005", DISPATCH, ["help command"], "help, help window, help wizard in the layout window; then in a color window.",
           ["help", "help window", "help wizard", "specialopen color", "help", "help wizard"]),
        ui("UI-G-009", DISPATCH, ["macro and imacro commands"],
           "macro list and macro help with the default system startup file (box tool).", ["macro list", "macro help"]),
        ui("UI-G-010", DISPATCH, ["macro and imacro commands", "Macro tables"], 'macro z, macro z "", macro z.',
           ["macro z", 'macro z ""', "macro z"]),
        ui("UI-G-011", DISPATCH, ["Macro key names"], 'macro ^R, macro Control_r, macro Shift_a "box", macro A.',
           ["macro ^R", "macro Control_r", 'macro Shift_a "box"', "macro A"]),
        ui("UI-G-012", DISPATCH, ["Macro tables"], "tool wiring; macro Button1; tool box; macro Button1.",
           ["tool wiring", "macro Button1", "tool box", "macro Button1"]),
        ui("UI-G-013", DISPATCH, ["macro and imacro commands"], "macro copy mytool; tool mytool; macro list.",
           ["macro copy mytool", "tool mytool", "macro list"]),
        ui("UI-G-014", DISPATCH, ["macro and imacro commands"], 'macro wind3d L "level up" in a build without 3-D.',
           ['macro wind3d L "level up"']),
        ui("UI-G-016", DISPATCH, ["Command logging"],
           'logcommands log.txt (a usage error in Magic 8.3.694); logcommands start log.txt; box; label "a b" n; '
           "setpoint 1 1; logcommands stop.",
           ["logcommands log.txt", "logcommands start log.txt", "box 0 0 10 10", 'label "a b" n', "setpoint 1 1",
            "logcommands stop"],
           outputs=["log.txt"]),
        ui("UI-G-017", DISPATCH, ["Command logging"], "logcommands log.txt update; box 0 0 1 1; logcommands.",
           ["logcommands log.txt update", "box 0 0 1 1", "logcommands"], outputs=["log.txt"]),
        ui("UI-G-018", DISPATCH, ["pushbutton and setpoint"], "setpoint 100 100 0, then setpoint with no arguments.",
           ["setpoint 100 100 0", "setpoint"]),
        ui("UI-G-019", DISPATCH, ["pushbutton and setpoint"],
           "setpoint 120 100 0; pushbutton left down; pushbutton left up; box (box tool).",
           ["box 0 0 10 10", "setpoint 120 100 0", "pushbutton left down", "pushbutton left up", "box values"]),
    ]


def tool_specs() -> list[Spec]:
    return [
        ui("UI-G-046", TOOLS, ["tool command", "Tool registry"], "tool x4; tool foo; tool w; tool info; tool type (Tcl form).",
           ["tool", "tool", "tool", "tool", "tool foo", "tool w", "tool info", "tool type", "tool"]),
        ui("UI-G-048", TOOLS, ["cursor command"],
           "cursor 22, cursor bogus (the cursor queries read the X pointer, uninitialised on Magic's null "
           "display: dropped per Q-UI-43).",
           ["setpoint 100 100 0", "cursor 22", "cursor bogus"]),
        ui("UI-G-049", TOOLS, ["Default key bindings"], "macro list per tool and client (Tcl variant of the system startup file).",
           ["macro list", "macro layout list", "macro wiring list", "macro color list", "tool wiring", "macro list",
            "tool box"]),
    ]


def specs() -> list[Spec]:
    return dispatch_specs() + tool_specs()
