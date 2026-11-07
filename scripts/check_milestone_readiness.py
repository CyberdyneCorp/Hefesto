#!/usr/bin/env python3
"""Check that a milestone's open questions are resolved before its work starts.

Roadmap principle 2 requires every open question cited by a milestone's
capabilities to carry a `Resolution:` line (hefesto-magic-compat-mode,
requirement "Resolution recorded for every open question").

The capabilities of milestone M<n> are read from the `**Capabilities:**` line of
its `## M<n>` section in docs/roadmap.md, as in scripts/check_roadmap_coverage.py.
Every Q-ID cited in those specs is looked up in docs/decisions/open-questions/.

Usage:
    check_milestone_readiness.py --milestone M1   # only M1
    check_milestone_readiness.py --through M1     # M0 and M1
    check_milestone_readiness.py --summary        # per-file Resolution counts

Exits 1 if a cited entry has no `Resolution:` line or does not exist.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SPECS = ROOT / "openspec" / "specs"
ROADMAP = ROOT / "docs" / "roadmap.md"
QUESTIONS = ROOT / "docs" / "decisions" / "open-questions"

QID = re.compile(r"Q-[A-Z]+-\d+")
ENTRY_HEADING = re.compile(r"^### (Q-[A-Z]+-\d+):", re.M)
RESOLUTION = re.compile(r"^- Resolution:", re.M)


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


def load_entries() -> dict[str, tuple[str, bool]]:
    """Map each Q-ID to (file name, has a Resolution line)."""
    entries: dict[str, tuple[str, bool]] = {}
    for path in sorted(QUESTIONS.glob("*.md")):
        text = path.read_text()
        headings = list(ENTRY_HEADING.finditer(text))
        for i, heading in enumerate(headings):
            end = headings[i + 1].start() if i + 1 < len(headings) else len(text)
            body = text[heading.end():end]
            entries[heading.group(1)] = (path.stem, bool(RESOLUTION.search(body)))
    return entries


def cited_ids(capability: str) -> set[str]:
    spec = SPECS / capability / "spec.md"
    return set(QID.findall(spec.read_text())) if spec.exists() else set()


def check_milestones(milestones: list[str], entries: dict[str, tuple[str, bool]]) -> int:
    """Report unresolved or missing cited entries; return the number of problems."""
    assignments = milestone_assignments(ROADMAP.read_text())
    problems = 0
    for milestone in milestones:
        unresolved: dict[str, list[str]] = {}
        missing: dict[str, list[str]] = {}
        for capability in assignments.get(milestone, []):
            for qid in sorted(cited_ids(capability)):
                if qid not in entries:
                    missing.setdefault(qid, []).append(capability)
                elif not entries[qid][1]:
                    unresolved.setdefault(qid, []).append(capability)
        count = len(unresolved) + len(missing)
        status = "ready" if count == 0 else f"{count} blocking"
        print(f"{milestone}: {status}")
        for qid in sorted(unresolved, key=sort_key):
            print(f"  unresolved {qid} ({entries[qid][0]}.md) cited by {', '.join(unresolved[qid])}")
        for qid in sorted(missing, key=sort_key):
            print(f"  missing    {qid} cited by {', '.join(missing[qid])}")
        problems += count
    return problems


def sort_key(qid: str) -> tuple[str, int]:
    area, number = qid.rsplit("-", 1)
    return area, int(number)


def print_summary(entries: dict[str, tuple[str, bool]]) -> None:
    counts: dict[str, list[int]] = {}
    for file_name, resolved in entries.values():
        row = counts.setdefault(file_name, [0, 0])
        row[0 if resolved else 1] += 1
    print(f"{'File':<12} {'Entries':>7} {'With':>5} {'Without':>7}")
    for file_name in sorted(counts):
        with_res, without = counts[file_name]
        print(f"{file_name:<12} {with_res + without:>7} {with_res:>5} {without:>7}")
    total_with = sum(row[0] for row in counts.values())
    total_without = sum(row[1] for row in counts.values())
    print(f"{'Total':<12} {total_with + total_without:>7} {total_with:>5} {total_without:>7}")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--milestone", help="check one milestone, e.g. M1")
    parser.add_argument("--through", help="check M0 up to and including this milestone")
    parser.add_argument("--summary", action="store_true", help="print per-file Resolution counts")
    args = parser.parse_args(argv)
    if not (args.milestone or args.through or args.summary):
        parser.error("give --milestone, --through or --summary")
    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    entries = load_entries()
    if args.summary:
        print_summary(entries)
    milestones: list[str] = []
    if args.milestone:
        milestones.append(f"M{milestone_number(args.milestone)}")
    if args.through:
        last = milestone_number(args.through)
        milestones.extend(f"M{n}" for n in range(last + 1) if f"M{n}" not in milestones)
    problems = check_milestones(milestones, entries) if milestones else 0
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
