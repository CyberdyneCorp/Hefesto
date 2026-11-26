#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Reference runs of task 3.3 (openspec/changes/m9-lvs-simulation): D-22, D-26 and D-27.

Runs the Hefesto-authored decks of decks/ with the reference ngspice of the capture image
(ngspice 47-hefesto2: the engine's pin and patch series, including patch 0005) against the
pinned ngspice model sets, and selects the branch of each conditional rule approved at G0:

  Q-SIMADV-3  sky130: does a top-level `.param MC_MM_SWITCH=1` after `.lib ... mc` take effect?
  Q-SIMADV-4  gf180mcu: does `.param sw_stat_*` after `.include design.ngspice` take effect
              (D-22)? Does `ngbehavior=hsa` fix a difference from `hs` (D-27)? Installed file
              names and the models' tnom.
  Q-SIMADV-9  does the last top-level definition of a name win over the one in an included
              file, in all three PDKs?
  Q-SIM-8     do the descriptor values load (files, sections, OSDI modules), and does sky130
              `combined` fail without `ng_nomodcheck` (D-26)?

A result that no branch covers is reported as such ("outside the rule"); it is never mapped
to a branch. Usage (inside the capture image, roots are the `version` directories of the
fetch_pdk.py sets sky130-ngspice, gf180mcu-ngspice and ihp-ngspice):

    run_refruns.py --ngspice /opt/ref/bin/ngspice --openvaf /opt/ref/bin/openvaf-r \
        --sky130 <root> --gf180mcu <root> --ihp <root> --out <dir>

Writes <dir>/results.json and <dir>/summary.md (also printed) and one directory per run with
the deck, the .spiceinit and the log. Exit status 0 when every rule selected a branch, 1 when a
result is outside its rule, 2 on a usage error.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
DECKS = HERE / "decks"
TIMEOUT_S = 900
SPREAD_EPS = 1e-9
SAME_REL = 1e-9

SKY130_INIT = ("set ngbehavior=hsa", "set ng_nomodcheck")
GF180_INIT = ("set ngbehavior=hsa",)
IHP_INIT = ("set ngbehavior=hsa",)
OSDI_MODULES = ("psp103", "psp103_nqs", "r3_cmc", "mosvar", "cap_cmomi", "cap_cmomf")
OSDI_SOURCES = {"psp103": "psp103/psp103.va", "psp103_nqs": "psp103/psp103_nqs.va", "r3_cmc": "r3_cmc/r3_cmc.va",
                "mosvar": "mosvar/mosvar.va", "cap_cmomi": "cap_cmomi/cap_cmomi.va",
                "cap_cmomf": "cap_cmomf/cap_cmomf.va"}
SKY130_SECTIONS = ("tt", "ff", "ss", "sf", "fs")
GF180_SECTIONS = ("typical", "ff", "ss", "fs", "sf")
IHP_LIBS = (("cornerMOSlv.lib", ("mos_tt", "mos_ff", "mos_ss", "mos_sf", "mos_fs")),
            ("cornerMOShv.lib", ("mos_tt", "mos_ff", "mos_ss", "mos_sf", "mos_fs")),
            ("cornerHBT.lib", ("hbt_typ", "hbt_bcs", "hbt_wcs")),
            ("cornerRES.lib", ("res_typ", "res_bcs", "res_wcs")),
            ("cornerCAP.lib", ("cap_typ", "cap_bcs", "cap_wcs")))
HREF_RE = re.compile(r"^HREF (\w+)=(\S+)")
ERROR_RE = re.compile(r"^\s*(Error|ERROR)\b|\berror:", re.MULTILINE)
WARNING_RE = re.compile(r"^\s*(Warning|WARNING)\b", re.MULTILINE)


@dataclass
class Run:
    name: str
    pdk: str
    deck: str
    subs: dict[str, str]
    init: tuple[str, ...]


@dataclass
class Result:
    name: str
    status: str  # "ok", "failed" or "skipped"
    values: dict[str, float] = field(default_factory=dict)
    errors: list[str] = field(default_factory=list)
    warnings: int = 0
    exit_code: int | None = None
    seconds: float = 0.0


def plan() -> list[Run]:
    """Every run, in order. The IHP runs add the OSDI lines to their init at run time."""
    runs = [Run("sky130-d22-override", "sky130", "sky130_mos.cir", {"SECTION": "mc", "OVERRIDE": ".param MC_MM_SWITCH=1"},
                SKY130_INIT),
            Run("sky130-d22-baseline", "sky130", "sky130_mos.cir", {"SECTION": "mc", "OVERRIDE": ""}, SKY130_INIT),
            Run("sky130-d26-without", "sky130", "sky130_mos.cir", {"SECTION": "tt", "OVERRIDE": ""},
                ("set ngbehavior=hsa",))]
    runs += [Run(f"sky130-section-{s}", "sky130", "sky130_mos.cir", {"SECTION": s, "OVERRIDE": ""}, SKY130_INIT)
             for s in SKY130_SECTIONS]
    gf_override = ".param sw_stat_global=1\n.param sw_stat_mismatch=1"
    runs += [Run("gf180mcu-d22-override", "gf180mcu", "gf180_mos.cir", {"SECTION": "typical", "OVERRIDE": gf_override},
                 GF180_INIT),
             Run("gf180mcu-d22-baseline", "gf180mcu", "gf180_mos.cir", {"SECTION": "typical", "OVERRIDE": ""},
                 GF180_INIT),
             Run("gf180mcu-d27-hs", "gf180mcu", "gf180_sweep.cir", {}, ("set ngbehavior=hs",)),
             Run("gf180mcu-d27-hsa", "gf180mcu", "gf180_sweep.cir", {}, ("set ngbehavior=hsa",))]
    runs += [Run(f"gf180mcu-section-{s}", "gf180mcu", "gf180_mos.cir", {"SECTION": s, "OVERRIDE": ""}, GF180_INIT)
             for s in GF180_SECTIONS]
    runs += [Run("ihp-d22-override", "ihp", "ihp_mos.cir", {"OVERRIDE": ".param sg13g2_lv_nmos_muew=0.5"}, IHP_INIT),
             Run("ihp-d22-baseline", "ihp", "ihp_mos.cir", {"OVERRIDE": ""}, IHP_INIT)]
    runs += [Run(f"ihp-lib-{lib.removesuffix('.lib')}-{s}", "ihp", "ihp_lib.cir", {"LIB": lib, "SECTION": s}, IHP_INIT)
             for lib, sections in IHP_LIBS for s in sections]
    return runs


def render(text: str, subs: dict[str, str]) -> str:
    for key, value in subs.items():
        text = text.replace(f"@{key}@", value)
    leftover = re.findall(r"@[A-Z0-9]+@", text)
    if leftover:
        raise ValueError(f"unresolved placeholders {sorted(set(leftover))}")
    return text


def parse_log(text: str) -> tuple[dict[str, float], list[str], int]:
    """HREF values, error lines and the warning count of one ngspice log."""
    values = {}
    for line in text.splitlines():
        m = HREF_RE.match(line.strip())
        if m:
            try:
                values[m.group(1)] = float(m.group(2))
            except ValueError:
                pass
    errors = [line.strip() for line in text.splitlines() if ERROR_RE.search(line)]
    return values, errors, len(WARNING_RE.findall(text))


def execute(run: Run, roots: dict[str, Path], tools: dict, out: Path) -> Result:
    if run.pdk not in roots:
        return Result(run.name, "skipped", errors=[f"no {run.pdk} root"])
    init = run.init + tools.get(f"{run.pdk}_init", ())
    if run.pdk == "ihp" and not tools.get("ihp_init"):
        return Result(run.name, "skipped", errors=["no OSDI modules (no OpenVAF)"])
    work = out / run.name
    work.mkdir(parents=True, exist_ok=True)
    subs = dict(run.subs, SKY130=str(roots.get("sky130", "")), GF180=str(roots.get("gf180mcu", "")),
                IHP=str(roots.get("ihp", "")))
    (work / "deck.cir").write_text(render((DECKS / run.deck).read_text(encoding="utf-8"), subs), encoding="utf-8")
    (work / ".spiceinit").write_text("* reference run init\n" + "\n".join(init) + "\n", encoding="utf-8")
    start = time.monotonic()
    try:
        proc = subprocess.run([tools["ngspice"], "-b", "deck.cir"], cwd=work, capture_output=True, text=True,
                              timeout=TIMEOUT_S, env={"HOME": str(work), "PATH": "/usr/bin:/bin"}, check=False)
        log, code = proc.stdout + proc.stderr, proc.returncode
    except subprocess.TimeoutExpired:
        log, code = "Error: timeout\n", None
    (work / "log.txt").write_text(log, encoding="utf-8")
    values, errors, warnings = parse_log(log)
    status = "ok" if code == 0 and values and not errors else "failed"
    return Result(run.name, status, values, errors[:20], warnings, code, round(time.monotonic() - start, 2))


def compile_osdi(openvaf: str, va_root: Path, out: Path) -> tuple[tuple[str, ...], list[str]]:
    """Compiles the six IHP modules; returns the `osdi` init lines and the failures."""
    out.mkdir(parents=True, exist_ok=True)
    lines, failures = [], []
    for module in OSDI_MODULES:
        target = out / f"{module}.osdi"
        proc = subprocess.run([openvaf, "-D__NGSPICE__", "-o", str(target), str(va_root / OSDI_SOURCES[module])],
                              capture_output=True, text=True, timeout=TIMEOUT_S, check=False)
        if proc.returncode == 0 and target.is_file():
            lines.append(f"osdi {target}")
        else:
            failures.append(f"{module}: exit {proc.returncode}: {(proc.stderr or proc.stdout)[-400:]}")
    return tuple(lines), failures


# --- decisions -------------------------------------------------------------------------------

def ok(results: dict[str, Result], *names: str) -> bool:
    return all(n in results and results[n].status == "ok" for n in names)


def value(results: dict[str, Result], name: str, key: str) -> float | None:
    r = results.get(name)
    return r.values.get(key) if r else None


def same(a: float | None, b: float | None) -> bool:
    if a is None or b is None:
        return False
    return abs(a - b) <= SAME_REL * max(abs(a), abs(b), 1e-30)


def precedence(results: dict[str, Result], pdk: str, override: float, nominal: float, spread: bool) -> str | None:
    """'last' if the later top-level definition took effect, 'first' if the included one stayed,
    None if the evidence is inconsistent or a run failed."""
    over, base = f"{pdk}-d22-override", f"{pdk}-d22-baseline"
    if not ok(results, over, base):
        return None
    sw_over, sw_base = value(results, over, "switch"), value(results, base, "switch")
    if not same(sw_base, nominal):
        return None
    if spread:
        moved = (value(results, over, "spread") or 0.0) > SPREAD_EPS
        flat = (value(results, base, "spread") or 0.0) <= SPREAD_EPS
    else:
        moved = not same(value(results, over, "i1"), value(results, base, "i1"))
        flat = True
    if same(sw_over, override) and moved and flat:
        return "last"
    if same(sw_over, nominal) and not moved and flat:
        return "first"
    return None


def decide_simadv3(results: dict[str, Result]) -> dict:
    p = precedence(results, "sky130", 1.0, 0.0, spread=True)
    branch = {"last": "a", "first": "b"}.get(p or "")
    return {"branch": branch, "precedence": p, "evidence": evidence(results, ("sky130-d22-override",
                                                                               "sky130-d22-baseline"))}


def decide_ngbehavior(results: dict[str, Result]) -> str | None:
    """D-27: 'a' (hs) when hs runs clean and equals hsa; 'b' (hsa) when hs fails or differs and hsa
    is clean with hs showing errors; None when both are clean but differ (no branch says which is right)."""
    hs, hsa = "gf180mcu-d27-hs", "gf180mcu-d27-hsa"
    if not ok(results, hsa):
        return None
    if ok(results, hs):
        equal = all(same(value(results, hs, k), value(results, hsa, k)) for k in ("in", "ip"))
        return "a" if equal else None
    return "b"


def decide_simadv4(results: dict[str, Result]) -> dict:
    p = precedence(results, "gf180mcu", 1.0, 0.0, spread=True)
    globals_ok = same(value(results, "gf180mcu-d22-override", "global"), 1.0)
    d22 = {"last": "a", "first": "b"}.get(p or "") if (p != "last" or globals_ok) else None
    d27 = decide_ngbehavior(results)
    branch = f"D-22 ({d22}), D-27 ({d27})" if d22 and d27 else None
    names = ("gf180mcu-d22-override", "gf180mcu-d22-baseline", "gf180mcu-d27-hs", "gf180mcu-d27-hsa")
    return {"branch": branch, "d22": d22, "d27": d27, "evidence": evidence(results, names)}


def decide_simadv9(results: dict[str, Result]) -> dict:
    found = {pdk: precedence(results, pdk, o, n, s) for pdk, o, n, s in
             (("sky130", 1.0, 0.0, True), ("gf180mcu", 1.0, 0.0, True), ("ihp", 0.5, 0.85, False))}
    kinds = set(found.values())
    branch = {"last": "b", "first": "a"}.get(kinds.pop()) if len(kinds) == 1 and None not in found.values() else None
    return {"branch": branch, "per_pdk": found}


def descriptor_loads(results: dict[str, Result]) -> tuple[str | None, list[str]]:
    """Q-SIM-8 descriptor values: (a) when every section of every descriptor file loads."""
    loads = [r for r in results.values() if r.name.startswith(("sky130-section-", "gf180mcu-section-", "ihp-lib-"))]
    failed = sorted(r.name for r in loads if r.status == "failed")
    if not loads or any(r.status == "skipped" for r in loads):
        return None, failed
    return ("b" if failed else "a"), failed


def nomodcheck_branch(results: dict[str, Result]) -> str | None:
    """D-26: (a) when sky130 combined fails without ng_nomodcheck and loads with it, else (b)."""
    without = results.get("sky130-d26-without")
    if not ok(results, "sky130-section-tt") or without is None or without.status == "skipped":
        return None
    return "a" if without.status == "failed" else "b"


def decide_sim8(results: dict[str, Result]) -> dict:
    descriptors, failed = descriptor_loads(results)
    nomodcheck = nomodcheck_branch(results)
    branch = f"descriptors ({descriptors}), ng_nomodcheck ({nomodcheck})" if nomodcheck and descriptors else None
    return {"branch": branch, "descriptors": descriptors, "failed_loads": failed, "ng_nomodcheck": nomodcheck,
            "evidence": evidence(results, ("sky130-section-tt", "sky130-d26-without"))}


def evidence(results: dict[str, Result], names: tuple[str, ...]) -> dict:
    return {n: {"status": results[n].status, "values": results[n].values, "warnings": results[n].warnings,
                "seconds": results[n].seconds} for n in names if n in results}


def decide(results: dict[str, Result]) -> dict:
    return {"Q-SIMADV-3": decide_simadv3(results), "Q-SIMADV-4": decide_simadv4(results),
            "Q-SIMADV-9": decide_simadv9(results), "Q-SIM-8": decide_sim8(results)}


# --- facts the rules also ask for ------------------------------------------------------------

def gf180_facts(root: Path) -> dict:
    base = root / "gf180mcuD" / "libs.tech" / "ngspice"
    names = sorted(p.name for p in base.glob("*") if p.is_file())
    model = base / "sm141064.ngspice"
    tnom = sorted(set(re.findall(r"tnom\s*=\s*([0-9.]+)", model.read_text(encoding="utf-8", errors="replace"))))
    return {"files": names, "tnom": tnom}


def ihp_spiceinit_osdi(root: Path) -> list[str]:
    init = root / "ihp-sg13g2" / "libs.tech" / "ngspice" / ".spiceinit"
    text = init.read_text(encoding="utf-8", errors="replace") if init.is_file() else ""
    return re.findall(r"^\s*osdi\s+\S*/(\w+)\.osdi", text, re.MULTILINE)


def summary(decisions: dict, facts: dict, results: dict[str, Result], label: str) -> str:
    lines = [f"# Reference runs ({label})", "", "| Entry | Branch | Detail |", "|---|---|---|"]
    for entry, d in decisions.items():
        branch = d["branch"] or "OUTSIDE THE RULE"
        detail = {k: v for k, v in d.items() if k not in ("branch", "evidence")}
        lines.append(f"| {entry} | {branch} | `{json.dumps(detail, sort_keys=True)}` |")
    lines += ["", f"Facts: `{json.dumps(facts, sort_keys=True)}`", "", "| Run | Status | Values | Warnings | s |",
              "|---|---|---|---|---|"]
    for r in results.values():
        shown = r.values or ("; ".join(r.errors[:2]) if r.errors else "")
        lines.append(f"| {r.name} | {r.status} | `{shown}` | {r.warnings} | {r.seconds} |")
    return "\n".join(lines) + "\n"


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--ngspice", required=True)
    ap.add_argument("--openvaf", default="")
    ap.add_argument("--sky130", type=Path)
    ap.add_argument("--gf180mcu", type=Path)
    ap.add_argument("--ihp", type=Path)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--label", default="local")
    ap.add_argument("--only", default="", help="run only the runs whose name starts with this prefix")
    args = ap.parse_args(argv)
    roots = {k: v.resolve() for k, v in (("sky130", args.sky130), ("gf180mcu", args.gf180mcu), ("ihp", args.ihp)) if v}
    args.out.mkdir(parents=True, exist_ok=True)
    tools, facts = {"ngspice": args.ngspice}, {}
    if "ihp" in roots and args.openvaf:
        va_root = roots["ihp"] / "ihp-sg13g2" / "libs.tech" / "verilog-a"
        tools["ihp_init"], facts["osdi_failures"] = compile_osdi(args.openvaf, va_root, args.out / "osdi")
        facts["ihp_spiceinit_osdi"] = ihp_spiceinit_osdi(roots["ihp"])
    if "gf180mcu" in roots:
        facts["gf180mcu"] = gf180_facts(roots["gf180mcu"])
    results = {}
    for run in (r for r in plan() if r.name.startswith(args.only)):
        results[run.name] = execute(run, roots, tools, args.out)
        print(f"{run.name}: {results[run.name].status} {results[run.name].values}", flush=True)
    decisions = decide(results)
    report = {"label": args.label, "decisions": decisions, "facts": facts,
              "runs": {n: r.__dict__ for n, r in results.items()}}
    (args.out / "results.json").write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")
    text = summary(decisions, facts, results, args.label)
    (args.out / "summary.md").write_text(text, encoding="utf-8")
    print(text)
    return 0 if all(d["branch"] for d in decisions.values()) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
