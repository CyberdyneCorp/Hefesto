#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""A placed-and-routed sky130 block in DEF, generated from the standard-cell LEF (m6-lef-def-oasis design D22).

No pinned data set ships a routed sky130 DEF, so the `def read` workload of
bench/lefdef/lefdef_bench.py reads this deterministic one. It is built from
``sky130_fd_sc_hd.lef`` alone:

- every single-height CORE macro with a signal input and a signal output is
  used, in name order, round robin, until the block has COMPONENTS cells;
- the cells abut in rows of ROW_SITES ``unithd`` sites, N and FS rows
  alternating, as a placer writes them;
- net ``n<i>`` joins the first output of cell i-1 to the first input of cell i,
  routed as a router writes it: an L1M1 via on each pin, a met1 run, and a met2
  jog with M1M2 vias when the two pins are on different rows;
- the special nets VGND and VPWR have a met1 FOLLOWPIN rail on every row
  boundary and met4 straps every STRAP_PITCH;
- two block pins, ``in`` and ``out``, drive the chain.

Coordinates are DEF database units (1000 per micron), on a 10 nm grid. The
same LEF and parameters always give the same bytes.

  python3 bench/lefdef/defgen.py sky130_fd_sc_hd.lef lefdef_block.def [--components 5000]
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path

DBU = 1000            # DEF database units per micron
GRID = 10             # database units: every coordinate is a multiple of this
SITE = "unithd"
SITE_W = 460          # unithd site, database units
ROW_H = 2720
ROW_SITES = 540       # sites per row (248.4 um)
COMPONENTS = 5000
STRAP_PITCH = 50_000  # met4 strap pitch, database units
STRAP_W = 1600
RAIL_W = 480
DESIGN = "lefdef_block"


@dataclass
class Macro:
    name: str
    width: int = 0          # database units
    height: int = 0
    core: bool = False
    inputs: list[tuple[str, int, int]] = field(default_factory=list)   # (pin, x, y) of its first RECT centre
    outputs: list[tuple[str, int, int]] = field(default_factory=list)

    def usable(self) -> bool:
        return self.core and self.height == ROW_H and bool(self.inputs) and bool(self.outputs)


def dbu(text: str) -> int:
    return round(float(text) * DBU)


def snap(value: float) -> int:
    return int(round(value / GRID)) * GRID


@dataclass
class _Pin:
    name: str
    direction: str = ""
    use: str = "SIGNAL"
    centre: tuple[int, int] | None = None


class _LefScanner:
    """A line scanner of the MACRO, PIN and RECT statements of a cell LEF (nothing else is needed)."""

    def __init__(self) -> None:
        self.macros: list[Macro] = []
        self.macro: Macro | None = None
        self.pin: _Pin | None = None

    def line(self, words: list[str]) -> None:
        if not words:
            return
        if words[0] == "MACRO" and len(words) > 1:
            self.macro, self.pin = Macro(words[1]), None
            self.macros.append(self.macro)
        elif self.macro is not None:
            self.in_macro(self.macro, words)

    def in_macro(self, macro: Macro, words: list[str]) -> None:
        key = words[0]
        if key == "CLASS":
            macro.core = len(words) > 1 and words[1] == "CORE"
        elif key == "SIZE" and len(words) >= 4:
            macro.width, macro.height = dbu(words[1]), dbu(words[3])
        elif key == "PIN" and len(words) > 1:
            self.pin = _Pin(words[1])
        elif self.pin is not None:
            self.in_pin(macro, self.pin, words)

    def in_pin(self, macro: Macro, pin: _Pin, words: list[str]) -> None:
        key = words[0]
        if key == "DIRECTION" and len(words) > 1:
            pin.direction = words[1]
        elif key == "USE" and len(words) > 1:
            pin.use = words[1]
        elif key == "RECT" and pin.centre is None and len(words) >= 5:
            x0, y0, x1, y1 = (float(w) for w in words[1:5])
            pin.centre = (snap((x0 + x1) * DBU / 2), snap((y0 + y1) * DBU / 2))
        elif key == "END" and len(words) > 1 and words[1] == pin.name:
            self.close_pin(macro, pin)
            self.pin = None

    @staticmethod
    def close_pin(macro: Macro, pin: _Pin) -> None:
        if pin.use != "SIGNAL" or pin.centre is None:
            return
        entry = (pin.name, *pin.centre)
        if pin.direction == "INPUT":
            macro.inputs.append(entry)
        elif pin.direction == "OUTPUT":
            macro.outputs.append(entry)


def parse_lef(text: str) -> list[Macro]:
    """The macros of a cell LEF, in file order."""
    scanner = _LefScanner()
    for raw in text.splitlines():
        scanner.line(raw.split("#", 1)[0].replace(";", " ").split())
    return scanner.macros


@dataclass
class Placed:
    name: str
    macro: Macro
    x: int
    y: int
    flipped: bool   # FS row

    def pin_at(self, pin: tuple[str, int, int]) -> tuple[int, int]:
        _, px, py = pin
        return self.x + px, self.y + (self.macro.height - py if self.flipped else py)


def place(macros: list[Macro], count: int) -> tuple[list[Placed], int, int]:
    """`count` cells round robin over `macros`, in abutting rows; returns them, the core width and the rows."""
    row_width = ROW_SITES * SITE_W
    placed: list[Placed] = []
    x = row = 0
    for index in range(count):
        macro = macros[index % len(macros)]
        if x + macro.width > row_width:
            x, row = 0, row + 1
        placed.append(Placed(f"u{index}", macro, x, row * ROW_H, row % 2 == 1))
        x += macro.width
    return placed, row_width, row + 1


def components_section(placed: list[Placed]) -> list[str]:
    lines = [f"COMPONENTS {len(placed)} ;"]
    lines += [f"- {c.name} {c.macro.name} + PLACED ( {c.x} {c.y} ) {'FS' if c.flipped else 'N'} ;" for c in placed]
    return lines + ["END COMPONENTS"]


def rows_section(rows: int) -> list[str]:
    return [f"ROW ROW_{r} {SITE} 0 {r * ROW_H} {'FS' if r % 2 else 'N'} DO {ROW_SITES} BY 1 STEP {SITE_W} 0 ;"
            for r in range(rows)]


def pins_section(placed: list[Placed], height: int) -> list[str]:
    first, last = placed[0], placed[-1]
    in_x = first.pin_at(first.macro.inputs[0])[0]
    out_x = last.pin_at(last.macro.outputs[0])[0]
    return ["PINS 2 ;",
            f"- in + NET in + DIRECTION INPUT + USE SIGNAL + LAYER met2 ( -140 0 ) ( 140 280 ) "
            f"+ PLACED ( {in_x} 0 ) N ;",
            f"- out + NET out + DIRECTION OUTPUT + USE SIGNAL + LAYER met2 ( -140 -280 ) ( 140 0 ) "
            f"+ PLACED ( {out_x} {height} ) N ;",
            "END PINS"]


def special_net(name: str, use: str, parity: int, width: int, rows: int, height: int) -> list[str]:
    """VGND on the even row boundaries, VPWR on the odd ones (N and FS rows alternate), and met4 straps."""
    shapes = [f"met1 {RAIL_W} + SHAPE FOLLOWPIN ( 0 {k * ROW_H} ) ( {width} * )"
              for k in range(rows + 1) if k % 2 == parity]
    offset = STRAP_PITCH // 2 if parity else STRAP_PITCH // 4
    shapes += [f"met4 {STRAP_W} + SHAPE STRIPE ( {x} 0 ) ( * {height} )" for x in range(offset, width, STRAP_PITCH)]
    lines = [f"- {name} ( * {name} ) + USE {use}", f"  + ROUTED {shapes[0]}"]
    lines += [f"    NEW {shape}" for shape in shapes[1:]]
    return lines + ["  ;"]


def route(start: tuple[int, int], end: tuple[int, int]) -> str:
    (x1, y1), (x2, y2) = start, end
    if y1 == y2:
        return (f"+ ROUTED li1 ( {x1} {y1} ) L1M1_PR_MR NEW met1 ( {x1} {y1} ) ( {x2} * ) "
                f"NEW li1 ( {x2} {y2} ) L1M1_PR_MR")
    return (f"+ ROUTED li1 ( {x1} {y1} ) L1M1_PR_MR NEW met1 ( {x1} {y1} ) ( {x2} * ) M1M2_PR "
            f"NEW met2 ( {x2} {y1} ) ( * {y2} ) M1M2_PR NEW li1 ( {x2} {y2} ) L1M1_PR_MR")


def nets_section(placed: list[Placed]) -> list[str]:
    first, last = placed[0], placed[-1]
    lines = [f"NETS {len(placed) + 1} ;",
             f"- in ( PIN in ) ( {first.name} {first.macro.inputs[0][0]} ) + USE SIGNAL ;"]
    for driver, sink in zip(placed, placed[1:]):
        out, inp = driver.macro.outputs[0], sink.macro.inputs[0]
        lines.append(f"- n{sink.name[1:]} ( {driver.name} {out[0]} ) ( {sink.name} {inp[0]} ) + USE SIGNAL")
        lines.append(f"  {route(driver.pin_at(out), sink.pin_at(inp))} ;")
    lines.append(f"- out ( {last.name} {last.macro.outputs[0][0]} ) ( PIN out ) + USE SIGNAL ;")
    return lines + ["END NETS"]


def generate(lef_text: str, count: int = COMPONENTS) -> str:
    macros = sorted((m for m in parse_lef(lef_text) if m.usable()), key=lambda m: m.name)
    if not macros:
        raise ValueError("no single-height CORE macro with a signal input and output in the LEF")
    if count < 1:
        raise ValueError("the block needs at least one component")
    placed, width, rows = place(macros, count)
    height = rows * ROW_H
    lines = ["VERSION 5.8 ;", 'DIVIDERCHAR "/" ;', 'BUSBITCHARS "[]" ;', f"DESIGN {DESIGN} ;",
             f"UNITS DISTANCE MICRONS {DBU} ;", f"DIEAREA ( 0 0 ) ( {width} {height} ) ;"]
    lines += rows_section(rows)
    lines += components_section(placed)
    lines += pins_section(placed, height)
    lines += ["SPECIALNETS 2 ;", *special_net("VGND", "GROUND", 0, width, rows, height),
              *special_net("VPWR", "POWER", 1, width, rows, height), "END SPECIALNETS"]
    lines += nets_section(placed)
    return "\n".join(lines + ["END DESIGN", ""])


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("lef", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--components", type=int, default=COMPONENTS)
    args = parser.parse_args(argv)
    try:
        args.output.write_text(generate(args.lef.read_text(), args.components))
    except (OSError, ValueError) as error:
        print(f"defgen: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
