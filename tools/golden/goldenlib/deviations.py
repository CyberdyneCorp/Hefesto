# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Documented deviations of golden cases (``tests/golden/deviations.toml``).

Some captured Magic outputs show behavior Hefesto deliberately does not
reproduce: a Magic bug fixed under decision X-2, or a resolved open
question. Such a case cannot pass, and masking it would hide every other
difference in the same output. Instead it is listed here with the decision
that justifies it and the outputs allowed to differ::

    [TECH-G43]
    decisions = ["Q-TECH-25"]        # docs/decisions entries
    outputs = ["stdout.txt"]         # output names of the case (at least one)
    reason = "tech revert * re-locks the layers locked in the technology file"

A case whose Magic output needs a feature of a later milestone (a command
or engine that does not exist yet) is listed with ``deferred``, the
milestone of docs/roadmap.md that brings it, instead of or beside
``decisions``; the reason names the missing feature::

    [CMDEDIT-LBL-07]
    deferred = "M3"                  # a "## M<n>" heading of docs/roadmap.md
    outputs = ["stdout.txt"]
    reason = "goto reports the DRC check plane, which arrives with the DRC engine"

Every entry needs at least one decision or a ``deferred`` milestone.

A deferred entry may give some of its outputs an earlier (or later) milestone than the
rest with ``deferred_outputs``, a table from output name to milestone; every key must be
one of ``outputs``. The other outputs keep ``deferred``::

    [SCHEM-LVS-01]
    deferred = "M9"
    deferred_outputs = { "inv_1.spice" = "M8" }   # M8 already checks the netlist
    outputs = ["inv_1.spice", "lvs.out"]
    reason = "the LVS result needs the lvs-* capabilities of M9"

``replay.py --milestone M<n>`` checks the state at the exit of milestone n: an output whose
milestone is n or earlier is no longer covered by its entry, so it must match.

An output name may also be a pattern over the case's collected outputs (``[[collect]]``,
goldenlib/collect.py), such as ``collected/*`` or ``collected/*.spice``: it covers every
``collected/<path>`` it matches (fnmatch), including one written by only one side.

replay.py reports such a case as ``deviation`` (skipped, not passed) when
only the listed outputs differ. It still fails when any other output
differs, and it fails when the case passes, so a stale entry is removed
instead of hiding a regression later. Every entry must name a captured case
(or, with ``deferred``, a case whose capture is pending in CI) and existing
outputs (``check``), and each decision must be a heading in ``docs/decisions``
(``tests/test_deviations.py``).
"""
from __future__ import annotations

import fnmatch
import re
import tomllib
from dataclasses import dataclass, replace
from pathlib import Path

FILE_NAME = "deviations.toml"


class DeviationError(ValueError):
    """The deviations file is malformed."""


@dataclass(frozen=True)
class Deviation:
    case_id: str
    decisions: tuple[str, ...]
    outputs: tuple[str, ...]
    reason: str
    deferred: str = ""  # the roadmap milestone that brings the missing feature
    deferred_outputs: tuple[tuple[str, str], ...] = ()  # (output, milestone) overriding ``deferred``

    def milestone_of(self, output_name: str) -> str:
        """The milestone by which ``output_name`` must match; "" when a decision, not a deferral, covers it."""
        return dict(self.deferred_outputs).get(output_name, self.deferred if self.covers(output_name) else "")

    def at_milestone(self, milestone: str) -> Deviation | None:
        """The entry as it stands at the exit of ``milestone``: outputs deferred to it or earlier are
        dropped (they must match by then). None when nothing is left to cover."""
        if not self.deferred:
            return self
        due = {name for name in self.outputs if _number(self.milestone_of(name)) <= _number(milestone)}
        if not due:
            return self
        if not self.decisions:
            outputs = tuple(name for name in self.outputs if name not in due)
            return replace(self, outputs=outputs,
                           deferred_outputs=tuple(p for p in self.deferred_outputs if p[0] not in due)) \
                if outputs else None
        return replace(self, deferred="", deferred_outputs=())  # a decided deviation outlives its deferral

    def basis(self) -> str:
        """The decisions and the deferral, e.g. ``Q-UI-12; deferred to M4``."""
        parts = [", ".join(self.decisions)] if self.decisions else []
        if self.deferred:
            parts.append(f"deferred to {self.deferred}")
        parts += [f"{name} deferred to {milestone}" for name, milestone in self.deferred_outputs]
        return "; ".join(parts)

    def describe(self) -> str:
        return f"documented deviation ({self.basis()}): {self.reason}"

    def covers(self, output_name: str) -> bool:
        """Whether the entry lists ``output_name``, or a ``collected/`` pattern matches it."""
        return any(output_name == name or (name.startswith(COLLECTED) and fnmatch.fnmatchcase(output_name, name))
                   for name in self.outputs)


COLLECTED = "collected/"  # goldenlib/collect.py PREFIX


def _strings(entry: dict, key: str, case_id: str) -> tuple[str, ...]:
    value = entry.get(key)
    if not isinstance(value, list) or not value or not all(isinstance(v, str) and v for v in value):
        raise DeviationError(f"{FILE_NAME} [{case_id}]: '{key}' must be a non-empty list of strings")
    return tuple(value)


def parse(text: str) -> dict[str, Deviation]:
    """Parse the deviations file; raise DeviationError when malformed."""
    data = tomllib.loads(text)
    out = {}
    for case_id, entry in data.items():
        if not isinstance(entry, dict):
            raise DeviationError(f"{FILE_NAME}: '{case_id}' must be a table")
        unknown = set(entry) - {"decisions", "outputs", "reason", "deferred", "deferred_outputs"}
        if unknown:
            raise DeviationError(f"{FILE_NAME} [{case_id}]: unknown keys {sorted(unknown)}")
        reason = entry.get("reason")
        if not isinstance(reason, str) or not reason.strip():
            raise DeviationError(f"{FILE_NAME} [{case_id}]: 'reason' must be a non-empty string")
        deferred = _deferred(entry.get("deferred", ""), case_id, "deferred")
        decisions = _strings(entry, "decisions", case_id) if "decisions" in entry or not deferred else ()
        outputs = _strings(entry, "outputs", case_id)
        per_output = _deferred_outputs(entry, case_id, deferred, outputs)
        out[case_id] = Deviation(case_id, decisions, outputs, reason.strip(), deferred, per_output)
    return out


def _deferred(value: object, case_id: str, key: str) -> str:
    if not isinstance(value, str) or (value and not re.fullmatch(r"M\d+", value)):
        raise DeviationError(f"{FILE_NAME} [{case_id}]: '{key}' must be a milestone such as \"M3\"")
    return value


def _deferred_outputs(entry: dict, case_id: str, deferred: str,
                      outputs: tuple[str, ...]) -> tuple[tuple[str, str], ...]:
    table = entry.get("deferred_outputs", {})
    if not isinstance(table, dict) or (table and not deferred):
        raise DeviationError(f"{FILE_NAME} [{case_id}]: 'deferred_outputs' must be a table and needs 'deferred'")
    pairs = []
    for name, milestone in table.items():
        if name not in outputs:
            raise DeviationError(f"{FILE_NAME} [{case_id}]: 'deferred_outputs' names {name!r}, not in 'outputs'")
        if not _deferred(milestone, case_id, f"deferred_outputs.{name}"):
            raise DeviationError(f"{FILE_NAME} [{case_id}]: 'deferred_outputs.{name}' must not be empty")
        pairs.append((name, milestone))
    return tuple(pairs)


def _number(milestone: str) -> int:
    """The milestone's number; an output with no deferral (a decision) is never due."""
    return int(milestone[1:]) if milestone else 1 << 30


def load(golden_root: Path) -> dict[str, Deviation]:
    """The deviations of ``<golden_root>/deviations.toml`` (empty when absent)."""
    path = Path(golden_root) / FILE_NAME
    return parse(path.read_text(encoding="utf-8")) if path.is_file() else {}


def check(deviation: Deviation, output_names: list[str], collects: bool = False) -> None:
    """Raise DeviationError when the entry names an output the case lacks (a ``collected/`` pattern
    needs a case with ``[[collect]]`` entries, ``collects``)."""
    missing = [name for name in deviation.outputs
               if name not in output_names and not (collects and name.startswith(COLLECTED))]
    if missing:
        raise DeviationError(f"{FILE_NAME} [{deviation.case_id}]: no such outputs {missing}")


def classify(deviation: Deviation | None, failed_outputs: list[str]) -> tuple[str, str]:
    """The case status and detail for the outputs that did not match.

    ``pass`` when nothing failed and no deviation is listed; ``fail`` when an
    output outside the deviation failed, or when a listed deviation no
    longer reproduces; ``deviation`` when only listed outputs failed.
    """
    if deviation is None:
        return ("fail", "") if failed_outputs else ("pass", "")
    if not failed_outputs:
        return "fail", (f"listed in {FILE_NAME} ({deviation.basis()}) but every output "
                        f"now matches: remove the entry")
    unexpected = [name for name in failed_outputs if not deviation.covers(name)]
    if unexpected:
        return "fail", f"outputs outside the documented deviation differ: {', '.join(unexpected)}"
    return "deviation", deviation.describe()
