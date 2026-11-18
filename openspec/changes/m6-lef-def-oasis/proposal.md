# Proposal: M6 LEF/DEF and OASIS

## Why

After M5, Hefesto generates masks and reads and writes GDS and CIF. It still cannot exchange designs with digital place-and-route flows, and it cannot read or write OASIS:
- The tech `lef` section is kept as raw lines, and no LEF layer table exists.
- `lef` and `def` are not registered. The command line and PDK startup paths already issue `lef read` and `def read` (`app/startup.cpp`, `script/pdk/startup.cpp`), and both fail as unknown commands.
- Abstract views can only be built by hand, from `.mag` cells with `LEFview` properties, as the M5 golden cases did.
- There is no OASIS code. Decision P-6 requires OASIS read and write, and `oasis-read`/`oasis-write` cite M5 decisions that M5 deliberately left for M6.
- MASK-OS-016 waits for `lef write`.

M6 (docs/roadmap.md, "M6") delivers:
- LEF/DEF: abstract views, and DEF import and export for place-and-route flows;
- OASIS: read and write, equivalent to the GDS path and validated with KLayout (`strm2oas`, `strmxor`).

Its exit criteria are:
- the common exit criteria;
- the LEFDEF and OASIS golden cases pass;
- the 419-byte OASIS reference file is confirmed by a third-party reader.

The 99 open questions that blocked M6 were approved as drafted on 2026-11-13, with the three judgement items decided as option (a) (commit "Resolve the 99 open questions blocking M6", `docs/decisions/m6-decision-table.md`). The specs were aligned in "Align the M6 specs with the approved decisions", and `scripts/check_milestone_readiness.py --milestone M6` reports ready. The one LEFDEF entry that no spec cites, Q-LEFDEF-114, and the plan's Open Questions 1–6 were decided by the user on 2026-11-18, each as recommended (design "Open Questions" and "Resolved Questions"): Q-LEFDEF-114 is compat; the `.ext` read-and-flatten subset and the extraction-style parts are pulled forward from M7 (1, 2: (a)); OASIS-012/030/061 carry an approved deviation for their missing commercial inputs and Q-OASIS-2's purchase of SEMI P39 is reopened (3: (b)); the M6 benches gate at their own baseline + 2 % (4: (a)); the desktop adds no menu items (5: (a)); and real-PDK LEF data sets and `LEFDEF-RT-<lib>` cases are added (6: (a)).

## What Changes

- **Reference data and tools first (PDK, GOLDEN).**
  - LEF data sets per PDK (tech LEF and standard-cell LEF) (Open Question 6 (a)).
  - KLayout and gdstk, pinned in the capture image as reference tools under the clean-room rule (design D18).
  - A capture mode that writes Magic's `.ext` files for the DEF-write net cases (Open Question 1 (a)).
- **LEF technology section (LEFTECH).** `lef-tech-section` adds the LEF layer table. It is parsed at tech load in Magic's string-hash order (`util::MagicHashTable`). Defaults come from the DRC rules with an explicit unset flag (Q-LEFDEF-1, 2), and the table is rescaled by `scalegrid` and finer-grid reads.
- **Via geometry (LEFVIA).** One generator serves:
  - LEF VIA/VIARULE vias, and via-rule vias with magic-compat OFF (Q-LEFDEF-7);
  - DEF generated vias and the VIAS properties (Q-LEFDEF-60, 62, 63);
  - cut growing to the output style's contact size (Q-LEFDEF-17);
  - DEF write's contact-region identification and via naming (Q-LEFDEF-105, 106, 107, 111, 115).
- **LEF read (LEFR).** `lef-read` adds:
  - the LEF/DEF tokenizer and message reporting;
  - the LAYER, VIA, NONDEFAULTRULE, SITE and MACRO sections;
  - abstract-view creation and the annotation of existing cells;
  - `lef read` and `lef datestamp`;
  - each compat split (Q-LEFDEF-3, 6, 7, 10–15, 20, 21).

  A read is one undoable transaction (Q-LEFDEF-22).
- **LEF write (LEFW).** `lef-write` adds:
  - macro construction from a flat copy;
  - pins by port index, with antenna areas;
  - `-hide`, `-pinonly`, `-toplayer` and `-nomaster`;
  - `-tech`, `writeall` and `nocheck`;
  - Magic's float32 number formatting (Q-LEFDEF-45);
  - the compat splits Q-LEFDEF-31–39 and 51.
- **DEF read (DEFR).** `def-read` adds:
  - DIEAREA, VIAS, COMPONENTS, PINS, NETS and SPECIALNETS, NONDEFAULTRULES and BLOCKAGES;
  - the both-modes default-width fix (Q-LEFDEF-1);
  - the compat splits Q-LEFDEF-60–84.
- **DEF write (DEFW).** `def-write` adds:
  - DIEAREA, VIAS, COMPONENTS in use-index order, PINS and NONDEFAULTRULES;
  - NETS, SPECIALNETS and BLOCKAGES from the `.ext` netlist (EXTSUB, Open Question 1 (a));
  - the compat splits Q-LEFDEF-91–112.
- **Netlist and extraction-style subsets (EXTSUB; Open Questions 1 and 2, decided (a)).**
  - The `.ext` read-and-flatten subset that `def write` calls (EFReadFile, EFFlatBuild, EFVisitNodes).
  - The device, antenna tie-down, substrate and active types of the extraction style that LEF and DEF write read.

  Both are built to the M7 specs in M7's area layout, so M7 extends them. Neither brings extraction into M6.
- **OASIS codec (OASFMT).** `oasis-format` adds the decoder and encoder: primitives, every record, modal variables, repetitions, name tables in strict and non-strict layout, CBLOCK through the bundled zlib, and CRC32/CHECKSUM32 validation (Q-OASIS-1–11). The 419-byte reference file is built from the spec listing.
- **OASIS read (OASR).** `oasis-read` drives M5's `mask::ReadEngine`, so scaling, layer mapping and labels match gds-read. It also adds:
  - the GDS-equivalence rules (Q-OASIS-10, 12, 30, 33, 37, 38, 39);
  - CIRCLE with platform-identical trig (Q-OASIS-31);
  - read-only vendor views with `OASIS_FILE`/`OASIS_CELL` and content-hash timestamps over decompressed bytes.
- **OASIS write (OASW).** `oasis-write` reuses M5's prepare and finish phases (subtree loading, Magic-order searches, mismatch fix-up) and the GDS naming, label and merge code, so the content is the one `gds write` would produce. It adds:
  - repetition detection;
  - the defaults Q-OASIS-65–67;
  - transcoding of read-only GDS cells (Q-OASIS-63) and OASIS cells (Q-OASIS-40).

  `gds write` transcodes read-only OASIS cells (Q-OASIS-35 (a), the new `gds-write` requirement).
- **Commands (LEFR, LEFW, DEFR, DEFW, OASCMD).**
  - One `lef`/`def` dispatcher, as Magic's CmdLef.
  - The `oasis` command, with the `gds` options and the OASIS-only options.
  - The 13 read settings shared with `gds` (Q-OASIS-90 (a)), split out of `gds::GdsSettings` by a behaviour-preserving refactor.
  - Format recognition on open by the OASIS magic bytes.
- **Desktop (SHELL).** Format recognition through the file picker and drag and drop, and progress and cancel for the new reads and writes. There are no new menu items (Open Question 5 (a)).
- **Golden, performance, fuzz and exit (GOLDEN, PERF, INTEGRATION).**
  - GOLDEN authors the 143 LEFDEF and 51 OASIS cases and MASK-OS-016, and corrects OASIS-091 to the approved Q-OASIS-90.
  - It adds an OASIS record writer for hand-built inputs, a clean-room OASIS decoder for the comparators, the `stream-geometry` comparator (OASIS against Magic's GDS), and the `oasis-validate` mode that runs KLayout and gdstk.
  - `lefdef-bench` and `oasis-bench`, each workload gated at its own callgrind baseline + 2 %, with the Magic and GDS comparisons informational (Open Question 4 (a)).
  - Fuzz targets are added for the LEF, DEF, `lef` section, OASIS and `.ext` readers.
  - A security review of the new readers.
  - The M6 exit check.

### Non-goals

- Extraction, `.ext` writing, netlisting and antenna checking (M7). EXTSUB reads `.ext` files that Magic wrote and parses only what LEF/DEF write need.
- Hierarchical DEF (`def writeall` is refused, as in Magic), and the DEF sections Magic skips (FILLS, ROWS, TRACKS, GROUPS, ...).
- Gzip-wrapped OASIS output (Q-OASIS-91), a private label-presentation property (Q-OASIS-32) and buying SEMI P39 (Q-OASIS-2's judgement item, reopened 2026-11-18 with option (b) and not decided).
- Running KLayout or gdstk from the product. They are capture-image reference tools only (P-18).
- Multithreaded LEF/DEF/OASIS I/O.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

None. M6 implements the existing requirements of the 9 capabilities listed in docs/roadmap.md ("M6"): `lef-tech-section`, `lef-read`, `lef-write`, `def-read`, `def-write`, `oasis-format`, `oasis-read`, `oasis-write` and `oasis-command`. These hold 215 requirements, 146 of them exact or behavioural.

M6 also implements `gds-write` "Read-only OASIS vendor cells", added by the Q-OASIS-35 alignment, and makes the M1–M5 changes listed in design D17.

Every open question these specs cite is resolved, and the specs were aligned. The change therefore sets `skip_specs: true`, following the M0–M5 precedent. A workstream that finds a requirement that must change raises it as an open item. INTEGRATION then edits the living spec in this change's branch. The 2026-11-18 answers needed no requirement change. They added prose only: `def-write` and `lef-write` name the pulled-forward `.ext` and extraction-style subsets, `oasis-format` records the OASIS-012/030/061 deviation, and the roadmap's M6 and M7 entries name the pull-forward and the deviation.

## Impact

- **New code:**
  - `src/core/{include/hefesto,src}/lef/{lex,tech,via,read,write,command}/`
  - `src/core/{include/hefesto,src}/def/{read,write}/`
  - `src/core/{include/hefesto,src}/oasis/{codec,read,write,command}/`
  - `src/core/{include/hefesto,src}/{extflat,extract/tech}/` (Open Questions 1 and 2 (a); `extract/tech/` is shared with M10's subset)
  - `src/core/src/gds/write/vendor_oasis.cpp`
  - `tests/unit/{lef,def,oasis,extflat,extract}/`
  - `tests/fuzz/{fuzz_lef_reader,fuzz_def_reader,fuzz_lef_section,fuzz_oasis_reader,fuzz_ext_reader}.cpp`
  - `bench/{lefdef,oasis}/`
- **Shared changes:**
  - `gds::GdsSettings` is split into `stream::StreamReadSettings` and `GdsSettings`.
  - `MaskSession` gains `stream_read` and `oasis`, and `EditorState` gains `lefdef`.
  - gds-read's stamp procedure and the inherited read helpers become shared functions.
  - `OASIS_FILE` makes a cell read-only.
  - `cell/timestamps.cpp` stamps `OASIS_FILE` cells.
  - `tech/loader.cpp` and `tech/scale_layout.cpp` build and rescale the LEF table.
  - The generic open path sniffs OASIS.
  - `magic_help.cpp` and the UI-G-005 deviation drop `def, lef`.
  - layout-undo lists `lef read`/`def read` (design D17).
- **Dependencies:** none new in the product. zlib provides raw deflate and CRC32. KLayout and gdstk are added to the capture image only, pinned in `tools/golden/pins.toml`.
- **Data:** LEF data sets `sky130-lef`, `gf180mcu-lef` and `ihp-lef` (Open Question 6 (a)), never committed and cached per set. No commercial OASIS inputs (Open Question 3 (b)).
- **Golden cases:**
  - LEFDEF: 143 rows in `tests/golden/LEFDEF/CASES.md`, plus one `LEFDEF-RT-<lib>` case per standard-cell library (Open Question 6 (a)).
  - OASIS: 51 rows in `tests/golden/OASIS/CASES.md`.
  - MASK-OS-016.
  - Deviations for the both-modes fixes Q-LEFDEF-1 (DEF-read width), 73 and 83, and for the missing commercial inputs of OASIS-012, 030 and 061 (Q-OASIS-2).
- **Docs:**
  - the diagnostic code documents `docs/diagnostics/codes/{lef,def,oasis}.md`, and `extflat.md` and `extract.md` if needed;
  - `docs/building.md` (LEF data, the reference tools);
  - `docs/contributing/` (areas, fuzz targets, golden modules, `oasis-validate`, the security review);
  - `docs/design/m6-ui-conformance.md`;
  - the roadmap status and CHANGELOG.
