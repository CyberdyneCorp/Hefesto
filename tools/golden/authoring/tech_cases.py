# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""TECH case specs (tests/golden/TECH/CASES.md) not hand-written earlier.

The table dumps (G20, G23, G24, G25, G28) use paint_dump.py (method recorded there).
"""
from __future__ import annotations

from casegen import Spec
from common import SCMOS_EXTERNAL, SKY130_TECH_EXTERNALS, TINY, box, erase, paint, tech_load
from paint_dump import TechType, dump_cmds, pairs, tech_types

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from goldenlib import external  # noqa: E402

FILE_FORMAT = "tech-file-format"
COMPOSE = "tech-compose-and-connect"
LOADING = "tech-loading-and-scaling"

NOT_AUTHORED = {
    "TECH-G29": "needs a graphics session (style mapping)",
    "TECH-G33": "partial state after a failed load differs by design (Q-TECH-10)",
    "TECH-G39": "startup .magicrc scripts do not run in hefesto-cli yet (app-startup, 4.3 deviation)",
    "TECH-G40": "needs PDK_ROOT in the environment of both tools",
}

TINY_DUMP_TYPES = ["nwell", "poly", "ndiff", "nfet", "metal1", "metal2", "pc", "ndc", "m2c", "pwell", "pm12c"]

TINY_CC = TINY.replace("    active  ndcontact,ndc\n", "    active  ndcontact,ndc\n    active  pdiffusion,pdiff\n"
                       "    active  pdcontact,pdc\n").replace(
    "    ndc  ndiff metal1\n", "    ndc  ndiff metal1\n    pdc  pdiff metal1\n").replace(
    "    compose nfet poly ndiff\n", "    compose nfet poly ndiff\n    compose pdc pdiff m1\n")

SMALL = """\
tech
    format 35
    small
end

planes
    active,a
    metal1,m1
end

types
    active  polysilicon,poly
    metal1  metal1,m1
end

contact
end

styles
    styletype mos
end

compose
end

connect
end

cifoutput
end

cifinput
end

drc
end

extract
end
"""

SCMOS_LIKE = """\
tech
    format 35
    sl
end

planes
    well,w
    active,a
    metal1,m1
end

types
    well    pwell,pw
    well    nwell,nw
    active  pdiffusion,pdiff
    active  ndiffusion,ndiff
    metal1  metal1,m1
    active  pdcontact,pdc
    active  ndcontact,ndc
end

contact
    pdc pdiff metal1
    ndc ndiff metal1
end

styles
    styletype mos
end

compose
    erase pdc pwell pdiff
end

connect
end

cifoutput
end

cifinput
end

drc
end

extract
end
"""


def tiny_with(old: str, new: str) -> str:
    if old not in TINY:
        raise ValueError(f"tiny.tech has no {old!r}")
    return TINY.replace(old, new)


def _cif(body: str) -> str:
    return tiny_with("cifoutput\nend\n", f"cifoutput\nstyle test\n{body}end\n")


def tiny_dump(prefix: list[str] | None = None) -> list[str]:
    selected = [(h, t) for h in range(len(TINY_DUMP_TYPES)) for t in range(len(TINY_DUMP_TYPES))]
    return ((prefix or tech_load("tiny")) + dump_cmds(TINY_DUMP_TYPES, selected, "paint", "paint_dump")
            + dump_cmds(TINY_DUMP_TYPES, selected, "erase", "erase_dump"))


def big_dump(tech: str, types: list[TechType]) -> list[str]:
    names, selected = [t.name for t in types], pairs(types, all_pairs=False)
    return tech_load(tech) + dump_cmds(names, selected, "paint", "paint_dump") + dump_cmds(
        names, selected, "erase", "erase_dump")


def dump_spec(cid: str, desc: str, cmds: list[str], files=None, ext=None, reqs=None) -> Spec:
    return Spec(cid, COMPOSE, reqs or ["Finalization step 3a, painting over a contact", "Finalization step 3c, stacked types as applied types", "Default paint and erase rules"],
                desc, cmds, files=files or {}, outputs=["paint_dump.mag", "erase_dump.mag"], streams=False,
                external=(ext if isinstance(ext, list) else [ext]) if ext else [], notes=["Table dump through paint/erase/save: authoring/paint_dump.py."])


def _external_types(ext: dict) -> list[TechType]:
    return tech_types(external.resolve(external.parse_external(ext, 0)).read_text(encoding="utf-8"))


def format_specs() -> list[Spec]:
    tech_variants = {"t34": "format 34", "t33": "format 33", "tx40": "formatX 40", "tabc": "format abc",
                     "tabc3": "a b c", "tnone": ""}
    files = {f"{n}.tech": tiny_with("    format 35\n", f"    {line}\n" if line else "") for n, line in tech_variants.items()}
    loads = [c for n in tech_variants for c in (f"tech load input/{n} -noprompt", "tech name")]
    return [
        Spec("TECH-G08", FILE_FORMAT, ["include directive"],
             "include of a sibling file from the tech directory; include of a file with no .tech suffix.",
             ["tech load input/tiny_inc -noprompt", "tech name", "tech load input/tiny2 -noprompt", "tech name"],
             files={"tiny_inc.tech": tiny_with("drc\nend\n", "include tiny-drc.tech\n"), "tiny-drc.tech": "drc\nend\n",
                    "tiny2.tech": tiny_with("drc\nend\n", "include common.inc\n"), "common.inc": "drc\nend\n"}),
        Spec("TECH-G09", FILE_FORMAT, ["include directive"], "A missing include inside connect (Q-TECH-2).",
             ["tech load input/tiny -noprompt"],
             files={"tiny.tech": tiny_with("    m2,m2c       m2,m2c\n", "    m2,m2c       m2,m2c\n    include nothere.tech\n")}),
        Spec("TECH-G11", FILE_FORMAT, ["tech section", "Format version gating"],
             "tech sections format 34, format 33, formatX 40, format abc, 'a b c' and none (extract query not authored).",
             loads, files=files),
    ]


def compose_specs() -> list[Spec]:
    def cell_case(cid, reqs, desc, body, tech="tiny", files=None, outputs=("a.mag",)):
        files = files if files is not None else {"tiny.tech": TINY}
        return Spec(cid, COMPOSE, reqs, desc, tech_load(tech) + ["load a"] + body + ["save a"], files=files,
                    outputs=list(outputs))
    stack_none = tiny_with("    stackable pc m2c pm12c\n", "    stackable\n")
    return [
        Spec("TECH-G18", "tech-contacts-and-stacking", ["stackable statement", "Stacking eligibility",
             "Stacked type properties"], "stackable with no arguments: stacked types saved in a .mag; pm12c lookup.",
             tech_load("tiny") + ["load a"] + paint("pc", 0, 0, 4, 4) + paint("m2c", 0, 0, 4, 4)
             + paint("ndc", 10, 0, 14, 4) + paint("m2c", 10, 0, 14, 4) + ["save a", "tech layers pm12c", "tech layers *"],
             files={"tiny.tech": stack_none}, outputs=["a.mag"]),
        cell_case("TECH-G19", ["Finalization step 3a, painting over a contact"], "ndiff painted over a 4x4 pc (Q-TECH-23).",
                  paint("pc", 0, 0, 4, 4) + paint("ndiff", 0, 0, 4, 4)),
        dump_spec("TECH-G20", "tiny.tech: full P/E dump over the user types and pm12c.", tiny_dump(),
                  files={"tiny.tech": TINY}),
        cell_case("TECH-G21", ["Explicit paint and erase rules", "Contact locking"],
                  "scmos-like tech with an explicit 'erase pdc pwell pdiff': paint pdc, erase pwell (Q-TECH-20).",
                  paint("pdc", 0, 0, 4, 4) + paint("pwell", 0, 0, 4, 4) + erase("pwell", 0, 0, 4, 4),
                  tech="sl", files={"sl.tech": SCMOS_LIKE}),
        Spec("TECH-G22", COMPOSE, ["Finalization step 3a, painting over a contact", "Finalization step 3c, stacked types as applied types"],
             "m2c over pc (a); pc,m2c on empty space (b); erase pc,m2c (c).",
             tech_load("tiny") + ["load a"] + paint("pc", 0, 0, 4, 4) + paint("m2c", 0, 0, 4, 4) + ["save a", "load b"]
             + paint("pc,m2c", 0, 0, 4, 4) + ["save b", "load c"] + paint("pc,m2c", 0, 0, 4, 4)
             + erase("pc,m2c", 0, 0, 4, 4) + ["save c"], files={"tiny.tech": TINY}, outputs=["a.mag", "b.mag", "c.mag"]),
        dump_spec("TECH-G23", "scmos.tech (generated from Magic's sources): P/E dump of intersecting-plane pairs.",
                  big_dump("scmos", _external_types(SCMOS_EXTERNAL)), ext=SCMOS_EXTERNAL),
        dump_spec("TECH-G24", "sky130A.tech: P/E dump of intersecting-plane pairs (connect table not authored).",
                  big_dump("sky130A", _external_types(SKY130_TECH_EXTERNALS[0])), ext=SKY130_TECH_EXTERNALS),
        dump_spec("TECH-G25", "tiny.tech plus pdc and 'compose pdc pdiff m1': P/E dump of pdc, pdiff, metal1.",
                  tech_load("tiny") + dump_cmds(["pdc", "pdiff", "metal1"], [(h, t) for h in range(3) for t in range(3)],
                                                "paint", "paint_dump")
                  + dump_cmds(["pdc", "pdiff", "metal1"], [(h, t) for h in range(3) for t in range(3)], "erase", "erase_dump"),
                  files={"tiny.tech": TINY_CC}, reqs=["Contact compose and decompose rules", "Finalization step 4, saved contact rules"]),
        Spec("TECH-G26", COMPOSE, ["connect section", "Connectivity finalization"],
             "poly connected to pc, pc to metal1, poly touching metal1: select net from the poly, then from metal1.",
             tech_load("tiny") + ["load a"] + paint("poly", 0, 0, 10, 4) + paint("pc", 10, 0, 14, 4)
             + paint("metal1", 10, 0, 30, 4) + paint("poly", 30, 0, 40, 4)
             + [box(1, 1, 1, 1), "select net", "what -list", "select bbox", "select clear", box(25, 2, 25, 2),
                "select net", "what -list", "select bbox"], files={"tiny.tech": TINY}),
        cell_case("TECH-G27", ["Contact locking"], "tech locked pc; erase metal1 over a pc tile.",
                  ["tech locked pc"] + paint("pc", 0, 0, 4, 4) + erase("metal1", 0, 0, 4, 4)),
        dump_spec("TECH-G28", "tiny, then tiny with a contact compose rule, then tiny again: P/E dump (Q-TECH-19).",
                  tiny_dump(["tech load input/tiny -noprompt", "tech load input/tiny_cc -noprompt"] + tech_load("tiny")),
                  files={"tiny.tech": TINY, "tiny_cc.tech": TINY_CC}, reqs=["Contact compose and decompose rules"]),
    ]


def loading_specs() -> list[Spec]:
    drc = tiny_with("drc\nend\n", 'drc\n    width metal1 3 "metal1 width < 3"\nend\n')
    return [
        Spec("TECH-G30", "tech-styles-section", ["styletype statement"],
             "styletype naming a family that does not exist, in a -dnull session (Q-TECH-24).",
             ["tech load input/tiny -noprompt", "tech name"],
             files={"tiny.tech": tiny_with("styletype mos", "styletype nosuchfamily")}),
        Spec("TECH-G34", LOADING, ["Effect on existing layout"],
             "Cell with metal2 paint; reload a technology without metal2.",
             tech_load("tiny") + ["load a"] + paint("metal2", 0, 0, 10, 10) + paint("metal1", 0, 0, 5, 5)
             + ["save a1", "tech load input/small -noprompt", "save a2"],
             files={"tiny.tech": TINY, "small.tech": SMALL}, outputs=["a1.mag", "a2.mag"]),
        Spec("TECH-G35", LOADING, ["Post-load adjustments", "Internal grid scale"],
             "scalegrid 1 2, then tech load of a tech with a drc width rule of 3 (tech drc query not authored).",
             ["scalegrid 1 2", "tech load input/tiny -noprompt", "tech lambda"], files={"tiny.tech": drc}),
        Spec("TECH-G36", LOADING, ["scalegrid command"],
             "box 0 0 3 5 and a tile edge at x=-3; scalegrid 1 2; scalegrid 2 1; scalegrid 2/4, 1:3, 0 2.",
             tech_load("tiny") + ["load a"] + paint("metal1", -3, 0, 3, 5) + [box(0, 0, 3, 5), "scalegrid 1 2", "box",
             "save a1", "scalegrid 2 1", "box", "save a2", "scalegrid 2/4", "box", "scalegrid 1:3", "box",
             "scalegrid 0 2", "box"], files={"tiny.tech": TINY}, outputs=["a1.mag", "a2.mag"]),
        Spec("TECH-G37", LOADING, ["Grid limit check"], "cifoutput scalefactor 5, gridlimit 5; scalegrid 1 2 and 1 20.",
             ["tech load input/tiny -noprompt", "scalegrid 1 2", "scalegrid 1 20"],
             files={"tiny.tech": _cif("    scalefactor 5\n    gridlimit 5\n")}),
        Spec("TECH-G38", LOADING, ["Post-load adjustments"], "cifoutput with options set-minimum-grid; tech lambda.",
             ["tech load input/tiny -noprompt", "tech lambda"],
             files={"tiny.tech": _cif("    scalefactor 10\n    gridlimit 5\n    options set-minimum-grid\n")}),
        Spec("TECH-G45", "tech-command", ["tech load"],
             "Painted layout; tech load -quiet (usage) and -noprompt (the 'no' dialog answer is not authored).",
             tech_load("tiny") + ["load a"] + paint("metal1", 0, 0, 4, 4)
             + ["tech load input/tiny -quiet", "tech load input/tiny -noprompt", "tech name"], files={"tiny.tech": TINY}),
    ]


def specs() -> list[Spec]:
    return format_specs() + compose_specs() + loading_specs()


# --- M3 (m3-drc design D12, written by m3_cases.py) --------------------------------

M3_NOT_AUTHORED: dict[str, str] = {}


def m3_specs() -> list[Spec]:
    queries = ["tech drc width m1", "tech drc spacing m1", "tech drc spacing m1 m2", "tech drc surround m2c m1",
               "tech drc direc m2c m1", "tech drc surround m1", "tech drc width nosuch", "tech drc"]
    spec = Spec("TECH-G44", "tech-command", ["tech drc"],
                "scmos.tech; tech drc width/spacing/surround/directional queries, a bad type and no option.",
                tech_load("scmos") + queries, external=[SCMOS_EXTERNAL])
    spec.keep_checkpaint = True
    # `tech drc width nosuch` prints TechError's stale "<file>: line <n>:" prefix, n being
    # scmos.tech's line count + 1; the generated scmos.tech differs in blank lines between
    # preprocessors (tests/data/tech/generate_scmos.cmake), so the number is masked.
    spec.stream_masks = ["regex:scmos.tech: line [0-9]+:"]
    spec.notes.append("The scmos.tech line number in TechError's prefix depends on the preprocessor that")
    spec.notes.append("generated the file, so it is masked (regex mask).")
    return [spec]
