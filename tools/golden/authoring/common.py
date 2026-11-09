# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Shared inputs and command helpers of the M1 case specs."""
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

SCMOS_EXTERNAL = {"source": "scmos", "to": "scmos.tech"}
SKY130_TECH_EXTERNAL = {"source": "pdk", "path": "sky130A/libs.tech/magic/sky130A.tech", "to": "sky130A.tech"}


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
