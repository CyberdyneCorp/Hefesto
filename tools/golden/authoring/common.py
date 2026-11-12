# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Shared inputs and command helpers of the M1 and M2 case specs."""
from __future__ import annotations

from casegen import GOLDEN

# tiny.tech: capability tech-file-format, requirement "Complete annotated example"
# (the same file as the hand-written TECH cases).
TINY = (GOLDEN / "TECH" / "cases" / "TECH-G13" / "input" / "tiny.tech").read_text(encoding="utf-8")

# The MODEL test technology `mt` of tests/golden/MODEL/CASES.md.
MT = """\
tech
    format 35
    mt
end

planes
    active,a
    metal1,m1
    metal2,m2
    metal3,m3
end

types
    active  ndiffusion,ndiff
    active  pdiffusion,pdiff
    active  polysilicon,poly
    active  ntransistor,nfet
    metal1  metal1,m1
    metal2  metal2,m2
    metal3  metal3,m3
    active  pdcontact,pdc
    metal1  via1
    metal2  via2
end

contact
    pdc   pdiff  metal1
    via1  metal1 metal2
    via2  metal2 metal3
    stackable via1 via2
end

styles
    styletype mos
end

compose
    compose nfet poly ndiff
end

connect
    metal1 via1
    metal2 via1,via2
    metal3 via2
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

# The `tech: fine` test technology of tests/golden/CMDEDIT/CASES.md (m2 design D9):
# tiny.tech with an output style of 2 centimicrons per lambda and a 1 centimicron grid
# limit, whose `set-minimum-grid` option makes lambda 2 internal units of U = 10 nm.
# (`scalefactor 20 nanometers` with `gridlimit 10` makes Magic set lambda to 1:0 and
# crash with SIGFPE on the next command.)
FINE = TINY.replace("    tiny\nend", "    fine\nend", 1).replace(
    "cifoutput\nend\n", "cifoutput\nstyle fine\n    scalefactor 2\n    gridlimit 1\n"
    "    options set-minimum-grid\nend\n", 1)

SCMOS_EXTERNAL = {"source": "scmos", "to": "scmos.tech"}
# The `include` part files of a PDK technology file (open_pdks since c6df28c1; m4-scripting
# design D15): Magic reads them from the directory of the main file, so every case that
# copies a PDK tech file in also copies its parts.
TECH_PARTS = ("cifout", "cifin", "drc", "extract")


def pdk_tech_externals(pdk_set: str, variant: str) -> list[dict]:
    """The ``[[external]]`` inputs of ``<variant>.tech`` and its part files (``set`` omitted
    for the default sky130 data set)."""
    base = {} if pdk_set == "sky130" else {"set": pdk_set}
    directory = f"{variant}/libs.tech/magic"
    names = [f"{variant}.tech", *(f"{variant}-{part}.tech" for part in TECH_PARTS)]
    return [{"source": "pdk", **base, "path": f"{directory}/{name}", "to": name} for name in names]


SKY130_TECH_EXTERNALS = pdk_tech_externals("sky130", "sky130A")


def sky130_cell_external(cell: str) -> dict:
    return {"source": "pdk", "path": f"sky130A/libs.ref/sky130_fd_sc_hd/mag/{cell}.mag", "to": f"{cell}.mag"}


def box(x1: int, y1: int, x2: int, y2: int) -> str:
    return f"box {x1} {y1} {x2} {y2}"


def paint(layer: str, *rect: int) -> list[str]:
    return [box(*rect), f"paint {layer}"]


def erase(layer: str, *rect: int) -> list[str]:
    return [box(*rect), f"erase {layer}"]


def split(direction: str, layer: str, *rect: int) -> list[str]:
    return [box(*rect), f"splitpaint {direction} {layer}"]


def label(text: str, x: int, y: int, *rest: str) -> list[str]:
    return [box(x, y, x, y), " ".join(["label", text, *rest])]


def tech_load(name: str) -> list[str]:
    return [f"tech load input/{name} -noprompt", "snap internal"]


def mag(tech: str, body: str, timestamp: int = 100) -> str:
    """A .mag file: header, the given body lines and << end >>."""
    return f"magic\ntech {tech}\ntimestamp {timestamp}\n{body}<< end >>\n"


def flatten_save(source_cell: str, dest: str, *options: str) -> list[str]:
    """Flatten the edit cell ``source_cell`` into ``dest`` and save ``dest``."""
    return [f"load {source_cell}", " ".join(["flatten", *options, dest]), f"load {dest}", f"save {dest}"]
