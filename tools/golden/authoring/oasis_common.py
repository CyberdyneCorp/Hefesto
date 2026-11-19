# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The framework of the OASIS golden cases (m6-lef-def-oasis design D19): technology, script lines
and the three case kinds. ``oasis_cases.py`` documents the references and drives the families
(``oasis_format_cases.py``, ``oasis_read_cases.py``, ``oasis_write_cases.py``).
"""
from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # goldenlib

import casegen  # noqa: E402
import oasequiv  # noqa: E402
import oasgen as o  # noqa: E402
import tclcase  # noqa: E402
from mask_cases import layer, out_style, tech  # noqa: E402

DRIVER = "m6_cases.py"
AREA = "OASIS"
TECH = "oastest"
CASES_DIR = casegen.GOLDEN / AREA / "cases"
CASES_MD = casegen.GOLDEN / AREA / "CASES.md"
REPORT = "report.txt"
CELLS = "derived:files:*.mag"
OUT = "out.oas"

# --- technology -----------------------------------------------------------------------------------

# CASES.md: metal1 on 49/0, metal1 pins on 49/16, labels on 49/5, metal2 on 51/0, scalefactor 10
# (one internal unit is 100 database units of a 1 nm file).
OUT_STYLE = out_style(layer("M1", "metal1", "labels metal1"), layer("M2", "metal2", "labels metal2", calma=(51, 0)))
IN_STYLE = """\
style gds
    scalefactor 10
    layer metal1 CMF
        labels CMF
    layer metal1 CMP
        labels CMP port
    layer metal1 CML
        labels CML text
    layer metal2 CMS
        labels CMS
    calma CMF 49 0
    calma CMP 49 16
    calma CML 49 5
    calma CMS 51 0
"""
# oasis-write "Complete annotated example": layer M1 = metal1 on 49/0 only.
REFERENCE_STYLE = out_style(layer("M1", "metal1"))


def technology(cifoutput: str = OUT_STYLE, cifinput: str = IN_STYLE) -> str:
    return tech(cifoutput, cifinput, name=TECH)


# --- script lines ---------------------------------------------------------------------------------

REP_PROC = """\
proc rep {script} {
    set code [catch {uplevel #0 $script} result]
    set f [open report.txt a]
    puts $f [list $code $result]
    close $f
}
proc specrep {value} {
    set f [open report.txt a]
    puts $f [list 0 $value]
    close $f
}"""


@dataclass(frozen=True)
class Line:
    """One command: Magic's and Hefesto's text (the same unless the formats differ), and whether
    its result goes to report.txt (``rep``)."""

    magic: str
    hefesto: str
    report: bool = False


def both(cmd: str) -> Line:
    return Line(cmd, cmd)


def rep(cmd: str) -> Line:
    return Line(cmd, cmd, True)


def opt(option: str) -> Line:
    """A ``gds`` option for Magic, the same ``oasis`` option for Hefesto."""
    return Line(f"gds {option}", f"oasis {option}")


def read(stem: str) -> Line:
    return Line(f"gds read {stem}", f"oasis read {stem}")


def spec_line(query: str, value: str) -> Line:
    """A report line fixed by the spec: Magic writes ``value``, Hefesto the result of ``query``."""
    return Line(f"#raw specrep {{{value}}}", query, True)


def _tcl(cmd: str, report: bool) -> str:
    if cmd.startswith("#raw "):
        return cmd[len("#raw "):]
    return f"rep {{{cmd}}}" if report else f"probe {{{cmd}}}"


def bodies(lines: list[Line]) -> tuple[str, str]:
    magic = [REP_PROC] + [_tcl(line.magic, line.report) for line in lines]
    hefesto = [REP_PROC] + [_tcl(line.hefesto, line.report) for line in lines]
    return "\n".join(magic), "\n".join(hefesto)


def prelude(datestamp: bool = True) -> list[Line]:
    """drc off, the case technology and, unless the case is about it, a fixed read datestamp
    (``gds datestamp 1`` / ``oasis datestamp 1``: both reads give fixed stamp 1, so the saved
    cells compare without the GDS dates Magic would copy)."""
    head = [both("drc off"), both(f"tech load input/{TECH} -noprompt"), both("snap internal")]
    return head + ([opt("datestamp 1")] if datestamp else [])


# A new cell ``anchor`` is always saved, so the saved-cells output exists even when a case's reads
# create nothing writable (refused files, read-only vendor cells).
FINISH = [both("load anchor"), both("box 0 0 1 1"), both("paint metal1"), rep("lsort [cellname list allcells]"),
          both("writeall force")]

# --- OASIS records --------------------------------------------------------------------------------


def rect(lay: tuple[int, int], x1: int, y1: int, x2: int, y2: int, rep_: bytes | None = None) -> bytes:
    return o.rectangle(lay[0], lay[1], x2 - x1, y2 - y1, x1, y1, rep_)


def ocell(name: str | int, *records: bytes) -> bytes:
    return o.cell(name) + b"".join(records)


def ofile(*cells: bytes, unit: float = 1000, validation: int = o.CRC32) -> bytes:
    return o.oasis_file(cells, unit=unit, validation=validation)


M1, M1PIN, M1LBL, M2 = (49, 0), (49, 16), (49, 5), (51, 0)
UNMAPPED = (60, 0)

# --- case kinds -----------------------------------------------------------------------------------

NOTE_READ = ("Magic GDS cross-check (design D19): Magic reads <stem>.gds, the GDS equivalent of <stem>.oas "
             "(authoring/oasequiv.py); Hefesto reads <stem>.oas (input/hefesto.tcl). Saved cells and report.txt "
             "are compared; the console is not (two different commands, informative wording).")
NOTE_WRITE = ("Magic GDS cross-check (design D19): Magic writes out.oas with gds write (GDS bytes), Hefesto with "
              "oasis write (input/hefesto.tcl); compared with stream-geometry and stream-hierarchy. KLayout and "
              "gdstk check Hefesto's file in golden-capture.yml mode oasis-validate.")
NOTE_SPEC = ("Hefesto self-golden (design D19), tool oasis-spec: the expected bytes are the spec listing, assembled "
             "by authoring/oasgen.py into input/spec/ and checked by tests/test_oasis_cases.py.")


@dataclass
class Read:
    """The inputs of a read case: OASIS files by stem, and GDS overrides (default: the equivalent)."""

    oasis: dict[str, bytes]
    gds: dict[str, bytes] = field(default_factory=dict)
    libnames: dict[str, str] = field(default_factory=dict)
    circle_points: int = oasequiv.DEFAULT_CIRCLE_POINTS

    def files(self) -> dict[str, bytes]:
        out: dict[str, bytes] = {}
        for stem, data in self.oasis.items():
            out[f"{stem}.oas"] = data
            out[f"{stem}.gds"] = self.gds.get(stem) or oasequiv.gds_equivalent(
                data, self.libnames.get(stem, "lib"), self.circle_points)
        return out


def _case(cid: str, capability: str, requirements: list[str], description: str, lines: list[Line],
          files: dict, outputs: list[str], notes: list[str], **kw) -> tclcase.TclCase:
    magic, hefesto = bodies(lines)
    return tclcase.TclCase(cid, capability, requirements, description, magic, files=files, outputs=outputs,
                           hefesto_body=hefesto, streams=False, driver=DRIVER, notes=notes, **kw)


def read_case(cid: str, capability: str, requirements: list[str], description: str, inputs: Read,
              steps: list[Line] | None = None, *, cifinput: str = IN_STYLE, extra: dict | None = None,
              datestamp: bool = True, notes: list[str] | None = None, masks: list[str] | None = None,
              **kw) -> tclcase.TclCase:
    """A Magic GDS cross-check of ``oasis read`` (module docstring); ``steps`` default to one read
    per input file."""
    files = {f"{TECH}.tech": technology(cifinput=cifinput), **inputs.files(), **(extra or {})}
    body = [*prelude(datestamp), *(steps if steps is not None else [read(s) for s in inputs.oasis]), *FINISH]
    cell_masks = ["mag-timestamp", *(masks or [])]
    return _case(cid, capability, requirements, description, body, files, [CELLS, REPORT],
                 [NOTE_READ, *(notes or [])], output_names={CELLS: "cells.txt"},
                 output_masks={CELLS: cell_masks, REPORT: list(masks or [])}, **kw)


def stream_outputs(names) -> dict:
    """The outputs of written streams: each file with ``stream-geometry`` and, from the same source,
    ``<name>.hierarchy.txt`` with ``stream-hierarchy``."""
    outputs, comparators, sources, labels = [], {}, {}, {}
    for name in names:
        hier = f"{name}.hierarchy"
        outputs += [name, hier]
        comparators.update({name: "stream-geometry", hier: "stream-hierarchy"})
        sources[hier], labels[hier] = name, f"{name}.hierarchy.txt"
    return {"outputs": outputs, "output_comparators": comparators, "output_sources": sources,
            "output_names": labels}


def write_case(cid: str, requirements: list[str], description: str, layout: list[Line], *,
               files: dict | None = None, cifoutput: str = OUT_STYLE, before: list[Line] | None = None,
               notes: list[str] | None = None, fonts: list[str] | None = None,
               settings: list[Line] | None = None, writes: list[Line] | None = None,
               outputs: tuple[str, ...] = (OUT,)) -> tclcase.TclCase:
    """A Magic GDS cross-check of ``oasis write`` (module docstring): ``before`` runs before ``load
    top``; ``writes`` default to one write per output (``gds write`` / ``oasis write``)."""
    writes = writes if writes is not None else [Line(f"gds write {n}", f"oasis write {n}") for n in outputs]
    body = [*prelude(False), *(before or []), both("load top"), *layout, *(settings or []), *writes]
    kinds = stream_outputs(outputs)
    return _case(cid, "oasis-write", requirements, description, body,
                 {f"{TECH}.tech": technology(cifoutput), **(files or {})}, kinds.pop("outputs"),
                 [NOTE_WRITE, *(notes or [])], fonts=list(fonts or []), **kinds)


def spec_case(cid: str, requirements: list[str], description: str, layout: list[Line],
              expected: dict[str, bytes], *, cifoutput: str = REFERENCE_STYLE, files: dict | None = None,
              notes: list[str] | None = None, capability: str = "oasis-write") -> tclcase.TclCase:
    """A Hefesto self-golden write (tool oasis-spec); ``layout`` must write every ``expected`` file."""
    body = [REP_PROC] + [_tcl(line.hefesto, line.report) for line in [*prelude(False), *layout]]
    inputs = {f"{TECH}.tech": technology(cifoutput), **(files or {}),
              **{f"spec/{name}": data for name, data in expected.items()}}
    return tclcase.TclCase(cid, capability, requirements, description, "\n".join(body), files=inputs,
                           outputs=list(expected), output_displays={n: "oasis-records" for n in expected},
                           streams=False, tool="oasis-spec", driver=DRIVER, notes=[NOTE_SPEC, *(notes or [])])
