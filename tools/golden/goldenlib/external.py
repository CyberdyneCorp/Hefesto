# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""External inputs of golden cases: pinned reference data that is never committed.

Design D1/D8 of openspec/changes/m1-core-viewer: PDK data is fetched, pinned, into
``.cache/pdks/`` and never committed, and Magic's ``scmos.tech`` is generated from
the pinned Magic sources (``tests/data/tech/generate_scmos.cmake``). A case that
needs such a file declares it in ``case.toml``::

    [[external]]
    source = "pdk"                                   # pdk | scmos | magic-sys
    path = "sky130A/libs.tech/magic/sky130A.tech"    # pdk: path in the pinned version
    to = "sky130A.tech"                              # file name in the work copy of input/

``capture.py`` and ``replay.py`` copy every external file into ``<work>/input/``
(``magic-sys`` files into ``<work>/cadroot/magic/sys/``) before the tool runs, so the reference tool (in a container without network) and
hefesto-cli read the same bytes. A missing file is an error for capture and a skip
for replay (:class:`ExternalUnavailable`).

Sources:

``pdk``    ``<pdk-root>/ciel/sky130/versions/<PDK_VERSION>/<path>``; the root is
           ``$HEFESTO_PDK_ROOT`` or ``<repo>/.cache/pdks`` and the version is the pin of
           ``scripts/fetch_pdk.py`` (which installs and verifies that subset).
``scmos``  ``$HEFESTO_SCMOS_TECH``, else ``<repo>/.cache/golden/scmos.tech``, generated
           on first use from ``build/ref/magic`` (``scripts/fetch_pdk.py magic-ref``)
           with ``tests/data/tech/generate_scmos.cmake`` (needs cmake and a C++ compiler).
``magic-sys`` ``<magic>/<path>`` of the pinned Magic checkout (``$HEFESTO_MAGIC_SRC`` or
           ``build/ref/magic``), e.g. ``graphics/FreeSans.pt3``: a file Magic installs in
           ``$CAD_ROOT/magic/sys`` (outline fonts). ``replay.py`` always sets ``CAD_ROOT``
           to ``<work>/cadroot``, so a replay never reads a developer's installation.
"""
from __future__ import annotations

import importlib.util
import os
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
SOURCES = ("pdk", "scmos", "magic-sys")
CAD_ROOT_DIR = "cadroot"
SYS_SUBDIR = Path("magic") / "sys"
GENERATE_TIMEOUT_S = 300


class ExternalUnavailable(RuntimeError):
    """An external input is not present on this machine."""


@dataclass(frozen=True)
class External:
    source: str
    to: str
    path: str = ""


def parse_external(raw: dict, index: int) -> External:
    """Validate one ``[[external]]`` table; raise ValueError when malformed."""
    where = f"external[{index}]"
    source, to, path = raw.get("source"), raw.get("to"), raw.get("path", "")
    if source not in SOURCES:
        raise ValueError(f"{where}: source must be one of {', '.join(SOURCES)}")
    if not isinstance(to, str) or "/" in to or to in ("", ".", ".."):
        raise ValueError(f"{where}: 'to' must be a plain file name")
    if source in ("pdk", "magic-sys") and (not isinstance(path, str) or not path or ".." in Path(path).parts
                                           or Path(path).is_absolute()):
        raise ValueError(f"{where}: a {source} input needs a relative 'path' inside the pinned version")
    return External(source, to, path)


def pdk_version() -> str:
    """The PDK pin of scripts/fetch_pdk.py (single source of truth)."""
    spec = importlib.util.spec_from_file_location("hefesto_fetch_pdk", REPO / "scripts" / "fetch_pdk.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.PDK_VERSION


def pdk_root() -> Path:
    env = os.environ.get("HEFESTO_PDK_ROOT")
    return Path(env) if env else REPO / ".cache" / "pdks"


def _resolve_pdk(ext: External) -> Path:
    path = pdk_root() / "ciel" / "sky130" / "versions" / pdk_version() / ext.path
    if not path.is_file():
        raise ExternalUnavailable(f"{path} not found (run scripts/fetch_pdk.py pdk)")
    return path


def _generate_scmos(target: Path) -> None:
    magic = REPO / "build" / "ref" / "magic"
    if not (magic / "scmos" / "scmos.tech.in").is_file():
        raise ExternalUnavailable(f"{magic}/scmos not found (run scripts/fetch_pdk.py magic-ref)")
    target.parent.mkdir(parents=True, exist_ok=True)
    argv = ["cmake", f"-DMAGIC={magic}", f"-DOUT={target.parent}", f"-DCXX={os.environ.get('CXX', 'c++')}",
            "-P", str(REPO / "tests" / "data" / "tech" / "generate_scmos.cmake")]
    try:
        subprocess.run(argv, check=False, capture_output=True, timeout=GENERATE_TIMEOUT_S)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ExternalUnavailable(f"cannot generate scmos.tech: {error}") from error
    if not target.is_file() or target.stat().st_size == 0:
        raise ExternalUnavailable("scmos.tech generation produced no file")


def _resolve_scmos() -> Path:
    env = os.environ.get("HEFESTO_SCMOS_TECH")
    if env:
        if not Path(env).is_file():
            raise ExternalUnavailable(f"HEFESTO_SCMOS_TECH={env} not found")
        return Path(env)
    target = REPO / ".cache" / "golden" / "scmos.tech"
    if not target.is_file():
        _generate_scmos(target)
    return target


def magic_source() -> Path:
    env = os.environ.get("HEFESTO_MAGIC_SRC")
    return Path(env) if env else REPO / "build" / "ref" / "magic"


def _resolve_magic_sys(ext: External) -> Path:
    path = magic_source() / ext.path
    if not path.is_file():
        raise ExternalUnavailable(f"{path} not found (run scripts/fetch_pdk.py magic-ref)")
    return path


def resolve(ext: External) -> Path:
    """Local path of an external input; raise ExternalUnavailable when absent."""
    if ext.source == "pdk":
        return _resolve_pdk(ext)
    if ext.source == "magic-sys":
        return _resolve_magic_sys(ext)
    return _resolve_scmos()


def cad_root(work: Path) -> Path:
    """The ``CAD_ROOT`` of a run in ``work`` (``magic-sys`` inputs land in its magic/sys)."""
    return work / CAD_ROOT_DIR


def stage(externals, input_copy: Path) -> None:
    """Copy every external input into ``input_copy`` (the work copy of input/), and
    ``magic-sys`` inputs into ``cad_root(<work>)/magic/sys``."""
    for ext in externals:
        target_dir = input_copy
        if ext.source == "magic-sys":
            target_dir = cad_root(input_copy.parent) / SYS_SUBDIR
            target_dir.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(resolve(ext), target_dir / ext.to)
