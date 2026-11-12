#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Report which `exact` and `behavioral` requirements have no automated test.

Capability hefesto-testing-strategy, requirement "Requirement-traceable tests":
every `exact` and `behavioral` requirement needs at least one automated test that
names the capability and the requirement in its metadata, and CI publishes the
requirements that have none.

Requirements are read from openspec/specs/<capability>/spec.md: a
`### Requirement: <name>` heading followed by a `Compatibility:` line. A line such
as `Compatibility: exact (content); behavioral (order)` counts as both classes.

Tests are found in two places:
- Catch2 sources under tests/ (by default tests/unit and tests/fuzz): the tag
  string of TEST_CASE, SCENARIO and the other test macros, for example
  `"[cap:hefesto-output-determinism][req:Magic string hash table]"`. Each
  `[req:...]` tag belongs to the nearest `[cap:...]` tag before it in the same
  test (or the first one after it, if the requirement comes first).
- Golden case manifests tests/golden/<AREA>/cases/<case-id>/case.json or
  case.toml, with a `capability` string and a `requirement` string or a
  `requirements` list. A case that tests/golden/deviations.toml defers to a
  later milestone (`deferred = "M<n>"`) covers nothing until then.

Usage:
    check_requirement_coverage.py                       # all capabilities, markdown
    check_requirement_coverage.py --milestone M0        # M0 capabilities only
    check_requirement_coverage.py --through M1 --format json --output report.json
    check_requirement_coverage.py --fail-under 80       # exit 1 below 80 % covered

Milestones come from the `**Capabilities:**` line of each `## M<n>` section of
docs/roadmap.md, as in scripts/check_roadmap_coverage.py.

Exits 0 unless --fail-under is given and coverage is below it (report-only by
default), or a usage error occurs (exit 2).
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

try:  # Python 3.11+; on older interpreters case.toml manifests are skipped
    import tomllib
except ModuleNotFoundError:  # pragma: no cover
    tomllib = None

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CLASSES = ("exact", "behavioral")
ALL_CLASSES = ("exact", "behavioral", "informative", "native")
CPP_SUFFIXES = {".cpp", ".cc", ".cxx", ".hpp", ".h", ".mm"}

# Requirements whose tests move to a later milestone by an approved decision:
# (capability, requirement) -> milestone. They are reported as deferred, not
# uncovered, and do not count in the coverage share.
DEFERRED = {
    # Q-SCRIPT-30 (m4-scripting design D11): `mazeroute` arrives with
    # router-maze-iroute, and its requirement's test with it.
    ("scripting-auxiliary-tools", "Netlist maze-routing script"): "M10",
}

REQUIREMENT_HEADING = re.compile(r"^### Requirement:\s*(.+?)\s*$")
COMPATIBILITY_LINE = re.compile(r"^Compatibility:\s*(.+)$")
TEST_MACRO = re.compile(
    r"\b(?:TEST_CASE|TEST_CASE_METHOD|SCENARIO|SCENARIO_METHOD|TEMPLATE_TEST_CASE|"
    r"TEMPLATE_TEST_CASE_SIG|TEMPLATE_PRODUCT_TEST_CASE|TEMPLATE_LIST_TEST_CASE|"
    r"METHOD_AS_TEST_CASE|REGISTER_TEST_CASE)\s*\("
)
STRING_LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
TAG = re.compile(r"\[(cap|req):([^\]]+)\]")


@dataclass(frozen=True)
class Requirement:
    capability: str
    name: str
    classes: tuple[str, ...]


@dataclass
class TestRef:
    capability: str
    requirement: str
    source: str  # repo-relative path, with line for C++ tests


@dataclass
class Report:
    requirements: list[Requirement]
    covered: dict[tuple[str, str], list[str]] = field(default_factory=dict)
    unknown: list[TestRef] = field(default_factory=list)

    @property
    def deferred(self) -> list[Requirement]:
        return [r for r in self.requirements
                if (r.capability, r.name) in DEFERRED and (r.capability, r.name) not in self.covered]

    @property
    def uncovered(self) -> list[Requirement]:
        return [r for r in self.requirements
                if (r.capability, r.name) not in self.covered and (r.capability, r.name) not in DEFERRED]

    @property
    def percent(self) -> float:
        total = len(self.requirements) - len(self.deferred)
        return 100.0 if total <= 0 else 100.0 * (total - len(self.uncovered)) / total


# --- Specs and roadmap -------------------------------------------------------


def compatibility_classes(value: str) -> tuple[str, ...]:
    """Return the classes named on a Compatibility line, in canonical order."""
    words = set(re.findall(r"[a-z]+", value.lower()))
    return tuple(c for c in ALL_CLASSES if c in words)


def parse_spec(capability: str, text: str) -> list[Requirement]:
    """Parse every requirement and its compatibility classes from one spec."""
    requirements: list[Requirement] = []
    current: str | None = None
    for line in text.splitlines():
        heading = REQUIREMENT_HEADING.match(line)
        if heading:
            current = heading.group(1)
            continue
        compat = COMPATIBILITY_LINE.match(line)
        if compat and current is not None:
            requirements.append(Requirement(capability, current, compatibility_classes(compat.group(1))))
            current = None
    return requirements


def load_requirements(specs_dir: Path, capabilities: set[str] | None, classes: set[str]) -> list[Requirement]:
    selected: list[Requirement] = []
    for spec in sorted(specs_dir.glob("*/spec.md")):
        capability = spec.parent.name
        if capabilities is not None and capability not in capabilities:
            continue
        for req in parse_spec(capability, spec.read_text(encoding="utf-8")):
            if classes.intersection(req.classes):
                selected.append(req)
    return selected


def milestone_assignments(roadmap: str) -> dict[str, list[str]]:
    """Map each milestone (M0, M1, ...) to the capabilities listed for it."""
    assignments: dict[str, list[str]] = {}
    for section in re.split(r"^## ", roadmap, flags=re.M)[1:]:
        match = re.match(r"(M\d+)\b", section)
        caps_line = re.search(r"^\*\*Capabilities:\*\*(.*)$", section, re.M)
        if match and caps_line:
            assignments[match.group(1)] = re.findall(r"`([a-z0-9-]+)`", caps_line.group(1))
    return assignments


def milestone_number(name: str) -> int:
    match = re.fullmatch(r"M(\d+)", name)
    if not match:
        raise SystemExit(f"invalid milestone '{name}' (expected M<n>)")
    return int(match.group(1))


def selected_milestones(milestones: list[str], through: str | None) -> list[str]:
    names = [f"M{milestone_number(m)}" for m in milestones]
    if through:
        names.extend(f"M{n}" for n in range(milestone_number(through) + 1))
    return list(dict.fromkeys(names))


def capabilities_for(milestones: list[str], roadmap: Path) -> set[str] | None:
    if not milestones:
        return None
    assignments = milestone_assignments(roadmap.read_text(encoding="utf-8"))
    missing = [m for m in milestones if m not in assignments]
    if missing:
        raise SystemExit(f"milestone(s) not found in {roadmap}: {', '.join(missing)}")
    return {cap for m in milestones for cap in assignments[m]}


# --- Test discovery ----------------------------------------------------------


STRING_MACRO = re.compile(r'^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)[ \t]+((?:"(?:[^"\\\n]|\\.)*"[ \t]*)+)$',
                          re.MULTILINE)


def expand_string_macros(text: str) -> str:
    """Replace uses of file-local `#define NAME "..."` macros by their literals.

    Tests often share one tag string per requirement, e.g.
    `#define HASH_TAGS "[cap:x][req:y]"` then `TEST_CASE("name", HASH_TAGS)`.
    Literals hold no newline, so line numbers are unchanged."""
    macros = {m.group(1): m.group(2).strip() for m in STRING_MACRO.finditer(text)}
    if not macros:
        return text
    definitions = {m.span() for m in STRING_MACRO.finditer(text)}
    use = re.compile(r"\b(" + "|".join(map(re.escape, macros)) + r")\b")
    pieces: list[str] = []
    pos = 0
    for start, end in sorted(definitions):
        pieces.append(use.sub(lambda m: macros[m.group(1)], text[pos:start]))
        pieces.append(text[start:end])
        pos = end
    pieces.append(use.sub(lambda m: macros[m.group(1)], text[pos:]))
    return "".join(pieces)


def macro_tag_text(text: str, start: int) -> str:
    """Concatenate the string literals of one test macro call, up to its body `{`."""
    parts: list[str] = []
    pos = start
    while pos < len(text):
        literal = STRING_LITERAL.search(text, pos)
        brace = text.find("{", pos)
        if literal is None or (brace != -1 and brace < literal.start()):
            break
        parts.append(literal.group(1))
        pos = literal.end()
    return "".join(parts)


def pair_tags(tag_text: str) -> list[tuple[str, str]]:
    """Pair each [req:...] with the nearest preceding [cap:...] (or the first one)."""
    tags = TAG.findall(tag_text)
    first_cap = next((value.strip() for kind, value in tags if kind == "cap"), "")
    pairs: list[tuple[str, str]] = []
    cap = first_cap
    for kind, value in tags:
        if kind == "cap":
            cap = value.strip()
        else:
            pairs.append((cap, value.strip()))
    return pairs


def scan_cpp_file(path: Path, root: Path) -> list[TestRef]:
    text = expand_string_macros(path.read_text(encoding="utf-8", errors="replace"))
    refs: list[TestRef] = []
    for macro in TEST_MACRO.finditer(text):
        line = text.count("\n", 0, macro.start()) + 1
        source = f"{path.relative_to(root).as_posix()}:{line}"
        for cap, req in pair_tags(macro_tag_text(text, macro.end())):
            refs.append(TestRef(cap, req, source))
    return refs


def manifest_refs(data: dict, source: str) -> list[TestRef]:
    capability = str(data.get("capability", "")).strip()
    names = data.get("requirements", [])
    if isinstance(names, str):
        names = [names]
    if "requirement" in data:
        names = [data["requirement"], *names]
    return [TestRef(capability, str(name).strip(), source) for name in names]


def load_manifest(path: Path) -> dict:
    if path.suffix == ".json":
        return json.loads(path.read_text(encoding="utf-8"))
    return tomllib.loads(path.read_text(encoding="utf-8"))


def deferred_cases(golden_dir: Path) -> set[str]:
    """Case ids that `deviations.toml` defers to a later milestone: they test nothing yet."""
    path = golden_dir / "deviations.toml"
    if tomllib is None or not path.is_file():
        return set()
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    return {case for case, entry in data.items() if isinstance(entry, dict) and "deferred" in entry}


def scan_golden(golden_dir: Path, root: Path) -> list[TestRef]:
    refs: list[TestRef] = []
    skipped = deferred_cases(golden_dir)
    patterns = ["*/cases/*/case.json"]
    if tomllib is not None:
        patterns.append("*/cases/*/case.toml")
    elif any(golden_dir.glob("*/cases/*/case.toml")):
        print("warning: Python < 3.11 has no tomllib; case.toml manifests skipped", file=sys.stderr)
    for pattern in patterns:
        for path in sorted(golden_dir.glob(pattern)):
            if path.parent.name in skipped:
                continue
            source = path.relative_to(root).as_posix()
            try:
                data = load_manifest(path)
            except ValueError as error:  # json.JSONDecodeError and tomllib.TOMLDecodeError
                print(f"warning: cannot parse {source}: {error}", file=sys.stderr)
                continue
            refs.extend(manifest_refs(data, source))
    return refs


def collect_tests(root: Path, test_dirs: list[Path], golden_dir: Path) -> list[TestRef]:
    refs: list[TestRef] = []
    for test_dir in test_dirs:
        for path in sorted(test_dir.rglob("*")):
            if path.suffix in CPP_SUFFIXES and path.is_file():
                refs.extend(scan_cpp_file(path, root))
    if golden_dir.is_dir():
        refs.extend(scan_golden(golden_dir, root))
    return refs


# --- Report ------------------------------------------------------------------


def build_report(requirements: list[Requirement], refs: list[TestRef], all_known: set[tuple[str, str]]) -> Report:
    report = Report(requirements)
    selected = {(r.capability, r.name) for r in requirements}
    for ref in refs:
        key = (ref.capability, ref.requirement)
        if key in selected:
            report.covered.setdefault(key, []).append(ref.source)
        elif key not in all_known:
            report.unknown.append(ref)
    return report


def render_markdown(report: Report, scope: str) -> str:
    total = len(report.requirements) - len(report.deferred)
    uncovered = report.uncovered
    lines = [
        "# Requirement coverage",
        "",
        f"Scope: {scope}. Covered {total - len(uncovered)} of {total} requirements "
        f"({report.percent:.1f} %).",
        "",
    ]
    lines += markdown_table("Uncovered requirements", ["Capability", "Requirement", "Class"],
                            [[r.capability, r.name, ", ".join(r.classes)] for r in uncovered])
    lines += markdown_table("Deferred requirements", ["Capability", "Requirement", "Milestone"],
                            [[r.capability, r.name, f"deferred ({DEFERRED[(r.capability, r.name)]})"]
                             for r in report.deferred])
    lines += markdown_table("Covered requirements", ["Capability", "Requirement", "Tests"],
                            [[cap, req, ", ".join(sources)] for (cap, req), sources in sorted(report.covered.items())])
    lines += markdown_table("Test tags naming no known requirement", ["Capability", "Requirement", "Source"],
                            [[u.capability or "(none)", u.requirement, u.source] for u in report.unknown])
    return "\n".join(lines) + "\n"


def markdown_table(title: str, header: list[str], rows: list[list[str]]) -> list[str]:
    lines = [f"## {title} ({len(rows)})", ""]
    if not rows:
        return lines + ["None.", ""]
    lines.append("| " + " | ".join(header) + " |")
    lines.append("|" + "---|" * len(header))
    for row in rows:
        lines.append("| " + " | ".join(cell.replace("|", "\\|") for cell in row) + " |")
    return lines + [""]


def render_json(report: Report, scope: str) -> str:
    data = {
        "scope": scope,
        "total": len(report.requirements) - len(report.deferred),
        "covered": len(report.requirements) - len(report.deferred) - len(report.uncovered),
        "percent": round(report.percent, 2),
        "uncovered": [{"capability": r.capability, "requirement": r.name, "classes": list(r.classes)}
                      for r in report.uncovered],
        "deferred": [{"capability": r.capability, "requirement": r.name,
                      "milestone": DEFERRED[(r.capability, r.name)]} for r in report.deferred],
        "covered_requirements": [{"capability": cap, "requirement": req, "tests": sources}
                                 for (cap, req), sources in sorted(report.covered.items())],
        "unknown_tags": [{"capability": u.capability, "requirement": u.requirement, "source": u.source}
                         for u in report.unknown],
    }
    return json.dumps(data, indent=2) + "\n"


# --- Command line ------------------------------------------------------------


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--milestone", action="append", default=[], help="limit to a milestone, e.g. M0 (repeatable)")
    parser.add_argument("--through", help="limit to M0 up to and including this milestone")
    parser.add_argument("--classes", default=",".join(DEFAULT_CLASSES),
                        help=f"comma list of compatibility classes to require tests for (default: exact,behavioral; "
                             f"any of {', '.join(ALL_CLASSES)})")
    parser.add_argument("--format", choices=("markdown", "json"), default="markdown")
    parser.add_argument("--output", type=Path, help="write the report to this file instead of stdout")
    parser.add_argument("--fail-under", type=float, metavar="PERCENT",
                        help="exit 1 if the covered share is below PERCENT")
    parser.add_argument("--root", type=Path, default=ROOT, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    unknown = set(args.classes.split(",")) - set(ALL_CLASSES)
    if unknown:
        parser.error(f"unknown compatibility class(es): {', '.join(sorted(unknown))}")
    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    root = args.root.resolve()
    specs_dir = root / "openspec" / "specs"
    milestones = selected_milestones(args.milestone, args.through)
    capabilities = capabilities_for(milestones, root / "docs" / "roadmap.md")
    classes = set(args.classes.split(","))

    requirements = load_requirements(specs_dir, capabilities, classes)
    all_known = {(r.capability, r.name) for r in load_requirements(specs_dir, None, set(ALL_CLASSES))}
    refs = collect_tests(root, [root / "tests" / "unit", root / "tests" / "fuzz"], root / "tests" / "golden")
    report = build_report(requirements, refs, all_known)

    scope = f"{', '.join(milestones) or 'all milestones'}; classes {', '.join(sorted(classes))}"
    text = render_json(report, scope) if args.format == "json" else render_markdown(report, scope)
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)

    if args.fail_under is not None and report.percent < args.fail_under:
        print(f"coverage {report.percent:.1f} % is below --fail-under {args.fail_under} %", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
