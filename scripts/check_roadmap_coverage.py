#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Check that every OpenSpec capability is assigned to exactly one roadmap milestone.

Milestones are the `## M<n>` sections of docs/roadmap.md; a capability is assigned
when it appears in backticks on that section's `**Capabilities:**` line.
Exits non-zero on unassigned, duplicated or unknown capabilities, and prints
per-milestone capability and requirement counts.
"""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SPECS = ROOT / "openspec" / "specs"
ROADMAP = ROOT / "docs" / "roadmap.md"


def requirement_count(capability: str) -> int:
    text = (SPECS / capability / "spec.md").read_text()
    return len(re.findall(r"^### Requirement", text, re.M))


def milestone_assignments(roadmap: str) -> dict[str, list[str]]:
    assignments: dict[str, list[str]] = {}
    for section in re.split(r"^## ", roadmap, flags=re.M)[1:]:
        match = re.match(r"(M\d+)\b", section)
        caps_line = re.search(r"^\*\*Capabilities:\*\*(.*)$", section, re.M)
        if match and caps_line:
            assignments[match.group(1)] = re.findall(r"`([a-z0-9-]+)`", caps_line.group(1))
    return assignments


def main() -> int:
    capabilities = {p.parent.name for p in SPECS.glob("*/spec.md")}
    assignments = milestone_assignments(ROADMAP.read_text())
    seen = [cap for caps in assignments.values() for cap in caps]

    errors = []
    unassigned = sorted(capabilities - set(seen))
    duplicated = sorted({cap for cap in seen if seen.count(cap) > 1})
    unknown = sorted(set(seen) - capabilities)
    if unassigned:
        errors.append(f"unassigned capabilities: {', '.join(unassigned)}")
    if duplicated:
        errors.append(f"assigned to more than one milestone: {', '.join(duplicated)}")
    if unknown:
        errors.append(f"unknown capabilities in roadmap: {', '.join(unknown)}")

    for milestone, caps in assignments.items():
        known = [cap for cap in caps if cap in capabilities]
        print(f"{milestone}: {len(caps)} capabilities, {sum(map(requirement_count, known))} requirements")

    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
