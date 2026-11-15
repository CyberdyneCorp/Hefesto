# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The MASK cases that run on the pinned PDKs (m5-mask-gds-cif design D19, tasks 13.6 and 13.7).

``rt_specs()``: the library round trips ``MASK-RT-<lib>`` (one per library with a single GDS
file) and ``MASK-RT-<lib>-fNN`` (file NN, 1-based, of a library with several), one fresh
session per file. Each copies ``tools/golden/harness/gds_rt/gds_rt.tcl`` into its ``input/``
and stages the library's ``libs.ref/<lib>/gds`` directory from the library GDS data set
(scripts/fetch_pdk.py) as ``input/lib``; the script picks the file by its index in sorted
order, so the cases need no file names (design D19 "Task 1.1 facts" gives the counts, which
the script checks). The output is too large to commit: ``expected/`` keeps the SHA-256 of the
decompressed, ``gds-mod-dates``-masked ``out.gds`` and its decoded summary
(goldenlib/derived.py ``gds-digest``, ``gds-summary``). The three standard-cell cases read the
``*-gds-sc`` sets and replay on every CI golden leg; the others carry ``gds_rt = true``
(label ``golden-gds-rt``) and are captured by golden-capture.yml ``capture-gds-rt`` in the
shards of scripts/gds_rt_shards.py. ``tools/golden/gds_rt_report.py`` adds Hefesto's
non-gating equivalence report of a second read and write.

``probe_specs()``: ``MASK-PROBE-<variant>``, one per PDK, record what the STYLES and INPUT
unit tests of tasks 3.7 and 7.4 derive from the technology files: for every output style,
Magic's list of its CIF layers (``cif see`` of an unknown layer prints every layer of the
current style, cif/CIFmain.c:382-392, so the count is ``cs_nLayers``), and for every input
style the ``Input style <name>: scaleFactor=<s>, multiplier=<m>`` line that selecting it prints.
"""
from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import casegen  # noqa: E402
import tclcase  # noqa: E402

DRIVER = "m5_cases.py"
HARNESS = casegen.REPO / "tools" / "golden" / "harness" / "gds_rt" / "gds_rt.tcl"
HARNESS_NAME = "gds_rt.tcl"
PREFIX = "MASK-RT-"
OUT = "out.gds"
DIGEST = f"derived:gds-digest:{OUT}"
SUMMARY = f"derived:gds-summary:{OUT}"
OUTPUT_NAMES = {DIGEST: "out.gds.sha256", SUMMARY: "out.gds.summary.txt"}
DEFAULT_TIMEOUT_S = 600  # the reference runner's default (goldenlib/refrunners.py)


@dataclass(frozen=True)
class Library:
    variant: str      # sky130A | gf180mcuD | ihp-sg13g2
    name: str         # libs.ref/<name>/gds
    files: int        # GDS files in the directory (design D19 "Task 1.1 facts")
    standard: bool = False   # the PDK's standard-cell library: replayed on every CI leg
    timeout: int = DEFAULT_TIMEOUT_S  # seconds per run, from task 1.1's Magic times with a margin


# Every library of the three GDS data sets (scripts/fetch_pdk.py) with its file count. Magic's slowest
# files (tasks.md 1.1): sky130_sram_2kbyte_1rw1r_32x512_8 469 s, efuse_wb_mem_1024x32 127 s,
# RM_IHPSG13_1P_8192x32_c4 2118 s; every other file is under 90 s.
LIBRARIES = (
    Library("sky130A", "sky130_fd_io", 9),
    Library("sky130A", "sky130_fd_pr", 1),
    Library("sky130A", "sky130_fd_sc_hd", 1, standard=True),
    Library("sky130A", "sky130_fd_sc_hdll", 1),
    Library("sky130A", "sky130_fd_sc_hs", 1),
    Library("sky130A", "sky130_fd_sc_hvl", 1),
    Library("sky130A", "sky130_fd_sc_lp", 1),
    Library("sky130A", "sky130_fd_sc_ls", 1),
    Library("sky130A", "sky130_fd_sc_ms", 1),
    Library("sky130A", "sky130_sram_macros", 4, timeout=1800),
    Library("gf180mcuD", "gf180mcu_as_sc_mcu7t3v3", 1),
    Library("gf180mcuD", "gf180mcu_fd_io", 2),
    Library("gf180mcuD", "gf180mcu_fd_ip_sram", 4),
    Library("gf180mcuD", "gf180mcu_fd_pr", 11),
    Library("gf180mcuD", "gf180mcu_fd_sc_mcu7t5v0", 1, standard=True),
    Library("gf180mcuD", "gf180mcu_fd_sc_mcu9t5v0", 1),
    Library("gf180mcuD", "gf180mcu_ocd_io", 1),
    Library("gf180mcuD", "gf180mcu_ocd_ip_sram", 4),
    Library("gf180mcuD", "gf180mcu_ocd_sc_rt12t3v3", 1),
    Library("gf180mcuD", "gf180mcu_osu_sc_gp12t3v3", 1),
    Library("gf180mcuD", "gf180mcu_osu_sc_gp9t3v3", 1),
    Library("gf180mcuD", "gf180mcu_re_efuse", 7, timeout=1200),
    Library("ihp-sg13g2", "sg13g2_io", 1),
    Library("ihp-sg13g2", "sg13g2_pr", 1),
    Library("ihp-sg13g2", "sg13g2_sram", 30, timeout=5400),
    Library("ihp-sg13g2", "sg13g2_stdcell", 1, standard=True),
)
STANDARD_CASES = tuple(f"{PREFIX}{lib.name}" for lib in LIBRARIES if lib.standard)

RT_REQUIREMENTS = ["Library header and trailer", "Structure ordering", "Mask geometry for each structure",
                   "Write option settings and defaults"]


def gds_set(lib: Library) -> str:
    """The data set of scripts/fetch_pdk.py that holds the library's GDS files."""
    pdk = tclcase.PDKS[lib.variant]
    return f"{pdk}-gds-sc" if lib.standard else f"{pdk}-gds"


def case_ids(lib: Library) -> list[str]:
    if lib.files == 1:
        return [f"{PREFIX}{lib.name}"]
    return [f"{PREFIX}{lib.name}-f{k:02d}" for k in range(1, lib.files + 1)]


def rt_case(lib: Library, index: int) -> tclcase.TclCase:
    cid = case_ids(lib)[index]
    which = "its GDS file" if lib.files == 1 else f"GDS file {index + 1} of {lib.files} (sorted by name)"
    body = "\n".join([tclcase.p("drc off"), f"source input/{HARNESS_NAME}",
                      f"rt::run_library input/lib {lib.files} {index} out"])
    return tclcase.TclCase(
        cid, "gds-write", RT_REQUIREMENTS,
        f"Library round trip of {lib.variant} {lib.name}, {which}: gds read, a wrapper placing every top "
        "structure with its timestamp pinned to 0, gds library yes, gds write.",
        body, files={HARNESS_NAME: HARNESS.read_text(encoding="utf-8")},
        outputs=[DIGEST, SUMMARY], output_names=dict(OUTPUT_NAMES), gds_rt=not lib.standard,
        # CIFhier's timed progress lines (Q-MASK-414), and the crash-backup file Magic names at
        # random during a long read (golden-capture run 43762085730, MASK-RT-gf180mcu_re_efuse-f04).
        stream_masks=["completed-percent", "magic-crash-file"],
        pdk=lib.variant, run_timeout=lib.timeout if lib.timeout != DEFAULT_TIMEOUT_S else None,
        external=[{"source": "pdk", "set": gds_set(lib), "path": f"{lib.variant}/libs.ref/{lib.name}/gds",
                   "to": "lib"}],
        driver=DRIVER,
        notes=["MASK-RT case (m5-mask-gds-cif design D19): fresh session with the PDK's rc and random seed 1.",
               "out.gds is compared through its masked SHA-256 and decoded summary (goldenlib/derived.py).",
               *(["A standard-cell case: every CI golden leg replays it (--require-inputs)."] if lib.standard
                 else ["Label golden-gds-rt: captured by golden-capture.yml capture-gds-rt "
                       "(scripts/gds_rt_shards.py), replayed by perf.yml gds-roundtrip."])])


def rt_specs() -> list[tclcase.TclCase]:
    return [rt_case(lib, k) for lib in LIBRARIES for k in range(lib.files)]


# --- PDK probes (tasks 3.7 and 7.4) ----------------------------------------------------------

PROBE_PREFIX = "MASK-PROBE-"
NO_LAYER = "mask_probe_no_such_layer"


def probe_case(variant: str) -> tclcase.TclCase:
    body = "\n".join([
        tclcase.p("drc off", "load mask_probe", "box 0 0 1 1", "cif listall ostyle", "cif listall istyle"),
        "foreach style [cif listall ostyle] {",
        "    probe [list cif ostyle $style]",
        f"    probe {{cif see {NO_LAYER}}}",
        "}",
        "foreach style [cif listall istyle] {",
        "    probe [list cif istyle $style]",
        "}"])
    return tclcase.TclCase(
        f"{PROBE_PREFIX}{variant}", "mask-output-styles",
        ["Style declaration and the style list", "Mask layer declarations"],
        f"{variant}: every output style's CIF layers (cif see of an unknown layer lists them) and every input "
        "style's 'Input style' line (cif istyle).",
        body, pdk=variant, driver=DRIVER,
        notes=["PDK probe of m5-mask-gds-cif tasks 3.7 and 7.4: Magic's per-style layer lists (cs_nLayers) and "
               "input-style scale lines, for the STYLES and INPUT unit tests."])


def probe_specs() -> list[tclcase.TclCase]:
    return [probe_case(variant) for variant in tclcase.PDKS]


def specs() -> list[tclcase.TclCase]:
    return rt_specs() + probe_specs()
