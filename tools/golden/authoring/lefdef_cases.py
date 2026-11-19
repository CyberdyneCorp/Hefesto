#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""LEFDEF golden cases (openspec/changes/m6-lef-def-oasis design D19, tasks 1.6, 15.1 and 15.5).

This module holds the shared framework of the LEFDEF cases and the ``.ext`` probe of task 1.6
(Open Question 1, decided (a)). The cases themselves are in ``lefdef_lef_cases.py`` (the
lef-tech-section, lef-read and lef-write rows), ``lefdef_def_cases.py`` (the def-read and
def-write rows) and ``lefdef_pdk_cases.py`` (LEFDEF-240 and the ``LEFDEF-RT-<lib>`` library
round trips of task 15.5); ``m6_cases.py`` is the driver (``--list``, ``--not-authored``,
``--exit-check``).

Every case is a ``tclcase.TclCase`` that Magic and ``hefesto-cli --magic`` both run from one
``input/commands.tcl``. Its row of tests/golden/LEFDEF/CASES.md gives the capability, the
requirements (resolved to the exact headings of ``openspec/specs/<capability>/spec.md``) and the
description. The session state is pinned as design D19 requires (D18 of M5):

* a fresh session per case (the capture runs ``magic -dnull``, where idle DRC never runs);
* ``drc off`` before anything else, then the case's technology and ``snap internal``;
* ``lef datestamp 100`` before a LEF read whose cells a case saves (except LEFDEF-023, which
  is about the datestamp), so the ``use`` timestamps of a parent are fixed; a saved ``.mag``
  header stamp is masked (``mag-timestamp``);
* one write per case, unless the case is a history case whose note names its steps;
* ``TZ=UTC`` and ``LC_ALL=C`` (goldenlib/refrunners.py), and for PDK cases ``--trust-pdk``,
  the PDK's rc file and ``random seed 1`` (tclcase.py).

The cases use small synthetic technologies, one per case family (``tech()``), committed in each
case's ``input/``, with a ``lef`` section, the DRC rules the LEF defaults read, an output style
(``scalefactor`` sets the LEF/DEF unit) and an extract style (the DEF-write net cases are
extracted by the probe). Each command runs under ``probe`` (one ``[list <code> <result>]`` line),
so a command Magic rejects is part of the captured console. LEF files are compared with
``lef-text``, DEF files with ``def-text``, the console with ``lef-text`` (the progress lines of
D19 removed), and the saved cells as one ``derived:files`` output after ``mag-timestamp``.

The DEF-write net cases (LEFDEF-317..328 and 330..334) read the ``.ext`` files of their layout,
which Hefesto cannot write before M7. Magic writes them: golden-capture.yml mode ``ext-inputs``
runs ``tools/golden/ext_inputs.py`` on the plan printed by ``--ext-plan``, and each target's
Magic session runs ``tools/golden/harness/ext_probe/ext_probe.tcl`` (``extract all`` of the
target's root cell). ``--sync-ext`` then copies the ``.ext`` files of the downloaded artifact
into each case's ``input/`` and into ``tests/data/extflat/<target>/`` with the scripts that made
them, where EXTSUB's read tests take them.

The targets are every DEF-write net case that has been authored (its ``input/`` holds one
``*.tech`` file and its ``commands.tcl`` loads the root cell), plus ``smoke``, a small
committed layout (``tests/data/extflat/smoke/input/``) that keeps the probe exercised
before the cases exist:

    python3 tools/golden/authoring/lefdef_cases.py                 # write the smoke layout and probe copy
    python3 tools/golden/authoring/lefdef_cases.py --ext-plan FILE [CASE_DIR...]
    python3 tools/golden/authoring/lefdef_cases.py --sync-ext DIR  # DIR: the ext-inputs artifact
    python3 tools/golden/authoring/lefdef_cases.py --dispatch-ext  # the golden-capture.yml dispatch

Never touches ``expected/``.
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # goldenlib

import casegen  # noqa: E402
import common  # noqa: E402
import tclcase  # noqa: E402
from goldenlib.extfile import check_ext, check_probe_log  # noqa: E402

PROBE = casegen.REPO / "tools" / "golden" / "harness" / "ext_probe" / "ext_probe.tcl"
EXTFLAT = casegen.REPO / "tests" / "data" / "extflat"
CASES = casegen.GOLDEN / "LEFDEF" / "cases"
RUN_SCRIPT = "run.tcl"
PROBE_LOG = "probe.log"

# The DEF-write net cases (tests/golden/LEFDEF/CASES.md; LEFDEF-329 has no .ext on purpose).
# No PDK case writes DEF (task 15.5 adds LEFDEF-240, a DEF read, and the LEF round trips), so
# every target has its own *.tech file.
DEF_NET_CASES = tuple(f"LEFDEF-{n}" for n in (*range(317, 329), *range(330, 335)))

SMOKE_ID = "smoke"
SMOKE_TECH = "extsmoke"
SMOKE_ROOT = "top"
SMOKE_EXTRACT = """\
extract
    style extsmoke
    lambda 1
    step 100
    sidehalo 0
    planeorder active 0
    planeorder metal1 1
    planeorder metal2 2
    planeorder metal3 3
    resist ndiff,pdiff,poly,pdc/a 1000
    resist metal1,pdc/m1,via1/m1 100
    resist metal2,via1/m2,via2/m2 50
    resist metal3,via2/m3 30
    areacap metal1 10
    areacap metal2 8
    areacap metal3 6
end
"""
# The MODEL technology `mt` (tests/golden/MODEL/CASES.md) with an extract style.
SMOKE_TECH_TEXT = common.MT.replace("    mt\nend", f"    {SMOKE_TECH}\nend", 1).replace(
    "extract\nend\n", SMOKE_EXTRACT, 1)
# A cell with two ports, used twice: one net crosses the hierarchy through an L-shaped
# wire (internal label "mid"), one is a top-level port reaching a subcell ("IN"), one
# stays inside a subcell (leaf_1's Y), and a rail goes through a via to metal2 ("vdd!").
SMOKE_LEAF = common.mag(SMOKE_TECH, (
    "<< metal1 >>\nrect 0 0 10 4\nrect 0 8 10 12\n"
    "<< labels >>\nrlabel metal1 0 0 10 4 0 A\nport 1 nsew\nrlabel metal1 0 8 10 12 0 Y\nport 2 nsew\n"))
SMOKE_TOP = common.mag(SMOKE_TECH, (
    "<< metal1 >>\nrect -6 0 0 4\nrect 14 0 18 8\nrect 18 0 20 4\nrect 10 8 18 12\nrect 0 20 36 24\n"
    "<< metal2 >>\nrect 36 20 40 30\n"
    "<< via1 >>\nrect 36 20 40 24\n"
    "use leaf leaf_0\ntimestamp 100\ntransform 1 0 0 0 1 0\nbox 0 0 10 12\n"
    "use leaf leaf_1\ntimestamp 100\ntransform 1 0 20 0 1 0\nbox 0 0 10 12\n"
    "<< labels >>\nrlabel metal1 -6 0 -6 4 0 IN\nport 1 nsew\nrlabel metal1 14 6 14 6 0 mid\n"
    "rlabel metal1 2 22 2 22 0 vdd!\n"))
SMOKE_FILES = {f"{SMOKE_TECH}.tech": SMOKE_TECH_TEXT, "leaf.mag": SMOKE_LEAF, f"{SMOKE_ROOT}.mag": SMOKE_TOP}

_LOAD = re.compile(r"^\s*(?:probe \{)?load (\S+?)\}?\s*$", re.MULTILINE)


@dataclass(frozen=True)
class ExtTarget:
    """One Magic session of the probe: ``input_dir`` is copied as ``<work>/input``."""
    id: str
    input_dir: Path
    tech: str                    # technology file stem in input_dir
    root: str                    # extracted with its subtree
    case_dir: Path | None = None  # the golden case whose input/ receives the .ext files


def smoke_dir() -> Path:
    return EXTFLAT / SMOKE_ID


def smoke_target() -> ExtTarget:
    return ExtTarget(SMOKE_ID, smoke_dir() / "input", SMOKE_TECH, SMOKE_ROOT)


def case_target(case_dir: Path) -> tuple[ExtTarget | None, str]:
    """The target of an authored case, or None and the reason it cannot be probed yet."""
    inputs = case_dir / "input"
    script = inputs / "commands.tcl"
    if not script.is_file():
        return None, "not authored (no input/commands.tcl)"
    techs = sorted(inputs.glob("*.tech"))
    if len(techs) != 1:
        return None, f"needs one input/*.tech file, has {len(techs)} (PDK cases: task 15.5)"
    match = _LOAD.search(script.read_text(encoding="utf-8"))
    if match is None:
        return None, "input/commands.tcl loads no cell"
    return ExtTarget(case_dir.name, inputs, techs[0].stem, match.group(1), case_dir), ""


def ext_targets(selected: list[Path] | None = None) -> tuple[list[ExtTarget], list[dict]]:
    """The probe targets (smoke first) and the skipped cases with their reasons.
    ``selected`` limits the cases to the given case directories; smoke always runs."""
    names = {Path(p).name for p in selected} if selected else None
    targets, skipped = [smoke_target()], []
    for case_id in DEF_NET_CASES:
        if names is not None and case_id not in names:
            continue
        target, reason = case_target(CASES / case_id)
        if target is None:
            skipped.append({"id": case_id, "reason": reason})
        else:
            targets.append(target)
    return targets, skipped


def run_script(target: ExtTarget) -> str:
    """The Magic script of a target, run from its work directory (ext_inputs.py)."""
    return (f"# .ext probe of {target.id} (m6-lef-def-oasis task 1.6; lefdef_cases.py)\n"
            f"source ext_probe.tcl\nextprobe::run {target.tech} {target.root}\nquit -noprompt\n")


def _rel(path: Path | None) -> str | None:
    return None if path is None else path.relative_to(casegen.REPO).as_posix()


def ext_plan(selected: list[Path] | None = None) -> dict:
    """The plan ext_inputs.py runs: paths are relative to the repository root."""
    targets, skipped = ext_targets(selected)
    return {
        "schema": 1,
        "probe": _rel(PROBE),
        "targets": [{"id": t.id, "input": _rel(t.input_dir), "case": _rel(t.case_dir),
                     "script": run_script(t)} for t in targets],
        "skipped": skipped,
    }


def write_all() -> list[Path]:
    """The smoke layout and the committed copy of the probe."""
    written = []
    smoke_dir().joinpath("input").mkdir(parents=True, exist_ok=True)
    for name, text in SMOKE_FILES.items():
        path = smoke_dir() / "input" / name
        path.write_text(text, encoding="utf-8", newline="")
        written.append(path)
    probe_copy = EXTFLAT / PROBE.name
    shutil.copyfile(PROBE, probe_copy)
    written.append(probe_copy)
    return written


def artifact_problems(source: Path) -> list[str]:
    """Why a target's artifact directory cannot be synced (empty when it can)."""
    if not source.is_dir():
        return ["not in the artifact"]
    log = source / PROBE_LOG
    problems = check_probe_log(log.read_text(encoding="utf-8", errors="replace")) if log.is_file() \
        else [f"no {PROBE_LOG}"]
    files = sorted(source.glob("*.ext"))
    if not files:
        problems.append("no .ext file")
    for path in files:
        problems += [f"{path.name}: {p}" for p in check_ext(path.read_bytes())]
    return problems


def _replace_ext(directory: Path, files: list[Path]) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    for old in directory.glob("*.ext"):
        old.unlink()
    for path in files:
        shutil.copyfile(path, directory / path.name)


def sync_target(target: ExtTarget, source: Path) -> None:
    """Copy one checked target: .ext files, run script and log to tests/data/extflat/<id>/,
    and the .ext files to the case's input/."""
    files = sorted(source.glob("*.ext"))
    data = EXTFLAT / target.id
    _replace_ext(data, files)
    for name in (RUN_SCRIPT, PROBE_LOG):
        if (source / name).is_file():
            shutil.copyfile(source / name, data / name)
    if target.case_dir is not None:
        _replace_ext(target.case_dir / "input", files)


def sync_ext(artifact: Path) -> tuple[list[str], dict[str, list[str]]]:
    """Sync every target the artifact has; returns the synced ids and the refused ones."""
    targets, _ = ext_targets()
    synced, refused = [], {}
    for target in targets:
        problems = artifact_problems(artifact / target.id)
        if problems:
            refused[target.id] = problems
            continue
        sync_target(target, artifact / target.id)
        synced.append(target.id)
    if synced:
        shutil.copyfile(PROBE, EXTFLAT / PROBE.name)
    return synced, refused


def dispatch_command() -> str:
    return "gh workflow run golden-capture.yml --ref <branch> -f mode=ext-inputs"


def _print_sync(artifact: Path) -> int:
    synced, refused = sync_ext(artifact)
    print(f"synced {len(synced)} target(s): {' '.join(synced) or '(none)'}")
    for target_id, problems in refused.items():
        print(f"refused {target_id}: " + "; ".join(problems), file=sys.stderr)
    return 1 if refused or not synced else 0


# --- LEFDEF case framework (task 15.1) --------------------------------------------------------

DRIVER = "m6_cases.py"
TECH = "lefdef"
CASES_MD = casegen.GOLDEN / "LEFDEF" / "CASES.md"
SPECS = casegen.REPO / "openspec" / "specs"
CAPABILITIES = ("lef-tech-section", "lef-read", "lef-write", "def-read", "def-write")
# Every .mag file the run saves, the input cells written back included (``writeall force``),
# compared as one output (goldenlib/derived.py ``files``).
CELLS = "derived:files:**/*.mag"
SAVE_ALL = "writeall force"
DATESTAMP = "lef datestamp 100"
WRITES = ("lef write", "lef writeall", "def write", "writeall", "save")
RAW = "#raw "  # a body line written as is (Tcl control flow), not under probe


@dataclass(frozen=True)
class Row:
    """One row of tests/golden/LEFDEF/CASES.md."""
    id: str
    capability: str
    inputs: str
    capture: str
    requirements: tuple[str, ...]


_ROW = re.compile(r"^\| (LEFDEF-\d+) \| ([a-z-]+) \| (.*) \| (.*) \| (.*) \|\s*$", re.MULTILINE)
_DECISION = re.compile(r"\(Q-[A-Z]+-[\d/]+(?:, Q-[A-Z]+-[\d/]+)*\)")


def rows() -> dict[str, Row]:
    """The rows of CASES.md, by ID."""
    out = {}
    for match in _ROW.finditer(CASES_MD.read_text(encoding="utf-8")):
        cid, cap, inputs, capture, reqs = match.groups()
        names = tuple(r.strip() for r in _DECISION.sub("", reqs).split(";") if r.strip())
        out[cid] = Row(cid, cap, inputs.strip(), capture.strip(), names)
    return out


def headings(capability: str) -> list[str]:
    text = (SPECS / capability / "spec.md").read_text(encoding="utf-8")
    return re.findall(r"^### Requirement: (.+?)\s*$", text, re.MULTILINE)


def resolve(capability: str, name: str) -> str:
    """The exact requirement heading that ``name`` (CASES.md's short form) stands for: the
    heading itself, else the one heading that starts with it."""
    known = headings(capability)
    if name in known:
        return name
    matches = [h for h in known if h.startswith(name)]
    if len(matches) != 1:
        raise SystemExit(f"{capability}: requirement '{name}' matches {matches or 'no heading'}")
    return matches[0]


# --- technology -------------------------------------------------------------------------------

# The default output style: one internal unit (lambda) is 10 nm, so LEF/DEF distances are
# multiples of 0.01 um; contact cuts are 20 nm squares with a 5 nm border.
OUT_BASE = """\
style lefdef
    scalefactor 10 nanometers
    layer M1 metal1,pc,ndc,pdc,via1
        labels metal1
        calma 68 20
    layer M2 metal2,via1,via2
        labels metal2
        calma 69 20
    layer M3 metal3,via2
        labels metal3
        calma 70 20
    layer M5 metal5
        labels metal5
        calma 72 20
    layer POLY poly,pc,nfet,pfet
        labels poly
        calma 66 20
    layer DIFF ndiff,pdiff,ndc,pdc,nfet,pfet
        calma 65 20
    layer NWELL nwell
        calma 64 20
    layer CONT pc,ndc,pdc
        squares 5 20 20
        calma 66 44
    layer VIA1 via1
        squares 5 20 20
        calma 68 44
    layer VIA2 via2
        squares 5 20 20
        calma 69 44
"""

# DRC rules read by the LEF defaults (lef-tech-section "Default routing parameters"): metal5
# has none, so its LEF entry keeps width 3 and spacing 4.
DRC_BASE = """\
    width metal1 3 "metal1 width < 3"
    spacing metal1 metal1 3 touching_ok "metal1 spacing < 3"
    width metal2 4 "metal2 width < 4"
    spacing metal2 metal2 4 touching_ok "metal2 spacing < 4"
    width metal3 5 "metal3 width < 5"
    spacing metal3 metal3 5 touching_ok "metal3 spacing < 5"
    width poly 2 "poly width < 2"
    width pc,ndc,pdc 4 "contact width < 4"
    width via1 4 "via1 width < 4"
    width via2 4 "via2 width < 4"
"""

LEF_BASE = """\
    masterslice nwell NWELL nwell
    masterslice poly POLY poly
    routing metal1 metal1 met1 M1
    routing metal2 metal2 met2 M2
    routing metal3 metal3 met3 M3
    routing metal5 metal5 met5 M5
    cut pc cont mcon
    cut via1 via1 via V1
    cut via2 via2 V2
    obstruction obsm1 metal1
    obstruction obsm2 metal2
    obstruction obsv1 via1
    bound boundary BOUNDARY
"""

EXTRACT_BASE = """\
    style {name}
    lambda 1
    step 100
    sidehalo 0
    planeorder well 0
    planeorder active 1
    planeorder metal1 2
    planeorder metal2 3
    planeorder metal3 4
    planeorder metal5 5
    planeorder obstruct 6
    planeorder bnd 7
    resist ndiff,pdiff,ndc/a,pdc/a 1000
    resist poly,pc/a 500
    resist metal1,pc/m1,ndc/m1,pdc/m1,via1/m1 100
    resist metal2,via1/m2,via2/m2 50
    resist metal3,via2/m3 30
    resist metal5 20
    areacap metal1 10
    areacap metal2 8
    areacap metal3 6
"""

TECH_TEMPLATE = """\
tech
    format 35
    {name}
end

planes
    well,w
    active,a
    metal1,m1
    metal2,m2
    metal3,m3
    metal5,m5
    obstruct,ob
    bnd,bd
end

types
    well      nwell,nw
    active    ndiffusion,ndiff
    active    pdiffusion,pdiff
    active    polysilicon,poly
    active    ntransistor,nfet
    active    ptransistor,pfet
    active    polycontact,pc
    active    ndcontact,ndc
    active    pdcontact,pdc
    metal1    metal1,m1
    metal1    via1,v1
    metal2    metal2,m2
    metal2    via2,v2
    metal3    metal3,m3
    metal5    metal5,m5
    obstruct  obsm1
    obstruct  obsm2
    obstruct  obsv1
    bnd       boundary
{types}end

contact
    pc    poly   metal1
    ndc   ndiff  metal1
    pdc   pdiff  metal1
    via1  metal1 metal2
    via2  metal2 metal3
{contact}end

styles
    styletype mos
end

compose
    compose nfet poly ndiff
    compose pfet poly pdiff
end

connect
    ndiff ndc
    pdiff pdc
    poly pc
    metal1 pc,ndc,pdc,via1
    metal2 via1,via2
    metal3 via2
end

cifoutput
{cifoutput}end

cifinput
end

drc
{drc}end

lef
{lef}end

extract
{extract}end
"""


def tech(name: str = TECH, *, lef: str = LEF_BASE, drc: str = DRC_BASE, cifoutput: str = OUT_BASE,
         types: str = "", contact: str = "", extract: str = "") -> str:
    """A LEFDEF test technology: the base planes and types (``types``/``contact`` add lines) with
    the given output style, DRC rules, ``lef`` section and extract lines (appended to the base
    extract style ``name``)."""
    return TECH_TEMPLATE.format(name=name, types=types, contact=contact, cifoutput=cifoutput, drc=drc,
                                lef=lef, extract=EXTRACT_BASE.format(name=name) + extract)


def out_style(scalefactor: str, extra: str = "") -> str:
    """OUT_BASE with another ``scalefactor`` line, followed by ``extra`` head statements."""
    return OUT_BASE.replace("    scalefactor 10 nanometers\n", f"    scalefactor {scalefactor}\n{extra}", 1)


# --- layout and LEF/DEF text --------------------------------------------------------------------

def mag(body: str, props: dict[str, str] | None = None, name: str = TECH) -> str:
    """A .mag file of the case technology, with optional string properties."""
    if props:
        body += "<< properties >>\n" + "".join(f"string {k} {v}\n" for k, v in props.items())
    return common.mag(name, body)


def rects(layer_name: str, *boxes: tuple[int, int, int, int]) -> str:
    return f"<< {layer_name} >>\n" + "".join(f"rect {x1} {y1} {x2} {y2}\n" for x1, y1, x2, y2 in boxes)


def use(cell: str, uid: str, x: int = 0, y: int = 0, size: tuple[int, int] = (100, 200),
        transform: tuple[int, int, int, int] = (1, 0, 0, 1), array: tuple[int, ...] | None = None) -> str:
    """A ``use`` block: ``transform`` is (a, b, d, e) of Magic's ``transform a b c d e f``."""
    a, b, d, e = transform
    head = f"use {cell}  {uid}\n" + (f"array {' '.join(map(str, array))}\n" if array else "")
    return head + f"timestamp 100\ntransform {a} {b} {x} {d} {e} {y}\nbox 0 0 {size[0]} {size[1]}\n"


def port_label(layer_name: str, box: tuple[int, int, int, int], text: str, index: int,
               use_class: str = "", flags: str = "nsew") -> str:
    """An ``rlabel`` and its ``port`` line (``use_class``: e.g. ``signal input``)."""
    tail = f" {use_class}" if use_class else ""
    return plain_label(layer_name, box, text) + f"port {index} {flags}{tail}\n"


def plain_label(layer_name: str, box: tuple[int, int, int, int], text: str) -> str:
    x1, y1, x2, y2 = box
    return f"rlabel {layer_name} {x1} {y1} {x2} {y2} 0 {text}\n"


def labels(*lines: str) -> str:
    return "<< labels >>\n" + "".join(lines)


LEF_HEAD = """\
VERSION 5.7 ;
BUSBITCHARS "[]" ;
DIVIDERCHAR "/" ;
"""


def lef(*parts: str, library_end: bool = True) -> str:
    """A LEF file: the version header, the given statements and ``END LIBRARY``."""
    return LEF_HEAD + "".join(parts) + ("END LIBRARY\n" if library_end else "")


def lef_layer(layer_name: str, *boxes: tuple[float, float, float, float], indent: str = "      ") -> str:
    """``LAYER <name> ;`` and one RECT per box (microns)."""
    rect = "".join(f"{indent}  RECT {_n(a)} {_n(b)} {_n(c)} {_n(d)} ;\n" for a, b, c, d in boxes)
    return f"{indent}LAYER {layer_name} ;\n{rect}"


def lef_pin(name: str, geometry: str, *statements: str) -> str:
    """A PIN with its statements (``DIRECTION INPUT``...) and one PORT holding ``geometry``."""
    head = "".join(f"    {s} ;\n" for s in statements)
    return f"  PIN {name}\n{head}    PORT\n{geometry}    END\n  END {name}\n"


def lef_obs(geometry: str) -> str:
    return f"  OBS\n{geometry}  END\n"


def lef_macro(name: str, *parts: str, size: tuple[float, float] | None = (1.0, 2.0),
              head: tuple[str, ...] = ("CLASS CORE",), origin: tuple[float, float] | None = (0, 0)) -> str:
    """A MACRO: ``head`` statements, ORIGIN and SIZE (None omits them), then ``parts``."""
    lines = [f"MACRO {name}\n", *(f"  {s} ;\n" for s in head)]
    if origin is not None:
        lines.append(f"  ORIGIN {_n(origin[0])} {_n(origin[1])} ;\n")
    if size is not None:
        lines.append(f"  SIZE {_n(size[0])} BY {_n(size[1])} ;\n")
    return "".join(lines) + "".join(parts) + f"END {name}\n"


def _n(value: float) -> str:
    """A number as written by hand: integers without a point, else up to 4 decimals."""
    text = f"{value:.4f}".rstrip("0").rstrip(".")
    return "0" if text == "-0" else text


DEF_HEAD = """\
VERSION 5.7 ;
DIVIDERCHAR "/" ;
BUSBITCHARS "[]" ;
"""


def def_file(design: str | None, *sections: str, units: int | None = 1000,
             diearea: tuple[int, int, int, int] | None = (0, 0, 20000, 20000), head: str = DEF_HEAD) -> str:
    """A DEF file: the header (``design`` None omits DESIGN, ``units`` None omits UNITS), the
    sections and ``END DESIGN``."""
    lines = [head]
    if design is not None:
        lines.append(f"DESIGN {design} ;\n")
    if units is not None:
        lines.append(f"UNITS DISTANCE MICRONS {units} ;\n")
    if diearea is not None:
        x1, y1, x2, y2 = diearea
        lines.append(f"DIEAREA ( {x1} {y1} ) ( {x2} {y2} ) ;\n")
    return "".join(lines) + "".join(sections) + "END DESIGN\n"


def def_section(keyword: str, count: int, *records: str) -> str:
    return f"{keyword} {count} ;\n" + "".join(f"{r}\n" for r in records) + f"END {keyword}\n"


# --- cases ------------------------------------------------------------------------------------

def output_kind(source: str) -> tuple[str, list[str]]:
    """(comparator, masks) of an output, by its name."""
    if source == CELLS or source.endswith(".mag"):
        return "bytes", ["mag-timestamp"]
    if source.endswith((".def", ".txt")):
        return "def-text", []
    if source.endswith((".lef", ".tlef")):
        return "lef-text", []
    return "lef-text", []


def writes(cmds: list[str]) -> list[str]:
    """The commands of ``cmds`` that write a file."""
    return [c for c in cmds if c == "lef writeall" or c.startswith(tuple(w + " " for w in WRITES))
            or c in ("lef write", "def write", "save")]


def history_note(cmds: list[str], history: str | None) -> list[str]:
    """Design D19 (D18 of M5): a case with more than one write is a history case that names its steps."""
    if history:
        return [f"History case (design D19): {history}."]
    steps = writes(cmds)
    if len(steps) > 1:
        return [f"History case (design D19): {len(steps)} writes in one session, compared side by side: "
                + "; ".join(steps) + "."]
    return []


def script_lines(cmds: list[str]) -> str:
    return "\n".join(c[len(RAW):] if c.startswith(RAW) else f"probe {{{c}}}" for c in cmds)


def prelude(technology: str | None, pin_datestamp: bool, pre_tech: list[str] | None = None) -> list[str]:
    """``drc off``, ``pre_tech`` (e.g. ``tech lambda``), the case technology and ``snap internal``,
    the LEF datestamp pin."""
    head = ["drc off", *(pre_tech or [])]
    if technology:
        head += [f"tech load input/{technology} -noprompt", "snap internal"]
    return head + ([DATESTAMP] if pin_datestamp else [])


def reads_lef_and_saves(body: list[str], outputs: list[str]) -> bool:
    return any(c.startswith("lef read") for c in body) and CELLS in outputs


def case(cid: str, body: list[str], *, files: dict[str, str | bytes] | None = None,
         outputs: list[str] | None = None, technology: str | None = None, tech_name: str = TECH,
         start: list[str] | None = None, notes: list[str] | None = None, datestamp: bool | None = None,
         history: str | None = None, requirements: list[str] | None = None, description: str | None = None,
         pre_tech: list[str] | None = None, masks: dict[str, list[str]] | None = None,
         **extra) -> tclcase.TclCase:
    """One LEFDEF case of CASES.md row ``cid``. ``technology`` is the case's tech file text (default
    ``tech()``), saved as ``<tech_name>.tech`` (PDK cases have none); ``start`` the commands after the
    prelude (default ``load top``); ``datestamp`` pins the LEF datestamp (default: when the body reads
    LEF and saves cells); ``pre_tech`` runs before the technology load; ``masks`` overrides the masks
    of an output; ``extra`` goes to ``TclCase`` (``pdk``, ``external``, ``fonts``)."""
    row = rows()[cid]
    files = dict(files or {})
    outs = list(outputs or [])
    if extra.get("pdk") is None:
        files.setdefault(f"{tech_name}.tech", technology if technology is not None else tech(tech_name))
    pin = reads_lef_and_saves(body, outs) if datestamp is None else datestamp
    head = prelude(f"{tech_name}.tech" if f"{tech_name}.tech" in files else None, pin, pre_tech)
    cmds = [*head, *(["load top"] if start is None else start), *body]
    kinds = {o: output_kind(o) for o in outs}
    kinds.update({o: (kinds[o][0], m) for o, m in (masks or {}).items()})
    reqs = requirements or [resolve(row.capability, r) for r in row.requirements]
    return tclcase.TclCase(
        cid, row.capability, reqs, description or f"{row.inputs}. Captures: {row.capture}.",
        script_lines(cmds), files=files, outputs=outs, output_masks={o: k[1] for o, k in kinds.items()},
        output_names={CELLS: "cells.txt"} if CELLS in outs else {},
        output_comparators={o: k[0] for o, k in kinds.items() if k[0] != "bytes"},
        stream_comparator="lef-text", stream_masks=["completed-percent"], driver=DRIVER,
        notes=["LEFDEF case (m6-lef-def-oasis design D19): fresh session, drc off, one write unless a history note "
               "names the steps.",
               *history_note(cmds, history), *(notes or [])], **extra)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--ext-plan", type=Path, metavar="FILE", help="write the ext_inputs.py plan (JSON)")
    group.add_argument("--sync-ext", type=Path, metavar="DIR", help="copy the .ext files of an ext-inputs artifact")
    group.add_argument("--dispatch-ext", action="store_true", help="print the ext-inputs dispatch command")
    parser.add_argument("cases", nargs="*", type=Path, help="with --ext-plan: limit the cases to these directories")
    args = parser.parse_args(argv)
    if args.cases and args.ext_plan is None:
        parser.error("case directories are taken only with --ext-plan")
    if args.ext_plan is not None:
        plan = ext_plan(args.cases or None)
        args.ext_plan.write_text(json.dumps(plan, indent=2) + "\n", encoding="utf-8")
        print(f"{len(plan['targets'])} target(s), {len(plan['skipped'])} case(s) skipped")
        return 0
    if args.sync_ext is not None:
        return _print_sync(args.sync_ext)
    if args.dispatch_ext:
        print(dispatch_command())
        return 0
    for path in write_all():
        print(path.relative_to(casegen.REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
