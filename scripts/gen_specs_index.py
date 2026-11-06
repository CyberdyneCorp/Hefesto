#!/usr/bin/env python3
"""Regenerate docs/specs-index.md from openspec/specs, grouped by area.

Every capability must match exactly one area prefix; the script fails otherwise,
so new capabilities cannot be silently left out of the index.
"""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SPECS = ROOT / "openspec" / "specs"
DECISIONS = ROOT / "docs" / "decisions" / "open-questions"

# (title, area key or None for product specs, capability-name prefixes)
AREAS = [
    ("Hefesto product (native)", None, ["hefesto-"]),
    ("Technology files", "TECH", ["tech-"]),
    ("Cell files (.mag)", "CELL", ["cell-"]),
    ("Layout data model", "MODEL", ["layout-"]),
    ("Design rule checking", "DRC", ["drc-"]),
    ("Mask generation, GDS, CIF", "MASK", ["mask-output", "mask-input", "mask-hier", "gds-", "cif-"]),
    ("OASIS", "OASIS", ["oasis-"]),
    ("LEF / DEF", "LEFDEF", ["lef-", "def-"]),
    ("Circuit extraction", "EXTRACT", ["extract-"]),
    ("Netlists and extresist", "NETLIST", ["ext-flattening", "netlist-spice", "netlist-sim", "netlist-extcheck", "extresist-"]),
    ("LVS (netgen-compatible)", "LVS", ["lvs-"]),
    ("Embedded simulation (ngspice)", "SIM", ["simulation-"]),
    ("AI layout agent", "AGENT", ["agent-"]),
    ("Editing commands", "CMDEDIT", ["cmd-"]),
    ("UI, display, startup", "UI", ["ui-command", "ui-windows", "ui-tools", "ui-color", "ui-feedback", "ui-wizard", "display-", "app-"]),
    ("Scripting, PCells, Tcl tools", "SCRIPT", ["scripting-", "pcell-", "ui-box", "ui-gui", "readspice"]),
    ("Antenna, mask hints, fonts, 3D, plot", "MISC", ["antenna-", "mask-hints", "font-", "view-", "plot-"]),
    ("Legacy: routers, plow, wiring, netlist window, IRSIM", "LEGACY", ["router-", "plow", "wiring-", "netlist-window", "irsim-", "scheme-", "openaccess"]),
]


def count(text: str, pattern: str) -> int:
    return len(re.findall(pattern, text, re.M))


def purpose(text: str) -> str:
    match = re.search(r"## Purpose\s*\n+(.+?)(?:\n\n|\n##)", text, re.S)
    summary = " ".join(match.group(1).split()) if match else ""
    summary = re.sub(r"\(Magic ref:[^)]*\)", "", summary).replace("|", "/")
    return summary[:180] + "…" if len(summary) > 180 else summary


def area_header(key: str | None) -> str:
    if key is None:
        return "Decisions: [product.md](decisions/product.md) · [cross-cutting.md](decisions/cross-cutting.md) · [roadmap](roadmap.md)"
    questions = count((DECISIONS / f"{key}.md").read_text(), r"^### Q-")
    return (f"Open questions: [{questions}](decisions/open-questions/{key}.md) · "
            f"Golden cases: [CASES.md](../tests/golden/{key}/CASES.md)")


def main() -> int:
    texts = {p.parent.name: p.read_text() for p in sorted(SPECS.glob("*/spec.md"))}
    used: set[str] = set()
    body: list[str] = []
    total_req = total_scen = 0
    for title, key, prefixes in AREAS:
        selected = [c for c in texts if c not in used and any(c.startswith(p) for p in prefixes)]
        used.update(selected)
        body += [f"## {title}" + (f" ({key})" if key else ""), "", area_header(key), "",
                 "| Capability | Req | Scen | Purpose |", "|---|---|---|---|"]
        for cap in selected:
            req, scen = count(texts[cap], r"^### Requirement"), count(texts[cap], r"^#### Scenario")
            total_req, total_scen = total_req + req, total_scen + scen
            body.append(f"| [`{cap}`](../openspec/specs/{cap}/spec.md) | {req} | {scen} | {purpose(texts[cap])} |")
        body.append("")
    missing = sorted(set(texts) - used)
    if missing:
        print(f"ERROR: capabilities matching no area: {', '.join(missing)}", file=sys.stderr)
        return 1
    questions = sum(count(p.read_text(), r"^### Q-") for p in DECISIONS.glob("*.md"))
    header = ["# Hefesto Specification Index", "",
              "Normative specs live in `openspec/specs/`; conventions in `docs/spec-conventions.md`; delivery plan in `docs/roadmap.md`.",
              "Each area has an open-questions log (`docs/decisions/open-questions/<AREA>.md`) and a golden-case inventory (`tests/golden/<AREA>/CASES.md`).", "",
              f"**Totals:** {len(texts)} capabilities, {total_req} requirements, {total_scen} scenarios, {questions} open questions.", ""]
    (ROOT / "docs" / "specs-index.md").write_text("\n".join(header + body))
    print(f"{len(texts)} capabilities, {total_req} requirements")
    return 0


if __name__ == "__main__":
    sys.exit(main())
