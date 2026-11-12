# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Writes golden cases whose reference script is run unchanged by both tools (M4).

Until M4, a case had a Tcl script for Magic and a ``.hef`` script for hefesto-cli
(``casegen.py``). With the embedded Tcl of M4 (m4-scripting design D3/D16), hefesto-cli
runs Magic's own command line (``--magic``), so a :class:`TclCase` has one
``input/commands.tcl`` that Magic and hefesto-cli both run as the script argument:

* ``magic -dnull -noconsole -norc [-rcfile <rc>] input/commands.tcl`` (GUI cases:
  ``-d X11`` under Xvfb, ``[reference] display = "xvfb"``);
* ``hefesto-cli --no-log --diagnostics=magic [--trust-pdk <pdk>] --magic <the same
  Magic options> input/commands.tcl``, from the same work directory, so ``info script``
  and every relative path print the same;
* a ``stdin`` case (SCRIPT-API-14) passes no script: both tools read ``commands.tcl``
  on standard input (``stdin`` of ``[reference]`` and ``[hefesto]``).

The script starts with ``addpath input`` (the case's cells load by name) and the ``probe``
procedure of tests/golden/SCRIPT/CASES.md (each
probed command prints ``[list <code> <result>]`` and flushes, so a case killed by a
timeout keeps its output) and ends with ``quit -noprompt``. PDK cases (``pdk``) copy
the PDK's ``libs.tech/magic`` (and the ``libs.ref`` cells they name) into
``input/pdk/<variant>/``, set ``PDK_ROOT`` to ``{work}/input/pdk``, start from the PDK's
rc file and run ``random seed 1`` before the body (design D16, Q-SCRIPT-44).
Only ``case.toml`` and ``input/`` are written; ``expected/`` is captured in CI.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from casegen import GOLDEN, HEADER, REPO, _output_block, _toml_list, _toml_str, _with_mask

PROBE = """\
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}"""



def p(*cmds: str) -> str:
    """One ``probe {cmd}`` line per command."""
    return "\n".join(f"probe {{{c}}}" for c in cmds)


def lines(*parts: str) -> str:
    return "\n".join(parts)


# The data set of scripts/fetch_pdk.py and the PDK identity hefesto-cli trusts, per variant.
PDKS = {"sky130A": "sky130", "gf180mcuD": "gf180mcu", "ihp-sg13g2": "ihp"}
RC_FILE = "input/pdk/{v}/libs.tech/magic/{v}.magicrc"
TIMEOUT_PREFIX = ["timeout", "-s", "KILL"]  # coreutils in the capture image
TECH_EXTERNALS = {"scmos": {"source": "scmos", "to": "scmos.tech"}}
STDIN = ['stdin = "input/commands.tcl"']


@dataclass
class TclCase:
    id: str
    capability: str
    requirements: list[str]
    description: str
    body: str                                          # Tcl, run after the prelude
    files: dict[str, str | bytes] = field(default_factory=dict)
    outputs: list[str] = field(default_factory=list)   # files written in the work dir
    output_masks: dict[str, list[str]] = field(default_factory=dict)
    output_names: dict[str, str] = field(default_factory=dict)  # name per source (derived sources)
    stream_masks: list[str] = field(default_factory=list)
    tech: str | None = None                            # "scmos": staged and loaded first
    pdk: str | None = None                             # sky130A | gf180mcuD | ihp-sg13g2
    pdk_cells: list[str] = field(default_factory=list)  # libs.ref paths under the variant
    rc: bool = True                                    # PDK cases: start with the PDK rc file
    gui: bool = False                                  # Tk wrapper under Xvfb
    timeout: int | None = None                         # seconds; the case is killed (Q-SCRIPT-3)
    magic_args: list[str] = field(default_factory=list)  # extra Magic options before the script
    script_args: list[str] = field(default_factory=list)  # words after the script
    env: dict[str, str] = field(default_factory=dict)
    external: list[dict] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)
    prelude: bool = True                               # the probe procedure and tech/PDK set-up
    also_pdks: list[str] = field(default_factory=list)  # further PDK trees the body switches to
    stdin: bool = False                                # feed commands.tcl on standard input instead
    allow_nonzero_exit: bool = False                   # Magic's exit status is part of the case
    driver: str = "m4_cases.py"

    @property
    def area(self) -> str:
        return self.id.split("-")[0]


def magic_options(case: TclCase) -> list[str]:
    display = ["-d", "X11"] if case.gui else ["-dnull"]
    rc = ["-rcfile", RC_FILE.format(v=case.pdk)] if case.pdk and case.rc else ["-norc"]
    script = [] if case.stdin else ["input/commands.tcl"]
    return [*display, "-noconsole", *rc, *case.magic_args, *script, *case.script_args]


def reference_argv(case: TclCase) -> list[str]:
    timeout = [*TIMEOUT_PREFIX, str(case.timeout)] if case.timeout else []
    return [*timeout, "magic", *magic_options(case)]


def hefesto_args(case: TclCase) -> list[str]:
    trust = ["--trust-pdk", case.pdk] if case.pdk else []
    return ["--no-log", "--diagnostics=magic", *trust, "--magic", *magic_options(case)]


def _pdk_input(variant: str, path: str) -> dict:
    base = {"source": "pdk"} if PDKS[variant] == "sky130" else {"source": "pdk", "set": PDKS[variant]}
    return {**base, "path": f"{variant}/{path}", "to": f"pdk/{variant}/{path}"}


def pdk_externals(case: TclCase) -> list[dict]:
    if not case.pdk:
        return []
    trees = [_pdk_input(v, "libs.tech/magic") for v in (case.pdk, *case.also_pdks)]
    return trees + [_pdk_input(case.pdk, cell) for cell in case.pdk_cells]


def externals(case: TclCase) -> list[dict]:
    tech = [TECH_EXTERNALS[case.tech]] if case.tech in TECH_EXTERNALS else []
    return [*tech, *pdk_externals(case), *case.external]


def environment(case: TclCase) -> dict[str, str]:
    pdk = {"PDK_ROOT": "{work}/input/pdk", "PDK": case.pdk} if case.pdk else {}
    return {**pdk, **case.env}


def _toml_table(name: str, items: dict[str, str]) -> str:
    return f"\n[{name}]\n" + "".join(f"{k} = {_toml_str(v)}\n" for k, v in items.items()) if items else ""


def case_toml(case: TclCase) -> str:
    lines = [HEADER, "#", f"# {case.id} ({case.area}/CASES.md). Inputs only: expected/ is written by",
             f"# capture.py in CI (golden-capture.yml). Generated by tools/golden/authoring/{case.driver}.",
             "# Magic and hefesto-cli both run input/commands.tcl (authoring/tclcase.py).",
             *(f"# {n}" for n in case.notes), "",
             "schema = 1", f"id = {_toml_str(case.id)}", f"area = {_toml_str(case.area)}",
             f"capability = {_toml_str(case.capability)}", f"requirements = {_toml_list(case.requirements)}",
             'tool = "magic"', f"description = {_toml_str(case.description)}", "",
             "[reference]", 'script = "input/commands.tcl"', f"argv = {_toml_list(reference_argv(case))}",
             *(['display = "xvfb"'] if case.gui else []), *(STDIN if case.stdin else []),
             *(["allow_nonzero_exit = true"] if case.timeout or case.allow_nonzero_exit else []), "",
             "[hefesto]", f"args = {_toml_list(hefesto_args(case))}", "requires_commands = []",
             *(STDIN if case.stdin else [])]
    text = "\n".join(lines) + "\n" + _toml_table("env", environment(case))
    for ext in externals(case):
        text += "\n[[external]]\n" + "".join(f"{k} = {_toml_str(v)}\n" for k, v in ext.items())
    masks = _with_mask(["magic-stamp-note", *case.stream_masks], "magic-tech-sections")
    text += _output_block("stdout.txt", "stdout", "magic-console", masks)
    text += _output_block("stderr.txt", "stderr", "magic-console", masks)
    for source in case.outputs:
        out_masks = case.output_masks.get(source, ["mag-timestamp"] if source.endswith(".mag") else [])
        name = case.output_names.get(source, source.replace("/", "__"))
        text += _output_block(name, source, "bytes", out_masks)
    return text


def script(case: TclCase) -> str:
    head = [f"# {case.id}: {case.description}"]
    if not case.prelude:
        return "\n".join(head + [case.body.rstrip("\n")]) + "\n"
    head += [PROBE, "addpath input"]  # the case's own cells are found by name
    if case.tech == "scmos":
        head.append("tech load input/scmos -noprompt")
    if case.pdk:
        head.append("random seed 1")
    # An error outside a probe ends the body, not the run: Magic's Tk wrapper would keep
    # its event loop running after a failed `source` and never reach `quit`.
    body = ["if {[catch {", case.body.rstrip("\n"), "} scriptError]} {",
            "    puts [list script-error $scriptError]", "    flush stdout", "}"]
    return "\n".join(head + body + ["quit -noprompt"]) + "\n"


def write(case: TclCase, golden: Path = GOLDEN) -> Path:
    root = golden / case.area / "cases" / case.id
    inp = root / "input"
    inp.mkdir(parents=True, exist_ok=True)
    (root / "case.toml").write_text(case_toml(case), encoding="utf-8")
    (inp / "commands.tcl").write_text(script(case), encoding="utf-8")
    for name, content in case.files.items():
        target = inp / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(content, bytes):
            target.write_bytes(content)
        else:
            target.write_text(content, encoding="utf-8", newline="")
    return root


__all__ = ["TclCase", "write", "case_toml", "script", "p", "lines", "REPO"]
