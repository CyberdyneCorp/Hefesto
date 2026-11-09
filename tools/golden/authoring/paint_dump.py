# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Paint/erase table dump through layout commands (TECH-G20, G23, G24, G25, G28).

TECH/CASES.md asks for the paint and erase tables P[p](h,t) and E[p](h,t) of a
technology, captured with "a small Tcl helper that paints t over h on a 1x1 box in a
scratch cell" (method recorded here, as CASES.md requires). Neither Magic nor
hefesto-cli has a table query both share (Magic's ``*showtech`` has no Hefesto
counterpart), so the dump goes through ``paint``, ``erase`` and ``save``:

* every pair (h, t) gets its own 1x1 box at ``(3*index(t), 3*index(h))`` (one empty
  unit between boxes, so no two results touch);
* the paint dump paints h, then t; the erase dump paints h, then erases t;
* each dump is saved as one ``.mag`` file, whose paint sections list, plane by plane
  and in tile order, the type left on every box: the table entries of every plane h
  and t touch, including contact images and residues.

The command list is flat (no Tcl loop), so Magic and hefesto-cli run exactly the
same commands. For a large technology only pairs whose planes intersect are dumped
(a type painted on a plane h does not touch is no table entry of h).
"""
from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class TechType:
    name: str
    planes: frozenset[str]


def _sections(text: str) -> dict[str, list[list[str]]]:
    """Token lines of each section (comments and continuations handled simply)."""
    sections: dict[str, list[list[str]]] = {}
    current = None
    for raw in text.replace("\\\n", " ").splitlines():
        tokens = raw.split("#", 1)[0].split()
        if not tokens:
            continue
        if current is None:
            current = tokens[0]
            sections.setdefault(current, [])
        elif tokens == ["end"]:
            current = None
        else:
            sections[current].append(tokens)
    return sections


def _type_names(types: list[list[str]]) -> tuple[list[tuple[str, str]], dict[str, str]]:
    ordered, alias = [], {}
    for tokens in types:
        if len(tokens) < 2:
            continue
        names = tokens[1].split(",")
        ordered.append((names[0], tokens[0].lstrip("-")))
        alias.update({n: names[0] for n in names})
    return ordered, alias


def tech_types(text: str) -> list[TechType]:
    """User types in declaration order, each with the planes it occupies."""
    sections = _sections(text)
    ordered, alias = _type_names(sections.get("types", []))
    plane_of = dict(ordered)
    planes = {name: {plane} for name, plane in ordered}
    for tokens in sections.get("contact", []):
        head = tokens[1:] if tokens[0] == "contact" else tokens
        if head[0] in ("stackable", "lock") or alias.get(head[0]) not in planes:
            continue
        for residue in head[1:]:
            if alias.get(residue) in plane_of:
                planes[alias[head[0]]].add(plane_of[alias[residue]])
    return [TechType(name, frozenset(planes[name])) for name, _ in ordered]


def pairs(types: list[TechType], all_pairs: bool) -> list[tuple[int, int]]:
    """(index of h, index of t) for every dumped pair."""
    return [(hi, ti) for hi, h in enumerate(types) for ti, t in enumerate(types)
            if all_pairs or h.planes & t.planes]


def dump_cmds(names: list[str], selected: list[tuple[int, int]], mode: str, cell: str) -> list[str]:
    """Commands of one dump (mode 'paint' or 'erase') saved as ``<cell>.mag``."""
    cmds = [f"load {cell}"]
    for hi, ti in selected:
        x, y = 3 * ti, 3 * hi
        cmds += [f"box {x} {y} {x + 1} {y + 1}", f"paint {names[hi]}", f"{mode} {names[ti]}"]
    return cmds + [f"save {cell}"]
