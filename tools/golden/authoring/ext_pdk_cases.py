# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The M7 PDK standard-cell cases (openspec/changes/m7-extraction design D21, task 17.4; Open Question 4 (a)).

``EXTRACT-PDK-<lib>`` extracts every cell of a PDK's standard-cell library with the PDK's default extract
style (the first style of its tech file, ``ngspice()`` for all three, task 2.1):

* ``sky130_fd_sc_hd`` (446 cells) and ``gf180mcu_fd_sc_mcu7t5v0`` (229) from the ``.mag`` files of the
  pinned tech sets (``libs.ref/<lib>/mag``, on the PDK rc file's search path);
* ``sg13g2_stdcell`` (84) from ``sg13g2_stdcell.gds`` of the ``ihp-gds-sc`` set (``gds read``), since the
  IHP set has no ``.mag`` library.

The script places every cell of the library in a wrapper cell ``pdk_lib``, one cell per row
``ROW_PITCH`` units apart (so no two uses interact), sets ``extract path ext`` (the directory is made
first, so neither tool prints ExtFileOpen's "attempting to create it" line) and runs ``extract all`` on
the wrapper: Magic writes one ``ext/<cell>.ext`` per cell plus ``ext/pdk_lib.ext``. The files are
collected (``*.ext``, ``ext-cap`` with ``ext-timestamp``); a cell whose file differs fails the case and is
named in the report (replay.py lists each differing collected file).

``NETLIST-PDK-<lib>`` stages the EXTRACT case's captured ``.ext`` files (``[[external]] source =
"golden"``, so the netlisters read Magic's own extraction) and runs, per cell, ``ext2spice lvs`` with
``hierarchy off`` (the flat LVS netlist, ``flat/<cell>.spice``) and ``ext2spice default``, ``hierarchy
on``, ``format ngspice`` (``hier/<cell>.spice``), then ``ext2sim pdk_lib`` (the whole library flattened
through the wrapper). Netlists use ``spice-cap``, the ``.sim`` ``sim-cap``.

Both run in a fresh session with the PDK's rc, ``--trust-pdk``, ``random seed 1``, ``TZ=UTC`` and
``LC_ALL=C`` (design D21 "Session-state pinning"). The NETLIST case can be captured only once the
EXTRACT case's ``expected/`` is committed (capture order: EXTRACT first). Size budget: 15 MB
uncompressed for the six cases (design D21); ``--sizes`` prints what is committed.

    python3 tools/golden/authoring/ext_pdk_cases.py            # write the six cases (never expected/)
    python3 tools/golden/authoring/ext_pdk_cases.py --sizes    # expected/ bytes per case and the total
"""
from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import casegen  # noqa: E402
import tclcase  # noqa: E402
from goldenlib.collect import Collect  # noqa: E402
from m7_capture import CONSOLE_MASKS  # noqa: E402
from tclcase import TclCase, lines, p  # noqa: E402

DRIVER = "m7_cases.py"
WRAPPER = "pdk_lib"
EXT_DIR = "ext"
ROW_PITCH = 20000  # units between two rows of the wrapper: far more than any standard cell's height
RUN_TIMEOUT_S = 1800
BUDGET_BYTES = 15 * 1000 * 1000  # design D21: the six PDK cases, uncompressed

EXT_COLLECT = (Collect("*.ext", "ext-cap", ("ext-timestamp", "ext-devres-uninit-width")),)  # Q-EXTRACT-36 (h)
NETLIST_COLLECT = (Collect("*.spice", "spice-cap"), Collect("*.sim", "sim-cap"), Collect("*.al", "bytes"),
                   Collect("*.nodes", "bytes"))


@dataclass(frozen=True)
class Library:
    name: str          # libs.ref/<name>
    variant: str       # sky130A | gf180mcuD | ihp-sg13g2
    cells: int         # task 2.1's counts
    gds_set: str = ""  # the GDS data set when the library has no .mag files

    @property
    def mag_dir(self) -> str:
        return f"libs.ref/{self.name}/mag"


LIBRARIES = (
    Library("sky130_fd_sc_hd", "sky130A", 446),
    Library("gf180mcu_fd_sc_mcu7t5v0", "gf180mcuD", 229),
    Library("sg13g2_stdcell", "ihp-sg13g2", 84, gds_set="ihp-gds-sc"),
)


def extract_id(lib: Library) -> str:
    return f"EXTRACT-PDK-{lib.name}"


def netlist_id(lib: Library) -> str:
    return f"NETLIST-PDK-{lib.name}"


def case_ids() -> list[str]:
    return [i for lib in LIBRARIES for i in (extract_id(lib), netlist_id(lib))]


def _cell_list(lib: Library) -> list[str]:
    """Tcl that sets ``cells`` to the library's cells, sorted, and prints their count."""
    if lib.gds_set:
        return ["set pdk_before [cellname list allcells]",
                p(f"gds read input/lib/{lib.name}.gds"),
                "set cells {}",
                "foreach cell [lsort [cellname list allcells]] {",
                "    if {[lsearch -exact $pdk_before $cell] < 0} {lappend cells $cell}",
                "}"]
    mags = f"input/pdk/{lib.variant}/{lib.mag_dir}"
    return ["set cells {}",
            f"foreach mag [lsort [glob -nocomplain -directory {mags} *.mag]] {{",
            "    lappend cells [file rootname [file tail $mag]]",
            "}"]


def extract_body(lib: Library) -> str:
    return lines(
        p("drc off"), *_cell_list(lib),
        "puts [list pdk cells [llength $cells]]",
        p(f"load {WRAPPER}"),
        "set row 0",
        "foreach cell $cells {",
        "    box 0 $row 0 $row",
        "    if {[catch {getcell $cell} err]} {puts [list pdk getcell-error $cell $err]}",
        f"    incr row {ROW_PITCH}",
        "}",
        p("select clear", "box 0 0 1 1"),
        f"file mkdir {EXT_DIR}",
        p(f"extract path {EXT_DIR}", "extract all"),
        f"puts [list pdk ext-files [llength [glob -nocomplain -directory {EXT_DIR} *.ext]]]")


def extract_case(lib: Library) -> TclCase:
    source = f"sg13g2_stdcell.gds of the {lib.gds_set} set (gds read)" if lib.gds_set else f"the .mag files of {lib.mag_dir}"
    external = ([{"source": "pdk", "set": lib.gds_set, "path": f"{lib.variant}/libs.ref/{lib.name}/gds", "to": "lib"}]
                if lib.gds_set else [])
    return TclCase(
        extract_id(lib), "extract-commands", ["Extract all, cell, parents and showparents", "Output path and file location"],
        f"{lib.variant} {lib.name} ({lib.cells} cells, {source}): every cell placed in a wrapper {WRAPPER}, "
        f"extract path {EXT_DIR}, extract all with the PDK's default style: one .ext per cell",
        extract_body(lib), pdk=lib.variant, pdk_cells=[] if lib.gds_set else [lib.mag_dir], external=external,
        stream_masks=CONSOLE_MASKS[1:], collect=EXT_COLLECT, run_timeout=RUN_TIMEOUT_S, driver=DRIVER,
        notes=["PDK case of m7-extraction design D21 (task 17.4): fresh session, the PDK's rc, --trust-pdk, random "
               "seed 1, TZ=UTC, LC_ALL=C.",
               f"Collected: {EXT_DIR}/<cell>.ext per cell and {EXT_DIR}/{WRAPPER}.ext (ext-cap); a differing cell "
               "is named by its file."])


def netlist_body() -> str:
    return lines(
        p("drc off"),
        f"addpath input/{EXT_DIR}",
        "set cells {}",
        f"foreach ext [lsort [glob -nocomplain -directory input/{EXT_DIR} *.ext]] {{",
        "    set cell [file rootname [file tail $ext]]",
        f"    if {{$cell ne \"{WRAPPER}\"}} {{lappend cells $cell}}",
        "}",
        "puts [list pdk cells [llength $cells]]",
        "file mkdir flat hier",
        p("ext2spice lvs", "ext2spice hierarchy off"),
        "foreach cell $cells {",
        "    probe [list ext2spice $cell]",
        "    if {[file exists $cell.spice]} {file rename -force $cell.spice flat/$cell.spice}",
        "}",
        p("ext2spice default", "ext2spice hierarchy on", "ext2spice format ngspice"),
        "foreach cell $cells {",
        "    probe [list ext2spice $cell]",
        "    if {[file exists $cell.spice]} {file rename -force $cell.spice hier/$cell.spice}",
        "}",
        p(f"ext2sim {WRAPPER}"))


def netlist_case(lib: Library) -> TclCase:
    staged = {"source": "golden", "path": f"EXTRACT/cases/{extract_id(lib)}/expected/collected/{EXT_DIR}",
              "to": EXT_DIR}
    return TclCase(
        netlist_id(lib), "netlist-spice-options", ["Run arguments and output file", "Flat netlist structure"],
        f"{lib.variant} {lib.name}: the .ext files captured by {extract_id(lib)}; per cell ext2spice lvs with "
        "hierarchy off (flat/) and ext2spice hierarchy on format ngspice (hier/); ext2sim of the wrapper "
        f"{WRAPPER} (the whole library)",
        netlist_body(), pdk=lib.variant, external=[staged], stream_masks=CONSOLE_MASKS[1:],
        collect=NETLIST_COLLECT, run_timeout=RUN_TIMEOUT_S, driver=DRIVER,
        notes=["PDK case of m7-extraction design D21 (task 17.4): fresh session, the PDK's rc, --trust-pdk, random "
               "seed 1, TZ=UTC, LC_ALL=C.",
               f"input/{EXT_DIR} is staged from {extract_id(lib)}'s expected/ (source golden): capture it after that "
               "case is committed."])


def cases() -> list[TclCase]:
    return [c for lib in LIBRARIES for c in (extract_case(lib), netlist_case(lib))]


def expected_bytes(case_id: str) -> int:
    area = case_id.split("-")[0]
    root = casegen.GOLDEN / area / "cases" / case_id / "expected"
    return sum(f.stat().st_size for f in root.rglob("*") if f.is_file()) if root.is_dir() else 0


def size_report() -> tuple[list[str], int]:
    rows = [(cid, expected_bytes(cid)) for cid in case_ids()]
    total = sum(n for _, n in rows)
    report = [f"{cid}: {n} bytes" for cid, n in rows]
    report.append(f"total: {total} bytes (budget {BUDGET_BYTES}: {'within' if total <= BUDGET_BYTES else 'OVER'})")
    return report, total


def main(argv: list[str]) -> int:
    if argv == ["--sizes"]:
        report, total = size_report()
        print("\n".join(report))
        return 0 if total <= BUDGET_BYTES else 1
    for case in cases():
        print(tclcase.write(case).relative_to(casegen.REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
