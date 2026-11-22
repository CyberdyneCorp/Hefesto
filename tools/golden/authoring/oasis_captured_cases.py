# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The OASIS cases whose inputs a third-party tool writes in the capture image (design D18, D19).

* OASIS-012 (task 15.6, Q-OASIS-2 as amended 2026-11-18): gdstk writes four small libraries, each
  as GDS and as OASIS with a CRC32 or CHECKSUM32 signature; two of the OASIS copies then get one
  END padding byte changed (``*_bad``), so only their signature fails. Hefesto reads each OASIS
  file, Magic the GDS equivalent of that file (``oasequiv``; gdstk's own GDS has the TEXT
  presentation OASIS cannot carry).
* OASIS-020/021 (task 15.2): KLayout converts the sky130 standard-cell library GDS
  (``sky130-gds-sc``) to OASIS (``strm2oas``, strict mode) and that OASIS file back to GDS
  (``strm2gds``, gzip-compressed, kept for ``oasis-validate``): Hefesto reads the OASIS file, Magic
  its GDS equivalent (``oasequiv``, committed gzip-compressed under ``equivalent/OASIS-020/``).
  KLayout's ``strm2gds`` is not that equivalent: strm2oas wrote some pin shapes and port TEXTs with a
  repetition holding a zero displacement (two coincident copies), which strm2gds writes once, while
  oasis-read reads one element per displacement ("Shape repetitions"); Magic then made one port label
  where Hefesto made a port label and a zero-area label in 45 cells. OASIS-021 repeats the read with
  each option of the row in turn (``OASIS-021-<option>``). The library's saved cells are compared
  through their digests (``derived:digest``); both sides read with the integer datestamp
  1700000000, so every cell's timestamp is that value (``<fmt> datestamp 1`` parses as ``yes``,
  which keeps the GDS structure date that an OASIS cell does not have).

``CAPTURES`` is the plan ``oasis_validate.py produce`` hands to ``check``; ``--sync-captured DIR``
(``oasis_cases.py``) copies the artifact's ``captured/<case>/`` files into
``tests/golden/OASIS/captured/<case>/`` with ``captured.json`` (tool versions, SHA-256). The cases are
written once their files are there; until then ``not_authored()`` names the dispatch they wait for.
"""
from __future__ import annotations

import hashlib
import json
import shutil
from dataclasses import dataclass, field
from pathlib import Path

import casegen
import common
import oasequiv
from oasis_common import REPORT, Line, Read, _case, both, opt, read_case, rep

CAPTURED_DIR = casegen.GOLDEN / "OASIS" / "captured"
EQUIVALENT_DIR = casegen.GOLDEN / "OASIS" / "equivalent"
MANIFEST = "captured.json"
SKY130_GDS = {"set": "sky130-gds-sc", "path": "sky130A/libs.ref/sky130_fd_sc_hd/gds/sky130_fd_sc_hd.gds"}
LIBRARY = "sky130_fd_sc_hd"
VALIDATION_LIBRARIES = ("crc32", "checksum32", "crc32_bad", "checksum32_bad")


@dataclass(frozen=True)
class Capture:
    """One captured input set: the tool request and the files it leaves in ``captured/<case>/``."""

    case: str
    tool: str
    op: str
    outputs: tuple[str, ...]
    source: dict | None = None
    options: dict = field(default_factory=dict)


CAPTURES = (
    Capture("OASIS-012", "gdstk", "make-validation-set",
            tuple(f"{name}.{ext}" for name in VALIDATION_LIBRARIES for ext in ("gds", "oas"))),
    Capture("OASIS-020", "klayout", "strm2oas-roundtrip", (f"{LIBRARY}.oas", f"{LIBRARY}.gds.gz"), SKY130_GDS,
            {"strict": True}),
)


def plan() -> list[dict]:
    """The captured-input requests of ``oasis_validate.py produce`` (relative output paths)."""
    return [{"case": c.case, "tool": c.tool, "op": c.op, "outputs": list(c.outputs), "source": c.source,
             "options": dict(c.options)} for c in CAPTURES]


def captured_files(case: str) -> dict[str, Path] | None:
    """The committed files of ``case``'s capture, or None while any is missing."""
    capture = next(c for c in CAPTURES if c.case == case)
    paths = {name: CAPTURED_DIR / case / name for name in capture.outputs}
    return paths if all(p.is_file() for p in paths.values()) else None


def sync(artifact: Path, versions: dict | None = None) -> list[Path]:
    """Copy ``artifact/captured/<case>/`` into ``CAPTURED_DIR/<case>/`` with ``captured.json``."""
    written = []
    for capture in CAPTURES:
        source = artifact / "captured" / capture.case
        if not all((source / name).is_file() for name in capture.outputs):
            continue
        target = CAPTURED_DIR / capture.case
        target.mkdir(parents=True, exist_ok=True)
        digests = {}
        for name in capture.outputs:
            shutil.copyfile(source / name, target / name)
            digests[name] = hashlib.sha256((target / name).read_bytes()).hexdigest()
        record = {"schema": 1, "case": capture.case, "tool": capture.tool, "op": capture.op,
                  "source": capture.source, "options": capture.options, "versions": versions or {},
                  "sha256": digests}
        (target / MANIFEST).write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        written.append(target)
    return written


Q_OASIS_2 = ("Approved deviation (Q-OASIS-2, amended 2026-11-18; design D19): no commercial OASIS writer's files "
             "are available, so the case reads gdstk-written files only. Not a deviations.toml entry: the case "
             "passes.")


# gdstk's GDS TEXT carries a PRESENTATION record (font 0, centred), which an OASIS TEXT cannot hold, so
# Magic read it as a FreeSans flabel and Hefesto's read of the OASIS file as an rlabel. Magic reads the
# oasequiv GDS equivalent of each OASIS file instead, as in every other read case (design D19).
GDS_NOTE = ("Magic reads the GDS equivalent of each gdstk OASIS file (authoring/oasequiv.py), not gdstk's own GDS, "
            "whose TEXT has a PRESENTATION record (font 0) that OASIS cannot carry (oasis-read \"TEXT elements\").")


def oasis_012():
    files = captured_files("OASIS-012")
    if files is None:
        return None
    oasis = {name: files[f"{name}.oas"].read_bytes() for name in VALIDATION_LIBRARIES}
    return read_case("OASIS-012", "oasis-format", ["Validation signature"],
                     "gdstk-written files with CRC32 and CHECKSUM32 signatures, and copies with one END padding "
                     "byte changed: every file's cells are read (a bad signature is a warning only).",
                     Read(oasis), notes=[Q_OASIS_2, "Inputs captured by oasis-validate "
                                         "(tests/golden/OASIS/captured/OASIS-012/captured.json).", GDS_NOTE])


LIBRARY_EXTERNALS = [{"source": "golden", "path": f"OASIS/captured/OASIS-020/{LIBRARY}.oas", "to": f"{LIBRARY}.oas"},
                     {"source": "golden", "path": f"OASIS/equivalent/OASIS-020/{LIBRARY}.gds.gz",
                      "to": f"{LIBRARY}.gds.gz"}]
CELL_DIGESTS = "derived:digest:*.mag"
READ_LIBRARY = Line(f"gds read input/{LIBRARY}.gds.gz", f"oasis read input/{LIBRARY}.oas")
# A fixed integer datestamp on both sides (E4, 2026-11-20): every saved cell gets this timestamp.
LIBRARY_DATESTAMP = 1700000000


def write_equivalent() -> Path:
    """``equivalent/OASIS-020/<library>.gds.gz``: the GDS equivalent of the captured OASIS file
    (``oasequiv``, deterministic gzip), rewritten only when its bytes change."""
    source = CAPTURED_DIR / "OASIS-020" / f"{LIBRARY}.oas"
    target = EQUIVALENT_DIR / "OASIS-020" / f"{LIBRARY}.gds.gz"
    data = common.gzip_bytes(oasequiv.gds_equivalent(source.read_bytes()))
    if not target.is_file() or target.read_bytes() != data:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    return target
OPTIONS_021 = {
    "ordering": [opt("ordering on")],
    "flatten": [opt("flatten yes")],
    "flatglob": [opt("flatglob *_CDNS_*"), opt("flatten yes")],
    "noduplicates": [READ_LIBRARY, opt("noduplicates yes")],
    "unique": [READ_LIBRARY, opt("unique yes")],
}


def library_case(cid: str, requirements: list[str], description: str, before: list[Line]):
    write_equivalent()
    body = [both("drc off"), opt(f"datestamp {LIBRARY_DATESTAMP}"), *before, READ_LIBRARY,
            rep("llength [cellname list allcells]"), both("writeall force")]
    return _case(cid, "oasis-read", requirements, description, body, {}, [CELL_DIGESTS, REPORT],
                 ["Magic GDS cross-check on the sky130 standard-cell library: Magic reads the GDS equivalent "
                  "(authoring/oasequiv.py) of KLayout's strm2oas file that Hefesto reads (captured by oasis-validate, "
                  "task 15.2); KLayout's strm2gds writes the coincident copies of a zero-displacement repetition "
                  "once, so it is not the equivalent. The saved cells are compared through their SHA-256 (the "
                  f"integer datestamp {LIBRARY_DATESTAMP} makes them stable)."],
                 output_names={CELL_DIGESTS: "cells.sha256"}, pdk="sky130A", external=list(LIBRARY_EXTERNALS),
                 run_timeout=1800)


def oasis_020():
    if captured_files("OASIS-020") is None:
        return None
    return library_case("OASIS-020", ["GDS-equivalence cross-check", "Cell processing", "Layer and datatype mapping"],
                        "The sky130 standard-cell library read by Hefesto from KLayout's strm2oas, by Magic from "
                        "the GDS equivalent of that file, with default options.", [])


def oasis_021() -> list:
    if captured_files("OASIS-020") is None:
        return []
    return [library_case(f"OASIS-021-{name}", ["Post-order reading and flattening options", "Read option set"],
                         f"The sky130 standard-cell library read with {name} (OASIS-020's inputs).", before)
            for name, before in OPTIONS_021.items()]


def specs() -> list:
    return [case for case in (oasis_012(), oasis_020(), *oasis_021()) if case is not None]


def not_authored() -> dict[str, str]:
    """The captured-input rows not written yet, with the dispatch they wait for."""
    waiting = ("captured input not synced yet: dispatch golden-capture.yml mode=oasis-validate, then "
               "oasis_cases.py --sync-captured <artifact dir>")
    rows = {}
    if captured_files("OASIS-012") is None:
        rows["OASIS-012"] = f"{waiting} (gdstk validation set, task 15.6)"
    if captured_files("OASIS-020") is None:
        rows["OASIS-020"] = f"{waiting} (KLayout strm2oas of {LIBRARY}; the job needs the sky130-gds-sc restore)"
        rows["OASIS-021"] = "follows OASIS-020 (the same captured inputs)"
    return rows
