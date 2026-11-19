# Design: M6 LEF/DEF and OASIS

## Context

See proposal.md for the motivation. M5 (openspec/changes/m5-mask-gds-cif) delivered:
- the cifoutput and cifinput readers and every mask operator;
- flat and hierarchical generation;
- the bplane use index;
- GDS read and write, with compressed files, read-only vendor views and content-hash timestamps;
- CIF read and write;
- DRC on mask layers.

The conventions of M0–M5 still hold:
- SPDX headers;
- Catch2 tags `[cap:<capability>][req:<exact requirement heading>]`;
- per-area diagnostic code tables `AREA-TOPIC-NNN` in `docs/diagnostics/codes/<area>.md`;
- cognitive complexity of at most 15 per function (flag the irreducible ones);
- no UI, GPU, Raven or process headers in the core;
- golden expected outputs captured only in CI (`.github/workflows/golden-capture.yml`);
- Magic 8.3.694 at fc5daa9e as the behavioural reference;
- the clean-room rule for netgen, xschem and KLayout;
- golden cases run with magic-compat ON, and the OFF behaviours are unit-tested.

M6 starts once the M5 branch (PR #65) has merged. M6 edits M5 code in only the places listed in D17, and only after that merge.

What M5 and earlier milestones leave for M6 (from the code at origin/main f03da5f2, 2026-11-17):

- **Technology.** The `lef` and `extract` sections are kept verbatim as `RawSection` lines (`tech/tech_file.cpp:46-47`, `tech/technology.hpp` `RawSection`). No LEF layer table exists, and no extraction style is parsed. The DRC default width and spacing that the LEF table needs exist as `drc::default_width` and `drc::default_spacing` (`drc/rules/query.hpp`, DRCGetDefaultLayerWidth/Spacing). `scalegrid` and finer-grid reads rescale through `tech/scale_layout.cpp`.
- **Mask and GDS (M5).**
  - `mask::OutputStyle` answers the contact cut size (CIFGetContactSize, `mask/style/style.hpp`). LEF cut growing (Q-LEFDEF-17: the output style) and DEF generated vias use it.
  - `mask::ReadSink` and `mask::ReadEngine` (`mask/input/{sink,engine}.hpp`) are the format-neutral read engine that the GDS and CIF parsers drive.
  - `gds::prepare_write`, `emit_write` and `finish_write` (`gds/write/write.hpp`) do the write's subtree loading, mismatch fix-up and Magic-order searches, then generate and emit.
  - `gds::merge_polygons`, the label translation (`gds/write/labels_text.cpp`), the character tables and string limits (`gds/write/emit.hpp` `char_table`, `string_options`), the Q-MASK-209 renaming (`gds/write/names.cpp`), the reference properties 61/99 (`gds/write/references.cpp`) and the verbatim copy of read-only GDS cells (`gds/write/vendor.cpp`) are the GDS-side pieces OASIS write must match.
  - `gds::content_stamps`, `VendorStamper`, `readonly_units_error` and `gds_file_value` (`gds/read/{stamps,vendor}.hpp`) implement the vendor views that OASIS repeats with `OASIS_FILE`.
  - The gzip source has the M5-S1 inflate cap (`gds/stream/gzip.cpp`).
  - `mask::MaskSession` (`mask/session.hpp`) holds `gds::GdsSettings` (`gds/command/settings.hpp`), with the read and write options in one struct. The read warning level and `rescale` are already shared with `cif`.
- **Database.**
  - `db::UseIndex` gives the use enumeration order (Q-MASK-204) that the OASIS writer, `lef writeall` (Q-LEFDEF-41) and DEF COMPONENTS follow.
  - `db::decompose_polygon` (`db/polygon.hpp`) is the polygon decomposition that LEF POLYGON and DEF pin and route polygons share (Q-LEFDEF-66, 70).
  - `db::flatten_to_new` (`db/hierarchy.hpp`) flattens a cell for LEF write.
  - `connect::search_flat` and `search_hierarchical` (`connect/search.hpp`) are the connectivity searches behind LEF pin nets and DEF net geometry.
  - `util::MagicHashTable` (`util/magic_hash_table.hpp`) reproduces Magic's string hash order (Q-LEFDEF-30, 82, 92, 116; X-1 as amended by D-1).
- **Cells.**
  - `cell::LookupContext`, `search_file` and `open_candidate` (`cell/search_path.hpp`) are the file lookup that `gds::find_gds_file` already uses.
  - `cell/timestamps.cpp` has the vendor-stamp hook and the fixed-stamp flag (`lef datestamp`, CDFIXEDSTAMP).
  - The `.mag` instance-id renaming is the rule Q-LEFDEF-83 reuses.
- **Commands.** Neither `lef` nor `def` nor `oasis` is registered. `ui/magic_help.cpp:129` already carries Magic's `lef` help line. UI-G-005's deviation (`tests/golden/deviations.toml:137`) lists `def` and `lef` as M6 commands. The startup paths already issue `lef read`/`def read` for `.lef`/`.def` arguments (`app/startup.cpp:72-77`) and for PDK startup files (`script/pdk/startup.cpp:207-211`), and today both fail as unknown commands.
- **Settings.** The project setting "global nets and power names" (`app/settings_layers.cpp`, `app/project_file.cpp`) holds the `vdd`, `gnd` and `globalNets` values that Q-LEFDEF-44 and Q-LEFDEF-102 use.
- **Extraction.** Nothing exists: extraction, `.ext` files and EFRead are M7. `def write` reads its nets from `.ext` files (`def-write` "Preconditions and diagnostics", defWrite.c:251-293, `EFReadFile`/`EFFlatBuild`). LEF write's antenna areas and well set, and DEF write's blockage filter, read the current extraction style (lef-write "Macro source geometry and layer masks", ExtTech.c:514-562; defWrite.c:816, 2538). Open Questions 1 and 2, decided (a) on 2026-11-18, pull both subsets forward into M6 (D10, EXTSUB).
- **Golden.**
  - `tests/golden/LEFDEF/CASES.md` has 143 rows: 001–023 (tech section, LEF read), 100–142 (LEF write), 200–240 (DEF read) and 300–335 (DEF write).
  - `tests/golden/OASIS/CASES.md` has 51 rows: 001–015 (format), 020–037 (read), 050–064 (write) and 090–092 (command).
  - None is authored.
  - MASK-OS-016 (the contact cut size query through LEF write) is deferred to M6.
  - OASIS-091 still describes independent `gds`/`oasis` settings, which the approved Q-OASIS-90 (a) reversed.
  - `tools/golden/goldenlib/gds.py` decodes GDS, and `tools/golden/compare/polygon_set.py` canonicalises Manhattan rectangle sets.
- **Reference tools.** The capture image (`tools/golden/container/Containerfile`, `tools/golden/pins.toml`) has Magic, netgen, ngspice and xschem. It has neither KLayout nor gdstk, which OASIS validation needs (D18).
- **Data, perf, fuzz.**
  - The PDK data sets hold `libs.tech/magic`, the `.mag` libraries and (M5) the library GDS. LEF and tech LEF files are not fetched.
  - `perf.yml` has `drc-bench`, `drc-profile`, `golden-slow`, `gds-bench` and `gds-roundtrip`.
  - Of the fuzz targets, none covers LEF, DEF, the `lef` section or OASIS.

M6 runs in four steps:
1. an early step with the reference data and reference tools (PDK, GOLDEN's tool pins and `.ext` probe);
2. a scaffold step (interfaces, `EditorState` members, command stubs, the build contract);
3. thirteen workstreams after the scaffold, ordered by the dependencies of D2;
4. an integration step.

## Goals / Non-Goals

**Goals:**
- The M6 exit criteria of docs/roadmap.md:
  - the common exit criteria for the 9 M6 capabilities: tagged unit tests for the 146 exact and behavioural requirements, fuzz targets for every new reader, CI green on every leg, and byte-identical output on Linux x86_64, Linux aarch64 and macOS arm64;
  - the LEFDEF and OASIS golden cases pass (D19);
  - the 419-byte OASIS reference file is confirmed by a third-party reader (D20).
- The roadmap deliverables: abstract views, and DEF import and export for place-and-route flows; OASIS read and write equivalent to the GDS path, validated with KLayout `strm2oas` and `strmxor`.
- One LEF/DEF tokenizer, one LEF layer table and one via-geometry generator, shared by the four LEF/DEF directions. One OASIS codec shared by the reader, the writer and the transcoders.
- Every compat decision has one branch point, testable in both modes.

**Non-Goals:**
- Extraction (M7). M6 pulls forward only the `.ext` read-and-flatten subset that `def write` needs and the extraction-style parts that LEF and DEF write read (Open Questions 1 and 2, decided (a)). Neither brings `extract` itself into M6.
- Rewriting M1–M5 code that meets its spec. M6 changes earlier code only where an M6 requirement or decision needs it (D17).
- A private OASIS label-presentation property (Q-OASIS-32: none in M6), gzip-wrapped OASIS output (Q-OASIS-91), and buying SEMI P39 (Q-OASIS-2's judgement item, reopened on 2026-11-18 with option (b) and not yet decided; if the user buys it, checking the drafts against it is separate work).

## Decisions

### D1. Layout of the M6 code

| Area | Public headers | Sources | Unit tests | Namespace |
|---|---|---|---|---|
| LEF/DEF tokenizer, messages and error limits | `src/core/include/hefesto/lef/lex/` | `src/core/src/lef/lex/` | `tests/unit/lef/lex/` | `hefesto::lef` |
| tech `lef` section, LEF layer table, via table, non-default rules | `src/core/include/hefesto/lef/tech/` | `src/core/src/lef/tech/` | `tests/unit/lef/tech/` | `hefesto::lef` |
| via geometry generator (LEF via rules, DEF generated vias and VIAS properties, cut growing) | `src/core/include/hefesto/lef/via/` | `src/core/src/lef/via/` | `tests/unit/lef/via/` | `hefesto::lef` |
| LEF reader | `src/core/include/hefesto/lef/read/` | `src/core/src/lef/read/` | `tests/unit/lef/read/` | `hefesto::lef` |
| LEF writer | `src/core/include/hefesto/lef/write/` | `src/core/src/lef/write/` | `tests/unit/lef/write/` | `hefesto::lef` |
| `lef` and `def` commands (one dispatcher, as CmdLef) | `src/core/include/hefesto/lef/command/` | `src/core/src/lef/command/` | `tests/unit/lef/command/` | `hefesto::lef` |
| DEF reader | `src/core/include/hefesto/def/read/` | `src/core/src/def/read/` | `tests/unit/def/read/` | `hefesto::def` |
| DEF writer | `src/core/include/hefesto/def/write/` | `src/core/src/def/write/` | `tests/unit/def/write/` | `hefesto::def` |
| OASIS codec (primitives, records, modal state, repetitions, CBLOCK, validation, encoder) | `src/core/include/hefesto/oasis/codec/` | `src/core/src/oasis/codec/` | `tests/unit/oasis/codec/` | `hefesto::oasis` |
| OASIS reader, vendor views, content hash | `src/core/include/hefesto/oasis/read/` | `src/core/src/oasis/read/` | `tests/unit/oasis/read/` | `hefesto::oasis` |
| OASIS writer, repetition detection, transcoders | `src/core/include/hefesto/oasis/write/` | `src/core/src/oasis/write/` | `tests/unit/oasis/write/` | `hefesto::oasis` |
| `oasis` command, settings, format recognition | `src/core/include/hefesto/oasis/command/` | `src/core/src/oasis/command/` | `tests/unit/oasis/command/` | `hefesto::oasis` |
| `.ext` netlist read and flattening subset (Open Question 1, decided (a)) | `src/core/include/hefesto/extflat/` | `src/core/src/extflat/` | `tests/unit/extflat/` | `hefesto::extflat` |
| extraction-style subset of the `extract-tech-section` reader, and `StyleView` (Open Question 2, decided (a)) | `src/core/include/hefesto/extract/tech/` | `src/core/src/extract/tech/` | `tests/unit/extract/tech/` | `hefesto::extract` |

`src/core/CMakeLists.txt` adds `lef def oasis extflat extract` to `HEFESTO_CORE_AREAS` (`extflat` and `extract` for Open Questions 1 and 2, decided (a); if M10's `extract/tech/` subset landed first, `extract` is already there). Each area uses per-subdirectory `sources.cmake` and `codes.inc` fragments, which the INTEGRATION-owned `<area>/{sources.cmake,codes.cpp}` assembles, as `mask/`, `gds/` and `cif/` do.

Diagnostic prefixes:

| Document | Topics |
|---|---|
| `lef.md` | `LEF-LEX-*` (LEFR), `LEF-TECH-*` (LEFTECH), `LEF-VIA-*` (LEFVIA), `LEF-READ-*` (LEFR), `LEF-WRITE-*` (LEFW), `LEF-CMD-*` (LEFR, LEFW) |
| `def.md` | `DEF-READ-*` (DEFR), `DEF-WRITE-*` (DEFW) |
| `oasis.md` | `OASIS-CODEC-*` (OASFMT), `OASIS-READ-*` (OASR), `OASIS-WRITE-*` (OASW), `OASIS-CMD-*` (OASCMD) |
| `extflat.md`, `extract.md` | `EXTFLAT-READ-*`, `EXTRACT-STYLE-*` (EXTSUB) |
| `gds.md` | `GDS-WRITE-*` additions for transcoding OASIS cells (OASW, D17) |

None of these codes reads like a golden case ID (LEFDEF-001, OASIS-001).

### D2. Workstreams and ownership

| Workstream (tasks.md section) | Capabilities | Owns (may edit) |
|---|---|---|
| PDK | none (reference data for LEFDEF-240 and the library LEF cases, Open Question 6, decided (a)) | `scripts/fetch_pdk.py`, `scripts/tests/test_fetch_pdk.py`, `tests/unit/common/reference_data.hpp` (LEF paths), the reference-data steps of `.github/workflows/{ci,golden-capture}.yml`, `docs/building.md` "Reference data for tests" |
| LEFTECH | `lef-tech-section` | `src/core/{include/hefesto,src}/lef/tech/`, `tests/unit/lef/tech/`, `tests/data/lef/tech/`, `lef.md` (TECH section); the `lef` section hooks in `tech/commands.cpp` (`load_session_technology`, `install_technology`, `scalegrid_command`; `tech/loader.cpp` holds no session state, round 2) and the rescale hook in `tech/scale_layout.cpp` |
| LEFVIA | `lef-read` "LEF VIA and VIARULE definitions", "Via redefinition", "Cut growing", "Generated via geometry"; `def-read` "Rectangle-based DEF vias", "Generated DEF vias", "Via redefinition"; `def-write` "Contact region identification", "Generated via definitions" | `src/core/{include/hefesto,src}/lef/via/`, `tests/unit/lef/via/`, `lef.md` (VIA section) |
| LEFR | `lef-read` (other requirements); the shared tokenizer and message reporting; `lef read`, `lef datestamp`, `lef help` | `src/core/{include/hefesto,src}/lef/{lex,read}/`, `tests/unit/lef/{lex,read}/`, `tests/data/lef/read/`, `lef.md` (LEX, READ sections); the fixed-stamp use in `cell/timestamps.cpp` |
| LEFW | `lef-write`; `lef write`, `lef writeall`, `lef nocheck` | `src/core/{include/hefesto,src}/lef/write/`, `tests/unit/lef/write/`, `tests/data/lef/write/`, `lef.md` (WRITE section) |
| DEFR | `def-read`; `def read`, `def datestamp` | `src/core/{include/hefesto,src}/def/read/`, `tests/unit/def/read/`, `tests/data/def/read/`, `def.md` (READ section) |
| DEFW | `def-write`; `def write` | `src/core/{include/hefesto,src}/def/write/`, `tests/unit/def/write/`, `tests/data/def/write/`, `def.md` (WRITE section) |
| EXTSUB | the `.ext` read and flatten subset (Open Question 1 (a)) and the extraction-style subset (Open Question 2 (a)), pulled forward from M7 and built to the M7 specs `ext-flattening-read`, `ext-flattening-hierarchy`, `ext-flattening-names` and `extract-tech-section` | `src/core/{include/hefesto,src}/{extflat,extract/tech}/`, `tests/unit/{extflat,extract/tech}/`, `tests/data/extflat/`, `extflat.md`, `extract.md`; the `extract` raw-section hand-off in `tech/loader.cpp` (shared with M10's TECH subset: whichever lands first adds it, the other extends it) |
| OASFMT | `oasis-format` | `src/core/{include/hefesto,src}/oasis/codec/`, `tests/unit/oasis/codec/`, `tests/data/oasis/codec/` (hand-assembled byte files and the 419-byte reference file), `oasis.md` (CODEC section) |
| OASR | `oasis-read` | `src/core/{include/hefesto,src}/oasis/read/`, `tests/unit/oasis/read/`, `tests/data/oasis/read/`, `oasis.md` (READ section); the `OASIS_FILE` read-only rule in `db/property.cpp` and the vendor-stamp hook in `cell/timestamps.cpp` (D17) |
| OASW | `oasis-write`; `gds-write` "Read-only OASIS vendor cells" (Q-OASIS-35) | `src/core/{include/hefesto,src}/oasis/write/`, `tests/unit/oasis/write/`, `tests/data/oasis/write/`, `oasis.md` (WRITE section); `src/core/src/gds/write/vendor_oasis.cpp` (new) and its call in `gds/write/vendor.cpp`, `gds.md` (its codes) (D17) |
| OASCMD | `oasis-command` | `src/core/{include/hefesto,src}/oasis/command/`, `tests/unit/oasis/command/`, `oasis.md` (CMD section); the format recognition in `app/` open paths (D15) |
| SHELL | `oasis-command` "Format recognition on open" on the desktop; progress and cancel of LEF/DEF/OASIS reads and writes (Open Question 5, decided (a): no menu items) | `src/shell/desktop/src/` (M6 entries), `src/core/src/gui/prompts.*` (new prompt kinds, if any), `tests/unit/shell/` (M6 tests), `docs/design/m6-ui-conformance.md` |
| GOLDEN | the LEFDEF and OASIS cases, MASK-OS-016, the reference tools in the capture image | `tests/golden/{LEFDEF,OASIS}/`, `tests/golden/MASK/cases/MASK-OS-016/`, `tests/data/extflat/` probes (task 1.6; EXTSUB only reads them), `tools/golden/` (`authoring/{lefdef_cases,oasis_cases,m6_cases,oasgen}.py`, `goldenlib/oasis.py`, the new comparators and masks), `tools/golden/container/Containerfile` and `tools/golden/pins.toml` (KLayout, gdstk; D18) |
| PERF | none (the D22 benches, Open Question 4, decided (a)) | `bench/{lefdef,oasis}/`, the `lefdef-bench` and `oasis-bench` jobs of `perf.yml`, `docs/design/m6-ui-conformance.md` "Performance" |
| INTEGRATION | none | everything shared: root and `src/core/CMakeLists.txt`, the `<area>/{sources.cmake,codes.cpp}` assemblers, the shared headers of D4, `mask/session.hpp`, `gds/command/settings.hpp` (the Q-OASIS-90 split, D15), `editor/`, `cmd/`, `diag/`, `src/cli/`, `CMakePresets.json`, `cmake/`, `tests/unit/CMakeLists.txt`, `tests/fuzz/`, `.github/` (other steps), `scripts/` (other files), `README.md`, `CHANGELOG.md`, `docs/roadmap.md`, `docs/building.md` (other sections), `docs/contributing/`, `docs/decisions/`, `docs/diagnostics/codes.md`, `openspec/specs/` (open items), this change's artifacts |

**PDK row: task 1.2 facts (measured 2026-11-18).** Every asset of the three pinned ciel releases was streamed and its `libs.ref/<lib>/{lef,techlef}/` members listed; nothing else of them was kept. For the variants Hefesto pins (sky130A, gf180mcuD, ihp-sg13g2):
- sky130 `5740c4a8…`: each standard-cell library (`sky130_fd_sc_{hd,hdll,hs,hvl,lp,ls,ms}`) has `lef/<lib>.lef` and `techlef/<lib>__{nom,min,max}.tlef` plus `techlef/<lib>__rram__{nom,min,max}.tlef`. `sky130_fd_sc_hd` also has `lef/sky130_ef_sc_hd.lef` (8 macros beside the 437 of `sky130_fd_sc_hd.lef`). `sky130_fd_io` has `lef/sky130_{fd,ef}_io.lef`, `sky130_fd_pr` has `lef/sky130_fd_pr.lef`, and `sky130_sram_macros` has 4 macro LEFs (24.9 MB). `sky130_ml_xx_hd` and `sky130_fd_pr_reram` have none, and `common` holds no `libs.ref`.
- gf180mcu `5740c4a8…`: `gf180mcu_fd_sc_mcu{7t5v0,9t5v0}`, `gf180mcu_as_sc_mcu7t3v3` and `gf180mcu_ocd_sc_rt12t3v3` have `lef/<lib>.lef` and `techlef/<lib>__{nom,min,max}.tlef`; `gf180mcu_osu_sc_gp{9t3v3,12t3v3}` have `lef/<lib>.lef` and `techlef/<lib>__nom.tlef` only. `gf180mcu_fd_io` (15 files), `gf180mcu_ocd_io` (1), `gf180mcu_fd_ip_sram` (4), `gf180mcu_ocd_ip_sram` (5) and `gf180mcu_re_efuse` (7) have `lef/` only. `gf180mcu_fd_pr` and `gf180mcu_ocd_alpha_{large,misc,small}` have none. `gf180mcu_fd_sc_mcu7t5v0.lef` has 229 macros.
- ihp-sg13g2 `5e6d592e…`: `sg13g2_stdcell` has `lef/sg13g2_stdcell.lef` (84 macros) and the tech LEF `lef/sg13g2_tech.lef`; there is no `techlef/` directory. `sg13g2_io` has `lef/sg13g2_io{,_notracks}.lef`, `sg13g2_sram` 30 macro LEFs, and `sg13g2_pr` none.
- Every LEF file is `VERSION 5.7`.
- The LEF sets of task 1.4 (`scripts/fetch_pdk.py`, PDK root `.cache/pdks/lef`) hold one standard-cell library per PDK:

  | Set | Kept | Downloaded | Kept size (cached) | Peak disk | Tree digest |
  |---|---|---|---|---|---|
  | `sky130-lef` | `sky130_fd_sc_hd/{lef,techlef}/`, 8 files | 134 MB | 2.0 MB (0.17 MB) | 0.88 GB | `b8ebb4f6…` |
  | `gf180mcu-lef` | `gf180mcu_fd_sc_mcu7t5v0/{lef,techlef}/`, 4 files | 350 MB | 0.77 MB (0.08 MB) | 1.63 GB | `1b748b5d…` |
  | `ihp-lef` | `sg13g2_stdcell/lef/`, 2 files | 441 MB | 0.22 MB (0.03 MB) | 1.13 GB | `fbc2d9ea…` |

  The peak is the scratch root's measured disk use during `pdk install` (archives and decompressed tars included). The download is dominated by the `common` archives, which `pdk install` needs and keeps nothing of here. `python3 scripts/fetch_pdk.py sky130-lef gf180mcu-lef ihp-lef --root <scratch>` reproduces the install. A second run reports each set "already present and verified".
- OASIS: one file in all three releases, `ihp-sg13g2/libs.tech/klayout/tech/lvs/testing/testcases/manual_tests/sram_support/SP01.oas` (26,667 bytes, IHP `common`). It is a KLayout LVS test layout, and its records match KLayout's writer: START version `1.0`, unit 1000, table offsets in END; the file-level standard properties `S_MAX_SIGNED_INTEGER_WIDTH`, `S_MAX_UNSIGNED_INTEGER_WIDTH` and `S_TOP_CELL`; `S_CELL_OFFSET` per cell; `S_GDS_PROPERTY`; CBLOCK compression; and an END record padded with `0x80` bytes. OASIS has no writer-name record, so the attribution is inferred. No library ships OASIS under `libs.ref`, and no file is from a commercial writer (D19 "Commercial OASIS files").

Rules (unchanged from M1–M5 D2):
- A workstream edits only what it owns. A needed change to a shared file or to another workstream's file is an `open_item` (what, why, and the exact edit if small). The owner or INTEGRATION applies it.
- GOLDEN authors new golden case directories from the requirement and the owning workstream's notes. `expected/` is captured in CI.
- Only the integration step commits and pushes, to `origin m6-lef-def-oasis` (or the branch its prompt names).

Dependencies:
- PDK and GOLDEN's reference-tool step (section 1) land first. LEFDEF-240, the `LEFDEF-RT-<lib>` cases (Open Question 6 (a)) and every KLayout cross-check need them.
- GOLDEN's `.ext` probe (task 1.6) lands with section 1 (Open Question 1 (a)), because EXTSUB's read tests and DEFW's net cases use the `.ext` files that Magic writes.
- The scaffold (section 2) publishes the interfaces of D4 with stubs.
- LEFTECH, LEFR's tokenizer, OASFMT and EXTSUB start at once.
- LEFVIA needs LEFTECH's tables.
- LEFR needs LEFTECH and LEFVIA.
- DEFR needs LEFR's tokenizer, LEFTECH and LEFVIA. It can start with RECT-only vias.
- LEFW needs LEFTECH, plus EXTSUB's style for the antenna areas and the well set only (Open Question 2 (a)).
- DEFW needs LEFTECH and LEFVIA, plus EXTSUB's netlist for NETS, SPECIALNETS and the net-based BLOCKAGES (Open Question 1 (a)), and EXTSUB's style for the active types of BLOCKAGES (Open Question 2 (a)).
- OASR needs OASFMT's decoder. OASW needs OASFMT's encoder.
- OASCMD's settings split comes first in the scaffold (D15). Its command options follow OASR and OASW.
- SHELL needs the commands.
- GOLDEN authors inputs from day one. PERF builds the benches once LEFR, DEFR, OASR or OASW lands.

### D3. Build and stub contract

- **No new library dependency.** OASIS CBLOCKs use raw DEFLATE (`inflateInit2`/`deflateInit2` with window bits -15) from the bundled zlib 1.3.1 (`hefesto::zlib`). The CRC32 validation uses zlib's `crc32`, and CHECKSUM32 is a byte sum. Q-OASIS-64 pins the deflate level, window, memory level and strategy to the bundled build. KLayout and gdstk are reference tools in the capture image only (D18). They are never linked, bundled or called from the product (P-18, `hefesto-licensing`).
- **Floating point.** The `lef`, `def` and `oasis` areas build with `-ffp-contract=off`.
  - LEF/DEF number output follows Magic's float32 `lefPrint` arithmetic (Q-LEFDEF-45) and the `%.10g` and `%f` formats of the specs.
  - LEF unit conversion rounds half away from zero in binary32 (lef-read "LEF unit conversion").
  - OASIS reals of types 0–7 decode exactly.
  - The CIRCLE vertices use a Hefesto sine/cosine built from IEEE basic operations only (no libm), so every platform gets the same bits (Q-OASIS-31).
  - D21 lists the pinned-value tests.
- **Stubs.** The scaffold registers `lef`, `def` and `oasis`:
  - `lef` and `def` share one dispatcher. As in CmdLef, both look options up in the `lef` table (lefCmd.c:152-200); the `def` table (lefCmd.c:171-181, including its split `read` entry) is used only for `def help`;
  - `oasis` gets the full option table of `oasis-command` "Command name and forms";
  - each option that is not implemented yet fails with a new code `CORE-NOTIMPL-006` ("not implemented yet: <command> <option>"), so completion, help, scripts and golden inputs see the final names;
  - INTEGRATION retires `CORE-NOTIMPL-006` at M6 exit, as `CORE-NOTIMPL-005` was retired in M5.
- **Scaffold tests** carry `[m6-scaffold]` and no `[cap:][req:]` tags. Owners replace them.

### D4. Session state and shared interfaces

`editor::EditorState` gains `lefdef` (`lef::LefSession`). `mask::MaskSession` gains `oasis` (`oasis::OasisSettings`) and `stream_read` (D15). INTEGRATION owns these structs; their members belong to the named workstreams:

| Member | Holds | Workstream |
|---|---|---|
| `lefdef.layers` | the LEF layer table: a `util::MagicHashTable` keyed by LEF name, holding the route, cut/via, masterslice, overlap, bound, ignore and obstruction records, aliases and the canonical name of each (lef-tech-section "LEF table lifecycle") | LEFTECH |
| `lefdef.vias`, `lefdef.rules` | the VIA definitions (LEF and DEF, including generated ones) and the NONDEFAULTRULE table, which persist across reads and writes as Magic's globals do (Q-LEFDEF-99 ON) | LEFVIA, LEFR, DEFW |
| `lefdef.datestamp` | `lef datestamp` (shared with `def datestamp`, lefCmd.c:446-482) | LEFR |
| `lefdef.nocheck` | the `lef nocheck` net list | LEFW |
| `lefdef.units` | the session-sticky LEF database units of `lef write` (Q-LEFDEF-34 ON) | LEFW |
| `mask.oasis` | the OASIS-only and OASIS-side options of `oasis-command` (Q-OASIS-90: `abstract`, `addendum`, `arrays`, `compress`, `contacts`, `labels`, `library`, `lower`, `merge`, `undefined`, `datestamp`, `nodatestamp`, `magscale`, `circlepoints`, `strict`, `validation`, `repetitions`, `layernames`, ...) | OASCMD (write side OASW, read side OASR) |
| `mask.stream_read` | the 13 read settings shared by `gds` and `oasis` (Q-OASIS-90 (a)), split out of `gds::GdsSettings` | INTEGRATION (D15) |

The LEF table changes only on tech load, `scalegrid` and finer-grid reads, and on `lef read`, `def read` and `def write`. It is guarded by the exclusive database lock that every command holds, so it needs no copy-on-write object as the mask styles do.

Shared interfaces created by the scaffold (INTEGRATION owns the files; the named workstream fills in the implementation):

**LEF/DEF core**
- `lef::Lexer` (`lef/lex/lexer.hpp`): LefNextToken and LefEstimate over a `gds::ByteSource`-style text source:
  - the 2048-character line semantics of lef-read "LEF/DEF tokenizer";
  - comments and quoted strings;
  - `;` glued to a number;
  - line and byte positions for messages.

  It comes with `lef::MessageCounter` (LefError: the 100-message cap and the totals, lef-read "LEF/DEF message reporting"). DEF read uses both, with DEF's own keyword tables (LEFR).
- `lef::LayerTable` and `lef::LayerRecord` (`lef/tech/table.hpp`), with these operations:
  - `from_section(const tech::RawSection&, const tech::Technology&, diag::Sink&)`;
  - `fill_defaults(const drc::RuleSet&, compat)` (Q-LEFDEF-1, 2);
  - `scale(n, d)`;
  - `find(lef_name)`;
  - `magic_to_lef(type) -> const LayerRecord*` (Q-LEFDEF-92 order);
  - `enumerate()`, in Magic hash order.

  The readers and writers use the classes as lef-tech-section "Use of entry classes by readers and writers" defines them (LEFTECH).
- `lef::ViaGenerator` (`lef/via/generate.hpp`), the shared generator of the decision table's "Shared generators" note, built once (LEFVIA). It covers:
  - the geometry of a generated via from CUTSIZE, LAYERS, CUTSPACING, ENCLOSURE, ROWCOL, ORIGIN, OFFSET and PATTERN (the last three OFF only, Q-LEFDEF-62);
  - LEF via-rule vias (Q-LEFDEF-7 OFF);
  - cut growing to the output style's contact size (`mask::OutputStyle::contact_size`, Q-LEFDEF-17);
  - the DEF-write side, which identifies contact regions and names generated vias (Q-LEFDEF-105, 106, 107, 111, 115).
- `lef::paint_polygon(CellDef&, TileType, std::span<const Point>, ...)` (`lef/via/polygon.hpp`): `db::decompose_polygon` plus port-label creation, shared by LEF POLYGON (Q-LEFDEF-23), DEF pin polygons (Q-LEFDEF-66 OFF) and DEF route polygons (Q-LEFDEF-70 OFF) (LEFVIA).

**Netlist and extraction style (Open Questions 1 and 2, decided (a))**
- `extflat::Netlist read_flat(const cell::LookupContext&, std::string_view root, const ReadOptions&)` (`extflat/read.hpp`): EFReadFile, then EFFlatBuild with `EF_FLATNODES | EF_NOFLATSUBCKT`, unscaled, with resistance unused. It returns the flat nodes with their hierarchical names (name preference of `ext-flattening-names`), their layer and location, and the `.ext` timestamps (Q-LEFDEF-110 OFF). It also offers `visit_nodes` in EFVisitNodes order (EXTSUB).
- `extract::StyleView` (`extract/tech/style_view.hpp`): the current extraction style's device classes per type, antenna tie-down types, substrate types and active types, a view over the tables of the one `extract-tech-section` reader (D10). Its two queries, `gate_types()` and `diff_types()`, return `std::nullopt` when there is no style (Q-LEFDEF-40: empty) (EXTSUB).

**OASIS**
- `oasis::Decoder` (`oasis/codec/decoder.hpp`): the record reader over a `gds::ByteSource`. It covers:
  - integers, reals, strings, deltas, point lists, repetitions and intervals;
  - `ModalState` with the reset rules (Q-OASIS-5);
  - name tables, with two-pass resolution and table offsets (Q-OASIS-8);
  - CBLOCK inflation with spans across the end (Q-OASIS-9), under the M5-S1-style inflate cap;
  - strict-mode checks (Q-OASIS-7);
  - validation (Q-OASIS-2).

  It yields typed records and never touches the database (OASFMT).
- `oasis::Encoder` (`oasis/codec/encoder.hpp`): minimal integers (Q-OASIS-1), modal field omission, name tables in strict or non-strict layout, the CBLOCK writer and the END record with validation (OASFMT).
- `oasis::Element` and `oasis::Repetition` (`oasis/codec/elements.hpp`): the decoded geometry vocabulary shared by the reader, the writer's transcoders and `gds/write/vendor_oasis.cpp` (OASFMT).

**Commands and progress**
- The entry points are:
  - `lef::read(EditorState&, path, const LefReadOptions&)`;
  - `lef::write(EditorState&, CellDef&, path, const LefWriteOptions&)`;
  - `def::read(...)`, `def::write(...)`;
  - `oasis::read(EditorState&, path, const OasisReadOptions&, const mask::Progress*)`;
  - `oasis::write(EditorState&, CellDef&, path, const OasisWriteOptions&, const mask::Progress*)`.

  Each returns a `cmd::CommandResult`. The commands, the CLI and the shell all call these.
- `oasis::write` reuses the M5 three-phase write unchanged: `gds::prepare_write` (exclusive lock: style check, subtree loading through `UseIndex::search`, mismatch fix-up), its own `emit` (shared lock: generation through `mask::generate_hierarchical`, then OASIS encoding), and `gds::finish_write`. The options come from `oasis::to_gds_write_options(const OasisSettings&, const StreamReadSettings&)`. If a GDS-only check in `prepare_write` must differ for OASIS, OASW raises it as an open item, and INTEGRATION adds a format parameter. The prepare code is not forked.
- LEF/DEF reads and writes are single-phase commands under the exclusive lock, as in Magic. They report progress through `mask::ProgressHost` (LefEstimate, Q-LEFDEF-88).

### D5. The tech `lef` section and the LEF layer table (LEFTECH; `lef-tech-section`)

- **One parse at tech load.** `LayerTable::from_section` ports lefTech.c (516 lines): the keyword grammar, magic type resolution, class mapping, aliases and canonical names. It runs after the DRC section is bound, because the defaults come from `drc::default_width` and `drc::default_spacing`. Each record keeps an *unset* flag for width, spacing and contact area instead of Magic's numeric sentinel (Q-LEFDEF-1).
- **Decisions.**

  | Decision | ON (Magic) | OFF |
  |---|---|---|
  | Q-LEFDEF-1 | the scaled sentinels; the fill runs only when the grid is not 1:1 and the sentinel survived the truncating scale (d < n < 2d for width and spacing, n < d for contact areas) | DRC defaults fill every unset value after each tech load and grid change (`scalegrid` included) |
  | Q-LEFDEF-2 | when the fill runs with spacing unset, the default spacing overwrites the width | the default spacing goes into spacing |
  | Q-LEFDEF-25 | an `obstruction` line with three or more types keeps the first two; other lines get `Can only define multiple types...` | `Too many types` (LEF-TECH-005) |

  Q-LEFDEF-26 (fix, approved 2026-11-19): an `ignore` line naming a mapped name reports `already used` and keeps the name; the -1 type index becomes the obstruction type or type 0. Q-LEFDEF-27 (compat, approved 2026-11-19; LEFVIA): the cut-growing warning names the via entry's normal type with ON and the cut's type with OFF. Q-LEFDEF-5 (fix): a ROUTE entry created by a LEF LAYER section starts like a `routing` line. Q-LEFDEF-82, 92 and 116 (compat, X-1 as amended): the table is a `MagicHashTable` with Magic's signed-byte hash on every platform.
- **Scaling.** `scale(n, d)` truncates route values toward zero and floors contact areas, as LefTechScale and DBScalePoint do. It is called from `tech/scale_layout.cpp` for `scalegrid` and finer-grid reads, which never refill (Q-LEFDEF-1 evidence). The OFF refill is a separate call after the scale.

### D6. LEF read (LEFR, LEFVIA; `lef-read`)

- **Reader.** lefRead.c (3,204 lines) is ported as a section dispatcher with one handler per statement:
  - the LAYER, VIA, VIARULE, NONDEFAULTRULE, SITE and MACRO sections;
  - in a MACRO, the PIN, PORT, OBS and PROPERTY statements;
  - unknown sections are skipped by LefSkipSection.

  Cells are created or annotated through `db` and `cell` APIs only.
- **One undoable transaction.** A LEF read (and a DEF read) is one undo transaction, without Magic's "This action cannot be undone." message (Q-LEFDEF-22, platform). INTEGRATION adds it to layout-undo's list of undoable operations.
- **Compat branch points.** Each decision is one helper taking `const CompatSettings&`:

  | Decision | ON (Magic) | OFF |
  |---|---|---|
  | Q-LEFDEF-3 | OBS secondary type and cut size carry over between LAYERs | both reset at every LAYER |
  | Q-LEFDEF-6 | last SPACING wins | first unqualified SPACING, else the minimum |
  | Q-LEFDEF-7 | via rules, RECT MASK/ITERATE and VIA POLYGON skipped | supported, through `ViaGenerator` |
  | Q-LEFDEF-10 | an unread `;` swallows the next NONDEFAULTRULE statement | consume through `;`, skip known VIA bodies |
  | Q-LEFDEF-11 | `-annotate` still creates unknown macros | skipped |
  | Q-LEFDEF-12 | geometry at g−ORIGIN−F, box (F, F+SIZE) | LEF's frame |
  | Q-LEFDEF-13 | `OUTPUT TRISTATE` reads as output | tristate |
  | Q-LEFDEF-14 | LEF 5.8 USE values only | also SCAN, RESET, TIEOFF |
  | Q-LEFDEF-15 | PATH and VIA geometry ignored | painted, with port labels |
  | Q-LEFDEF-20 | per-RECT error on IGNORE layers | silent skip |
  | Q-LEFDEF-21 | annotation paints and converts non-port labels | labels only |

  Fixes in both modes: Q-LEFDEF-4 (absent table entries), 8 (via redefinition), 9 (rule renaming `_0`, `_1`, ...), 18 (POLYGON on an unknown layer), 19 (empty CLASS/SYMMETRY). Q-LEFDEF-23 is replicated: one decomposition, identical on every plane.
- **Files and dates.** `lef read <name>` tries the name, then `<name>.lef`, through `cell::search_file` (Q-LEFDEF-24, platform: the search path on desktop, the document picker on iPadOS). `lef datestamp` sets the fixed stamp through `cell/timestamps.cpp`.

### D7. LEF write (LEFW; `lef-write`)

- **Macro construction.** lefWrite.c (2,403 lines) is ported in these steps:
  1. flatten (`db::flatten_to_new` into a scratch library, labels from the cell only);
  2. build the type sets R, H, W, P, G, D;
  3. find the pin nets (`connect::search_flat` from each port label);
  4. enumerate the pins in port-index order;
  5. take the antenna areas (G and D from `extract::StyleView`, Open Question 2 (a));
  6. emit the geometry tile by tile in Magic's tile order (Q-LEFDEF-39), with the contact cut records.

  `-hide`, `-pinonly`, `-toplayer` and `-nomaster` are separate passes over the flat copy.
- **Numbers.** `lefPrint` float32 arithmetic and `-0.000` are kept (Q-LEFDEF-45). The `%f` outputs (antenna areas, tech values, CUT AREA) are not rounded.
- **Compat branch points:**

  | Decision | ON (Magic) | OFF |
  |---|---|---|
  | Q-LEFDEF-31 | what Magic stores (no WIDTH/SPACING/DIRECTION on a 1:1 grid) | filled width, spacing and DIRECTION |
  | Q-LEFDEF-32 | `CUT AREA` (4× area) | `WIDTH` |
  | Q-LEFDEF-33 | the written cell's FIXED_BBOX | the site cell's own |
  | Q-LEFDEF-34 | session-sticky database units | recomputed per write |
  | Q-LEFDEF-36 | empty keywords | `USE SIGNAL`; no DIRECTION for undefined classes |
  | Q-LEFDEF-37 | `-pinonly p` under `-hide d`, p < d: no effect | removal applied |
  | Q-LEFDEF-38 | setback strips copy whole tiles | clipped |
  | Q-LEFDEF-39 | first half of a doubly writable split tile | both halves |
  | Q-LEFDEF-51 | whole chunk painted back | chunk clipped to the p ring plus the label |

  Replicated, with a warning where the row says so: Q-LEFDEF-35, 45, 46, 47, 50, 52. Fixes: Q-LEFDEF-40 (no extraction style: G and D are empty) and Q-LEFDEF-53 (unpainted boundary layer). Q-LEFDEF-30 and 41 (the PROPERTYDEFINITIONS and writeall orders) come from `MagicHashTable` and `UseIndex::enumerate_all`. Q-LEFDEF-43 (platform): output file naming.
- **Power names.** The `vdd`/`gnd` names come from the project setting "global nets and power names" (Q-LEFDEF-44, D-14). The Tcl `$VDD`/`$GND` variables seed it, as the M4 binding does.

### D8. DEF read (DEFR, LEFVIA; `def-read`)

- **Reader.** defRead.c (2,776 lines) is ported as a section dispatcher:
  - the header statements, DIEAREA, VIAS, COMPONENTS, PINS, NETS and SPECIALNETS, NONDEFAULTRULES and BLOCKAGES;
  - every other section is skipped as the spec says.

  Uses are created through `db::place`, in file order, so the use index sees Magic's insertion sequence. Masters are found by name among loaded cells, then as `.mag` files on the search path (DBCellLookDef, then the cell search path).
- **Route painting.** The wire and via geometry follows defRead.c's DefAddRoutes. In both modes, a non-positive layer width falls back to the DRC default width or DEFAULT_WIDTH, because Magic paints nothing there (Q-LEFDEF-1, tier 1, a recorded golden deviation).
- **Compat branch points:**

  | Decision | ON (Magic) | OFF |
  |---|---|---|
  | Q-LEFDEF-60 | the generated-via flag persists through VIAS | reset per via |
  | Q-LEFDEF-62 | VIAS ORIGIN/OFFSET/PATTERN/POLYGON/MASK ignored | implemented (`ViaGenerator`) |
  | Q-LEFDEF-63 | a route on the top layer stays there after a generated via | switch to the via's other layer |
  | Q-LEFDEF-64 | pin DIRECTION/USE carry over | reset per pin |
  | Q-LEFDEF-65 | leftover pin rectangles painted with the next pin | per-port collection |
  | Q-LEFDEF-66 | pin POLYGON and VIA ignored | painted and labelled |
  | Q-LEFDEF-69 | an unknown special-net `+` keyword swallows the next net | skip to `+` or `;` |
  | Q-LEFDEF-70 | route POLYGON vertices become wire points | painted polygon |
  | Q-LEFDEF-71 | VIRTUAL points wired | reference point only |
  | Q-LEFDEF-72 | via orientation and MASK messages; unrotated via | MASK silent; orientation applied |
  | Q-LEFDEF-73 | RECT offsets truncate | floor (undefined reference and failed parse fixed in both modes) |
  | Q-LEFDEF-74 | NONDEFAULTRULE applies to the next routing statement | to the whole net |
  | Q-LEFDEF-77 | polygonal DIEAREA from the first two points | bounding box of all |
  | Q-LEFDEF-78 | fallback design name is the raw argument | basename without `.def` |
  | Q-LEFDEF-79 | UNITS multiplies; integer prefix | sets, warns, strict |
  | Q-LEFDEF-81 | annotation looks up terminals in the edit cell, area unexpanded | design cell, area + 1 |
  | Q-LEFDEF-84 | FILLS/SLOTS/STYLES recovered noisily | skipped as sections |

  Fixes: Q-LEFDEF-61, 67, 68, 75, 76, 83 (duplicate component names renamed with the `.mag` rule, a recorded deviation), 89. Replicated: Q-LEFDEF-85.
- **Platform.** Q-LEFDEF-86: no window or box is needed, and the read works headless. Q-LEFDEF-88: document edits are blocked during the read, which reports through the progress host.

### D9. DEF write (DEFW, LEFVIA, EXTSUB; `def-write`)

- **Writer.** defWrite.c (3,224 lines) is ported in Magic's section order. The VIAS, COMPONENTS (`UseIndex::enumerate_all` order), PINS, NONDEFAULTRULES, DIEAREA and the non-net BLOCKAGES come from the layout and the LEF tables. NETS, SPECIALNETS and the net-based BLOCKAGES come from `extflat::read_flat` (Open Question 1 (a)). The net geometry is found by `connect::search_flat` from each node's location, in Magic's maximal-horizontal-strip tile order (Q-LEFDEF-108).
- **Files.** NETS and SPECIALNETS are buffered in memory instead of Magic's `_part.def` file, with the same bytes (Q-LEFDEF-90, platform). The final file is written to a temporary file and renamed, as the M5 writers do.
- **Compat branch points:**

  | Decision | ON (Magic) | OFF |
  |---|---|---|
  | Q-LEFDEF-91 | underscore filter, uncounted VIAS records | only this write's vias |
  | Q-LEFDEF-93 | Magic's two COMPONENTS formulas | T(R).ll for every instance |
  | Q-LEFDEF-94 | signed array element count | true count |
  | Q-LEFDEF-95 | integer half width in endpoint insets | exact half width |
  | Q-LEFDEF-96 | dogleg test compares y with x2 | with y1 |
  | Q-LEFDEF-98 | first rule entry repeated, unscaled WIREEXT | each entry, scaled |
  | Q-LEFDEF-99 | the whole global rule table | only referenced, write-local rules |
  | Q-LEFDEF-103 | Magic's PINS bytes | RESET/SCAN/TIEOFF, newline fixed |
  | Q-LEFDEF-105 | asymmetric surround probe | symmetric |
  | Q-LEFDEF-107 | inverted y test (message only) | as x |
  | Q-LEFDEF-110 | no staleness check | `.ext` timestamp warning |
  | Q-LEFDEF-111 | contact chain bounding box | exact rectangles |
  | Q-LEFDEF-112 | duplicates for connected seeds | each tile once per net |
  | Q-LEFDEF-114 | Magic's swapped text of `No LEF layer corresponding to layer %s of pin "%s".` (label text, then layer) | layer name, then pin name |

  Fixes: Q-LEFDEF-100, 101, 104, 106, 113, 115. Replicated: Q-LEFDEF-97 (line breaks). Platform: Q-LEFDEF-102 (special-net names from the project setting, plus the trailing-`!` rule).

### D10. The `.ext` netlist and extraction-style subsets (EXTSUB; Open Questions 1 and 2)

Open Questions 1 and 2 were decided (a) on 2026-11-18, so both subsets are M6 work, owned by EXTSUB.

EXTSUB ports the part of extflat that `def write` calls:
- EFReadFile with the `.ext` grammar of `extract-file-format` (read side only);
- EFFlatBuild with `EF_FLATNODES | EF_NOFLATSUBCKT`;
- EFVisitNodes;
- the node-name preference of `ext-flattening-names`.

That is part of five files: EFread.c, EFbuild.c (nodes, connections, merges and kills; no device parameters), EFflat.c (nodes only), EFname.c and EFvisit.c. Together they hold 6,693 of extflat's 9,441 lines, and the device, capacitance and resistance parts are left out.

It is built to the M7 specs, in M7's area layout, so M7 extends it rather than replacing it. Its tests carry `[cap:ext-flattening-read]` and the other M7 tags. Those requirements stay M7's in `check_requirement_coverage.py --milestone M7`, and the M6 tasks list which of them EXTSUB covers.

EXTSUB also writes the parts of the `extract-tech-section` reader that `StyleView` needs, in that capability's home `extract/tech/` and with its requirement tags:
- style selection;
- `device` lines (class and types only);
- `antenna` tie-down types (`exts_antennaTieTypes`);
- `substrate` and the active types.

Every other keyword is accepted and skipped silently, as M10's subset does (m10-3d-view design D5): no capacitance, resistance or device parameters. There is one extract reader. M10's TECH subset (style, scale, units, plane order, heights) and this one share its files and style selection: whichever lands first writes the style framework, and the other adds its keywords. M7 replaces the remaining skips with its parsers in the same files, and `StyleView` keeps its interface. Those requirements stay M7's in `check_requirement_coverage.py --milestone M7`, as for the netlist subset.

### D11. OASIS codec (OASFMT; `oasis-format`)

- **Decoder.** One function per record family, following the record tables of `oasis-format`. Integers are read as unsigned 64-bit, with the 65-bit overflow error, and non-minimal encodings are accepted (Q-OASIS-1). `ModalState` holds every modal variable and is reset by the format's rules (Q-OASIS-5).
- **Format decisions** (all fix):
  - Q-OASIS-3: RECTANGLE S=1 sets the modal geometry-h;
  - Q-OASIS-4: CTRAPEZOID modal w/h become the decoded bounding box;
  - Q-OASIS-6: XYABSOLUTE and XYRELATIVE end attachment, so a following PROPERTY goes to the cell;
  - Q-OASIS-8: table offsets may point into a CBLOCK;
  - Q-OASIS-9: a span across a CBLOCK end is accepted at any byte;
  - Q-OASIS-11: path extension values are stored numerically.
- **Repetitions.** Repetitions are decoded lazily into displacement iterators. Repetition counts are bounded by the resource caps of D21, with a diagnostic, never by allocating the expanded list.
- **CBLOCK.** Raw inflate with the M5-S1-style cap (256 × compressed size, at least 1 GiB). The decompressed stream positions are what the content hash and the error positions report.
- **Validation.** CRC32 and CHECKSUM32 follow gdstk's coverage and byte order (Q-OASIS-2 (a)).
- **Encoder.** It writes minimal integers, the narrowest real form the spec allows, and modal omission (`oasis-write` "Modal field omission"). Name tables are written inline (non-strict) or as strict tables with offsets (Q-OASIS-7, 8), and CBLOCKs per cell at the session `compress` level.
- **Reference file.** The 419-byte file of `oasis-format` "Complete annotated example" is assembled from the spec listing by `tests/data/oasis/codec/make_reference.py` (Python, with tests). It is committed with its SHA-256. Decoding it and re-encoding the decoded cell are unit tests (D20).

### D12. OASIS read (OASR; `oasis-read`)

- **Engine.** OASR is a `mask::ReadSink` driver, like the GDS reader. Records become `begin_cell`, `paint`, `label`, `call`, `property` and `end_cell` calls on `mask::ReadEngine`, so scaling, mid-read rescale, cifinput layer mapping (by number only, Q-OASIS-38, 39), boundary, mask-hints and labels behave exactly as in gds-read.
- **Two passes.** Pass 1 indexes the cells and name tables, resolving forward references (`#<n>` for an unnamed refnum, Q-OASIS-34). Pass 2 reads the cells in post-order when `ordering` is on, and in file order otherwise.
- **GDS equivalence.** Every rule that oasis-read inherits from gds-read calls the M5 helper rather than copying it:
  - quarter-turn rounding (Q-OASIS-12);
  - timestamp 0 for an unknown date (Q-OASIS-30);
  - the duplicate-use rule for coincident placements (Q-OASIS-10);
  - discarded user properties (Q-OASIS-37);
  - negative path extensions (Q-OASIS-33 following Q-MASK-114);
  - the instance properties 61/99/98.

  Where a helper is private to `gds/read/`, OASR raises an open item, and INTEGRATION moves it to a shared header.
- **Shapes.** RECTANGLE, POLYGON, PATH, TRAPEZOID, CTRAPEZOID and CIRCLE (an inscribed `circlepoints`-gon with the D3 trig, Q-OASIS-31) become `ReadShape` polygons. Non-Manhattan shapes go through `db::decompose_polygon`, as GDS BOUNDARY does.
- **Vendor views.** With `readonly`, OASR sets `OASIS_FILE` and `OASIS_CELL` and applies the units check (`gds::readonly_units_error`). The content-hash stamps hash the decompressed cell bytes with FNV-1a 64, then apply gds-read's memoized `stamp(s)` procedure. That procedure becomes a shared function of `gds/read/stamps.cpp`, and INTEGRATION moves it (D17).
- **Extensions.** XNAME, XELEMENT and XGEOMETRY are kept for transcoding and skipped for the layout, with the warnings of Q-OASIS-40.

### D13. OASIS write (OASW; `oasis-write`, `gds-write` "Read-only OASIS vendor cells")

- **Content.** OASIS write produces the same layout content as `gds write` would (oasis-write "GDS-consistent content"):
  - the same generation, through `prepare_write` and generation (D4);
  - the same structure set and order (`UseIndex`);
  - the same names (`gds/write/names.cpp`, Q-OASIS-61 following Q-MASK-209; the character mapping of Q-OASIS-60);
  - the same 200-point polygon limit (Q-OASIS-62);
  - the same merged polygons (`gds::merge_polygons` with `merge yes`);
  - the same label translation, with presentation dropped and one warning per write (Q-OASIS-32);
  - the same reference properties (b-string S_GDS_PROPERTY, Q-OASIS-69).

  The bytes come from `oasis::Encoder`.
- **Defaults** (Q-OASIS-65, 66, 67, other): validation `crc32`; integer-width standard properties declared 4/4; repetition detection on (`repetitions yes`, which reorders elements, so OASIS-052 compares with `repetitions no`).
- **Read-only cells.**
  - Read-only GDS cells are transcoded from the GDS stream with `gds::RecordReader` (Q-OASIS-63). An odd-width path is a write error naming the cell, and the round ends of an even-width path become CIRCLE caps.
  - Read-only OASIS cells are re-encoded from their decoded records, and extension records are kept with the Q-OASIS-40 warning.
  - The full-dump prefix follows Q-MASK-211 (Q-OASIS-68).
- **GDS side (Q-OASIS-35 (a)).** `gds write` of a hierarchy holding read-only OASIS cells transcodes each one from its OASIS file in `gds/write/vendor_oasis.cpp`. A layer or datatype above 32767, or a polygon above 8191 points, fails the write with an error. GOLDEN's cases cover layer 32767 (written) and 32768 (error).
- **Determinism.** The bundled zlib is used with fixed parameters (Q-OASIS-64). The output is compared byte for byte on every CI leg (OASIS-056).
- **No gzip output** (Q-OASIS-91). Compression is by CBLOCK only. Gzip-wrapped input is still read.

### D14. Commands: `lef`, `def` (LEFR, LEFW, DEFR, DEFW)

- **One dispatcher.** `lef/command/command.cpp` ports CmdLef's option parsing for both commands:
  - both commands look their option up in the `lef` table: `read`, `write`, `writeall`, `nocheck`, `datestamp` and `help` (lefCmd.c:186-200), so `def nocheck` and `def datestamp` are accepted as in Magic;
  - `def writeall` is refused with Magic's message (lefCmd.c:275-280, `def-write` "def writeall is not supported"), and `def help` prints the `def` table;
  - Magic's invalid-option message and the help text are kept.

  Each option calls its workstream's entry point. The scaffold owns the dispatcher, and each workstream owns its argument parser in its own area (`lef/read/args.cpp`, `lef/write/args.cpp`, `def/read/args.cpp`, `def/write/args.cpp`).
- **UI-G-005.** The deviation text drops `def, lef`, and GOLDEN replays the case.

### D15. The `oasis` command and the shared read settings (OASCMD, INTEGRATION; `oasis-command`, Q-OASIS-90)

- **Settings split (scaffold, INTEGRATION).**
  - `gds::GdsSettings` is split. A new `stream::StreamReadSettings` holds the 13 read settings that Q-OASIS-90 (a) shares: `drccheck`, `flatten` (with its limit), `flatglob`, `maskhints`, `noduplicates`, `ordering`, `polygon subcells`, `path subcells`, `readonly`, `savepaths` and `unique`. The other two, `rescale` and `warning`, are already shared through `MaskSession::rescale_allowed` and `MaskSession::warnings`.
  - `GdsSettings` keeps the write options, `magscale`, `datestamp` and `nodatestamp` (both sides), and the Q-MASK-111 rewound flag.
  - `MaskSession` holds `stream_read`, `gds` and `oasis`.
  - The split is behaviour-preserving for `gds`. All M5 tests and goldens must pass unchanged before OASCMD builds on it.
- **Command.** `oasis` has the `gds` option table of `oasis-command` "Options shared with the gds command" and the OASIS-only options. A shared option reads and writes `stream_read`, and an independent option reads and writes `oasis`. A query returns the value as the command result (`oasis-command` "Scripting access").
- **Format recognition on open.** The generic open path (`app/` document open, used by the desktop picker, drag and drop, the iPadOS Files request and the scripting open) sniffs the first 13 bytes, after gzip, for `%SEMI-OASIS\r\n`. It dispatches to `oasis read` with the current settings, whatever the extension. SHELL wires the desktop entry points to it (D16).

### D16. Desktop (SHELL)

The scope is Open Question 5, decided (a) on 2026-11-18:
- format recognition on open, through the existing File > Open picker and drag and drop;
- LEF/DEF/OASIS reads and writes with the M5 progress and cancel hosts (`MaskSession::read_host`, `write_host`, `emit_host`);
- no new menu item and no new board: Magic's wrapper has no LEF, DEF or OASIS item (wrapper.tcl:1235-1239), and the console reaches all three commands.

No menu item is added, so DESIGN draws no board for M6. `docs/design/m6-ui-conformance.md` records the open path and the progress and cancel hosts.

### D17. Ownership of the M1–M5 changes M6 makes

| Change | Owner | Decision or requirement |
|---|---|---|
| `GdsSettings` split into `StreamReadSettings` + `GdsSettings`; `MaskSession` gains `stream_read` and `oasis` | INTEGRATION (scaffold) | Q-OASIS-90 (a), `oasis-command` "Settings shared with the gds command" |
| gds-read's memoized `stamp(s)` and the vendor lookup made shared functions (`gds/read/stamps.hpp`) | INTEGRATION, on OASR's open item | `oasis-read` "Content-hash timestamps for read-only OASIS cells" |
| gds-read helpers that oasis-read inherits (transform rounding, duplicate-use rule, instance properties) exposed in a shared header | INTEGRATION, on OASR's open items | `oasis-read` "GDS-equivalence cross-check" |
| `gds/write/vendor_oasis.cpp` and its call in `gds/write/vendor.cpp`; GDS-WRITE codes | OASW | Q-OASIS-35, `gds-write` "Read-only OASIS vendor cells" |
| `OASIS_FILE` makes a cell read-only, as `GDS_FILE` does (`db/property.cpp`) | OASR | `oasis-read` "Read-only OASIS vendor views" rule 1 |
| vendor stamps for `OASIS_FILE` cells in `cell/timestamps.cpp` | OASR | same, and `cell-timestamps` |
| `lef` section parsed at tech load and installed with the session (`tech/commands.cpp`), rescaled by `scalegrid` and finer-grid reads (`tech/scale_layout.cpp`), refilled with magic-compat OFF after `scalegrid` (`tech/commands.cpp`) and finer-grid reads (`cell/io_session.cpp`, `mask/input/scale.cpp`, INTEGRATION round 2) | LEFTECH | `lef-tech-section` |
| `lef read`/`def read` added to layout-undo's undoable operations | INTEGRATION (spec), LEFR/DEFR (code) | Q-LEFDEF-22 |
| generic open sniffs OASIS (`app/`) | OASCMD | `oasis-command` "Format recognition on open" |
| `magic_help.cpp`'s `def` and `oasis` lines; UI-G-005 deviation text | LEFR (help), GOLDEN (deviation) | D14 |

### D18. Reference tools: KLayout and gdstk in the capture image (GOLDEN)

- **What they are for.**
  - KLayout runs `strm2oas`, `strm2gds`, `strmxor`, `strmcmp`, and its reader with `expect_strict_mode`. gdstk provides `oas_validate` and an independent reader (OASIS CASES.md "KLayout cross-check").
  - Both are reference tools under the clean-room rule. They run as separate programs in the capture container. Hefesto never links, bundles or calls them (P-18, `hefesto-licensing`), and their source is not read for implementation.
- **Pins.** `pins.toml` gains `[tools.klayout]` and `[tools.gdstk]` (version, source, digest). The Containerfile repeats them as ARGs, and `tools/golden/tests/test_pins.py` checks that the two agree.
  - Task 1.3 decides the install source from facts and records it here: a Debian package at the image's APT snapshot if one exists, otherwise KLayout's upstream `.deb` for Debian 13 with a SHA-256; and a gdstk wheel pinned by version and hash.
  - The task also records the image size increase. The M8 plan adds `gawk` to the same Containerfile (m8-schematic task 3.1), so whichever lands second rebases onto the other.
  - *As decided (task 1.3, 2026-11-18).*
    - KLayout: the Debian package `klayout 0.30.0-1+b1` exists at the image's APT snapshot (20261020T000000Z), so it is used. The Containerfile checks its `.deb` against the SHA-256 of the snapshot's Packages index (`421e473f…`). The package ships only `/usr/bin/klayout`, without the `strm2gds`, `strmxor` and `strmcmp` buddy programs. Their operations (read with `expect_strict_mode`, GDS2 and OASIS writes, per-layer XOR, `LayoutDiff`) run as a KLayout batch script (`klayout -b`, `tools/golden/harness/oasis_validate/klayout_check.py`) through KLayout's documented scripting API. KLayout publishes no Debian 13 `.deb` (Ubuntu builds only), so the fallback did not apply.
    - gdstk: Debian has no gdstk package. The PyPI wheel `gdstk-1.0.2-cp313-cp313-manylinux_2_24_x86_64.manylinux_2_28_x86_64.whl` (SHA-256 `0429cc3e…`) is installed without dependencies into a virtual environment (`/opt/ref/py`, `$HEFESTO_GDSTK_PYTHON`) that uses the snapshot's `python3-numpy` 2.2.4.
    - Image size increase, estimated from the snapshot's Packages index: 87 new packages, 90 MiB to download and 395 MiB installed (KLayout and its Qt 5, Ruby and Python libraries 354 MiB; `python3-venv` and `python3-numpy` 41 MiB), plus the 1.7 MB unpacked wheel. Measured (golden-capture run 44289117039, 2026-11-18, the oasis-validate job's notice): the whole image is 1,635,348,350 bytes (1.52 GiB) with KLayout and gdstk; no earlier run recorded the size, so the increase stays the estimate above. The build log prints `KLayout 0.30.0` and `gdstk 1.0.2` (runs 44289108952 and 44289117039).
    - The M8 `gawk` change has not landed.
- **Where they run.**
  - The capture jobs of `golden-capture.yml` produce the KLayout-made inputs and references: `strm2oas` of the sky130 standard-cell GDS for OASIS-020/021, and the KLayout-made equivalent of OASIS-013.
  - A dispatch mode `oasis-validate` runs KLayout and gdstk on Hefesto's outputs: the OASIS-05x writes, the strict-mode files and the 419-byte file. It records the verdicts as artifacts and job summaries. As landed (task 1.7): `tools/golden/oasis_validate.py` skips, with the reason, each case not authored yet and each run that stops on `CMD-DISPATCH-001` or `CORE-NOTIMPL-*`. Before Hefesto writes OASIS, smoke files made by gdstk and KLayout keep every check exercised, with two negative controls: a strict file read as non-strict must warn, and a file with one byte changed must fail `oas_validate`.
  - CI's build legs do not have the tools. Their replays use the in-repo comparators of D19.

### D19. Golden cases (GOLDEN)

- **Authoring.**
  - `tools/golden/authoring/lefdef_cases.py` covers the 143 LEFDEF rows, and `oasis_cases.py` the 51 OASIS rows.
  - `m6_cases.py` is the driver, as `m5_cases.py` is: `--list`, `--not-authored` and `--exit-check`.
  - Cases are single `input/commands.tcl` files that both Magic and `hefesto-cli --magic` run. They use small synthetic technology files committed per case family.
  - `authoring/oasgen.py` is a clean-room OASIS record writer, built from the spec, with tests in `tools/golden/tests/`. It produces the hand-assembled OASIS-001..015 inputs.
- **Session-state pinning.** D18 of M5 applies: a fresh session per case, one write, `drc off`, `TZ=UTC`, `LC_ALL=C`, and `--trust-pdk` with `random seed 1` for PDK cases.
- **References of the OASIS cases** (CASES.md):
  - *Magic GDS cross-check*: Magic runs the GDS equivalent. Hefesto's OASIS read must give the same `.mag` files as Magic's `gds read` of the KLayout `strm2gds` equivalent, and Hefesto's OASIS write must give the same per-layer merged geometry as Magic's `gds write` (X-1).
  - *KLayout cross-check*: run in `oasis-validate` (D18).
  - *Hefesto self-golden*: byte files fixed by the spec listings, checked against them by a unit test before capture.
- **Comparators.**
  - `oasis-records`: a decoded diff for display, from `goldenlib/oasis.py`, a clean-room Python decoder built from `oasis-format`.
  - `stream-geometry`: decode a GDS file (`goldenlib/gds.py`) and an OASIS file, then compare per-layer merged geometry. Manhattan geometry goes through `compare/polygon_set.py`. Non-Manhattan geometry is compared as canonical polygon lists, and the cases with arbitrary angles also run KLayout `strmxor` in `oasis-validate`.
  - `lef-text`/`def-text`: plain text, with the informative progress lines masked (`Diagnostic:` lines, `(This can take a while!)`, LefEstimate percentages).
- **Inputs with `.ext` files (Open Question 1 (a)).** The DEF-write net cases (17 rows: LEFDEF-317..328 and 330..334) need `.ext` files that Hefesto cannot produce before M7. Task 1.6 adds a capture mode that runs Magic's `extract all` on each such case's layout and syncs the `.ext` files into the case's `input/`, as M5's use-order traces were synced. Both tools then run only `def write`. As landed (task 1.6): mode `ext-inputs`, `tools/golden/ext_inputs.py` and `tools/golden/harness/ext_probe/ext_probe.tcl`. The probe also runs on a committed smoke layout (`tests/data/extflat/smoke/`), so it is exercised before the cases exist (task 15.1). `lefdef_cases.py --sync-ext` refuses any target whose `.ext` file is incomplete or whose run printed an error.
- **Corrections.** OASIS-091 is rewritten to the approved Q-OASIS-90 (a): `gds flatten yes` then `oasis read` *does* flatten, and `gds readonly true` makes `oasis read` cells read-only. MASK-OS-016 is authored with `lef write` of a contact cell.
- **Deviations.** These need an approved decision:
  - the both-modes fixes that change Magic-comparable output: Q-LEFDEF-1 (DEF-read default width), Q-LEFDEF-73 (undefined reference, failed RECT parse) and Q-LEFDEF-83 (renamed duplicate instance);
  - the fixes whose output has no Magic counterpart: none, because OASIS references are not Magic's;
  - Q-LEFDEF-114 needs none: it is compat, and the capture's magic-compat ON prints Magic's text (LEFDEF-316);
  - OASIS-012, OASIS-030 and OASIS-061: an approved deviation under Q-OASIS-2 (amended 2026-11-18) naming the missing commercial-tool input (below).

  The capture runs with magic-compat ON, and the OFF behaviours are unit-tested.
- **Commercial OASIS files (Open Question 3, decided (b)).** The user named no source of commercial OASIS files, so OASIS-012, OASIS-030 and OASIS-061 are authored and captured with their gdstk- and KLayout-written inputs and readers only. Each carries an approved deviation naming the missing commercial writer's or reader's input, under Q-OASIS-2 as amended 2026-11-18. The cases still pass, so the deviation is not a `tests/golden/deviations.toml` entry (that file lists cases whose outputs differ): it is recorded in each case's notes and in its row of `tests/golden/OASIS/CASES.md`, and `m6_cases.py --exit-check` lists it with its decision. Q-OASIS-2's judgement item is reopened with option (b), buy SEMI P39. If commercial files or P39 arrive later, GOLDEN adds them without a plan change.
  - Task 1.2 (2026-11-18) confirmed it: the only OASIS file in the pinned releases is IHP's KLayout-written LVS test layout `SP01.oas` (D2, PDK row), so the PDKs add no commercial-writer input. GOLDEN may use `SP01.oas` as a further KLayout-written reader input, after checking that its licence allows it as a test input.

### D20. The 419-byte OASIS reference file (OASFMT, GOLDEN; exit criterion)

The exit criterion "the 419-byte OASIS reference file is confirmed by a third-party reader" is met when all of these hold:
1. `tests/data/oasis/codec/reference_419.oas`, generated from the `oasis-format` listing (D11), is 419 bytes, and Hefesto decodes it into the cell the spec describes (unit test, every leg);
2. `oasis write` of that cell (OASIS-050) gives the same 419 bytes (golden, self-reference);
3. in `oasis-validate`:
   - KLayout reads the file with no warning, with `expect_strict_mode` matching the file's mode;
   - `strmcmp` against the KLayout-made equivalent of OASIS-013 reports no difference;
   - gdstk reads the file and `oas_validate` accepts its signature.

The run ID and tool versions are recorded in tasks.md and in OASIS-013's case notes.

### D21. Fuzz targets, determinism and resource caps (INTEGRATION with LEFR, DEFR, LEFTECH, OASFMT, OASR, EXTSUB)

`hefesto-testing-strategy` "Parser fuzzing" requires a target for every external reader. M6 adds:

| Target | Input | Covers |
|---|---|---|
| `fuzz_lef_reader` | LEF text | tokenizer, every section, macro creation and annotation into a scratch session with a fixed synthetic tech |
| `fuzz_def_reader` | DEF text, after a fixed LEF | every section, generated vias, routes, pins, blockages |
| `fuzz_lef_section` | a tech file whose `lef` section is the fuzz input | the table, defaults, scaling |
| `fuzz_oasis_reader` | raw bytes; a flag byte selects plain or gzip-wrapped | the decoder, CBLOCK, validation, the full read into a scratch session, the content hash, the read-only path, then an in-memory `oasis write` and `gds write` |
| `fuzz_ext_reader` | `.ext` text (Open Question 1 (a)) | the read and flatten subset |

- Seeds come from the case inputs, `oasgen.py` output, the PDK LEF files (Open Question 6 (a)) and the 419-byte file.
- Each target has resource caps so that timeouts are real hangs: token and line lengths, DEF declared counts (never pre-allocated from the file), generated-via ROWCOL, OASIS repetition counts, point counts, `circlepoints`, the name-table size, CBLOCK inflation and placement depth. Caps that change behaviour relative to Magic or the spec become proposed decisions, as M5's Q-MASK-416 did (task 17.3).
- The 60 s CI run and the seed replay on every leg follow M3–M5.
- **Determinism.** The determinism job gains `--require-area LEFDEF --require-area OASIS` once those expected outputs exist. Pinned-value tests run on every leg for these float paths: LEF/DEF float32 printing, the LEF and DEF unit conversion, the CIRCLE vertices for r = 1000 and N = 64 (Q-OASIS-31 scenario), and the OASIS real types.

### D22. Performance (PERF)

`hefesto-performance` sets no target for LEF, DEF or OASIS, so M6 adds no requirement. Open Question 4 was decided (a) on 2026-11-18, with no spec change:
- `lefdef-bench`: `lef read` of `sky130_fd_sc_hd.lef`, `def read` of a placed-and-routed sky130 block, and `lef writeall` of the standard-cell library;
- `oasis-bench`: `oasis read` and `oasis write` of the M5 `gds-bench` workloads, against Hefesto's own `gds read`/`gds write` of the same layouts.

Both measure callgrind instructions on the x86_64 runner, as `gds-bench` does. Each workload is gated at its recorded baseline + 2 %, and the comparison with Magic (LEF/DEF) or with the GDS path (OASIS) is reported for information. `gds-bench` and `drc-profile` must still pass, because D15 touches the GDS settings and D17 the stamps.

### D23. Security review (INTEGRATION; `docs/contributing/security.md`)

LEF, DEF, OASIS and `.ext` files are attacker-controlled. The review covers:
- **M6-S1.** Fixed-size buffers: Magic's 256-byte names (Q-LEFDEF-113) and 2048-character lines become unbounded strings with token caps.
- **M6-S2.** DEF declared counts, and LEF/DEF ROWCOL and repetition counts: no pre-allocation from file values, and 64-bit arithmetic on coordinates × UNITS (overflow reported).
- **M6-S3.** OASIS integer overflow (65 bits), real division by zero (type 2/3 with 0), lazy repetition expansion, and point-count caps.
- **M6-S4.** CBLOCK decompression bombs (inflate cap), nested CBLOCKs, and a record spanning a CBLOCK end (Q-OASIS-9).
- **M6-S5.** Name tables: table-offset cycles, duplicate refnums, and refnums without names (Q-OASIS-34).
- **M6-S6.** Placement cycles (a cell that places itself, directly or not) and nesting depth in OASIS and DEF; the post-order read bounded as in gds-read.
- **M6-S7.** CIRCLE `circlepoints` bounds and the non-Manhattan decomposition budget (the M5-S7 work budget is reused).
- **M6-S8.** `.ext` input (Open Question 1 (a)): recursion through `use` records, and node-merge chains.
- **M6-S9.** Path handling: `OASIS_FILE` values with PDK variables, and LEF/DEF output naming (Q-LEFDEF-43) inside the project root on iPadOS.

Each finding is fixed with a regression test or recorded as an accepted limit. A cap that changes behaviour is drafted as a proposed decision for the user.

### D24. Items deferred to M6 from earlier milestones

| Item (source) | M6 home |
|---|---|
| MASK-OS-016 (contact cut size through LEF write; m5 task 13.4) | LEFW + GOLDEN |
| MASK-GW-032/034 and MASK-GR-009, built from `.mag` abstract cells in M5: one case each with a real `lef read` abstract | GOLDEN (optional, with the Open Question 6 (a) LEF sets) |
| `lef read`/`def read` from the command line and PDK startup (`app/startup.cpp`, `script/pdk/startup.cpp`) | LEFR, DEFR (work once the commands exist; GOLDEN adds a startup case) |
| UI-G-005 deviation `def, lef` | GOLDEN |
| `oasis-read`/`oasis-write` decisions shared with M5 (Q-MASK-204, 207, 209, 211, 216, 114) | OASR, OASW |

## Area contracts

The headers are the contract, and each one is documented. The key types and functions of each area, which the scaffold writes as stubs:

### LEFTECH: `hefesto/lef/tech/`
- `LayerTable::from_section(const tech::RawSection&, const tech::Technology&, diag::Sink&) -> LayerTable`
- `LayerTable::fill_defaults(const drc::RuleSet&, const tech::Technology&, const compat::CompatSettings&)`, `scale(n, d)`, `find(name)`, `magic_to_lef(type)`, `enumerate(visitor)`
- `LayerRecord` (class, magic type, obstruction types, width/spacing/pitch/direction with unset flags, contact area, canonical name)

### LEFVIA: `hefesto/lef/via/`
- `ViaGenerator::generated(const GeneratedViaParams&, const LayerTable&, const mask::OutputStyle&, compat) -> ViaGeometry`
- `grow_cut(Rect, const LayerRecord&, const mask::OutputStyle&) -> Rect`
- `contact_regions(const db::CellDef&, const LayerTable&, compat) -> std::vector<ContactRegion>`, `via_name(...)`
- `paint_polygon(...)`

### LEFR: `hefesto/lef/lex/`, `hefesto/lef/read/`
- `Lexer`, `MessageCounter`
- `read(EditorState&, const platform::Path&, const LefReadOptions&) -> CommandResult`
- `find_lef_file(cell::LookupContext&, std::string_view)`

### LEFW: `hefesto/lef/write/`
- `write(EditorState&, db::CellDef&, const platform::Path&, const LefWriteOptions&) -> CommandResult`, `write_all(...)`
- `MacroBuilder` (flatten, type sets, pins, antenna, geometry), `format_distance(double, LefUnits)`

### DEFR: `hefesto/def/read/`
- `read(EditorState&, const platform::Path&, const DefReadOptions&) -> CommandResult`

### DEFW: `hefesto/def/write/`
- `write(EditorState&, db::CellDef&, const platform::Path&, const DefWriteOptions&) -> CommandResult`

### EXTSUB: `hefesto/extflat/`, `hefesto/extract/tech/`
- `read_flat(...) -> diag::Outcome<Netlist>`, `Netlist::visit_nodes(visitor)`
- `StyleView::from_section(...)`, `gate_types()`, `diff_types()`, `active_types()`, `substrate_types()`

### OASFMT: `hefesto/oasis/codec/`
- `Decoder` (`next() -> Record`, `modal()`, `names()`), `Encoder`, `ModalState`, `Repetition`, `Element`
- `validate(ByteSource&) -> ValidationResult`, `inflate_cblock(...)`

### OASR: `hefesto/oasis/read/`
- `read(EditorState&, const platform::Path&, const OasisReadOptions&, const mask::Progress*) -> CommandResult`
- `content_stamps(ByteSource&) -> gds::StampTable`, `find_oasis_file(...)`

### OASW: `hefesto/oasis/write/`
- `write(...)`, `emit(const gds::PreparedWrite&, const OasisSettings&, path, Progress*)`
- `transcode_gds_cell(...)`, `reencode_oasis_cell(...)`, `detect_repetitions(...)`
- `gds::transcode_oasis_cell(...)` (in `gds/write/vendor_oasis.cpp`)

### OASCMD: `hefesto/oasis/command/`
- `OasisSettings`, `register_oasis_command(CommandRegistry&)`, `sniff_format(ByteSource&) -> FileFormat`

## As-landed scaffold contracts

The scaffold (tasks.md 2.2–2.6, branches `m6-r1-areas` and `m6-r1-iface`, integrated with the D4 remainder on `m6-round1`) landed the areas, headers and stubs below. The headers are the contract; this section records where they are, who fills them in, and where they differ from "Area contracts" and D4. Include paths are under `src/core/include/hefesto/`, sources under `src/core/src/`.

### Files and owners

| Path | Contents | Owner (fills in) |
|---|---|---|
| `{lef,def,oasis,extract}/{sources.cmake,codes.cpp}`, `extflat/codes.cpp` | area sources and code tables assembled from `<sub>/codes.inc` | INTEGRATION |
| `extflat/{sources.cmake,codes.inc}` | one EXTSUB directory (no subdirectories) | EXTSUB |
| `lef/session.hpp` | `LefSession` (`EditorState::lefdef`: `layers`, `vias`, `rules`, `datestamp`, `nocheck`, `units`) | INTEGRATION (members as D4) |
| `mask/session.hpp`, `mask/stream_read.hpp` | `MaskSession::stream_read` (`stream::StreamReadSettings`, `stream::PolygonSubcells`) and `MaskSession::oasis` | INTEGRATION (D15) |
| `lef/lex/lexer.hpp`, `src/lef/lex/lexer.cpp` | `Token`, `Lexer` (`next`, `line`, `estimate`), `MessageCounter` (`count`, `errors`, `warnings`, `kLimit`) | LEFR |
| `lef/tech/table.hpp`, `src/lef/tech/table.cpp` | `LayerClass`, `RouteDirection`, `LayerRecord`, `LayerTable` (`from_section`, `fill_defaults`, `scale`, `find`, `magic_to_lef`, `enumerate`, `size`); `ViaShape`, `ViaDefinition`, `NonDefaultRule`, `ViaTable`, `RuleTable` | LEFTECH (layer table); LEFVIA, LEFR (via and rule records) |
| `lef/via/{generate,polygon,cut,regions}.hpp`, `src/lef/via/{generate,cut}.cpp` | `GeneratedViaParams`, `ViaGeometry`, `ViaGenerator::generated`; `paint_polygon`; `grow_cut`; `ContactRegion`, `contact_regions`, `via_name` | LEFVIA |
| `lef/read/{read,find_file}.hpp`, `src/lef/read/{read,find_file}.cpp` | `LefReadOptions`, `lef::read`; `find_lef_file` | LEFR |
| `lef/write/{write,macro,number}.hpp`, `src/lef/write/{write,macro}.cpp` | `LefWriteOptions`, `lef::write`, `lef::write_all`; `TypeSets`, `MacroBuilder`; `LefUnits`, `format_distance` | LEFW |
| `lef/command/command.hpp`, `src/lef/command/command.cpp` | the shared `lef`/`def` dispatcher: `lef_options`, `def_options`, `register_lef_def_commands` | INTEGRATION (dispatcher); LEFR, LEFW, DEFR, DEFW (their options' argument parsers, D14) |
| `def/read/read.hpp`, `src/def/read/read.cpp` | `DefReadOptions`, `def::read` | DEFR |
| `def/write/write.hpp`, `src/def/write/write.cpp` | `DefWriteOptions`, `def::write` | DEFW |
| `oasis/codec/{elements,modal,decoder,encoder,validate,cblock}.hpp`, `src/oasis/codec/{codec,validate}.cpp` | `Point64`, `Repetition`, `expand`, `ElementKind`, `Element`; `ModalState`; `RecordId`, `Record`, `NameTables`, `DecoderOptions`, `Decoder`; `EncoderOptions`, `Encoder`; `ValidationScheme`, `ValidationResult`, `validate`; `inflate_cblock`, `deflate_cblock` | OASFMT |
| `oasis/read/{read,stamps,find_file}.hpp`, `src/oasis/read/{read,stamps}.cpp` | `OasisReadOptions`, `oasis::read`; `content_stamps`; `find_oasis_file` | OASR |
| `oasis/write/{write,emit,transcode,repetitions}.hpp`, `src/oasis/write/{write,emit}.cpp` | `OasisWriteOptions`, `oasis::write`, `to_gds_write_options`; `emit`; `transcode_gds_cell`, `reencode_oasis_cell`; `RepetitionGroup`, `detect_repetitions` | OASW (`to_gds_write_options`: INTEGRATION) |
| `src/gds/write/vendor_oasis.{hpp,cpp}` (private) | `gds::transcode_oasis_cell(VendorIo&, const db::CellDef&) -> CopyResult` | OASW |
| `oasis/command/{command,settings,format}.hpp`, `src/oasis/command/{command,format}.cpp` | `oasis_options`, `register_oasis_command`; `OasisSettings`; `FileFormat`, `kOasisMagic`, `sniff_format` | OASCMD |
| `extflat/read.hpp`, `src/extflat/read.cpp` | `ReadOptions`, `FlatNode`, `FileStamp`, `Netlist` (`visit_nodes`, `stamps`), `read_flat` | EXTSUB |
| `extract/tech/style_view.hpp` (header-only) | `StyleTypes`, `StyleView` (`from_section`, `has_style`, `gate_types`, `diff_types`, `active_types`, `substrate_types`) | EXTSUB |
| `cmd/stubs.hpp`, `src/cmd/stubs.cpp` | `not_implemented`, `not_implemented_status`, `not_implemented_diagnostic` (`CORE-NOTIMPL-006`) | INTEGRATION (removed at M6 exit) |
| `tests/unit/{lef,def,oasis,extract}/<sub>/`, `tests/unit/extflat/` | one executable per directory once it has a `*_test.cpp` | as the source directory |
| `tests/unit/core/m6_scaffold_test.cpp`, `m6_shared_interfaces_test.cpp` | the `[m6-scaffold]` tests | INTEGRATION; owners replace them with `[cap:][req:]` tests |
| `docs/diagnostics/codes/{lef,def,oasis,extflat,extract}.md` | one section per owning workstream and the generated table | INTEGRATION (sections: their workstreams) |
| `docs/design/m6-ui-conformance.md` | seeded with the D16 items | SHELL ("Performance": PERF) |

### Differences from "Area contracts" and D4

- **Session headers.** `LefSession` is in a new INTEGRATION header `lef/session.hpp` (D4 names no file). `StreamReadSettings` is in `mask/stream_read.hpp`, namespace `hefesto::stream`, next to `mask/session.hpp`, not in a `stream/` area. It has the 11 options of D15 (12 fields with `flatten_limit`); D15's "13" counts `rescale` and `warning`, which stay in `MaskSession::rescale_allowed` and `MaskSession::warnings`. `gds::PolygonSubcells` is an alias of `stream::PolygonSubcells`. `GdsReadOptions` gained `stream_read`, which `gds read` copies from the session.
- **Via and rule tables.** `lefdef.vias` and `lefdef.rules` are `util::MagicHashTable<ViaDefinition>` and `util::MagicHashTable<NonDefaultRule>` (`ViaTable`, `RuleTable` in `lef/tech/table.hpp`, which D1 gives the "via table, non-default rules"). The records hold only what the scaffold tests need; LEFVIA and LEFR extend them.
- **LayerRecord.** Distances are `std::optional<db::Coord>` (unset = nullopt) in internal units; `obstruction_types` is a `tech::TypeMask`; `contact_area` is `std::optional<db::Rect>`. `LayerTable::find` and `magic_to_lef` return `const LayerRecord*` (nullptr when absent); `enumerate` takes a `std::function` visitor. The aliases of a layer share one record, as Magic's `lefLayer` is shared (lef-tech-section "LEF name aliasing and canonical names"): `LayerTable` keeps a vector of records and a `util::MagicHashTable<std::size_t>` from each LEF name to its record's index, so `find` returns one pointer per layer and `scale`/`fill_defaults` update each layer once; `size()` counts LEF names. The private storage is LEFTECH's to change as long as aliases keep sharing one record. `find` and `enumerate` already work over the (empty) table.
- **Lexer.** `Lexer(gds::ByteSource&)`; `next()` returns `diag::Outcome<std::optional<Token>>` (nullopt at end of input), `estimate()` returns the consumed fraction. `MessageCounter::count(Severity)` returns whether the message is printed; the scaffold counts and always prints (the cap is LEFR's).
- **Via generator.** `ViaGenerator::generated` is static and returns `ViaGeometry` (`ViaShape`s relative to the origin); `GeneratedViaParams` holds the VIARULE fields in internal units. `paint_polygon(CellDef&, TileType, std::span<const db::Point>, std::string_view port_name = {}) -> diag::Status` is in `lef/via/polygon.hpp`.
- **Entry points.** `lef::read`, `lef::write`, `lef::write_all`, `def::read` and `def::write` take no `mask::Progress*`: they are single-phase commands that report through `mask::ProgressHost` (D4). `LefReadOptions` holds `-import`/`-annotate`, `LefWriteOptions` the `lef write`/`writeall` switches of lefCmd.c (`-tech`, `-hide`, `-pinonly`, `-toplayer`, `-nomaster`, `-notop`, `-all`), `DefReadOptions` `-labels`/`-annotate`/`-noblockage` and `DefWriteOptions` `-units`/`-analog`/`-allspecial`; each also carries the typed name and the file system. `OasisWriteOptions` is `{OasisSettings settings; gds::GdsWriteOptions prepare;}`, so `oasis::write` hands `prepare` to `gds::prepare_write` unchanged.
- **to_gds_write_options** is implemented, not a stub: it copies the write options `oasis` shares with `gds` by syntax (and `magscale`) from `OasisSettings` and sets GDS compression to 0. `datestamp` is not mapped: it is read-side only for OASIS (oasis-command "Options shared with the gds command": OASIS has no dates), and its nullopt means "yes" in `oasis` but "the cell's timestamp" in `gds`, so `GdsSettings::datestamp` stays nullopt. Its `StreamReadSettings` parameter is kept for the D4 signature; `prepare_write` reads no read setting today.
- **OASIS codec.** `ModalState` is in its own header `oasis/codec/modal.hpp` (the decoder and the encoder both keep one); `reset()` clears every variable until OASFMT lands the exact Q-OASIS-5 rule. `Decoder::next()` returns `diag::Outcome<std::optional<Record>>`; `names()` returns `NameTables` (four maps by reference number). `Encoder` has `begin`, `begin_cell`, `element`, `finish`. `Repetition` keeps the grid fields (types 1–3) and the explicit offsets (4–11); `expand` gives the instance offsets. `Point64` carries OASIS's 64-bit coordinates. `validate` returns `diag::Outcome<ValidationResult>`; `deflate_cblock` is added beside `inflate_cblock`.
- **OASW.** `emit(const gds::PreparedWrite&, const OasisSettings&, path, const mask::Progress*)` takes no `StreamReadSettings`; OASW may add it when it exists in the write path. `transcode_gds_cell` and `reencode_oasis_cell` take a `gds::ByteSource`, the record offset, the output name and an `Encoder&`. `detect_repetitions(std::span<const db::Point>)` returns `RepetitionGroup`s. `gds::transcode_oasis_cell(VendorIo&, const db::CellDef&) -> CopyResult` is private to `gds/write/`.
- **LEFW.** `format_distance` takes a `float` value (Magic's `lefPrint` argument is float, Q-LEFDEF-45), not a `double`.
- **EXTSUB.** `read_flat(cell::LookupContext&, std::string_view root, const ReadOptions&) -> diag::Outcome<Netlist>`; its stub moved from the header into `src/extflat/read.cpp` (round 1 integration) so it can use `cmd::not_implemented_diagnostic`. `StyleView` stays header-only, in namespace `hefesto::extract`; M10's plan names `hefesto::extract::tech::ExtractStyleSubset` in the same directory, and whichever of M6 EXTSUB and M10 TECH lands second lines the two up.
- **Commands.** `lef` and `def` share one dispatcher with Magic's two tables; both look options up in the `lef` table, `def writeall` gets Magic's refusal, `def datestamp` and `def nocheck` answer "only for LEF", every other option answers `CORE-NOTIMPL-006`. The window check copies `gds`'s (no window and no point: `VIEW-WINDOW-003`); Magic's CmdLef first tries the selected cell, then the only window, then the box, and the option owners refine it. `oasis` has 34 options (help, the `gds` options in cmdCalmaOption order, then `strict`, `validation`, `repetitions`, `layernames`, `circlepoints`); an unknown or ambiguous option is a `CMD-USAGE-001` error. Startup paths (`app/startup.cpp`, `script/pdk/startup.cpp`) get `CORE-NOTIMPL-006` for `lef read`/`def read` until LEFR and DEFR land.
- **Help lines (D14, D17).** `ui/magic_help.cpp` already had Magic's `lef` and `def` lines, which now print because the commands are registered. No `oasis` line is added: the table holds Magic's help strings, which `help` prints with magic-compat ON (UI-G-005 compares them with Magic), so an `oasis` entry would add a line Magic does not print. With magic-compat OFF `oasis` is listed from its own help text. The UI-G-005 deviation text no longer lists `def, lef` (round 1).
- **Edits outside the new areas.** `src/core/CMakeLists.txt` (`HEFESTO_CORE_AREAS`), `diag/codes.cpp` and `area_codes.hpp` (five areas, `CORE-NOTIMPL-006`), `cmd/all_commands.cpp`, `editor/editor_state.hpp`, `gds/write/sources.cmake` (`vendor_oasis.cpp`), the GDS reader files of the D15 split (`gds/command/read_options.cpp`, `gds/read/{reader,shapes,sref,structure}.*`), `tests/fuzz/fuzz_gds_reader.cpp`, the `tests/unit/gds/` field renames, `docs/diagnostics/codes.md` and `docs/contributing/README.md`.

### Round 2 contract changes

Recorded at the round-2 integration (branch `m6-round2`). The headers are the contract; this list says what moved since the scaffold.

- **LEFTECH (`lef/tech/table.hpp`, `lef/tech/lifecycle.hpp`).** `LayerRecord` gained `obstruction_type` (Magic's primary `obsType`); `obstruction_types` is that type plus a cut's secondary via-obstruction type. `LayerTable::from_section` has an overload taking `(rules, compat)`; `find_mutable`, `add` and `release` were added for the readers (Q-LEFDEF-5 LAYER records and LEF VIA records go into the table through `add` so that `magic_to_lef` sees them, Q-LEFDEF-92). The session hooks are `lef::read_layer_section`, `install_layer_table` and `refill_layer_table` in `lifecycle.hpp`, called from `tech/commands.cpp` (D2 row amended). A partial `tech load` of other sections does not rebuild the table.
- **LEFVIA (`lef/via/`).** New `ViaContext{tech, style*, compat, sink}` (`context.hpp`). `grow_cut(cut, type, named_type, ViaContext)` replaces `grow_cut(Rect, LayerRecord, OutputStyle)`; `ViaGenerator::generated(params, ViaContext)` replaces the `(params, LayerTable, OutputStyle, compat)` form. `GeneratedViaParams::layers` holds resolved `TileType`s (LEF/DEF layer resolution is the readers') plus `float scale` (input units per internal unit). `ViaGeometry` is `{type, area, shapes head-first}`. `paint_polygon` takes `const tech::Technology&` and an optional `PortLabel`. `contact_regions` takes the `Technology`, not the `LayerTable`; `via_name`, `region_via`, `register_vias` and `via_rects` take a `DefViaContext`. `store.hpp` adds `begin_via`, `add_via_rect` and `shared_names`; `ViaDefinition` now mirrors Magic's lefLayer of class VIA (class, optional normal and obstruction types, contact area in half units, canonical name, `ref_count`). Review of PR #69: `context.hpp` adds `via_message_dialect(code)`, the LEF/DEF prefix Magic prints a LEF-VIA message under (cut growing is LEF even in `def read`); DEFR's sink must take the prefix from it, not from the reader. `store.hpp` adds `scale_via_areas`, which `tech::scale_layout` calls next to `LayerTable::scale` (LefTechScale scales via areas). The `cut`-flavor contact area is GeoNullRect (0, 0, 0, 0).
- **LEFR (`lef/lex/`, `lef/read/`).** `lef/lex/statements.hpp` (`lookup_full`, `parse_number`, `skip_statement`, `parse_end`, `skip_section`, `MessageTarget`) is shared with DEFR. The reader edits the layer table through `lef::LayerAccess` (`lef/read/layers.hpp`); the adapter over `LayerTable` (find, `add_route`, `update_route`, `reserve`) is still to be written (tasks.md 5.6). `NonDefaultRule` gained `name`, `hard_spacing` and `layers` (`NonDefaultRuleLayer`, `RuleTarget`).
- **OASFMT (`oasis/codec/`).** `Element` gained `reference`, `point_list_type`, `attribute`, `payload` and `ElementKind::xelement`; `Repetition` gained `grid`, and its offsets hold d_1..d_{n-1}. `ModalState` x/y default to 0 and gained the reference, point-list-type and last-property fields. `Record` gained `start`, `end`, `property`, `target`, `layer_name`, `xname` and `in_cblock`; `NameTables` gained `xnames` and `layernames`. `Decoder` and `Encoder` are pimpl; `Encoder` gained `standard_properties`, `file_property`, `layer_name` and `property`. New headers `primitives.hpp` and `property.hpp`. Decoded records keep names as refnums (resolve through `names()`). The encoder writes TRAPEZOID only as record 23 and never the RECTANGLE S form. The 419-byte reference file is `tests/data/oasis/codec/reference_419.oas` (D20).
- **EXTSUB (`extflat/`, `extract/tech/`).** `FlatNode` gained `port`, `top_port`, `names` and `locations` after `substrate` (aggregate initialisers need all eight fields under `-Wmissing-field-initializers`). `extflat::ReadOptions` needs `tech_name`, `global_nets`, `cell_directory`, `units_per_lambda`, `style_units` and `magic_compat` from the session (DEFW). `extflat::ReadLimits` holds the D21 caps. `extract::StyleView` gained `from_section(first|named)` and `from_technology`; it has no units-per-lambda yet. The extract-section load hand-off in `tech/loader.cpp` is not added.

### Round 3 contract changes

Recorded at the round-3 integration (branch `m6-round3`).

- **Dispatcher (`lef/command/command.cpp`).** `def read` goes to `def::read_command` (`hefesto/def/read/command.hpp`) before the window check (Q-LEFDEF-86: no window needed). `lef write|writeall|nocheck` go to `lef::run_write_command` (`lef/write/args.hpp`), which finds its own target cell, also before the window check. `lef read` and `lef datestamp` go to `read_option` and `datestamp_option` (`lef/read/args.hpp`) after it, as in Magic. Only `def write` still answers `CORE-NOTIMPL-006`. The startup paths (`app/startup.cpp`, `script/pdk/startup.cpp`) already run `lef read`/`def read` as commands, so they reach the readers with no change.
- **LEFR (`lef/read/`).** New public header `lef/read/macro.hpp` (`lef::MacroHost`, behind which `macro.cpp`, `pin.cpp` and `port_geometry.cpp` create and annotate cells). `lef/read/session_layers.hpp` is the `LayerAccess` adapter over `LayerTable` plus the `ViaTable`. The VIA/VIARULE dispatch calls LEFVIA's `begin_via` and `add_via_rect` (`via_section.cpp`, `via_body.hpp`). `lef/read/args.{hpp,cpp}` hold the `lef read`/`lef datestamp` parsers; `session_host.cpp` makes one native undo event per read.
- **LEFW (`lef/write/`).** `MacroBuilder(MacroContext, CellDef, MacroOptions)` with `flatten()`, `type_sets()` and `write(out, messages)`. `TypeSets::masterslice` became `master_planes` (a `PlaneMask`) and `boundary` was added. `LefWriteOptions` gained `gds::SubtreeLoader loader` (writeall subtree) and `const mask::Progress* progress` (interrupt). `number.hpp` gained `output_scale`, `database_units` and `format_real`; `LefUnits` defaults to 1000, Magic's initial `LEFdbUnits`. LEFW reads the extraction style through `extract::StyleView::from_technology` on every write: there is no session hand-off yet, so `extract style NAME` is not honoured (EXTSUB/INTEGRATION follow-up).
- **DEFR (`def/read/`).** `DefReadOptions` gained `read_master` (`std::function<bool(db::CellDef&)>`, DBCellRead for COMPONENTS masters) and `progress` (`const mask::Progress*`). New: `find_def_file(cell::LookupContext&, name)`, `read(editor, gds::ByteSource&, file_name, options, size_hint)` for tests and fuzzing (`fuzz_def_reader`, task 17.2, can drive it over a `MemorySource`), and the header `def/read/command.hpp` (`parse_read_arguments`, `read_command`). The reader's message sink takes each LEF-VIA message's prefix from `lef::via_message_dialect`. DEF read keeps LEFVIA's split `ViaTable`/`LayerTable`: a via name shadows a layer name of the same name, and the route layer after a via uses `LayerTable::magic_to_lef` only.
- **OASR (`oasis/read/`; D12, D17 amended).** OASR is not a `mask::ReadSink` driver. It transcodes each OASIS element instance into its GDS-equivalent element in an in-memory GDSII stream and reads that with the public `gds::read(EditorState&, ByteSource&, ...)`, so every gds-read rule is reused exactly and D17's "gds-read helpers in a shared header" move is not needed. The costs are that a read holds the whole GDS-equivalent stream in memory, and the transcode caps in `oasis/read/internal.hpp` (`kMaxStreamBytes`, `kMaxInstances`; proposed, D21). New public headers `oasis/read/circle.hpp` (CIRCLE vertices, N = 64) and `oasis/read/vendor.hpp` (`oasis::VendorStamper`, hooked into `cell/timestamps.cpp`). The `OASIS_FILE` read-only rule is in `db/cell.cpp` `set_property`, next to the `GDS_FILE` rule (`OASIS_FILE` and `OASIS_CELL` join the forced-rename clear list); `db/property.cpp` got only the string coercion. `OasisReadOptions::files` takes a `platform::FileSystem` (fuzz_oasis_reader can use a `MemoryFileSystem`). `oasis/read/stamps.cpp` holds a verbatim copy of gds-read's `calmaStructStamp` block until gds-read exposes it (e.g. `gds::stamp_entries` in `gds/read/stamps.hpp`; open, task 11.7). `tests/data/oasis/read/read.tech` is a copy of `tests/data/gds/read/read.tech`, which the shared fixture loads; keep them identical.
- **Round-3 review.** LEFVIA: `cut_pattern` returns a `lef::CutPattern` (the pattern's row groups and digit runs, `at(row, column)`) instead of a rows x columns `vector<vector<bool>>`, which a hostile ROWCOL made tens of GB; it is parsed only with magic-compat OFF and a PATTERN given. The generator collects its cuts and reverses them once, and the readers add RECTs through `lef::ViaRectBuilder` (`add` appends, `commit` puts them in `add_via_rect`'s order), so neither costs the square of the count; `add_via_rect` keeps its contract. OASR: `content_stamps` streams (a `KeepingSource` under the decoder keeps only the raw bytes since the last record, one CBLOCK's data is held at a time, and each cell's FNV runs as its bytes go by); the stamps are unchanged. The dispatcher answers `def writeall|datestamp|nocheck` before the window check (Q-LEFDEF-86). `def read` loads the design with `cell::detail::load_into_window` (ignore-tech, DBWloadWindow) instead of the `load` command, so the design name is never parsed as an option.

## Risks / Trade-offs

- **R1. The M7 dependency of DEF write and of the antenna areas.** Without a netlist reader, `def write` has no nets, and without an extraction style, LEF antenna lines are missing. → Open Questions 1 and 2, decided (a). EXTSUB is built to the M7 specs in M7's layout, and the `.ext` inputs come from Magic, so no Hefesto extraction is assumed.
- **R2. Float32 formatting of LEF/DEF numbers.** `-0.000`, half-grid rounding and the `%.10g` outputs are where byte identity breaks first. → Port `lefPrint` literally, add pinned-value tests per leg, and use the LEFDEF-102/105 cases.
- **R3. Hash-order dependence.** LEF write PROPERTYDEFINITIONS, the layer record after a via, VIAS and BLOCKAGES order (Q-LEFDEF-30, 82, 92, 116, LEFDEF-334). → `util::MagicHashTable`, already validated against Magic's hash in M1–M5. LEFDEF-334 forces a table rebuild.
- **R4. OASIS spec fidelity without P39.** The codec rests on KLayout and gdstk parity (Q-OASIS-2 (a)). → The KLayout and gdstk cross-checks, the strict-mode round trips and the gdstk and KLayout inputs of OASIS-012, 030 and 061. No commercial files were available (Open Question 3, decided (b)): those cases carry an approved deviation, and buying P39 is reopened (Q-OASIS-2 (b)). The residual risk, a detail where KLayout and gdstk both differ from P39, is accepted for M6.
- **R5. GDS equivalence drift.** OASIS read and write must track gds-read and gds-write exactly. → Reuse `ReadEngine`, `prepare_write`, the naming, labels and merge code. The `stream-geometry` comparator runs against Magic's GDS. OASIS-064 is a round trip.
- **R6. Settings split regression (D15).** → Behaviour-preserving refactor first, and the whole M5 suite unchanged before OASCMD starts.
- **R7. Port size.** About 13,000 lines of Magic C in `lef/`, plus part of extflat's 6,693 lines in EFread/EFbuild/EFflat/EFname/EFvisit (Open Question 1 (a)), and a new OASIS codec of comparable size. → Shared tokenizer, tables and generators, a dispatch table per section and per record, one helper per compat branch, flagged exceptions of at most 25.
- **R8. Malformed input.** LEF, DEF, OASIS and `.ext` are attacker-controlled. → D21 caps, D23 review, fuzz targets.
- **R9. Reference-tool drift.** A KLayout or gdstk upgrade can change a verdict. → Exact pins with digests (D18), and versions recorded with each `oasis-validate` run.

## Migration Plan

User data does not change: `.mag` files are read and written as before.
- Projects that ran `lef read`/`def read` in startup scripts now get the read instead of an unknown-command error.
- With `gds readonly`, `flatten` and the other shared read settings, `oasis read` now follows the `gds` values (Q-OASIS-90 (a)). CHANGELOG records it, and `oasis-command` already states it.
- `gds write` of a hierarchy holding read-only OASIS cells transcodes them (Q-OASIS-35 (a)). Before M6 such cells could not exist.
- `CORE-NOTIMPL-006` exists only during M6.
- The CI reference data gains the LEF cache keys (Open Question 6 (a)), and the capture image gains KLayout and gdstk (D18).

## Open Questions

These are scope and product decisions. **All six were decided by the user on 2026-11-18, each as recommended**: option (a) for 1, 2, 4, 5 and 6, and for 3 the fallback (b), because the user named no source of commercial OASIS files. Options are listed with the recommended one first. The decisions are in `docs/decisions/m6-decision-table.md`, "Planning questions raised by the M6 plan".

1. **The nets of `def write` need the `.ext` netlist, which is M7.** `def-write` "Preconditions and diagnostics" reads `<cell>.ext` with EFReadFile/EFFlatBuild. NETS, SPECIALNETS, net classification, naming, connections, geometry discovery, wire segments, route assembly and the net-based BLOCKAGES depend on it: about 9 of the 25 requirements, and 17 golden rows (LEFDEF-317..328 and 330..334).
   - (a) *Recommended:* pull forward the read-and-flatten subset of `ext-flattening-read`, `-hierarchy` and `-names` (D10, EXTSUB), built to the M7 specs in M7's area layout. The cases take Magic-written `.ext` files as inputs (D19, task 1.6). M7 extends this code.
   - (b) M6 writes DEF without nets (the "no .ext" path). The net requirements and cases are deferred to M7 with `deferred = "M7"` deviations, and the M6 exit criterion "the LEFDEF golden cases pass" is amended to exclude them.
   - (c) Move `def-write` as a whole to M7.

   **Decided 2026-11-18: (a).** EXTSUB builds the subset (D10, tasks section 9), GOLDEN's task 1.6 captures Magic's `.ext` files, and the `def-write` spec and the roadmap's M6 and M7 entries name the pulled-forward subset.
2. **LEF antenna areas and DEF blockage filtering read the extraction style, which is M7.** The gate set G, the diffusion set D and the well set W of `lef-write`, and the active types of `def-write` BLOCKAGES (defWrite.c:816, 2538), come from ExtCurStyle. With no style, Q-LEFDEF-40 makes G and D empty, so PDK LEF would lack ANTENNAGATEAREA and ANTENNADIFFAREA.
   - (a) *Recommended:* pull forward the device-class, antenna tie-down, substrate and active-type parts of `extract-tech-section` (D10, EXTSUB), behind `extract::StyleView`, which M7 re-sources.
   - (b) Treat the style as absent until M7. LEFDEF-118/119 and every PDK LEF write case get `deferred = "M7"`, and the exit criterion is amended.

   **Decided 2026-11-18: (a).** EXTSUB writes these parts of the one extract reader in `extract/tech/` (D10, task 9.3); the `lef-write` and `def-write` specs and the roadmap name them.
3. **Commercial OASIS files for the release gates OASIS-012, OASIS-030 and OASIS-061** (Q-OASIS-2 (a)). The decision table says obtaining them is part of M6. Nothing in the workspace has them, and no PDK in the pinned releases ships OASIS from a commercial writer (to be confirmed in task 1.2).
   - (a) *Recommended:* the user supplies, or names a source for, two or three small OASIS files written by a commercial tool (validation signature, CTRAPEZOIDs, strict mode). They are committed if their licence allows it, or fetched with a digest as a cached data set otherwise.
   - (b) If none can be obtained before the M6 exit, the three cases carry an approved deviation naming the missing input, and Q-OASIS-2 is reopened (option (b): buy P39).

   **Decided 2026-11-18: (b).** The user named no source. OASIS-012, 030 and 061 carry the approved deviation (D19, task 15.6), Q-OASIS-2's judgement item is reopened with option (b) (`docs/decisions/open-questions/OASIS.md`), and the `oasis-format` spec and the roadmap's M6 exit criteria say so.
4. **Performance gate for LEF/DEF/OASIS.** `hefesto-performance` names none.
   - (a) *Recommended:* the D22 benches, each workload gated at its recorded callgrind baseline + 2 %. The comparison with Magic (LEF/DEF) or with Hefesto's GDS path (OASIS) is informational. No spec change.
   - (b) Record only, no gate.
   - (c) Add a spec target, "no slower than Magic" for LEF/DEF and "no slower than the GDS path" for OASIS, as a hard gate (amends `hefesto-performance`).

   **Decided 2026-11-18: (a).** No spec change (D22, tasks section 16).
5. **Desktop scope for M6.**
   - (a) *Recommended:* format recognition on open (required by `oasis-command`) through the existing picker and drag and drop, and progress and cancel for the new reads and writes (D16). No new menu items or boards. LEF/DEF/OASIS are reachable from the console.
   - (b) Also add "Read LEF", "Read DEF", "Read OASIS" and "Write OASIS" items next to the GDS ones (native, not in Magic's wrapper). DESIGN draws them first.

   **Decided 2026-11-18: (a).** No new menu items (D16, tasks section 14).
6. **Real-PDK LEF/DEF coverage.** The CASES.md rows are synthetic, except LEFDEF-240.
   - (a) *Recommended:* add LEF data sets (`sky130-lef`, `gf180mcu-lef`, `ihp-lef`: the tech LEF and the standard-cell LEF of each pinned release, verified by size and digest in task 1.2). Add one `LEFDEF-RT-<lib>` case per standard-cell library: `lef read` of the tech and cell LEF, then `lef writeall`, compared with Magic on every CI leg. Add LEFDEF-240 on a small OpenROAD-style sky130 DEF.
   - (b) Only the CASES.md rows, with LEFDEF-240 on a hand-written DEF.

   **Decided 2026-11-18: (a).** PDK adds the LEF sets (tasks 1.2, 1.4), GOLDEN the `LEFDEF-RT-<lib>` cases and LEFDEF-240 (task 15.5).

## Resolved Questions

- **The 99 open questions that blocked M6** were approved as drafted on 2026-11-13, with the three judgement items decided as option (a) (Q-OASIS-2, Q-OASIS-35, Q-OASIS-90). `docs/decisions/m6-decision-table.md` holds the table, the counts, the evidence corrections and the spec changes. The specs were aligned in "Align the M6 specs with the approved decisions", and `check_milestone_readiness.py --milestone M6` reports ready. This design cites each decision where it shapes the code (D5–D13, D15).
- **36 LEFDEF entries resolved earlier** (2026-11-06/07, by X-1 as amended by D-1, X-2 tier 1, X-4, D-5 and D-14) are cited in D5–D9. The decision table lists them in its section "Entries resolved before the M6 round".
- **Q-LEFDEF-114** (the arguments of `No LEF layer corresponding to layer %s of pin "%s".` are swapped, defWrite.c:2432-2435) was approved by the user on 2026-11-18 as compat: magic-compat ON prints Magic's swapped text, so LEFDEF-316 needs no deviation, and OFF prints the corrected text (D9). Every one of the 136 LEFDEF and OASIS entries now has a `Resolution:` line.
- **Q-LEFDEF-25, 26 and 27** (more than two types on a tech `lef` line; an `ignore` line naming a mapped name; the type the cut-growing warning names) were raised by the round-2 LEFTECH and LEFVIA implementations and approved by the user on 2026-11-19: 25 and 27 compat, 26 fix in both modes (D5). The 139 entries all have a `Resolution:` line.
- **Open Questions 1–6** were decided by the user on 2026-11-18 as recommended (see "Open Questions"). Q-OASIS-2's judgement item (buy SEMI P39) is reopened by Open Question 3 (b); its signature `Resolution:` stays, so readiness is unaffected.
