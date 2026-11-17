# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Raw outputs of a run, kept for bisecting a difference (capture.py and replay.py ``--keep-raw``).

A golden case keeps only its declared outputs, and a MASK-RT case's ``out.gds`` only as a digest
and a summary (m5-mask-gds-cif design D19). To find the first differing structure the full files
are needed: with ``--keep-raw DIR`` every file a run writes in its work directory (``input/``
left out: it holds the staged PDK and library data) and its two streams are copied to
``DIR/<case-id>/``. golden-capture.yml's ``keep-raw`` input uploads them as an artifact.

``--feedback-probe`` also inserts ``feedback save out.feedback.tcl`` before the last
``quit -noprompt`` of the case's script (the work copy only), so the run leaves its feedback
list, one ``box`` and ``feedback add`` line pair per entry (Magic's CmdFeedback SAVE,
commands/CmdFI.c:417-451): the "CIF error in cell <cell>, layer <layer>: <message>" entries of
CIFError (cif/CIFmain.c:417-433), e.g. "parent and child disagree on CIF" (cif/CIFhier.c:534).
``feedback save`` prints nothing, so the run's streams stay comparable with expected/.
"""
from __future__ import annotations

import shutil
from dataclasses import dataclass
from pathlib import Path

FEEDBACK_FILE = "out.feedback.tcl"
FEEDBACK_PROBE = f"catch {{feedback save {FEEDBACK_FILE}}}\n"
QUIT_LINE = "quit -noprompt"
STREAMS = ("stdout.txt", "stderr.txt")


def add_feedback_probe(script: Path) -> bool:
    """Insert the feedback save before the script's last ``quit -noprompt``; False when it has none."""
    lines = script.read_text(encoding="utf-8").splitlines(keepends=True)
    for index in range(len(lines) - 1, -1, -1):
        if lines[index].strip() == QUIT_LINE:
            lines.insert(index, FEEDBACK_PROBE)
            script.write_text("".join(lines), encoding="utf-8")
            return True
    return False


def written_files(work: Path) -> list[Path]:
    """The files a run wrote in ``work``, relative to it (``input/`` left out)."""
    return sorted(p.relative_to(work) for p in work.rglob("*")
                  if p.is_file() and not p.is_symlink() and p.relative_to(work).parts[0] != "input")


@dataclass(frozen=True)
class RawKeep:
    root: Path
    feedback: bool = False

    def prepare(self, case, work: Path) -> None:
        """Before the run: add the feedback probe to the work copy of the case's script."""
        script = case.reference.get("script")
        if self.feedback and not (script and add_feedback_probe(work / script)):
            print(f"{case.id}: no '{QUIT_LINE}' in its script, feedback probe not added")

    def keep(self, case_id: str, work: Path, stdout: bytes, stderr: bytes) -> Path:
        """After the run: copy its streams and written files to ``<root>/<case_id>/``."""
        target = self.root / case_id
        if target.exists():
            shutil.rmtree(target)
        target.mkdir(parents=True)
        for name, data in zip(STREAMS, (stdout, stderr)):
            (target / name).write_bytes(data)
        for rel in written_files(work):
            dest = target / "files" / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(work / rel, dest)
        return target
