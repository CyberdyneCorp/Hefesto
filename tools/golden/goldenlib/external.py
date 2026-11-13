# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""External inputs of golden cases: pinned reference data that is never committed.

Design D1/D8 of openspec/changes/m1-core-viewer: PDK data is fetched, pinned, into
``.cache/pdks/`` and never committed, and Magic's ``scmos.tech`` is generated from
the pinned Magic sources (``tests/data/tech/generate_scmos.cmake``). A case that
needs such a file declares it in ``case.toml``::

    [[external]]
    source = "pdk"                                   # pdk | scmos | magic-sys | magic-tcl
    set = "sky130"                                   # pdk only: sky130 (default) | gf180mcu | ihp
    path = "sky130A/libs.tech/magic/sky130A.tech"    # pdk: path in the pinned version
    to = "sky130A.tech"                              # path in the work copy of input/

``capture.py`` and ``replay.py`` copy every external file into ``<work>/input/``
(``magic-sys`` files into ``<work>/cadroot/magic/sys/``, ``magic-tcl`` files into
``<work>/cadroot/magic/tcl/``) before the tool runs, so the reference tool (in a container without network) and
hefesto-cli read the same bytes. A missing file is an error for capture and a skip
for replay (:class:`ExternalUnavailable`).

Sources:

``to`` is a file name or a relative path (``pdk/sky130A/libs.tech/magic``: the SCRIPT
cases rebuild a PDK tree under ``input/pdk`` for ``PDK_ROOT``); a ``pdk`` ``path`` that
names a directory is copied whole.

``pdk``    ``<pdk-root>/ciel/<family>/versions/<pin>/<path>``; the root is
           ``$HEFESTO_PDK_ROOT`` or ``<repo>/.cache/pdks``. ``set`` names the data set of
           ``scripts/fetch_pdk.py`` (``DATA_SETS``, m3-drc design D15) whose family and pin
           are used: ``sky130`` (the default), ``gf180mcu`` or ``ihp``.
``scmos``  ``$HEFESTO_SCMOS_TECH``, else ``<repo>/.cache/golden/scmos.tech``, generated
           on first use from ``build/ref/magic`` (``scripts/fetch_pdk.py magic-ref``)
           with ``tests/data/tech/generate_scmos.cmake`` (needs cmake and a C++ compiler).
``magic-sys`` ``<magic>/<path>`` of the pinned Magic checkout (``$HEFESTO_MAGIC_SRC`` or
           ``build/ref/magic``), e.g. ``graphics/FreeSans.pt3``: a file Magic installs in
           ``$CAD_ROOT/magic/sys`` (outline fonts). ``replay.py`` always sets ``CAD_ROOT``
           to ``<work>/cadroot``, so a replay never reads a developer's installation.
``magic-tcl`` ``<magic>/<path>`` of the pinned Magic checkout, e.g. ``tcltk/strip_reflibs.tcl``:
           a script Magic installs in ``$CAD_ROOT/magic/tcl`` (the auxiliary scripts a case
           sources from ``$CAD_ROOT``), staged in ``<work>/cadroot/magic/tcl``.
"""
from __future__ import annotations

import importlib.util
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
SOURCES = ("pdk", "scmos", "magic-sys", "magic-tcl")
MAGIC_SOURCES = ("magic-sys", "magic-tcl")
CAD_ROOT_DIR = "cadroot"
SYS_SUBDIR = Path("magic") / "sys"
CAD_ROOT_SUBDIRS = {"magic-sys": SYS_SUBDIR, "magic-tcl": Path("magic") / "tcl"}
GENERATE_TIMEOUT_S = 300


class ExternalUnavailable(RuntimeError):
    """An external input is not present on this machine."""


# The data sets of scripts/fetch_pdk.py (DATA_SETS): the M4 tech/.mag sets and the M5 library GDS sets
# (m5-mask-gds-cif D19), which live under their own PDK root (<pdk-root>/gds).
PDK_SETS = (
    "sky130", "gf180mcu", "ihp",
    "sky130-gds", "gf180mcu-gds", "ihp-gds",
    "sky130-gds-sc", "gf180mcu-gds-sc", "ihp-gds-sc",
)


@dataclass(frozen=True)
class External:
    source: str
    to: str
    path: str = ""
    set: str = "sky130"


def parse_external(raw: dict, index: int) -> External:
    """Validate one ``[[external]]`` table; raise ValueError when malformed."""
    where = f"external[{index}]"
    source, to, path = raw.get("source"), raw.get("to"), raw.get("path", "")
    if source not in SOURCES:
        raise ValueError(f"{where}: source must be one of {', '.join(SOURCES)}")
    if not _is_relative_inside(to):
        raise ValueError(f"{where}: 'to' must be a relative path inside the work copy of input/")
    if source in ("pdk", *MAGIC_SOURCES) and (not isinstance(path, str) or not path or ".." in Path(path).parts
                                           or Path(path).is_absolute()):
        raise ValueError(f"{where}: a {source} input needs a relative 'path' inside the pinned version")
    pdk_set = raw.get("set", "sky130")
    if "set" in raw and source != "pdk":
        raise ValueError(f"{where}: 'set' applies to pdk inputs only")
    if pdk_set not in PDK_SETS:
        raise ValueError(f"{where}: set must be one of {', '.join(PDK_SETS)}")
    return External(source, to, path, pdk_set)


def _is_relative_inside(path) -> bool:
    """A non-empty relative path without ``..`` (a file name, or ``pdk/sky130A/...``)."""
    return (isinstance(path, str) and path not in ("", ".") and not Path(path).is_absolute()
            and ".." not in Path(path).parts)


def _fetch_pdk_module():
    """scripts/fetch_pdk.py, the single source of truth of the PDK pins (loaded once)."""
    name = "hefesto_fetch_pdk"
    if name not in sys.modules:
        spec = importlib.util.spec_from_file_location(name, REPO / "scripts" / "fetch_pdk.py")
        module = importlib.util.module_from_spec(spec)
        sys.modules[name] = module  # dataclasses resolve their module through sys.modules
        spec.loader.exec_module(module)
    return sys.modules[name]


def pdk_version() -> str:
    """The sky130 PDK pin of scripts/fetch_pdk.py."""
    return _fetch_pdk_module().PDK_VERSION


def pdk_root() -> Path:
    env = os.environ.get("HEFESTO_PDK_ROOT")
    return Path(env) if env else REPO / ".cache" / "pdks"


def pdk_set_root(pdk_set: str) -> Path:
    """``<set root>/ciel/<family>/versions/<pin>`` of a PDK data set of scripts/fetch_pdk.py.

    The set root is ``<pdk-root>`` for the tech/.mag sets and ``<pdk-root>/<pdk_dir>`` for the GDS sets.
    """
    return _fetch_pdk_module().DATA_SETS[pdk_set].version_dir(pdk_root())


def _resolve_pdk(ext: External) -> Path:
    path = pdk_set_root(ext.set) / ext.path
    if not path.exists():
        raise ExternalUnavailable(f"{path} not found (run scripts/fetch_pdk.py {ext.set})")
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
    if ext.source in MAGIC_SOURCES:
        return _resolve_magic_sys(ext)
    return _resolve_scmos()


def cad_root(work: Path) -> Path:
    """The ``CAD_ROOT`` of a run in ``work`` (``magic-sys`` inputs land in its magic/sys)."""
    return work / CAD_ROOT_DIR


def stage(externals, input_copy: Path) -> None:
    """Copy every external input into ``input_copy`` (the work copy of input/), and
    ``magic-sys`` and ``magic-tcl`` inputs into ``cad_root(<work>)/magic/sys`` and ``/magic/tcl``."""
    for ext in externals:
        target_dir = input_copy
        if ext.source in CAD_ROOT_SUBDIRS:
            target_dir = cad_root(input_copy.parent) / CAD_ROOT_SUBDIRS[ext.source]
            target_dir.mkdir(parents=True, exist_ok=True)
        source, target = resolve(ext), target_dir / ext.to
        target.parent.mkdir(parents=True, exist_ok=True)
        if source.is_dir():
            shutil.copytree(source, target, dirs_exist_ok=True)
        else:
            shutil.copyfile(source, target)
