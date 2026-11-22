# Design: M7 Extraction and netlisting

## Context

See proposal.md for the motivation. M6 (openspec/changes/m6-lef-def-oasis) delivered LEF/DEF and OASIS, and pulled forward two pieces of M7 (its Open Questions 1 and 2, decided (a) on 2026-11-18):
- **EXTSUB, the `.ext` read-and-flatten subset** (`src/core/src/extflat/`, about 2,400 lines; m6 tasks 9.1, 9.2): EFReadFile over the `extract-file-format` grammar, the node, connection, merge and kill records, EFFlatBuild with `EF_FLATNODES | EF_NOFLATSUBCKT`, EFVisitNodes and the name preference. `extflat::read_flat` returns a `Netlist` of `FlatNode`s; `extflat::ReadLimits` caps the input (EXTFLAT-READ-009/010/011).
- **The extraction-style subset** (`src/core/src/extract/tech/style_reader.cpp`, `style_view.cpp`; m6 task 9.3): style selection, `device` class and types, `tiedown`, `substrate`, the active and non-electrical types, behind `extract::StyleView` (`gate_types`, `diff_types`, `active_types`, `substrate_types`). It re-reads the raw section on each query. It keeps no style in the session, and there is no loader hand-off.

The conventions of M0–M6 still hold:
- SPDX headers;
- Catch2 tags `[cap:<capability>][req:<exact requirement heading>]`;
- per-area diagnostic code tables `AREA-TOPIC-NNN` in `docs/diagnostics/codes/<area>.md`;
- cognitive complexity of at most 15 per function (flag the irreducible ones);
- no UI, GPU, Raven or process headers in the core;
- golden expected outputs captured only in CI;
- Magic 8.3.694 at fc5daa9e as the behavioural reference;
- the clean-room rule for netgen, xschem and KLayout;
- golden cases run with magic-compat ON, and the OFF behaviours are unit-tested.

What exists for M7 to build on (origin/main 0c5f29fb, 2026-11-20):

- **Requirement coverage.** `check_requirement_coverage.py --milestone M7` reports 28 of 204 exact and behavioural requirements covered, all by EXTSUB's tests. Of those:
  - **covered fully (18):**
    - `ext-flattening-read` (8): "Locating and recursively reading .ext files", "Line tokenization", "Keyword recognition", "Scale record", "Node and substrate records", "Equivalence, merge and kill records", "Use, port, cap, resist, attr and connect records" (read side), and "Technology, style, version and resistance classes";
    - `ext-flattening-hierarchy` (5): "Container iteration order", "Flat node construction", "Node merge semantics", "Kills", "Global name merging". "Flat node construction" is tagged, but M6 ported only the standard-cell path (efFlatNodesStdCell); the efFlatNodes path the netlisters and antennacheck use is not there, so D13 and task 10.2 reopen it;
    - `ext-flattening-names` (5): "Hierarchical name structure", "Component hash", "Instance and array element names", "Name preference order", "Global and ground detection".
  - **covered in part (10):**
    - `ext-flattening-read` "Device records" (node side only) and "Parameters records" (messages only);
    - `ext-flattening-hierarchy` "Visit orders for output" (nodes only);
    - `extract-tech-section` (7): "Extract section line grammar and keyword matching", "Extraction styles and variants", "Style defaults", "Substrate declaration", "Device declaration parsing" (class and types), "Non-electrical types", "Antenna keywords parse-level reference" (`tiedown`).

  Task 10.0 lists these 18 as regression-guarded. M6's open task 9.2 note (the visit-order comparison with Magic's probe lines) moves to EXTFLAT if M6 has not closed it.
- **M10 3D P1 (in flight).** M10's scaffold (`m10-r1-scaffold`, 73d4d195) landed `hefesto::extract::HeightView` (`extract/tech/height_view.hpp`: `from_section`, `from_technology`, `rescaled(n, d)`, `StyleHeights`) beside `StyleView`, over M6's `detail::read_style`. There is no `ExtractStyleSubset` and no `parse_extract_subset`. Its "As-landed scaffold contracts" note says there is no loader hand-off: the view reads `Technology::raw_sections()`, which already keeps the raw `extract` section. TECH (m10 tasks 3.1, 3.2) is now extending `read_style`'s pass in `style_reader.cpp` with `style` and variants, `scale`, `units`, `planeorder`, `noplaneordering`, `height`, the end-of-style plane-order check, the defaults and the micron conversion and grid rescale of h and t. When it lands it touches five more `extract-tech-section` requirements, in part: "Units, lambda, cscale and rscale", "Plane ordering", "Layer height and thickness defaults", "Micron conversion", "Scaling to internal units". So no tech-load or `scalegrid` hook exists for an extraction style, and M7 must add one (D7, D20). M10 P3 (overlays) waits for M7.
- **Database.**
  - `db::Plane` (`db/plane.hpp`) is a faithful port of Magic's corner-stitched plane, with the maximal-horizontal-strip decomposition and Magic's enumeration order (X-1). Q-EXTRACT-5 relies on it.
  - It exposes `enumerate`, `enumerate_triangle`, `tile_at` and `neighbors`, and `TileView::tile`, an opaque handle to the private tile.
  - The private `detail::Tile` has no client field. `detail::TilePlane` has `split_x` and `join_y`, but they are not public. Magic's extractor keeps a region pointer in `ti_client` on every tile it visits (ExtRegion.c:71-88 extGetRegion, :533 ExtResetTiles; about 40 uses in `extract/`). extresist uses it on about 60 lines (`resis/ResMain.c`, `ResUtils.c`, `ResJunct.c`) and edits stitches directly (ResFract.c:436-488, TiSplitX and TiJoinY1). This is Open Question 1.
  - `connect::search_flat` and `search_hierarchical` (`connect/search.hpp`) are DBSrConnect and the hierarchical copy-connect search. They keep visited tiles in a `std::set` keyed by area (`connect/search.cpp:80-95`). antennacheck (DBTreeCopyConnect, EFantenna.c:532) and extresist's net yank use them.
  - `util::MagicHashTable` reproduces Magic's string hash order (`merge` groups and netlist orders, Q-EXTRACT-7, Q-NETLIST-2).
  - `db::UseIndex` gives the use enumeration order, and `db::flatten_to_new` flattens a cell (the yank buffers).
- **Cells and settings.**
  - `cell/timestamps.cpp` holds `cd_timestamp`, which incremental extraction compares with the `.ext` header (ExtMain.c:968-990).
  - `cell::LookupContext` and `search_file` find `.ext` files (EXTSUB uses them).
  - The project setting "global nets and power names" (`hefesto-settings`) holds `globalNets`, `sub`, `vdd` and `gnd` (Q-NETLIST-11, Q-NETLIST-33, Q-EXTRACT-9).
- **Commands.**
  - None of `extract`, `ext`, `ext2spice`, `exttospice`, `ext2sim`, `exttosim`, `extresist` and `antennacheck` is registered. UI-G-005's deviation lists them as M7 commands (`tests/golden/deviations.toml:134-137`).
  - SCRIPT-API-16 (reads `$SUB` through `extract all`) and SCRIPT-API-20 (`extract style`) are `deferred = "M7"`.
  - The desktop top bar paints a disabled "Extract" button whose tooltip names M7 (`docs/design/m1-ui-conformance.md:91`).
- **Golden.**
  - `tests/golden/EXTRACT/CASES.md` has 70 rows: EX-001..070 on the synthetic `exttest.tech`.
  - `tests/golden/NETLIST/CASES.md` has 67 rows, on hand-written `.ext` and `.res.ext` inputs: NL-FLAT 18, NL-SPICE 14, NL-HIER 6, NL-SIM 5, NL-CHK 1, NL-RES 8, NL-RESNET 15.
  - `tests/golden/MISC/CASES.md` has MISC-ANT-01..09.
  - None is authored.
  - M6 left these tools: the `ext-inputs` capture mode (`tools/golden/ext_inputs.py`, `harness/ext_probe/`), `goldenlib/extfile.py` (an `.ext` record checker) and the `ext-cap` canonicalizer (`compare/ext_cap.py`, Q-EXTRACT-7). `compare/spice_canonical.py` is M8's electrical comparator for xschem netlists. It was written clean-room and compares devices, not text.
  - Magic's default build makes only `magic` (Makefile.in:10, 128-130), so the capture image has no `extcheck` for NL-CHK-01.
- **Data.**
  - The PDK sets hold `sky130_fd_sc_hd/mag` (446 cells) and `gf180mcu_fd_sc_mcu7t5v0/mag` (229 cells).
  - IHP ships no `.mag` standard cells. `sg13g2_stdcell` (84 cells) is in the `ihp-gds-sc` set (the standard-cell library alone; `scripts/fetch_pdk.py`), so the 456 MB `ihp-gds` set is not needed.
  - `tests/golden` holds about 46 MB today.
- **Performance.** `hefesto-performance` "Batch throughput relative to Magic" requires single-threaded extraction and SPICE generation to take no more wall time than Magic, on the reference standard-cell block. DRC and GDS are judged by callgrind instruction counts (Open Question 5).
- **Size of the port.** Magic's `extract/` has 25,628 lines, `extflat/` 9,441 (EXTSUB ported part of 6,693), `ext2spice/` 7,437, `ext2sim/` 2,151, `resis/` 12,922 and `extcheck/` 192: about 58,000 lines, roughly twice M6's.

M7 runs in seven rounds (D1). The netlisters are built against Magic-written `.ext` files, so they run in parallel with the extractor instead of after it.

## Goals / Non-Goals

**Goals:**
- The M7 exit criteria of docs/roadmap.md:
  - the common exit criteria for the 21 capabilities: tagged unit tests for the 204 exact and behavioural requirements, fuzz targets for every new reader, CI green on every leg, and byte-identical output on Linux x86_64, Linux aarch64 and macOS arm64;
  - the EXTRACT and NETLIST golden cases pass (D21);
  - `.ext` and SPICE output match Magic's after canonicalization on the PDK standard cells (D21, Open Question 4).
- The roadmap deliverables: hierarchical extraction with parasitics (fringe-halo model included) and device recognition; ext2spice in all five dialects and ext2sim; R-C extraction with extresist; antenna checks.
- One extract-section reader and one `ExtStyle`, shared with M6's LEF/DEF writers and M10's 3D view. One `.ext` writer. One `.ext` reader and flattener for every consumer (netlisters, extcheck, antennacheck, `def write`, M9).
- Every compat decision has one branch point, testable in both modes.
- The entry points M9 calls: extract a hierarchy if stale, netlist it with one-run settings, and run extresist for `rc` mode (D18).

**Non-Goals:**
- LVS, simulation, overlays (M9, M10 P3).
- Rewriting M1–M6 code that meets its spec. M7 changes earlier code only where D20 lists it.
- `*extract`, sim2simp, desktop panels, and iPadOS extraction UI (proposal "Non-goals").

## Decisions

### D1. Rounds and gates

| Round | Starts when | Delivers | Exit |
|---|---|---|---|
| R0 DECIDE (done 2026-11-21) | now | the user's answers to Open Questions 1–9 and Q-EXTRACT-27 (all approved 2026-11-21 as recommended); the spec edits they imply (Open Question 5, Q-EXTRACT-27) | every row has a `Resolution:`; `check_milestone_readiness.py --milestone M7` ready; `openspec validate --all --strict` |
| R1 SCAFFOLD | R0 | areas, headers and stubs (D2, D5); command stubs; the plane client slot and stitch header (D6); the M7 tech-load and `scalegrid` hook (D7); `extcheck` in the capture image; the capture modes, the external-command case mode and the comparators; the 44 NETLIST cases with hand-written inputs authored and captured; the 53 flat EX cases (D21) authored and captured | the build passes with `-Werror` on every preset; `[m7-scaffold]` tests list every option; NETLIST and flat-EX expected outputs committed |
| R2 CORE | R1 merged | EXTTECH (handlers in new files, called from the existing keyword switch; Open Question 2), EXTNODE, EXTDEV, EXTCAP on single cells; **in parallel** EXTFLAT, SPICE and SIM against the captured NETLIST cases | the unit tests pinned to the captured flat-EX values pass (no EX case replays yet: replay needs the `extract` driver of R3); NL-FLAT, NL-SPICE and NL-SIM replay once EXTTECH's resist and device parsing (tasks 4.3, 4.5) is wired, which does not wait for M10 (D3) |
| R3 HIER | R2 merged | EXTHIER, EXTCMD, ANTENNA, the subcell parts of EXTDEV (task 7.7); the dispatch-table refactor (task 4.1) if M10's TECH has merged; NL-HIER, NL-CHK | every EX case (the flat ones included) and every MISC-ANT case replays, except those that need extresist |
| R4 RESIST | R3 merged (EXTNODE's regions and EXTFLAT's overlay read needed) | RESNET, RESCMD; SHELL | NL-RES and NL-RESNET replay |
| R5 GOLDEN | R4 merged | the PDK cases (Open Question 4), the deferred SCRIPT cases, the catalogue audit; PERF benches and gates (Open Question 5) | D21 counts all pass; the benches meet their gate |
| R6 EXIT | R5 | fuzz, security review, determinism areas, coverage, `CORE-NOTIMPL` retirement, exit check | D25's exit list; `m7_cases.py --exit-check` clean |

M7's start does not wait for M6's exit work (benches, fuzz, the security review; m6 tasks 16 and 17). Those tasks do not edit `extflat/` or `extract/tech/`, except `fuzz_ext_reader` (17.2) and the `ReadLimits` proposals (17.3). M7 extends that target once it lands, and inherits the decided caps. M7 does not wait for M10 P1 either (Open Question 2, decided (a) on 2026-11-21). The user asked on 2026-11-20 that M7 run alongside the 3D work to gain time, and D3 orders the work so that only the dispatch-table refactor of `style_reader.cpp` (task 4.1) waits for M10's TECH merge. Nothing on the R2 exit waits for it.

### D2. Layout of the M7 code

| Area | Public headers | Sources | Unit tests | Namespace |
|---|---|---|---|---|
| extraction style: the full `extract` section reader, `ExtStyle`, micron conversion, grid rescale (extends M6's and M10's subset) | `src/core/include/hefesto/extract/tech/` | `src/core/src/extract/tech/` | `tests/unit/extract/tech/` | `hefesto::extract` |
| regions, nodes, labels, node names, unique naming | `.../extract/node/` | `src/core/src/extract/node/` | `tests/unit/extract/node/` | `hefesto::extract` |
| `.ext` record writer | `.../extract/file/` | `src/core/src/extract/file/` | `tests/unit/extract/file/` | `hefesto::extract` |
| capacitance and lumped resistance | `.../extract/cap/` | `src/core/src/extract/cap/` | `tests/unit/extract/cap/` | `hefesto::extract` |
| devices and substrate | `.../extract/device/` | `src/core/src/extract/device/` | `tests/unit/extract/device/` | `hefesto::extract` |
| hierarchy: subtree, interactions, arrays, yank buffers, hard-way names | `.../extract/hier/` | `src/core/src/extract/hier/` | `tests/unit/extract/hier/` | `hefesto::extract` |
| `extract` command, run driver, incremental extraction, options, paths | `.../extract/command/` | `src/core/src/extract/command/` | `tests/unit/extract/command/` | `hefesto::extract` |
| path length | `.../extract/length/` | `src/core/src/extract/length/` | `tests/unit/extract/length/` | `hefesto::extract` |
| `.ext` reader and flattener (extends M6's single directory; no subdirectories) | `.../extflat/` | `src/core/src/extflat/` | `tests/unit/extflat/` | `hefesto::extflat` |
| `ext2spice` | `.../netlist/spice/` | `src/core/src/netlist/spice/` | `tests/unit/netlist/spice/` | `hefesto::netlist` |
| `ext2sim` | `.../netlist/sim/` | `src/core/src/netlist/sim/` | `tests/unit/netlist/sim/` | `hefesto::netlist` |
| `extcheck` (library part) | `.../netlist/extcheck/` | `src/core/src/netlist/extcheck/` | `tests/unit/netlist/extcheck/` | `hefesto::netlist` |
| extresist network construction and reduction | `.../extresist/network/` | `src/core/src/extresist/network/` | `tests/unit/extresist/network/` | `hefesto::extresist` |
| extresist command, `.ext` input model, cutoffs | `.../extresist/command/` | `src/core/src/extresist/command/` | `tests/unit/extresist/command/` | `hefesto::extresist` |
| `.res.ext`, `.res.lump`, FastHenry and geometry output | `.../extresist/output/` | `src/core/src/extresist/output/` | `tests/unit/extresist/output/` | `hefesto::extresist` |
| `antennacheck` and the shared antenna rules | `.../antenna/` | `src/core/src/antenna/` | `tests/unit/antenna/` | `hefesto::antenna` |
| the standalone `hefesto-extcheck` (Open Question 6 (a)) | none | `src/cli/extcheck/` | `tests/unit/netlist/extcheck/` | none |

`src/core/CMakeLists.txt` adds `netlist extresist antenna` to `HEFESTO_CORE_AREAS` (`extract` and `extflat` exist). Each subdirectory has its `sources.cmake` and `codes.inc`, assembled by the INTEGRATION-owned `<area>/{sources.cmake,codes.cpp}`, as M6's areas do.

The specs name the netlisters `netlist-*` and Magic names its directories `ext2spice/`, `ext2sim/`. Hefesto follows the specs. The area `netlist` holds only the layout netlisters. The netlist window (`netlist-window`, M12) will live in its own area.

Diagnostic prefixes:

| Document | Topics |
|---|---|
| `extract.md` | `EXTRACT-STYLE-*` (EXTTECH), `EXTRACT-NODE-*`, `EXTRACT-FILE-*` (EXTNODE), `EXTRACT-CAP-*` (EXTCAP), `EXTRACT-DEV-*` (EXTDEV), `EXTRACT-HIER-*` (EXTHIER), `EXTRACT-CMD-*`, `EXTRACT-LEN-*` (EXTCMD) |
| `extflat.md` | `EXTFLAT-READ-*`, `EXTFLAT-FLAT-*`, `EXTFLAT-ARGS-*` (EXTFLAT) |
| `netlist.md` | `NETLIST-SPICE-*` (SPICE), `NETLIST-SIM-*`, `NETLIST-CHECK-*` (SIM) |
| `extresist.md` | `EXTRESIST-NET-*` (RESNET), `EXTRESIST-CMD-*`, `EXTRESIST-OUT-*` (RESCMD) |
| `antenna.md` | `ANTENNA-CHECK-*` (ANTENNA) |

None of these codes reads like a golden case ID (EX-001, NL-SPICE-01, MISC-ANT-01).

### D3. Workstreams and ownership

| Workstream (tasks.md section) | Capabilities (requirements) | Owns (may edit) |
|---|---|---|
| PDK | none (reference data; Open Question 4) | `scripts/fetch_pdk.py` (if a set changes), `tests/unit/common/reference_data.hpp` (standard-cell paths), `docs/building.md` "Reference data for tests" |
| EXTTECH | `extract-tech-section` (22) | `src/core/{include/hefesto,src}/extract/tech/`, `tests/unit/extract/tech/`, `tests/data/extract/tech/`, `extract.md` (STYLE section); `extract/tech/height_view.cpp` once M10's TECH has merged (re-sourcing only, D7); the style builder that the editor-side tech-load and `scalegrid` hook calls (the hook itself is INTEGRATION's, D20) |
| EXTNODE | `extract-nodes-and-labels` (10), `extract-file-format` (7) | `extract/{node,file}/`, their tests, `tests/data/extract/node/`, `extract.md` (NODE, FILE sections) |
| EXTCAP | `extract-capacitance` (13), `extract-resistance-lumped` (5) | `extract/cap/`, its tests, `extract.md` (CAP section), `src/core/src/util/cr_atan.*` if Open Question 3 is answered (a) |
| EXTDEV | `extract-devices` (19), `extract-substrate` (8) | `extract/device/`, its tests, `extract.md` (DEV section) |
| EXTHIER | `extract-hierarchy` (12) | `extract/hier/`, its tests, `extract.md` (HIER section) |
| EXTCMD | `extract-commands` (12) | `extract/{command,length}/`, their tests, `extract.md` (CMD, LEN sections); the `extract` and `ext` registrations' argument parsers (D4) |
| EXTFLAT | `ext-flattening-read` (11), `ext-flattening-hierarchy` (11), `ext-flattening-names` (7); 18 of the 29 covered by M6 | `src/core/{include/hefesto,src}/extflat/`, `tests/unit/extflat/`, `tests/data/extflat/`, `extflat.md` |
| SPICE | `netlist-spice-options` (10), `netlist-spice-devices` (10), `netlist-spice-hierarchy` (6) | `netlist/spice/`, its tests, `netlist.md` (SPICE section) |
| SIM | `netlist-sim` (7), `netlist-extcheck` (2) | `netlist/{sim,extcheck}/`, `src/cli/extcheck/`, their tests, `netlist.md` (SIM, CHECK sections) |
| RESNET | `extresist-network` (15) | `extresist/network/`, its tests, `extresist.md` (NET section) |
| RESCMD | `extresist-command` (6), `extresist-output` (5) | `extresist/{command,output}/`, their tests, `extresist.md` (CMD, OUT sections) |
| ANTENNA | `antenna-check` (9) | `src/core/{include/hefesto,src}/antenna/`, its tests, `antenna.md`; the antenna-area call in `lef/write/macro.cpp` (D20) |
| SHELL | none (Open Question 7) | `src/shell/desktop/src/` (M7 entries: the Extract button, progress and cancel), `tests/unit/shell/` (M7 tests), `docs/design/m7-ui-conformance.md` |
| GOLDEN | the EXTRACT, NETLIST, MISC-ANT, PDK and deferred SCRIPT cases | `tests/golden/{EXTRACT,NETLIST}/`, `tests/golden/MISC/cases/MISC-ANT-*`, `tests/golden/SCRIPT/cases/SCRIPT-API-{16,20}/`, `tools/golden/` (`authoring/{extract_cases,netlist_cases,antenna_cases,ext_pdk_cases,m7_cases}.py`, `authoring/exttest.tech`, `compare/{spice_cap,sim_cap,res_ext}.py`, capture modes), `tools/golden/container/Containerfile` (the `extcheck` build), `tests/golden/deviations.toml` (M7 rows) |
| PERF | none (D23) | `bench/{extract,extresist}/`, the `extract-bench` and `extresist-bench` jobs of `perf.yml` |
| INTEGRATION | none | everything shared: root and `src/core/CMakeLists.txt`, the area assemblers, `editor/` (the D5 members), `db/plane.hpp` and the new `db/plane_stitch.hpp` with their `src/db/` implementation (D6, with the MODEL review), `cmd/`, `diag/`, `src/cli/` (other files), `tests/fuzz/`, `.github/`, `scripts/`, `CMakePresets.json`, `README.md`, `CHANGELOG.md`, `docs/roadmap.md`, `docs/contributing/`, `docs/decisions/`, `openspec/specs/` (open items), this change's artifacts |

Requirement totals: 22 + 17 + 18 + 27 + 12 + 12 + 29 + 26 + 9 + 15 + 11 + 9 = 207. tasks.md names each requirement in the task that covers it.

Rules (unchanged from M1–M6 D2):
- A workstream edits only what it owns. A needed change to a shared file or to another workstream's file is an `open_item` (what, why, and the exact edit if small). The owner or INTEGRATION applies it.
- GOLDEN authors case directories from the requirements and the owning workstream's notes. `expected/` is captured in CI.
- Only the integration step of each round commits and pushes, to `origin m7-extraction` or the branch its prompt names.

Dependencies:
- The scaffold (R1) publishes D5's interfaces with stubs, the `ExtStyle` data model with a test builder (`extract::testing::StyleBuilder`), and the plane client slot. Every extractor workstream starts from these, not from a parsed tech file.
- EXTTECH starts at once, in new handler files (Open Question 2 (a)). Before M10's TECH merges it calls them from the existing switch of `StyleReader::apply` in `style_reader.cpp` (one `case` line per keyword); the dispatch-table refactor (task 4.1) runs only after TECH has merged (D7).
- EXTNODE needs the client slot (D6). EXTCAP and EXTDEV need EXTNODE's regions and writer. They start on the scaffold's stubs and integrate in R2. EXTDEV's subcell parts ("Subcell device priority", "Subcell substrate connections") need EXTHIER and move to R3 (task 7.7).
- EXTHIER needs EXTNODE, EXTCAP and EXTDEV on single cells. EXTCMD needs EXTHIER's `extract_cell` for the run driver; its option parsing and incremental logic start in R2. EX cases replay only through that driver, so they replay from R3; in R2 the extractor workstreams verify with unit tests pinned to the captured EX values.
- EXTFLAT starts at once, on M6's code and the captured NETLIST inputs. SPICE and SIM need EXTFLAT's visitors (device, cap, resist and hierarchical) and start on its stubs.
- **SPICE and SIM need EXTTECH 4.3 and 4.5 wired.** ext2spice and ext2sim call ExtGetDevInfo (ext2spice.c:932, ext2sim.c:647; ExtTech.c:312-466) on the current extract style for the device-type order, the source, drain and substrate resistance classes and the default substrate names (with `$` substitution). Those values feed the `EFDevTypes` indices, `as`/`ad`/`ps`/`pd` and the substrate terminals. They read them through `ExtStyle::device_info()` (D5) from `extract.current`, which the tech-load hook (D7) fills. Until EXTTECH's resist and device handlers are wired, the netlister unit tests use `StyleBuilder` fixtures; the NL-SPICE and NL-SIM replays need the wired handlers, which do not wait for M10 because they are called from the existing switch.
- ANTENNA needs EXTTECH's antenna rules, EXTFLAT's device visits and `connect::search_hierarchical`.
- RESNET needs EXTNODE's regions, the stitch header (D6) and `connect`. RESCMD needs EXTFLAT's tokenizer (its own record handling, ResReadExt) and RESNET.
- SHELL needs EXTCMD's run driver with progress.
- GOLDEN authors inputs from R1. The 44 NETLIST cases with hand-written inputs and the 53 flat EX cases (D21) are captured in R1, so that R2's unit tests can pin the captured values; the other EX cases and the MISC-ANT, NL-RES and NL-RESNET cases as they are authored; the PDK cases in R5.
- PERF builds the benches once EXTCMD and SPICE land (R5).

### D4. Build and stub contract

- **No new library dependency**, unless Open Question 3 is answered (a) with the vendored option, which adds one MIT-licensed source file and a `THIRD_PARTY_NOTICES.md` entry (`hefesto-licensing`).
- **Floating point.** `extract`, `extflat`, `netlist`, `extresist` and `antenna` build with `-ffp-contract=off`.
  - Numbers are printed with Magic's formats (`%lg`, `%g`, `%.*f`, `esSIvalue`) through one formatting helper per area, with the C locale.
  - Q-EXTRACT-20's single-precision sidewall denominator and Q-NETLIST-25's integer heap keys (ON) are kept as Magic's types.
  - `sqrt` is IEEE-exact. `atan` (ExtCouple.c:718-737, 1298, 1492-1508, 1874-1875) is Open Question 3.
  - D22 lists the pinned-value tests.
- **Stubs.** The scaffold registers `extract` and `ext`, `ext2spice` and `exttospice`, `ext2sim` and `exttosim`, `extresist` and `antennacheck`, each with its spec's full option table. Options are matched as `extract-commands` "Command syntax and option matching" and `netlist-spice-options` "Command options" define (Q-NETLIST-13, Q-NETLIST-41 per mode).
  - Each option that is not implemented yet fails with the next free core code ("not implemented yet: <command> <option>"). `CORE-NOTIMPL-007` is taken by the M8 scaffold (`m8-r1-scaffold`, `docs/diagnostics/codes/schem.md:26`), so this is `CORE-NOTIMPL-008` unless another milestone takes it first; the rule is "next free". Completion, help, scripts and golden inputs therefore see the final names.
  - INTEGRATION retires the code at M7 exit.
- **Scaffold tests** carry `[m7-scaffold]` and no `[cap:][req:]` tags. Owners replace them.

### D5. Session state and shared interfaces

`editor::EditorState` gains three members. INTEGRATION owns the structs; their members belong to the named workstreams:

| Member | Holds | Workstream |
|---|---|---|
| `extract.styles` | the style table of the loaded technology: names in declaration order, variants expanded, each `ExtStyle` parsed on first selection as ExtLoadStyle does | EXTTECH |
| `extract.current` | `std::shared_ptr<const ExtStyle>`, the current style after micron conversion and grid rescale; replaced, never mutated | EXTTECH |
| `extract.options` | ExtOptions (`do`/`no` capacitance, coupling, resistance, length, adjust, aliases, labelcheck, local, unique, extresist, fringehalo, corners), the warning set, `extract path` | EXTCMD |
| `extract.overrides` | the `halo` and `stepsize` overrides of the loaded style (Q-EXTRACT-25: lost on style reload, scaled on grid rescale) | EXTCMD |
| `extract.length` | the `extract length` driver and receiver lists | EXTCMD |
| `netlist.spice`, `netlist.sim` | the session-sticky `ext2spice` and `ext2sim` options (format, scale, cthresh, rthresh, merge, subcircuits, hierarchy, blackbox, renumber, global, shorts, resistor tee, extresist, lvs preset) | SPICE, SIM |
| `netlist.names` | the name-conversion flags: one session-wide set with magic-compat ON, per run with it OFF (Q-NETLIST-16) | EXTFLAT |
| `extresist.options` | tolerance, `all`, `simplify`, `extout`, `lumped`, `silent`, `skip` masks (Q-NETLIST-23), `ignore`/`include` lists, `fasthenry` frequency (Q-NETLIST-21: 10 MHz), `mindelay` (ps, Q-NETLIST-28), threshold, `box` | RESCMD |
| `extresist.heap` | the path-resistance heap state kept across nets with magic-compat ON (Q-NETLIST-46) | RESNET |

The style changes only on tech load, `extract style`, `scalegrid` and finer-grid reads. Readers hold the `shared_ptr` they started with, so a run is not affected by a style change on another thread. The netlist and extresist options are guarded by the exclusive lock their commands hold.

Shared interfaces created by the scaffold (INTEGRATION owns the files; the named workstream fills in the implementation):

**Extraction style**
- `extract::ExtStyle` (`extract/tech/style.hpp`), the ExtCurStyle data model:
  - scale: name, `scale`, `units`, `cscale`, `rscale`, step, side halo, fringe shield halo, the fringe model default;
  - planes: plane order and status (`planeorder`, `noplaneordering`), height and thickness per type;
  - connectivity: node, device and resistance connectivity overrides (`exts_nodeConn`, `exts_deviceConn`, `exts_resistConn`);
  - resistance classes: types, sheet resistance, contact resistance, contact sizes;
  - capacitance rules: area and perimeter caps, overlap rules with shield masks, sidewall rules with offsets (float, Q-EXTRACT-20), sideoverlap rules, the completed coupling tables;
  - devices: the device table per type, an ordered list of `DevicePattern` (class, model, terminal type masks, substrate types and node, parameters with scales and offsets, parameter ranges, identifier layers);
  - substrate: the substrate declaration (types, shield types, node name with `$` variable);
  - other types: the non-electrical and active types;
  - antenna: per-type antenna rules (calculation model, surface or sidewall, ratios, `tiedown` types).

  It also provides `StyleView`'s four queries, the h, t and plane-order accessors M10's `HeightView` and `LayerStack` read, and `device_info() -> std::span<const DeviceInfo>`, ExtGetDevInfo's table (per device in ExtGetDevInfo's index order: model name, device tile type, source and drain resistance classes, substrate resistance class, default substrate node name after `$` substitution), which SPICE, SIM and ANTENNA read. `extract::StyleTable load_styles(const tech::RawSection&, const tech::Technology&, diag::DiagnosticSink&, const compat::CompatSettings&)`, `ExtStyle::rescale(n, d, compat)` (ExtTechScale) and `micron_convert(const mask::OutputStyle*)` (ExtTechFinal, with the cifoutput scale) are its builders (EXTTECH). `StyleView` and M10's `HeightView` keep their interfaces as views over it. `HeightView::rescaled(n, d)` is re-sourced to return the heights of the `ExtStyle` that `extract.current` holds after `ExtStyle::rescale`, so there is one grid-rescale path, not two (D7).

**Database access (Open Question 1)**
- `db::Plane::client(const TileView&) -> std::uintptr_t`, `set_client(const TileView&, std::uintptr_t)` and `reset_clients(const Rect&, std::uintptr_t)` (ExtResetTiles), with `db::kClientDefault`, Magic's CLIENTDEFAULT (INTEGRATION, MODEL review).
- `db::StitchEditor` (`db/plane_stitch.hpp`, a restricted header that only `extresist/` and `extract/` include, checked by `scripts/check_core_includes.py`): `split_x`, `join_y` and the walk helpers ResFracture needs, on a scratch plane that no cell owns (INTEGRATION, MODEL review).

**Extraction**
- `extract::RegionFinder` (`extract/node/regions.hpp`): ExtFindRegions over a cell's planes with a connectivity table and per-region callbacks (first, each). It yields `NodeRegion`, `DeviceRegion` and `LabelRegion` lists in Magic's order, with the reference tile, per-class area and perimeter, and attached labels (EXTNODE).
- `extract::ExtWriter` (`extract/file/writer.hpp`): one emitter per record of `extract-file-format` "Record grammar", Magic's number formats, the record order of "Record order", and the canonical `cap` order of Q-EXTRACT-7 (EXTNODE; called by EXTCAP, EXTDEV, EXTHIER, EXTCMD).
- `extract::CellResult extract_cell(ExtractContext&, db::CellDef&, ExtWriter&, bool is_top)`: ExtCell's pipeline of header, basic (nodes, caps, devices), subtree, arrays and length. `ExtractContext` holds the style pointer, options and yank buffers of one run, with no globals (EXTHIER; the pipeline steps belong to their workstreams).
- `extract::run(EditorState&, const RunRequest&, const mask::Progress*) -> cmd::CommandResult`: `extract`'s driver, the incremental check and the bottom-up stack (EXTCMD). `extract::ensure_extracted(EditorState&, std::string_view cell, const mask::Progress*) -> diag::Outcome<RunReport>` is the M9 entry (D18).

**Flattening**
- `extflat::read(cell::LookupContext&, std::string_view root, const ReadOptions&) -> diag::Outcome<Hierarchy>` and `extflat::flatten(const Hierarchy&, FlatFlags) -> Netlist`, generalising M6's `read_flat` (kept as a wrapper with `EF_FLATNODES | EF_NOFLATSUBCKT`). `FlatFlags` covers `EF_FLATNODES`, `EF_FLATCAPS`, `EF_FLATRESISTS`, `EF_NOFLATSUBCKT`, `EF_NONAMEMERGE`, `EF_WARNABSTRACT` and `EF_TRIM*` (EXTFLAT).
- `Netlist::visit_devices`, `visit_caps`, `visit_resists`, `visit_nodes` in Magic's visit orders, and `Hierarchy::visit_defs`, `visit_uses` (EFHierVisit*) for the hierarchical netlister (EXTFLAT).
- `extflat::CommonArgs parse_common_args(std::span<const std::string>, ...)` (EFArgs), `name_to_spice(const HierName&, Dialect, NameFlags&)` and `name_to_sim(...)` (EXTFLAT).

**Netlisters and extresist**
- `netlist::spice::run(EditorState&, const SpiceRun&) -> CommandResult`, and `netlist::spice::render(EditorState&, std::string_view cell, const SpiceSettings&) -> diag::Outcome<std::string>`, which leaves the session's settings untouched (SPICE; M9's entry, D18).
- `netlist::sim::run(EditorState&, const SimRun&)` and `netlist::extcheck::run(std::span<const std::string> args, std::ostream& out, std::ostream& err) -> int` (SIM).
- `extresist::run(EditorState&, const ResistRun&, const mask::Progress*) -> CommandResult` (RESCMD); `extresist::Network` (nodes, resistors, devices, breakpoints) with `build_network(const NetGeometry&, const ExtStyle&, ...)` and `reduce(Network&, const Tolerances&, HeapState&)` (RESNET); `write_res_ext`, `write_res_lump`, `write_fasthenry`, `geometry_elements` (RESCMD).
- `antenna::check(EditorState&, db::CellDef&, const AntennaOptions&) -> CommandResult` and `antenna::RuleView` (from `ExtStyle`), the rules `lef write` reads (ANTENNA).

**Commands and progress**
- Each command calls its entry point; the CLI, the shell and M9 call the same functions.
- Long runs report through `mask::ProgressHost`, as M5 and M6 reads do, and test the cancel flag at cell boundaries and in every per-tile loop that Magic guards with SigInterruptPending (ExtCell.c:515-531, ExtArray.c:295-385, ExtBasic.c:242-285).

### D6. Reusing the tile database and connectivity (Open Question 1)

Magic's extractor walks each cell's own planes. It keeps a region pointer in every tile's `ti_client`, and resets the pointers after each pass (ExtRegion.c:142-209 ExtFindRegions, :533 ExtResetTiles). Hefesto reuses `db::Plane` as it is (the tile order of Q-EXTRACT-5 is already exact, X-1) and adds only what is missing:
- **Client slot (Open Question 1, decided (a)).** `detail::Tile` gains one pointer-sized `client` field, the field Magic's `Tile` has (tiles/tile.h:65-78), initialised to `kClientDefault`. `Plane::client`, `set_client` and `reset_clients` reach it through `TileView::tile`. It is not part of the plane's value: `clone()`, `operator==`, undo and the `.mag` writer ignore it.
- **Ownership of the slot.** A run takes the cell's planes under the shared lock and owns their client fields for the run. The fields are run-scoped scratch: they are written only by the thread that holds the extraction run, and every pass resets them before it ends, as Magic does. A debug check verifies that the slots are all `kClientDefault` when the run releases the lock. Two extraction runs never overlap, because each holds the run token of `ExtractSession`. DRC, rendering and connectivity never read the slot.
- **Stitch edits.** extresist fractures a scratch copy of the net (ResFract.c:64-251) with TiSplitX and TiJoinY1. `db::StitchEditor` exposes `detail::TilePlane::split_x` and `join_y` on a plane that no cell owns (a yank buffer). The restricted header keeps every other caller on the paint API, which preserves the maximal-strip invariant.
- **Connectivity.** antennacheck and extresist yank nets with `connect::search_hierarchical` into scratch cells (DBTreeCopyConnect), and the extractor's interaction yanks use `db::flatten_to_new` and the `UseIndex` order (ExtYank.c). `connect/` keeps its area-keyed visited set. Changing it to the client slot is a performance follow-up, not M7 scope.

### D7. The extraction style (EXTTECH; `extract-tech-section`; Open Question 2)

- **One reader.** EXTTECH completes the reader M6 and M10 started (`detail::read_style` in `style_reader.cpp`), in its home `extract/tech/` with its tags. Each handler group lives in its own source file:
  - `cap_rules.cpp`: areacap, perimc, overlap, sidewall, sideoverlap, the defaults, fringeshieldhalo;
  - `resist_rules.cpp`: resist, contact;
  - `device_rules.cpp`: device and fet, with parameters and ranges;
  - `antenna_rules.cpp`: antenna, model, tiedown;
  - `conversion.cpp`: micron conversion and rescale.

  M10's TECH keeps the style framework and its keywords (`style`, `variants`, `scale`, `units`, `planeorder`, `noplaneordering`, `height`).
- **Parallel start (Open Question 2 (a)).** M10's TECH is editing the pass of `read_style` and its keyword switch now, so EXTTECH does not restructure that file while TECH is open:
  - *Before TECH merges:* EXTTECH writes and unit-tests its handlers in the new files above, against section fixtures. It wires each one with one `case` line in the existing switch of `StyleReader::apply`, which dispatches on the keys of `kKeywords` (`style_reader.cpp:38`), moving the keyword from "accepted silently" to its handler. These are additive one-line edits; whichever of EXTTECH and TECH merges second rebases them.
  - *After TECH merges:* task 4.1 turns `kKeywords` into a dispatch table of `{keyword, min, max, usage, handler}` and removes the switch. This refactor touches the pass TECH wrote, so it never runs while TECH is open.
  - M10's `HeightView` becomes a view over `ExtStyle` (task 4.7), and `LayerStack` and `build_layout_stack` do not change.
- **Load and rescale.** No hook exists today: M6's `StyleView` and M10's `HeightView` re-read `Technology::raw_sections()` on each call, and the tech loader hands nothing off. M7 adds the hook on the editor side, so that `tech/` does not learn about `EditorState`: after a technology loads into the session, and after `scalegrid` and finer-grid reads, the editor calls EXTTECH's builder (`load_styles`, then `select`, `micron_convert` and `rescale`) and stores the result in `extract.styles` and `extract.current` (D5, D20). `extract style <name>` parses the named style (ExtLoadStyle). Micron conversion runs with the current cifoutput scale (ExtTechFinal, ExtTech.c:3962-3966; Q-EXTRACT-3 per mode). `scalegrid` and finer-grid reads rescale the current style and the session overrides (ExtTechScale, ExtTech.c:4046-4093; Q-EXTRACT-19, Q-EXTRACT-25). `HeightView::rescaled` is re-sourced from that result, so h and t have one rescale path.
- **Decisions applied.** Q-EXTRACT-1 (shield direction by plane index ON, plane order OFF), Q-EXTRACT-2 (diode OTHER is terminal and substrate types), Q-EXTRACT-3 (gscap scaling per mode), Q-EXTRACT-19 (step and halo truncation ON, rounding OFF), Q-EXTRACT-21 (limits /1000 ON, milli-units OFF), and three antenna fixes that apply in both modes (X-2 tier 1): Q-MISC-2 (the antenna model flags start empty for every style and on reload), Q-MISC-3 (an antenna line without a calculation type stores "none"), and Q-MISC-5 (an `antenna` line with a calculation type and a single ratio reports an error and is ignored). Each has a regression test (task 4.5).
- **No new diagnostics on PDKs.** sky130A, gf180mcuD and ihp-sg13g2 load with exactly the messages Magic prints (the extract-section lines of the TECH golden cases and a load test per PDK).

### D8. Nodes, labels and the `.ext` writer (EXTNODE; `extract-nodes-and-labels`, `extract-file-format`)

- **Regions.** `RegionFinder` ports ExtFindRegions and the region callbacks of ExtBasic.c and ExtRegion.c over the client slot, in the plane order of the style and the tile order of the plane. It handles split tiles as `extract-nodes-and-labels` "Node connectivity" defines, with the halo across planes.
- **Names and labels.** The node reference point and generated name (`metal1_n10_n6#`) and label attachment by type and trailing character. Name selection follows Q-EXTRACT-4: Magic's list order with magic-compat ON, the first attached name with it OFF, one branch in `node/names.cpp`. Ports, attributes and aliases follow.
- **Unique naming.** ExtUnique.c's renaming (Q-EXTRACT-23: the later node keeps the name, an earlier port wins), the duplicate-label warning, and the temporary `_uqN` labels reverted in every renamed cell, the top cell included (Q-EXTRACT-6, fix). The command forms are EXTCMD's.
- **Writer.** `ExtWriter` emits every record of "Record grammar" with Magic's spacing and number formats:
  - the header: timestamp, version `8.3` (Q-EXTRACT-10), tech, style, scale, resistance classes;
  - the record order of "Record order";
  - `cap` records sorted by terminal names, smaller name first (Q-EXTRACT-7, `hefesto-output-determinism` "Deterministic order where Magic's order is arbitrary");
  - `merge` groups in Magic's hash order (`util::MagicHashTable`).

  The writer's tests read every emitted file back with EXTFLAT's reader ("Reader constraints honored by the writer"), and the spec's annotated example is a round-trip fixture ("Annotated example", informative).

### D9. Capacitance and lumped resistance (EXTCAP; Open Question 3)

- **Substrate and overlap.** Area and perimeter caps, with the diagonal-edge perimeter correction of split tiles (`(int)sqrt(w² + h²)`, ExtPerim.c:105-117), then overlap coupling with shield masks in plane order (ExtBasic.c, ExtCouple.c).
- **Sidewall.** Edge enumeration, nearest-neighbour shadowing and the coupling value, with the single-precision denominator (Q-EXTRACT-20, replicate).
- **Sideoverlap.** The legacy model (Q-EXTRACT-18 per mode) and the fringe-halo model (Q-EXTRACT-17 per mode). The fringe fractions are `0.6366 · atan(mult · d)` (ExtCouple.c:718-737, 1298, 1492-1508, 1874-1875). The fringe-halo model is on by default for tech format 34 and above ("Fringe model default chosen from tech format"), and the three PDK tech files are format 35, so it runs on every PDK cell. The `atan` must give Magic's bits on every platform (Open Question 3).
- **Corners and substrate coupling.** Corner fringe to substrate and corner coupling (Q-EXTRACT-24 per mode), coupling to the substrate node, and coupling record output (Q-EXTRACT-7).
- **Lumped resistance.** Per-class area and perimeter, the L/W formula (`S = sqrt(P² − 16A)`, ExtBasic.c:585-645, the `sqrt` at :641), rounding, the record values and the class header. The shorted-ports warning follows Q-EXTRACT-22 per mode.
- **One branch per decision.** Each compat split is a named function in `cap/compat_branches.cpp` taking the compat setting, with a unit test per mode.

### D10. Devices and substrate (EXTDEV)

- **Devices.** Device regions and subcell priority, then terminal discovery on and off the device plane, with terminal area, perimeter and sharing.
  - Record selection by identifier layers and substrate variants, and model selection by parameter ranges (Q-EXTRACT-21 per mode). Missing terminals copy or reject as Magic does.
  - MOSFET-family length and width from boundary lists, with the effective length and width of non-rectangular gates (extComputeEffectiveLW, ExtBasic.c:1259-1579; its only `sqrt` is the diagonal-segment width at :1535). Two other dimension estimates use `sqrt` and get their own pinned values: the width of a terminal-indexed `w<n>` parameter (ExtBasic.c:1916-1920) and the equivalent-square fallback of capacitors when extComputeCapLW fails (:2948, :3206).
  - Resistors and capacitors, with the zero-size capacitor written once (Q-EXTRACT-8, fix). Diodes compute L and W before range matching (Q-EXTRACT-13, fix).
  - Parameter values, with `r` written as 0 for zero width or area (Q-EXTRACT-15, fix); terminal output and ordering, `parameters` records, and the `device` cell property passed through in written order (Q-EXTRACT-16).
- **Substrate.** The backward-compatible mode, isolated substrate regions, global substrate node discovery and naming (`$SUB` from the project setting, Q-EXTRACT-9), the `substrate` record, device substrate terminals, subcell substrate connections, and substrate coupling relocation.

### D11. Hierarchy (EXTHIER)

- **Order and interactions.** `extract_cell` runs bottom-up (ExtMain.c extExtractStack). For each cell it builds interaction areas from the halo and step squares (ExtSubtree.c:164 on, ExtInter.c), yanks them into the cumulative and interaction buffers (ExtYank.c), and runs pass 1 (connections) and pass 2 (merge adjustments).
- **Names and records.** Name unification order, `merge` records with the pass-2 adjustments, hierarchical `cap` records, hard-way node names (ExtHard.c), array interactions (ExtArray.c), and sticky labels unified in the cumulative def (Q-EXTRACT-26, replicate).
- **Buffers.** Yank buffers are scratch `db::CellDef`s owned by the `ExtractContext`, never registered in the cell table. Their planes use the client slot like any other.
- **Section order.** The file's hierarchical sections follow "Hierarchical section order in the file".

### D12. Commands, incremental extraction, path length and locking (EXTCMD; Q-EXTRACT-27)

- **Options.** Every `extract` form and option with Magic's matching, defaults and warning set: `all`, `cell`, `parents`, `showparents`, `do`/`no`, `warn`, `style`, `halo`, `step`, `path`, `unique`, `length`, `help`. Cell properties that affect extraction (`noextract`, `LEFview`/abstract, `device`) are honoured.
- **Incremental extraction.** A cell is skipped when its `.ext` header timestamp equals `cd_timestamp` (ExtMain.c:968-990). Parents of re-extracted cells are re-extracted. Output paths follow Q-EXTRACT-11: Magic's behaviour on desktop hosts, the project folder on sandboxed hosts.
- **Atomic output (Q-EXTRACT-27, fix, approved 2026-11-21).** Magic opens `<cell>.ext` for writing, writes the header first and stops writing on interrupt (ExtCell.c:99-116, 515-531). The truncated file keeps a valid `timestamp` line, so the next `extract` treats it as current (ExtMain.c:295-296, 984-986), and the netlisters read a partial netlist. extresist has the same flaw: it stops at the next node on interrupt (ResRex.c:1395) and leaves a truncated `.res.ext` (and `.res.lump`, `.fh`) that is newer than the `.ext`, so D18's `rc` staleness rule trusts it and `ext2spice extresist on` reads it silently. One mechanism, `extract::AtomicOutput` in `extract/file/` (EXTNODE; RESCMD uses it), covers all four files:
  - the target path is resolved first exactly as ExtFileOpen does (ExtCell.c:153 on: `ExtLocalPath`, then the directory of `cd_file`, then the retry with the path stripped), and the temporary file is created in the directory of that resolved path;
  - when the target is a symbolic link, Magic writes through it. Hefesto resolves the link and renames over the link's target, so the link survives as it does in Magic;
  - if no temporary file can be created in that directory (a writable file in a read-only directory), Hefesto writes in place as Magic does;
  - the rename happens after the cell's (or net file's) last record; on cancel or error the temporary file is removed and the previous file is unchanged.

  Uninterrupted runs give the same bytes, names and console output as Magic.
- **Path length.** The flood of ExtLength.c, ported exactly (Q-EXTRACT-12), so `distance` records are byte-identical.
- **Locking (three phases, as M5's `gds write`).**
  - *Prepare*, under the exclusive lock: load unread subcells, compute the bottom-up stack and the incremental set, and apply the temporary unique labels.
  - *Extract*, under the shared lock: `extract_cell` for each cell, writing files.
  - *Finish*, under the exclusive lock: revert unique labels (Q-EXTRACT-6), update flags and timestamps, and print the summary.

  Rendering continues during the extract phase; edits wait, as for every M5 write.
- **Progress and summary.** "Extracting <cell> into <file>:" and the error and warning totals as Magic prints them ("Progress and summary messages", informative).

### D13. Flattening (EXTFLAT)

EXTFLAT extends M6's code; nothing is rewritten. M6 ported only the standard-cell node path, efFlatNodesStdCell (`EF_NOFLATSUBCKT`; `extflat/flatten.cpp:143`), which is all DEF write uses. ext2spice and ext2sim flatten with `EF_FLATNODES` and no `EF_NOFLATSUBCKT` (ext2spice.c:1066, 1312; ext2sim.c:674, 806), and antennacheck adds `EF_WARNABSTRACT` (EFantenna.c:234). They all go through efFlatNodes (EFflat.c:141-150, 320-345). So M6's "Flat node construction" coverage is only partial and is reopened here. EXTFLAT adds:
- **efFlatNodes.** The non-standard-cell path: recursion through every use, subcircuits included; `FLATNODE_DOWARN` with the spec's flat-mode warning for names that do not resolve; `FLATNODE_NOABSTRACT` with "Error:  Cell <name> was extracted as an abstract view."; and `FLATNODE_HIER`/`FLATNODE_CHILD`, which stop child capacitance from being counted twice. `flatten(…, FlatFlags)` picks the path as EFFlatBuild does; `read_flat` keeps `EF_FLATNODES | EF_NOFLATSUBCKT`, so DEF write is unchanged.
- **Read side.** The full device records (every class of NL-FLAT-04, parameters and attributes), `parameters` records with scale and offset (Q-NETLIST-8 per mode, reversed order per Q-NETLIST-9), the `resist` value offset (Q-NETLIST-12 per mode), and the `.res.ext` overlay ("Resistance-extraction overlay").
- **Flattening.** Coupling capacitance and resistor flattening with the thresholds, lumped node resistance, device length and width, the device, cap and resistor visit orders, and one-level flattening for the hierarchical netlister (EFhier.c).
- **Common arguments.** EFArgs (`-c -C -r -R -p -s -S -t -T -v -o`), shared by ext2spice, ext2sim, extcheck and antennacheck. `-o` applies to one run (Q-NETLIST-42, fix). As landed (R2 fix pass): `parse_common_args(args, extra, ArgOptions)`; `-o` (ext2spice, ext2sim) and `-T` (standalone `hefesto-extcheck`) are opt-in through `ArgOptions`, so extcheck answers `-o` and the embedded netlisters answer `-T` as Magic does; `-h` is passed to `extra` before the usage text (EFargs.c case 'h'); `-s` and `-S` (read through `ArgOptions::read_file`) build `CommonArgs::symbols` in argument order, and every netlister passes the symbols, the run's capacitance threshold and `gnd` to `flatten` in `FlattenOptions`. The flattener counts instances and merge elements per traversal (`max_instances` bounds one visit of the hierarchy, not the sum of the six visits of a full flatten).
- **Name conversion.** SPICE and sim name conversion: Q-NETLIST-30 per mode (`!`/`#` anywhere ON, trailing OFF; the leading-comma guard in both), Q-NETLIST-16 per mode (session-wide flags ON, per-run OFF), and Q-NETLIST-10 (lexically greater name wins a full tie).
- **M6 leftovers.** The visit-order comparison with Magic's probe lines (m6 task 9.2), if M6 has not closed it.
- **Caps.** The `ReadLimits` caps keep their M6 status; whatever the user decides for them in M6 task 17.3 applies unchanged.

### D14. SPICE netlister (SPICE)

- **Structure.** The ext2spice.c and ext2hier.c port, flat and hierarchical, over EXTFLAT's visitors. The five dialects differ in node names, the header, the scale card and SI formatting (`esSIvalue`).
- **Decisions applied per mode.** Q-NETLIST-4, 5, 7, 13, 19, 20, 31, 32, 34, 35, 37 and 41. Q-NETLIST-6 (multipliers keyed by device, 1-terminal devices get M = 1) and Q-NETLIST-42 are fixed in both modes.
- **Flat coupling order.** Flat coupling capacitors follow the deterministic order of Q-NETLIST-1. GOLDEN's `spice-cap` comparator handles Magic's address order (D21).
- **The `lvs` preset.** The `lvs` preset and the settings `simulation-netlist-preparation` overrides are a `SpiceSettings` value. `render` takes one by value and never writes back to `netlist.spice` (D18).

### D15. ext2sim and extcheck (SIM; Open Question 6)

- **ext2sim.** The ext2sim.c port: `.sim`, `.al` and `.nodes` in MIT, LBL and SU formats.
  - Decisions per mode: Q-NETLIST-15 (`C <name>0` ON, `C <name> GND` OFF), Q-NETLIST-16, Q-NETLIST-17 and Q-NETLIST-18. Q-NETLIST-14 is fixed in both modes.
  - The BJT L/W `sqrt` follows ext2sim.c:1233.
  - sim2simp is documented as not provided (Q-NETLIST-48).
- **extcheck.** The library part is `netlist::extcheck::run(args, out, err, program) -> int`, which returns the exit status and prints the counts in Magic's format with the defaults of Q-NETLIST-36. It parses the arguments as Magic's standalone build does (EFArgs without `MAGIC_WRAPPER`: `-T` accepted, the usage text ending `[-T techname] rootfile`, a missing root or a bad flag printing the usage text with exit status 1), and reads with no technology, so a `scale` record keeps the file's own unit (Q-NETLIST-50, fix in both modes, approved 2026-11-22). Open Question 6 decides how users run it. GOLDEN builds Magic's `extcheck` in the capture image (`make -C extcheck` after the main build, at the same pin). Its Makefile links `extflat/libextflat.o` and `utils/libutils.a` only, but in a Tcl build `libextflat.o` contains EFantenna.c, which references `ExtCurStyle` (EFantenna.c:184, 440-466) and Tcl symbols, so the link may fail. Task 2.2 tries the Tcl build's objects first, then a separate non-Tcl configure of the same pin for `extcheck` alone; if neither links, design R7 applies. Neither links at fc5daa9e (golden-capture runs 44961645329, 44964258826: 152 and 147 missing symbols), and the user chose R7 option (a) on 2026-11-22 (M7-9): NL-CHK-01's expected output is hand-derived (D21).
- **As landed (R2, SIM).** `ext2sim` and `extcheck` read the fields EXTFLAT adds to extflat's records and `ReadOptions` in the same round under `if constexpr (requires ...)` guards, so each workstream's branch builds alone; INTEGRATION removes the guards once both are on main. `extcheck` reads the tree twice: extflat takes the global names as a list of leaves (the Tcl rule of ext-flattening-names "Global and ground detection"), while Magic's standalone extcheck is a non-Tcl build whose EFHNIsGlob makes every leaf ending in `!` global, so the first read collects those leaves and the second uses them. A failed read prints `<argv[0]>: <code> <text>` and exits with status 1, where Magic goes on to EFFlatBuild with no root definition and dereferences it. `ext2sim` uses the style's device table only when the `.ext` files name the current style (EFStyle, ext2sim.c:657) and passes extGetDevType and DBNumTypes to the read for the device visit order.
- **The NL-CHK-01 case.** It runs a standalone program, not `commands.tcl`. The harness gains an external-command case mode (task 2.3): `input/command.toml` names the program by its role (`extcheck`), its arguments and the files it reads; the harness maps the role to `extcheck` when capturing and `hefesto-extcheck` when replaying, and captures stdout, stderr and the exit status. Magic's usage text does not name the program (extflat/EFargs.c:158-165 starts with `Standard arguments:`), and `hefesto-extcheck` prints the same text, so the usage lines compare byte for byte with no mask. The `prog-name` mask therefore acts on Hefesto's side only: it replaces a leading `hefesto-extcheck` or `extcheck` (with or without a directory, followed by `:`, a blank or the end of the line) with `<prog>`, which is the prefix `hefesto-extcheck` puts before a diagnostic of its own (`<argv[0]>: <code> <text>`, for example a read failure); on Magic's side, and on every line of NL-CHK-01's expected output, no line starts with a program name, so it changes nothing there. The `extcheck-memory` mask replaces the value of the informative `Memory used:` line with `<masked>` on both sides. Everything else is compared byte for byte.

### D16. extresist (RESNET, RESCMD)

- **RESNET.** For each net that RESCMD selects, RESNET builds the network:
  - it yanks the net's geometry (`connect::search_hierarchical` into a scratch cell, minus the `skip` types) and fractures it with the stitch editor (ResFracture);
  - it annotates devices, chooses the start tile and origin, places breakpoints and processes tiles in ResMain's order;
  - it builds tile resistors, cuts near devices (Q-NETLIST-43 per mode) and adds contact resistors;
  - it merges nodes and applies the construction-time reductions (Q-NETLIST-44 per mode; Q-NETLIST-45 resistor-list order in both modes);
  - it distributes path resistance and capacitance (Q-NETLIST-24 replicate; Q-NETLIST-25 and 46 per mode), prunes by Elmore delay, simplifies, and runs the final scrunch with a stable sort (Q-NETLIST-26, Q-NETLIST-47 guards).

  The region callbacks reuse EXTNODE's `RegionFinder` on the scratch cell.
- **Split tiles (task 13.1; decided 2026-11-22).** Q-NETLIST-53 (compat): with `magic-compat` ON ResFracture makes a split tile Manhattan with `set_type` (its left type, as ResSplitX assigns) and then splits it; with it OFF a split tile is never split. Q-EXTRACT-29 (compat): `db::StitchEditor::split_x` copies Magic's TiSplitX on a split tile with `magic-compat` ON and refuses with it OFF; as landed in R1 it refuses in both modes, and the ON branch lands with task 13.1 under the MODEL review. Task 13.1 adds a test per mode for each.
- **RESCMD.**
  - The command and its options (Q-NETLIST-23 comma lists, fixed in both modes; Q-NETLIST-28 `mindelay` in ps).
  - The run flow, and the input model from the `.ext` file. ResReadExt keeps its own record handling over EXTFLAT's tokenizer, with Q-NETLIST-29 per mode and its guards in both.
  - The first cutoff, the second cutoff after extraction, and the lumped file.
  - The `.res.ext` overlay grammar and order (Q-NETLIST-39 per mode), node naming, FastHenry output (Q-NETLIST-40 per mode; Q-NETLIST-27 and Q-NETLIST-21 fixed) and the geometry centerlines.
  - `ext2spice extresist on` and `ext2sim extresist on` consume the overlay through EXTFLAT ("Consumption by netlisters").
  - `.res.ext`, `.res.lump` and `.fh` are written through `extract::AtomicOutput` (Q-EXTRACT-27 as extended, D12), so a cancelled extresist run leaves the previous files and no truncated file that D18's `rc` staleness rule would trust.
- **Equal-value and high-fanout networks.** Q-NETLIST-45 (triangle search in resistor-list order for nodes with 10 or more resistors) and Q-NETLIST-26 (stable sort in simplification) change which triangle is reduced or which equal resistor is folded first. That changes the network's topology and values, not only the order of records, and Magic's own result can differ between runs and platforms. No comparator can absorb that, so the golden inputs that exercise them are re-authored to have one outcome (D21, task 17.3).

### D17. Antenna check (ANTENNA)

`antennacheck` follows EFantenna.c:
- it requires a plane order (EFantenna.c:184-189), reads and flattens with `EF_FLATNODES | EF_WARNABSTRACT`, and builds the device-type table from the style (:227-252);
- it visits each gate net once and yanks it with `connect::search_hierarchical`, cumulatively and per plane. Plane-order indices with no plane are skipped instead of reusing the previous or an uninitialised check plane (Q-MISC-6, fix in both modes, X-2 tier 1; a regression test in task 15.3);
- it measures surface and sidewall areas, counts contact cuts (Q-MISC-4 per mode, with the no-rule and zero-pitch guards in both), applies the connectivity restriction (Q-MISC-7 per mode) and the cumulative model (Q-MISC-1 per mode);
- it reports violations as feedback with Magic's text, and prints progress every 100 gates.

`antenna::RuleView` exposes the per-type rules and the tie-down set. `lef write` reads ANTENNAGATEAREA and ANTENNADIFFAREA inputs through it ("Shared antenna definitions for LEF output"), replacing M6's StyleView query (D20).

### D18. How M9 and M10 consume the output

- **LVS** (`lvs-command` "Layout netlist from extraction"): `extract::ensure_extracted` brings the hierarchy up to date as `extract` (incremental, ExtIncremental with extDefListFuncIncremental, ExtMain.c:284-299, 918-945) does, re-extracting only stale or missing cells and their parents. It does not behave like `extract all`, which ignores timestamps (ExtAll with extDefListFunc, ExtMain.c:360-384). Then `netlist::spice::render` with the `lvs` preset produces the layout netlist in memory. M9 never parses Hefesto's console output, and M7's golden cases fix the text it reads.
- **Simulation** (`simulation-netlist-preparation`): the source `layout <cell>` calls `ensure_extracted` (stale or missing `.ext` re-extracted, "Stale extraction refreshed"), then `render` with the simulation settings as a value, which leaves the user's `ext2spice` settings untouched. Mode `rc` calls `extresist::run` for cells whose `.res.ext` is missing or older than the `.ext`, then renders with `extresist on`.
- **The `.ext` files** stay the interchange format: `lvs-netlist-input` reads `.ext` as netgen does, and Magic's own tools read Hefesto's files (the EX goldens compare them byte for byte after `ext-cap`).
- **M10 P3 overlays** read node capacitances and couplings through `extflat::flatten` with `EF_FLATCAPS`, and resistance networks from `.res.ext` through the overlay reader (m10 task 15.3). No 3D-specific API is added in M7.
- **Stability.** The entry points of D5 are public headers. A change after M7 exit is an M9 or M10 open item.

### D19. Desktop (SHELL; Open Question 7)

With Open Question 7 (a):
- the top bar's "Extract" button (painted and disabled since M1) runs `extract all` on the root of the active window, with the M5 progress panel and cancel. Errors and warnings go to the console and feedback, as for DRC.
- `ext2spice`, `ext2sim`, `extresist` and `antennacheck` are reachable from the console.
- No board is added. `docs/design/m7-ui-conformance.md` records the button's states (enabled with a technology that has an extract style, disabled with a tooltip otherwise).

### D20. Changes to M1–M6 code

| Change | Files | Owner |
|---|---|---|
| tile client slot; restricted stitch header | `db/plane.hpp`, `src/db/tile_plane.{hpp,cpp}`, new `db/plane_stitch.hpp` | INTEGRATION (MODEL review; D6) |
| `EditorState::extract`, `netlist`, `extresist` | `editor/state.hpp` and the session headers | INTEGRATION (D5) |
| extract style built when a technology loads into the session, rescaled on `scalegrid` and finer-grid reads (a new hook; none exists, and M10 added none) | `editor/extract_hook.{hpp,cpp}` (`install_extract_styles`, `rescale_extract_style`), calling EXTTECH's builder; `tech/` gains one call line and one include in each of two files: `tech/commands.cpp` (`install_technology` calls `install_extract_styles`) and `tech/scale_layout.cpp` (`scale_layout` calls `rescale_extract_style`), the only places a technology is installed and the grid rescaled (amended 2026-11-22, M7-13) | INTEGRATION places the call (R1, stubbed); EXTTECH fills the builder (task 4.6) |
| `StyleView` re-sourced from `ExtStyle` | `extract/tech/style_view.{hpp,cpp}` | EXTTECH |
| `HeightView` re-sourced from `ExtStyle`; `rescaled` derived from `extract.current` | `extract/tech/height_view.{hpp,cpp}` (M10's file) | EXTTECH, after M10's TECH merges; M10's `build_layout_stack` tests must pass unchanged |
| `lef write` and `def write` use the selected extract style (Magic's writers read `ExtCurStyle`, lefWrite.c:1268, defWrite.c:816, 2538), not the default one that `StyleView::from_technology` returns | `lef/write/write.cpp:79`, `def/write/write.cpp:118`: build the view from `EditorState::extract.current` (passed in by the `lef write` and `def write` commands) | EXTTECH (with LEFW's and DEFW's files as open items); a test selects a non-default style before `lef write` and `def write` |
| install rule and packaging of `hefesto-extcheck` (desktop hosts only) | `src/cli/extcheck/CMakeLists.txt`, the root install rules, the desktop packaging scripts | SIM writes the target; INTEGRATION the install and packaging |
| `lef write` antenna areas through `antenna::RuleView` | `lef/write/macro.cpp` | ANTENNA (LEFW's file; the LEFDEF antenna cases must still pass) |
| `read_flat` kept as a wrapper of `read` + `flatten` | `extflat/read.hpp` | EXTFLAT (DEFW's call unchanged) |
| command registrations; UI-G-005 deviation and help | `cmd/`, `ui/magic_help.cpp`, `tests/golden/deviations.toml` | INTEGRATION, GOLDEN |
| core-include check for `plane_stitch.hpp` | `scripts/check_core_includes.py` | INTEGRATION |
| Extract button | `src/shell/desktop/src/` top bar | SHELL |

### D21. Golden cases (GOLDEN; Open Questions 4 and 9)

- **Catalogues and counts.**

  | Family | Rows | Input | Captured outputs |
  |---|---|---|---|
  | EXTRACT EX-001..070 | 70 | `exttest.tech` and a layout per case, `commands.tcl` | every `.ext` file, stdout, layout labels where the case says |
  | NETLIST NL-FLAT, NL-SPICE, NL-HIER, NL-SIM, NL-CHK | 44 | hand-written `.ext` files and a minimal tech | `.spice`, `.sim`, `.al`, `.nodes`, Tcl results, stdout |
  | NETLIST NL-RES, NL-RESNET | 23 | layouts (extracted in the case) or hand-written `.ext`/`.res.ext` | `.res.ext`, `.res.lump`, `.fh`, stdout |
  | MISC-ANT-01..09 | 9 | synthetic tech with antenna rules | feedback count, debug output, LEF (MISC-ANT-09) |
  | `EXTRACT-PDK-<lib>` (Open Question 4 (a)) | 3 | every cell of `sky130_fd_sc_hd` (446), `gf180mcu_fd_sc_mcu7t5v0` (229), `sg13g2_stdcell` (84, read from the `ihp-gds-sc` set) | one `.ext` per cell |
  | `NETLIST-PDK-<lib>` (Open Question 4 (a)) | 3 | the `.ext` files of the matching EXTRACT-PDK case | `ext2spice lvs` (flat) and `ext2spice hierarchy on format ngspice` per cell, `ext2sim` of the whole library |
  | SCRIPT-API-16, SCRIPT-API-20 | 2 | existing cases | their deferred outputs |
  | LEFDEF-336 (Open Question 9 (a)) | 1 | LEFDEF-317's layout: `extract all`, then `def write` | the DEF file |

  The total is 155 cases. GOLDEN also audits the catalogues against the 204 exact and behavioural requirements. Each requirement whose behaviour Magic shows gets a case or a pinned unit test. Missing rows are added as EX-071 on and NL-<family>-<n+1> (task 17.2).
- **The flat EX subset (captured in R1, task 2.6).** The 53 EX rows whose layout has no subcell and that need no command driver beyond `extract all` on one cell: EX-001..042, EX-044..046, EX-048 and EX-057..063. The other 17 (EX-043, EX-047, EX-049..056, EX-064..070) need hierarchy or the command options and are authored and captured from R2 on (task 17.1). All EX cases replay from R3, through EXTCMD's driver.
- **Size budget.** The PDK cases add 759 × (one `.ext`, two SPICE files) plus one `.sim` per library. A standard cell's `.ext` is a few kB and each netlist about 1 kB, so the estimate is 10–15 MB, against about 46 MB in `tests/golden` today. `expected/` is committed uncompressed, so diffs stay reviewable. The budget is 15 MB for the six PDK cases; if a capture exceeds it, GOLDEN raises an open item (a smaller cell set, or gzip-compressed expected files with a decompressing comparator) before committing.
- **Authoring.**
  - `tools/golden/authoring/{extract_cases,netlist_cases,antenna_cases,ext_pdk_cases}.py` build the cases, and `m7_cases.py` drives them (`--list`, `--not-authored`, `--exit-check`).
  - Cases are `input/commands.tcl` files that both Magic and `hefesto-cli --magic` run, with `exttest.tech` committed once under `tools/golden/authoring/`. NL-CHK-01 is the exception: it uses the external-command case mode of D15 (`input/command.toml`, the program mapped from `extcheck` to `hefesto-extcheck`, stdout, stderr and exit status captured; `prog-name` masks the program name a line of Hefesto's own diagnostics starts with, which no Magic line and no line of the expected output has, D15). Its expected output is hand-derived (R7 option (a), approved 2026-11-22, M7-9): `authoring/netlist_cases.py` writes the four runs' `.stdout`, `.stderr` and `.exit` files with every line traced to a line of extcheck/extcheck.c, extflat/EFargs.c or the flattener, the case is `hand_derived` instead of `capture_blocked`, naming the decision in `case.toml` and `expected/provenance.json` (`reference_captured = false`), and `m7_cases.py --exit-check` lists it with the decision. `tests/golden/deviations.toml` held it as a deferral naming Q-NETLIST-50 until SIM (task 12.4) and EXTFLAT made it replay; it then left the file, because replay fails a listed case whose outputs all match (goldenlib/deviations.py).
  - The synthetic technology additions of R1 are accepted (approved 2026-11-22, M7-11): `exttest.tech` has the types `ndc`, `ndiffh` and `thkox`, the last on a seventh plane `oxide`, and its CIF output style uses a 0.5 um scale.
  - The PDK cases run with `--trust-pdk` and the PDK's default extract style.
- **Session-state pinning.** D18 of M5 applies: a fresh session per case (Q-NETLIST-16 and Q-NETLIST-46 depend on it), `drc off`, `TZ=UTC`, `LC_ALL=C`, and `random seed 1` for PDK cases.
- **Capture.** `golden-capture.yml` gains the outputs above, collected by name pattern (`*.ext`, `*.spice`, `*.sim`, `*.al`, `*.nodes`, `*.res.ext`, `*.res.lump`, `*.fh`). Every EX case collects `*.ext` and every NL case `*.spice`, `*.cdl`, `*.sim`, `*.al` and `*.nodes` (`[[collect]]`); none lists those files as fixed outputs, so a file only one side writes fails the case. A multi-run case renames each run's files to `<cell>.<tag>.<ext>`, which the same patterns collect. Other files a case compares (EX-024's saved layouts) stay named outputs. The image builds Magic's `extcheck` (D15). The NETLIST cases need no Hefesto extraction, so they are captured in R1, and SPICE and SIM develop against them from the first day.
- **Comparators.**
  - `ext-cap` (exists): `.ext` files with canonical `cap` records, every other line byte for byte (Q-EXTRACT-7).
  - `spice-cap` (new): flat coupling-capacitor lines, the `C` lines whose two nodes are both non-ground, are compared as a multiset. Terminals are swapped so the smaller name comes first, and the device index is masked. Every other line, the order of device and node lines included, is compared byte for byte (Q-NETLIST-1). `spice-cap:<node>[,<node>...]` names the substrate nodes a netlist prints its node capacitances to (`C1 A VSUB 3f`, and `c2/VSUB` in a hierarchical parent); they count as ground, so those lines stay byte for byte (NL-FLAT-14, NL-HIER-06). This is how the address-ordered `cap` lines are compared when the substrate is named, as landed in R1 (approved 2026-11-22, M7-10).
  - `sim-cap` (new): the same rule for `.sim` coupling lines.
  - A capacitor device without a model (`device devcap None`) prints with the coupling shape (`C<n> top bottom value`, ext2spice.c:3195-3217), so neither canonicalizer can tell it apart. A NETLIST case therefore uses `spice-cap` and `sim-cap` only when one of its `.ext` inputs has a `cap` record, and `bytes` otherwise (38 of the 44 rows).
  - `res-ext` (new): the resistor records within one net of a `.res.ext` are compared in canonical order (by terminal names, then value), which absorbs a different record order for the same network. It does not absorb Q-NETLIST-45 or Q-NETLIST-26: those change which triangle is reduced or which equal resistor is folded first, so the network's topology and values differ, and Magic's capture may differ between runs (D16). The cases that exercise them, NL-RESNET-14 (≥ 10 equal-value sub-tolerance resistors) and NL-RESNET-15 (a high-fanout node inside a triangle, run twice), are re-authored so that the outcome is unique: one triangle per high-fanout node and distinct resistor values, so neither the search order nor the sort order can change the result. If a requirement can only be shown on an ambiguous input, the case is recorded as a deviation under the decision and checked for electrical equivalence (the same resistance between every pair of ports, within 1e-9 relative) instead of text. Node, device and attribute records are compared byte for byte.
  - `magic-console` (exists) for stdout, with the timing lines of ExtTimes masked.
  - `extcheck-console` (new in R2, NL-CHK-01): an external program's stdout, stderr or exit status. It applies `prog-name` and `extcheck-memory` to both sides and then compares the bytes; the module docstring documents the canonicalization.
  - PDK cases: `ext-cap` on every `.ext`, `spice-cap` on every netlist. A cell that differs fails the case and is named in the report.
- **DEF write cases (Open Question 9 (a)).** LEFDEF-317..328 and 330..334 keep their Magic-written `.ext` inputs. One new end-to-end case, LEFDEF-336, runs `extract all` then `def write` on LEFDEF-317's layout, so Hefesto's extractor feeds Hefesto's DEF writer once.
- **Deviations.** These need an approved decision:
  - the both-modes fixes whose output Magic can produce in a case: Q-EXTRACT-6 (EX-024's layout labels after the run), Q-EXTRACT-8, Q-EXTRACT-13, Q-EXTRACT-15, Q-NETLIST-6, Q-NETLIST-23 and Q-NETLIST-42. Each case that exercises one records it in `deviations.toml` with the decision;
  - Q-NETLIST-14 (ext2sim's unterminated merge table), Q-NETLIST-21 and Q-NETLIST-27 (uninitialised memory), Q-NETLIST-47 (null dereferences in the final pass), Q-MISC-2 and Q-MISC-3 (uninitialised antenna model flags and calculation type), Q-MISC-5 (a read past the arguments) and Q-MISC-6 (an uninitialised check plane) change outputs only where Magic has undefined behaviour or crashes, so the cases avoid those inputs (NL-RES-07 sets the frequency), and the guards are unit-tested;
  - Q-EXTRACT-27 changes nothing Magic-comparable (no case interrupts a run).

  The capture runs with magic-compat ON, and the OFF behaviours are unit-tested.

### D22. Fuzz targets, determinism and resource caps (INTEGRATION with EXTTECH, EXTFLAT, RESCMD, EXTCMD)

`hefesto-testing-strategy` "Parser fuzzing" requires a target for every external reader. M7 adds:

| Target | Input | Covers |
|---|---|---|
| `fuzz_extract_section` | a tech file whose `extract` section is the fuzz input | every keyword, style loading, micron conversion and rescale |
| `fuzz_ext_netlist` | `.ext` text (extends M6's `fuzz_ext_reader`) | the full reader and flattener, then in-memory `ext2spice` flat and hierarchical, `ext2sim` and `extcheck` |
| `fuzz_res_ext_reader` | `.ext` plus `.res.ext` text | ResReadExt's input model and the overlay reader |
| `fuzz_extract_layout` | a byte stream decoded into paint and labels on `exttest.tech`, two-level hierarchy | `extract_cell` in memory, the file read back by EXTFLAT, and `extresist` on its first net |

- Seeds come from the case inputs, the PDK extract sections and the EX layouts.
- Each target has resource caps so that timeouts are real hangs: tokens per line (128), devices per node, coupling pairs per interaction area, step squares per cell, array interaction counts, network nodes per net and delta-star iterations. A cap that changes behaviour relative to Magic becomes a proposed decision, as M5's and M6's did.
- The 60 s CI run and the seed replay on every leg follow M3–M6.
- **Determinism.**
  - Tile-derived order comes from `db::Plane` (Q-EXTRACT-5), name-keyed order from `util::MagicHashTable` (Q-EXTRACT-7 `merge`, Q-NETLIST-2), and address-keyed order from the canonical order (Q-EXTRACT-7 `cap`, Q-NETLIST-1, Q-NETLIST-45). Sorts are stable (Q-NETLIST-26).
  - The determinism job gains `--require-area EXTRACT --require-area NETLIST` once the expected outputs exist.
  - Pinned-value tests run on every leg for: `esSIvalue` and the `%lg`/`%g` outputs, the fringe fractions (Open Question 3), the float32 sidewall denominator, the lumped-resistance L/W `sqrt` (ExtBasic.c:641), the diagonal perimeter correction (ExtPerim.c:117), the non-rectangular gate estimate (ExtBasic.c:1535), the `w<n>` terminal width (:1919) and the capacitor equivalent square (:2948, :3206), the Elmore delays and the delta-star arms.

### D23. Performance (PERF; Open Questions 5 and 8)

`hefesto-performance` required extraction and SPICE generation to be no slower than Magic in single-threaded wall time on the same machine. Open Question 5, decided (a) on 2026-11-21, measures them as DRC and GDS are measured. The requirement text, applied to `hefesto-performance` "Batch throughput relative to Magic" (with P-24 extended to match):

> **Extraction and SPICE generation: instruction counts.** Each reference workload SHALL take at most Magic's callgrind instruction count on the same x86_64 runner. Once Hefesto's count for a workload is recorded as its baseline, the workload SHALL take at most that baseline + 2 %. Wall time is reported for information.

Workloads:
- `extract-bench`: `extract all` of the reference all-cells block of `sky130_fd_sc_hd` (the `gds-bench` block), `ext2spice` flat with `lvs`, `ext2spice hierarchy on`, and `ext2sim`. Hefesto and Magic both run under callgrind.
- `extresist-bench`: `extresist all` on the placed-and-routed sky130 block of m6 task 16.1 (`bench/lefdef/`, read with `def read`, then `extract all`). Gated at its own baseline + 2 %, with the Magic count reported (extresist has no spec target). If M6 16.1 has not landed when PERF starts, PERF commits that block first, as an open item to M6's PERF.

Both jobs run on the x86_64 runner with path filters on the M7 areas. `drc-profile` and `gds-bench` must still pass, since D6 adds a field to every tile. Open Question 8 (a) keeps M7 single-threaded: `ExtractContext` holds no globals, so per-cell parallelism can be added later without an interface change.

### D24. Security review (INTEGRATION; `docs/contributing/security.md`)

Technology files, `.ext` and `.res.ext` files and layouts are attacker-controlled. The review covers:
- **M7-S1.** `.ext` and `.res.ext` input: recursion through `use`, merge chains, and parameter expressions (inherits M6-S8 and the `ReadLimits` decisions).
- **M7-S2.** Extract-section numbers: huge halos and steps (the step-square count), parameter ranges, and the 64-bit arithmetic of micron conversion.
- **M7-S3.** Extraction resource bounds on hostile layouts: nodes, devices, coupling pairs within a halo, step squares, array interactions and hard-way name searches.
- **M7-S4.** Output paths: `extract path`, `ext2spice -o`, `extresist` and FastHenry file names, inside the project root on sandboxed hosts (Q-EXTRACT-11); temporary files and renames (Q-EXTRACT-27).
- **M7-S5.** `$`-variable substitution in substrate names: lookup only, never evaluation (Q-EXTRACT-9).
- **M7-S6.** extresist network sizes: delta-star and simplification loops, heap growth, FastHenry output size.
- **M7-S7.** Integer overflow in area, perimeter and capacitance accumulations: Magic's `int` areas widened only where its output is unchanged, otherwise reported.
- **M7-S8.** `hefesto-extcheck` arguments.

Each finding is fixed with a regression test or recorded as an accepted limit. A cap that changes behaviour is drafted as a proposed decision for the user.

### D25. Items deferred to M7 from earlier milestones, and the exit list

| Item (source) | M7 home |
|---|---|
| SCRIPT-API-16 (`$SUB` through `extract all`) | EXTDEV + GOLDEN |
| SCRIPT-API-20 (`extract style` result format) | EXTCMD + GOLDEN |
| UI-G-005 deviation: `extract`, `ext`, `ext2sim`, `ext2spice`, `exttosim`, `exttospice`, `extresist`, `antennacheck` | INTEGRATION (the deviation keeps only its M10 and M12 commands) |
| `scripting-api` "Script variables read by the engine", extraction-side scenarios | EXTDEV |
| M6 task 9.2 visit-order probe comparison, if still open | EXTFLAT |
| DEF write from Hefesto's own `.ext` (Open Question 9) | GOLDEN (LEFDEF-336) |

Exit list (R6):
- every capability's `check_requirement_coverage.py --capability <cap>` reports 100 % of exact and behavioural requirements tagged, and the three informative ones are documented;
- `m7_cases.py --exit-check` reports the 155 cases (LEFDEF-336 included) passing, or deviating with an approved decision;
- the determinism job requires EXTRACT and NETLIST and is identical on every leg;
- every fuzz target replays its seeds on every leg and runs 60 s without a crash;
- the security findings are fixed or accepted, with every cap decided;
- the benches meet their gates (Open Question 5);
- no `CORE-NOTIMPL` stub remains, and the code is retired;
- every function is at cognitive complexity 15 or below, or flagged as irreducible with a reason.

## Area contracts

The headers are the contract, and each one is documented. The key types and functions of each area, which the scaffold writes as stubs:

### EXTTECH: `hefesto/extract/tech/`
- `ExtStyle` (D5 data model), `DevicePattern`, `CapRules` (`area`, `perimeter`, `overlap`, `sidewall`, `sideoverlap`), `ResistClass`, `AntennaRule`, `SubstrateDecl`
- `StyleTable load_styles(const tech::RawSection&, const tech::Technology&, diag::DiagnosticSink&, const compat::CompatSettings&)`, `StyleTable::names()`, `StyleTable::select(name) -> std::shared_ptr<const ExtStyle>`
- `ExtStyle::rescale(int n, int d, const compat::CompatSettings&) const -> ExtStyle`, `micron_convert(const mask::OutputStyle*) const -> ExtStyle`
- `StyleView` (unchanged interface), M10's `HeightView` (unchanged interface; `rescaled` derived from `extract.current`)
- `ExtStyle::device_info() -> std::span<const DeviceInfo>` (ExtGetDevInfo; read by SPICE, SIM and ANTENNA)
- `testing::StyleBuilder` (scaffold test helper)

### EXTNODE: `hefesto/extract/node/`, `hefesto/extract/file/`
- `RegionFinder::find(const db::CellDef&, const Rect&, const tech::TypeMask&, const ConnectTable&, RegionCallbacks&)`, `NodeRegion`, `LabelRegion`, `reset(const db::CellDef&)`
- `node_name(const NodeRegion&, const compat::CompatSettings&) -> std::string`, `generated_name(...)`
- `UniqueRenamer` (`apply(db::CellDef&, UniqueMode)`, `revert()`)
- `ExtWriter` (`header`, `node`, `attr`, `equiv`, `port`, `substrate`, `device`, `parameters`, `cap`, `resist`, `use`, `merge`, `killnode`, `distance`, `finish`), `format_value(double) -> std::string`
- `AtomicOutput` (`open(const cell::LookupContext&, const db::CellDef&, std::string_view suffix)`, resolving the path as ExtFileOpen does; `commit()`; `abandon()`), used by EXTCMD for `.ext` and by RESCMD for `.res.ext`, `.res.lump` and `.fh` (Q-EXTRACT-27)

### EXTCAP: `hefesto/extract/cap/`
- `substrate_caps(const NodeRegionList&, const ExtStyle&, ...)`, `overlap_caps(...)`, `sidewall_caps(...)`, `sideoverlap_caps(..., FringeModel)`, `corner_caps(...)`, `CouplingTable` (address-free, name-keyed at output)
- `lumped_resistance(const NodeRegion&, const ExtStyle&) -> std::int64_t`
- `fringe_fraction(double mult, double d) -> double` (Open Question 3)

### EXTDEV: `hefesto/extract/device/`
- `find_devices(const db::CellDef&, const ExtStyle&, const NodeRegionList&, ...) -> std::vector<DeviceRecord>`, `select_pattern(...)`, `gate_dimensions(...)`, `resistor_dimensions(...)`, `capacitor_dimensions(...)`
- `SubstrateNodes prepare_substrate(const db::CellDef&, const ExtStyle&, const settings::GlobalNets&)`

### EXTHIER: `hefesto/extract/hier/`
- `ExtractContext`, `extract_cell(ExtractContext&, db::CellDef&, ExtWriter&, bool is_top) -> CellResult`
- `InteractionAreas interactions(const db::CellDef&, const ExtStyle&, const Overrides&)`, `YankBuffers`

### EXTCMD: `hefesto/extract/command/`, `hefesto/extract/length/`
- `ExtractSession` (D5 members), `RunRequest`, `run(EditorState&, const RunRequest&, const mask::Progress*) -> cmd::CommandResult`, `ensure_extracted(...)`
- `register_extract_commands(cmd::CommandRegistry&)`, `extract_options()`
- `path_lengths(ExtractContext&, const LengthLists&, ExtWriter&)`

### EXTFLAT: `hefesto/extflat/`
- `read(...) -> diag::Outcome<Hierarchy>`, `flatten(const Hierarchy&, FlatFlags) -> Netlist`, `read_flat(...)` (M6 wrapper)
- `FlatDevice`, `FlatCap`, `FlatResist`, `Netlist::{visit_devices, visit_caps, visit_resists, visit_nodes}`, `Hierarchy::{visit_defs, visit_uses}`
- `CommonArgs`, `parse_common_args(...)`, `NameFlags`, `name_to_spice(...)`, `name_to_sim(...)`, `read_res_overlay(...)`

### SPICE: `hefesto/netlist/spice/`
- `SpiceSettings`, `Dialect`, `run(EditorState&, const SpiceRun&) -> cmd::CommandResult`, `render(EditorState&, std::string_view, const SpiceSettings&) -> diag::Outcome<std::string>`, `register_spice_commands(...)`
- `format_si(double) -> std::string` (esSIvalue)

### SIM: `hefesto/netlist/sim/`, `hefesto/netlist/extcheck/`
- `SimSettings`, `SimFormat`, `run(EditorState&, const SimRun&) -> cmd::CommandResult`, `register_sim_commands(...)`
- `extcheck::run(std::span<const std::string>, std::ostream&, std::ostream&) -> int`

### RESNET: `hefesto/extresist/network/`
- `NetGeometry yank_net(...)`, `fracture(db::StitchEditor&, const Rect&)`, `Network build_network(const NetGeometry&, const ExtStyle&, const DeviceAnnotations&)`, `HeapState`, `reduce(Network&, const Tolerances&, HeapState&, const compat::CompatSettings&)`

### RESCMD: `hefesto/extresist/command/`, `hefesto/extresist/output/`
- `ResistSession`, `ResistRun`, `run(EditorState&, const ResistRun&, const mask::Progress*) -> cmd::CommandResult`, `register_extresist_command(...)`
- `InputModel read_input(...)`, `write_res_ext(...)`, `write_res_lump(...)`, `write_fasthenry(...)`, `geometry_elements(...)`

### ANTENNA: `hefesto/antenna/`
- `RuleView::from_style(const extract::ExtStyle&)`, `rules(type)`, `tiedown_types()`
- `check(EditorState&, db::CellDef&, const AntennaOptions&) -> cmd::CommandResult`, `register_antenna_command(...)`

### INTEGRATION: `hefesto/db/`
- `Plane::client`, `set_client`, `reset_clients`, `kClientDefault`
- `StitchEditor` (`db/plane_stitch.hpp`): `split_x`, `join_y`, `tile_at`, `neighbors`

## As-landed scaffold contracts

The scaffold (tasks.md 3.2, 3.3, 3.5, 3.6; branch `m7-r1-scaffold`) landed the areas, headers and stubs below. Task 3.4 (the tile client slot and `db/plane_stitch.hpp`) is INTEGRATION's with the MODEL review and is not part of it. The headers are the contract; this section records where they are, who fills them in, and where they differ from "Area contracts", D4 and D5. Include paths are under `src/core/include/hefesto/`, sources under `src/core/src/`.

### Files and owners

| Path | Contents | Owner (fills in) |
|---|---|---|
| `extract/{sources.cmake,codes.cpp}` | the area's source list and code table, now assembled from `extract/<sub>/codes.inc` for `tech node file cap device hier command length` | INTEGRATION |
| `extract/<sub>/{sources.cmake,codes.inc}` for `node file cap device hier command length` | one source list and code fragment per subdirectory (no codes yet) | the D3 owner |
| `netlist/{sources.cmake,codes.cpp}`, `netlist/<sub>/{sources.cmake,codes.inc}` for `spice sim extcheck` | the new area; `HEFESTO_CORE_AREAS` gains `netlist extresist antenna` | INTEGRATION; SPICE, SIM |
| `extresist/{sources.cmake,codes.cpp}`, `extresist/<sub>/{sources.cmake,codes.inc}` for `network command output` | the new area | INTEGRATION; RESNET, RESCMD |
| `antenna/{sources.cmake,codes.cpp,codes.inc}` | the new area (one directory, as `extflat/`) | INTEGRATION (`codes.cpp`); ANTENNA |
| `docs/diagnostics/codes/{netlist,extresist,antenna}.md`; `extract.md` fragment table | one section per owning workstream, the generated (empty) tables | INTEGRATION; sections: their workstreams |
| `cmd/m7_stubs.hpp`, `src/cmd/m7_stubs.cpp` | `m7_not_implemented{,_diagnostic,_status}(what)`, `CORE-NOTIMPL-008` (`diag::codes::kExtractNotImplemented`) | INTEGRATION (removed at M7 exit) |
| `extract/tech/style.hpp`, `src/extract/tech/style.cpp` | `DeviceClass`, `DeviceParam`, `ParamRange`, `DevicePattern`, `ResistClass`, `OverlapRule`, `SidewallRule`, `SideOverlapRule`, `TypeCap`, `CapRules`, `AntennaModel`, `AntennaRule`, `SubstrateDecl`, `FringeModel`, `DeviceInfo`, `TypeHeightThickness`, `ExtStyle` (with `device_info()`, `set_device_info`, the four StyleView queries, `rescale`, `micron_convert`), `StyleTable` (`names`, `select`), `load_styles` | EXTTECH |
| `extract/tech/testing/style_builder.hpp` | `extract::testing::StyleBuilder`, header-only, fluent setters, `build()`, `shared()` | INTEGRATION; EXTTECH may add setters |
| `extract/node/{regions,names,unique}.hpp`, `src/extract/node/node_stubs.cpp` | `ConnectTable`, `ClassArea`, `LabelRegion`, `NodeRegion`, `DeviceRegion`, `NodeRegionList`, `RegionCallbacks`, `RegionFinder::find`, `reset`; `node_name`, `generated_name`; `UniqueMode`, `UniqueRenamer` | EXTNODE |
| `extract/file/{writer,atomic_output}.hpp`, `src/extract/file/file_stubs.cpp` | `ExtHeader`, `ExtTerminal`, `format_value`, `ExtWriter` (one emitter per record, `finish`); `AtomicOutput` (`open`, `write`, `commit`, `abandon`, `target`) | EXTNODE |
| `extract/cap/caps.hpp`, `src/extract/cap/cap_stubs.cpp` | `CouplingTable` (index-keyed, smaller first), `CapInputs`, `substrate_caps`, `overlap_caps`, `sidewall_caps`, `sideoverlap_caps`, `corner_caps`, `lumped_resistance`, `fringe_fraction` | EXTCAP |
| `extract/device/devices.hpp`, `src/extract/device/device_stubs.cpp` | `DeviceTerminal`, `DeviceRecord`, `Dimensions`, `find_devices`, `select_pattern`, `gate_dimensions`, `resistor_dimensions`, `capacitor_dimensions`, `SubstrateNodes`, `prepare_substrate` | EXTDEV |
| `extract/hier/extract_cell.hpp`, `src/extract/hier/hier_stubs.cpp` | `YankBuffers`, `ExtractContext`, `CellResult`, `InteractionAreas`, `extract_cell`, `interactions` | EXTHIER |
| `extract/command/session.hpp` | `ExtractOptions` (Magic's start-up defaults), `Overrides`, `LengthLists`, `ExtractSession {styles, current, options, overrides, length}`; `EditorState::extract` | INTEGRATION (members: EXTTECH `styles`/`current`, EXTCMD the rest) |
| `extract/command/run.hpp`, `src/extract/command/run_stubs.cpp` | `RunMode`, `RunRequest`, `RunReport`, `run`, `ensure_extracted` | EXTCMD |
| `extract/command/commands.hpp`, `src/extract/command/commands.cpp` | `extract_options`, `extract_do_options`, `extract_warn_options`, `extract_length_options`, `extract_unique_options` (Magic's five tables, byte for byte), `register_extract_commands` (`extract`, `ext`) | INTEGRATION (dispatcher); EXTCMD (option parsers) |
| `extract/length/length.hpp`, `src/extract/length/length_stubs.cpp` | `path_lengths` | EXTCMD |
| `extflat/records.hpp`; `extflat/read.hpp` | `FlatTerminal`, `FlatDevice`, `FlatCap`, `FlatResist`; `Netlist` gains `devices`, `caps`, `resists` and `visit_devices`, `visit_caps`, `visit_resists` (M6's constructor and `visit_nodes` unchanged) | EXTFLAT |
| `extflat/{hierarchy,args,names}.hpp`, `src/extflat/m7_stubs.cpp` | `FlatFlags` (`operator|`, `has`), `HierDef`, `HierUse`, `Hierarchy` (`visit_defs`, `visit_uses`), `read`, `flatten`, `OverlayRecord`, `read_res_overlay`; `CommonArgs`, `ExtraFlags`, `parse_common_args`; `SpiceDialect`, `NameFlags`, `name_to_spice`, `name_to_sim` | EXTFLAT |
| `netlist/first_word.hpp`, `src/netlist/first_word.cpp`; private `src/netlist/table_command.{hpp,cpp}` | `FirstWord`, `FirstWordResult`, `classify_first_word` (Q-NETLIST-13); the shared dispatcher of `ext2spice`, `ext2sim` and `antennacheck` (`help` prints `    <entry>` per entry) | INTEGRATION |
| `netlist/session.hpp` | `NetlistSession {spice, sim, names}`; `EditorState::netlist` | INTEGRATION (members: SPICE, SIM, EXTFLAT) |
| `netlist/spice/{settings,spice}.hpp`, `src/netlist/spice/spice_command.cpp` | `Dialect` (= `extflat::SpiceDialect`), `MergeMode`, `ShortMode`, `TopSubckt`, `SpiceSettings` (defaults of "Command options", static `lvs(base)`), `format_si`; `SpiceRun`, `run`, `render`, `spice_options`, `register_spice_commands` (`ext2spice`, `exttospice`) | INTEGRATION (registration); SPICE |
| `netlist/sim/sim.hpp`, `src/netlist/sim/sim_command.cpp` | `SimFormat`, `SimMerge`, `SimSettings`, `SimRun`, `run`, `sim_options`, `register_sim_commands` (`ext2sim`, `exttosim`) | INTEGRATION (registration); SIM |
| `netlist/extcheck/extcheck.hpp`, `src/netlist/extcheck/extcheck_stub.cpp` | `extcheck::run(args, out, err, program)` | SIM |
| `src/cli/extcheck/{CMakeLists.txt,main.cpp}` | the `hefesto-extcheck` executable over `netlist::extcheck::run` (built where `hefesto-cli` is); `src/cli/CMakeLists.txt` adds the subdirectory | SIM (target); INTEGRATION (install, packaging) |
| `extresist/network/network.hpp`, `src/extresist/network/network_stubs.cpp` | `NetGeometry`, `NetNode`, `NetResistor`, `NetDevice`, `Network`, `DeviceAnnotations`, `Tolerances`, `HeapState`, `yank_net`, `fracture`, `build_network`, `reduce` | RESNET |
| `extresist/command/{session,command}.hpp`, `src/extresist/command/command.cpp` | `ResistOptions` (defaults of "Command options"), `ResistSession {options, heap}` (`EditorState::extresist`); `ResistRun`, `run`, `extresist_options`, `register_extresist_command` | INTEGRATION (struct, registration); RESCMD, RESNET (`heap`) |
| `extresist/output/output.hpp`, `src/extresist/output/output_stubs.cpp` | `InputNode`, `InputModel`, `GeometryElement`, `read_input`, `write_res_ext`, `write_res_lump`, `write_fasthenry`, `geometry_elements` | RESCMD |
| `antenna/antenna.hpp`, `src/antenna/antenna.cpp` | `RuleView` (`from_style`, `rules`, `tiedown_types`), `AntennaOptions`, `check`, `antennacheck_options`, `register_antenna_command` | INTEGRATION (registration); ANTENNA |
| `editor/extract_hook.hpp`, `src/editor/extract_hook.cpp` | `install_extract_styles(EditorState&, DiagnosticSink&)`, `rescale_extract_style(EditorState&, n, d)` | INTEGRATION; EXTTECH (the builders it calls) |
| `editor/editor_state.hpp` | members `extract`, `netlist`, `extresist` (after `lefdef`) | INTEGRATION |
| `cmd/all_commands.cpp` | registers the five commands after `oasis` | INTEGRATION |
| `tests/unit/{extract/<sub>,netlist/<sub>,extresist/<sub>,antenna}/` | one executable per directory once it has a `*_test.cpp` (data: `tests/data/<area>/<sub>`) | the D3 owners |
| `tests/unit/core/m7_scaffold_test.cpp` | the `[m7-scaffold]` tests: code areas, every option of the five commands, Magic's invalid-option messages, the first-word rule per mode, the stubs' values, the D5 defaults, the hook | INTEGRATION; owners replace them with `[cap:][req:]` tests |
| `docs/design/m7-ui-conformance.md` | the seed (every row **todo** or **partial**) | SHELL |

### Stub values

Every option of the five commands except `help` answers `CORE-NOTIMPL-008` with the text `not implemented yet: <command> <option>` (bare `extract`: `extract`; the run paths: `ext2spice run`, `ext2sim run`, `extresist run`, `antennacheck run`). Before that the dispatchers already do Magic's part: `extract` strips a leading `list`/`listall`, prints `"<w>" isn't a valid extract option.  Type "extract help" for a list of valid options.` for an unknown or ambiguous option, `Point to a window first` (VIEW-WINDOW-003, as the `lef` scaffold) without a window or command point for every option but `style` and `help`, the help text, and the wrong-argument message for `extract help <x>`; `extresist` prints `Ambiguous option: <w>` and `Unknown option: <w>` and its unindented help; `ext2spice`, `ext2sim` and `antennacheck` apply the first-word rule and print their help as Magic's usage loop. The entry points (`extract::run`, `ensure_extracted`, `netlist::spice::run`, `render`, `netlist::sim::run`, `extresist::run`, `antenna::check`) fail with `CORE-NOTIMPL-008`; `netlist::extcheck::run` prints it to `err` and returns 1. Accumulators, readers and writers that return a status fail with it; pure functions answer neutral values (`fringe_fraction` and `lumped_resistance` 0, the dimension functions zero, `select_pattern` nullopt, `node_name` empty, `format_value` and `format_si` empty, `name_to_spice`/`name_to_sim` the components joined with `/`, `flatten` an empty netlist, `ExtStyle::rescale` and `micron_convert` a copy, `load_styles` an empty table, `StyleTable::select` nullptr; `ExtWriter`'s emitters append nothing). `RuleView` and `ExtStyle::gate_types` are implemented (plain views of the data model).

### The tech-load and grid hook

`editor::install_extract_styles` runs at the end of `tech::install_technology` (`tech/commands.cpp`, after the grid, the DRC rules, the cifoutput styles and the display styles are installed): it clears the style table, the current style and the overrides (Q-EXTRACT-25), then calls `load_styles` on the technology's last `extract` section, selects the first style, micron-converts it with the current cifoutput style and scales it to the session grid (TechLoad's `ExtTechScale(DBLambda[0], DBLambda[1])`). `editor::rescale_extract_style` runs inside `tech::scale_layout`, next to LefTechScale, so `scalegrid`, finer-grid `.mag` reads and mask-input reads (every DBScaleEverything site) rescale the style once; a layout factor n/d is `ExtStyle::rescale(d, n)`, Magic's ExtTechScale argument order. Until EXTTECH lands `load_styles` the table is empty and `extract.current` stays null, so no output changes.

### Differences from "Area contracts", D4 and D5

- **Hook placement.** The hook's code is in `editor/`; the technology is installed and the grid rescaled in `tech/commands.cpp` (`install_technology`) and `tech/scale_layout.cpp` (`scale_layout`), so each has one call line and one include, as D20 now states (amended 2026-11-22, M7-13); there is no editor-side path to hang it on without moving M1's loader.
- **Stub helper.** D4's "next free core code" is `CORE-NOTIMPL-008`, in `cmd/m7_stubs.hpp` (one helper for the five M7 areas) rather than M6's `cmd/stubs.hpp`, which M6 retires at its exit.
- **`ExtStyle::rescale(n, d)`** takes Magic's ExtTechScale order (distances × d/n), documented in the header.
- **`ExtStyle`** is a class with public data members and a private `device_info_` (set with `set_device_info`); `CapRules::area` and `perimeter` are lists of `TypeCap {type, outside, cap}`.
- **`node_name`** and `generated_name` take the `tech::Technology` (plane and type names for the generated name), which the contract left out.
- **`RegionFinder::find`** returns `diag::Outcome<NodeRegionList>`; device and label regions are carried in `DeviceRegion` and `NodeRegion::labels`.
- **`AtomicOutput::open`** is static and returns `diag::Outcome<AtomicOutput>`; `write(text)` was added.
- **Capacitance accumulators** take a `CapInputs {cell, style, compat}` bundle instead of the elided parameters.
- **`prepare_substrate`** takes `app::GlobalNets` (`app/project_file.hpp`); there is no `settings::GlobalNets`. It returns `diag::Outcome<SubstrateNodes>`.
- **`extract_cell` and `interactions`** return `diag::Outcome<...>`; `ExtractContext` carries copies of the options, overrides and length lists, the compat settings and the progress pointer. `Overrides`, `ExtractOptions` and `LengthLists` live in `extract/command/session.hpp`, which `hier/` and `length/` include.
- **`name_to_spice` / `name_to_sim`** take the name as its components (`std::span<const std::string>`) and the compat settings; the `.ext` HierName stays private to `extflat/`. `Dialect` is defined in `extflat/names.hpp` as `SpiceDialect` and aliased as `netlist::spice::Dialect`, so `extflat/` does not depend on `netlist/`.
- **`parse_common_args`** takes an `ExtraFlags` callback for each netlister's own flags.
- **`extcheck::run`** takes a fourth argument, the program name for the usage text (`argv[0]`, D15's masking).
- **`fracture`** takes `db::StitchEditor&`, forward-declared until task 3.4 lands `db/plane_stitch.hpp`.
- **`antenna::check`** keeps the contract signature; `AntennaOptions` holds the run arguments and the `debug` flag.
- **ext2sim and antennacheck first word.** Their tables start with the same `[run] [options]` entry as ext2spice's, so they follow Q-NETLIST-13's rule in both modes: Q-NETLIST-49 (`ext2sim`) and Q-MISC-91 (`antennacheck`), compat, approved 2026-11-22 (M7-2). The antenna-check spec now says that the unique-prefix match against `run`, `debug`, `help` is the magic-compat OFF behaviour.
- **`extract list`** with nothing after it makes Magic look up a NULL argv entry and crash (CmdE.c:1151-1162); Hefesto prints the wrong-argument message in both modes, for `listall` as well: Q-EXTRACT-28, fix, approved 2026-11-22 (M7-1).
- **M4's `ext2spice`.** PDKINT registered a partial `ext2spice` in M4 (`script/pdk/commands.cpp`: `scale` as the setting `ext2spice-scale`, everything else `UI-CLIENT-002`), so a second registration failed `register_all_commands`. `cmd/all_commands.cpp` no longer calls `script::register_pdk_commands`; M7's dispatcher hands `scale` to the newly exported `script::ext2spice_scale`, and the other options answer `CORE-NOTIMPL-008` (the PDKINT test "ext2spice scale is a setting" now expects that code). Decided 2026-11-22 (M7-3): M4's partial command is folded into M7's `ext2spice`, `scale` is kept, and SPICE moves it into `SpiceSettings` in round R2, with the PDK startup tests (`tests/unit/script/pdk/`).
- **UI-G-005** still lists the M7 commands as unregistered in `tests/golden/deviations.toml` (GOLDEN's file): an open item of this round.

## Risks / Trade-offs

- **R1. Port size.** About 58,000 lines of Magic C, twice M6. → Seven rounds, with the netlisters in parallel with the extractor on Magic-written inputs. One dispatch table per reader. One helper per compat branch, and flagged exceptions of at most 25.
- **R2. Parallel work with M10 on `extract/tech/`.** → Open Question 2 (a): the handler files are separate; while TECH is open EXTTECH adds only one-line `case` entries to `StyleReader::apply`, and the dispatch-table refactor waits for TECH's merge. The tech-load hook is M7's own, on the editor side. M10's `HeightView` becomes a view over `ExtStyle`, so `LayerStack` is untouched.
- **R3. Memory and speed of the tile client slot.** One more field on every tile of every plane, on desktop and on iPad (`hefesto-performance` "Memory bound on iPad"). → Same layout as Magic's `Tile`. `drc-profile`, `gds-bench` and the iPad memory check run before R2 starts. The fallback, Open Question 1 (b), is kept as an option.
- **R4. Floating-point identity.** `atan` in the fringe-halo model, `%lg` output, float32 sidewalls. → Open Question 3, `-ffp-contract=off` and pinned values per leg. The PDK cases exercise the fringe-halo model on 759 cells.
- **R5. Hash and address order.** `merge` order (hash), `cap` and coupling order (address). → `MagicHashTable`, already validated against Magic's hash, and the `ext-cap`, `spice-cap`, `sim-cap` and `res-ext` canonicalizers, each documented in its module.
- **R6. extresist's direct stitch surgery.** ResFracture breaks the maximal-strip invariant on purpose. → Only on scratch planes, through a restricted header (D6). Debug builds check that no cell plane is passed.
- **R7. `extcheck` absent from Magic's default build.** → GOLDEN builds it in the image (D15). If it no longer builds at the pin, NL-CHK-01 is hand-derived from the source and marked so, with an approved deviation. It did not build at fc5daa9e; the user chose this option on 2026-11-22 (M7-9).
- **R8. Malformed input.** → D22 caps, D24 review, fuzz targets.
- **R9. Decisions changing later.** The plan follows the answers to Open Questions 1–9 approved on 2026-11-21 (each the recommended one). → The table "Decisions this design depends on" names the D sections each answer shapes.

## Migration Plan

User data does not change: `.mag` files are read and written as before.
- Technologies load the same way. The extract section is now parsed in full, and PDK sections give no new diagnostics (D7).
- `.ext` files written by Hefesto are read by Magic and netgen, as the goldens check.
- Scripts that call `extract`, `ext2spice`, `ext2sim`, `extresist` or `antennacheck` now run instead of failing as unknown commands.
- `lef write` takes its antenna rules from the shared view. The output is unchanged, as the LEFDEF antenna cases check.
- `CORE-NOTIMPL-008` (or the next free code when the scaffold lands) exists only during M7.
- CI gains the `extcheck` build in the capture image and the `extract-bench` and `extresist-bench` jobs.

## Open Questions

These shape or block the work. **All nine were approved by the user on 2026-11-21, each with its recommended option (a).** Options are listed with the recommended one first. The decision table holds the same rows ("Planning questions raised by the M7 plan"), and the behaviour question Q-EXTRACT-27, approved the same day as a fix in both modes, is in `docs/decisions/open-questions/EXTRACT.md` (D12).

1. **Where the extractor keeps its per-tile state.** Magic's extractor stores a region pointer in `ti_client` on every tile it visits and resets it after each pass (ExtRegion.c:71-88, 142-209, 533; about 40 uses in `extract/`). extresist uses it on about 60 lines and calls TiSplitX and TiJoinY1 on its fracture plane (ResFract.c:436-488). Hefesto's `detail::Tile` has no client field, and `split_x`/`join_y` are private.
   - (a) *Recommended:* add one pointer-sized `client` field to `detail::Tile` (Magic's `Tile` has it, tiles/tile.h:73), reached through `Plane::client`, `set_client` and `reset_clients`. Add the restricted header `db/plane_stitch.hpp` for split and join on scratch planes (D6). One field per tile; O(1) access, as in Magic. This is the only way to stay within the "no slower than Magic" target with a literal port.
   - (b) A side table per run, keyed by `TileView::tile` (a hash map), so no tile changes size. The extractor visits every tile several times per pass, which adds a hash lookup to each visit (the cost `connect/search.cpp`'s `std::set` already shows). Stitch edits still need (a)'s header.
   - (c) An extractor-private copy of the tile module with a client field, used for yank buffers and cell planes copied in. The tile code would exist twice, and the copy would cost a pass over every cell.

   *Why (a):* it matches the port one to one, keeps Q-EXTRACT-5's order free (no copy), and its cost (one field) is measured by the existing benches before R2.

   **Decided 2026-11-21: (a).** The per-tile `client` field and the restricted `db/plane_stitch.hpp` land in R1 (D6); their cost is measured by `drc-profile`, `gds-bench` and the iPad memory check before R2.

2. **Starting M7 while M10's 3D P1 extends `extract/tech/`.** The user asked on 2026-11-20 that M7 run with the 3D work to gain time. M10's TECH (tasks 3.1, 3.2) is writing the style framework and `scale`/`units`/`planeorder`/`height` in the same reader.
   - (a) *Recommended:* start M7 now. EXTTECH writes its handlers in new files (D7) and wires each one with a one-line `case` in the existing switch of `StyleReader::apply`; the dispatch-table refactor (task 4.1) waits for TECH's merge. M7 adds its own tech-load and `scalegrid` hook on the editor side (M10 added none: its `HeightView` reads `Technology::raw_sections()`). The other M7 workstreams build on the scaffold's `ExtStyle` and `StyleBuilder`, not on parsed tech files. M10's `HeightView` becomes a view of `ExtStyle`, with `rescaled` derived from `extract.current`.
   - What this means for the R2 exit: SPICE and SIM need EXTTECH's resist and device parsing (tasks 4.3, 4.5) for ExtGetDevInfo (D3). Under (a) those handlers are wired through the existing switch, so the NL-SPICE and NL-SIM replays do not wait for M10; until they land, `StyleBuilder` fixtures stand in for the netlisters' unit tests.
   - (b) Start M7 now except EXTTECH, which waits for TECH's merge. This is safer for M10, but EXTTECH's 22 requirements move to R3, and EXTCAP's and EXTDEV's PDK tests wait with it.
   - (c) Hand `extract/tech/` to M7 now and have M10's TECH consume M7's reader. That stops in-flight M10 work.

   *Why (a):* it gains the most time. The only overlap with TECH while it is open is additive `case` lines in one switch, and the one refactor of TECH's pass runs after TECH has merged.

   **Decided 2026-11-21: (a).** M7 starts alongside M10's 3D P1. EXTTECH wires its handlers through the existing switch, the dispatch-table refactor (task 4.1) waits for TECH's merge, and M7 owns the editor-side tech-load and `scalegrid` hook (D3, D7).

3. **Bit-identical `atan` for the fringe-halo model.** The fringe fractions are `0.6366 · atan(mult · d)` (ExtCouple.c:718-737, 1298, 1492-1508, 1874-1875). The model is on by default for tech format 34 and above, and sky130A, gf180mcuD and ihp-sg13g2 are format 35, so it runs on every PDK cell. `.ext` values must match Magic's (glibc on Linux x86_64) byte for byte after `%lg`, on Linux x86_64, Linux aarch64 and macOS arm64 (`hefesto-output-determinism`). The platform `atan` functions are not guaranteed to agree to the last bit.
   - (a) *Recommended:* a correctly rounded binary64 `atan`, either vendored from CORE-MATH (MIT; a licence check and a `THIRD_PARTY_NOTICES.md` entry) or written in-house from IEEE basic operations as M6 did for the OASIS CIRCLE sine and cosine (Q-OASIS-31). glibc dropped `atan`'s correctly rounded slow paths years ago, so a correctly rounded result can differ from Magic's glibc in the last ULP. The check therefore runs on what the cases compare: the capture image records its glibc version (`ldd --version` in the image manifest), and a probe compares the final `%lg` text that Magic's formula prints (`0.6366 · atan(mult · d)` through the full cap computation) with Hefesto's, on every argument the EX and PDK cases produce. A last-ULP difference that `%lg`'s six significant digits hide is harmless. Any text difference that remains is handled by the fallback, decided now: a pinned exception table (argument → glibc's double, generated on the capture image and committed with the glibc version) that `fringe_fraction` consults first, so the output stays byte-identical on every platform. If the table would exceed 1,000 entries, the affected cells are instead recorded as an approved deviation under this question and checked with `ext-cap` at `%lg` with one unit of tolerance in the last digit.
   - (b) The platform `atan`, accepting rare last-digit differences in `%lg` output on non-glibc platforms, with a per-case tolerance where they appear. This breaks the byte-identity rule.
   - (c) Port glibc's `atan`. It is LGPL, which `hefesto-licensing` does not allow in the core.

   **Decided 2026-11-21: (a), with the fallback as written.** A correctly rounded `atan` (vendored from CORE-MATH after the licence check, or written in-house), checked against Magic on the final `%lg` text; any remaining difference goes to the pinned exception table, or, above 1,000 entries, to an approved deviation for the affected cells (D4, D9, D22).

4. **Scope of the PDK standard-cell golden cases (the exit criterion).** The roadmap requires `.ext` and SPICE to match Magic's after canonicalization "on the PDK standard cells", without naming cells.
   - (a) *Recommended:* every cell of one standard-cell library per PDK. `sky130_fd_sc_hd` (446 `.mag` cells) and `gf180mcu_fd_sc_mcu7t5v0` (229) come from the existing `.mag` sets, and `sg13g2_stdcell` (84) from the existing `ihp-gds-sc` set (the standard-cell library alone, not the 456 MB `ihp-gds`) via `gds read`. That gives `EXTRACT-PDK-<lib>` (`.ext` per cell) and `NETLIST-PDK-<lib>` (`ext2spice lvs` flat, `ext2spice hierarchy on` in ngspice format, and `ext2sim`): 6 cases on 759 cells. No new data set is needed. Size: about 10–15 MB of uncompressed expected text against 46 MB in `tests/golden` today, with a 15 MB budget (D21).
   - (b) A sample of about 20 cells per library (inverters, NAND/NOR, flops, a mux, a tap, a fill), which captures faster but leaves most device shapes untested.
   - (c) sky130 only.

   *Why (a):* Magic extracts a standard cell in milliseconds, so the full libraries cost little capture time. They cover the device variants, the substrate and well handling and the fringe-halo model of three different tech files.

   **Decided 2026-11-21: (a).** All 759 cells of the three libraries in 6 cases, within the 15 MB budget for uncompressed expected text (D21, D25).

5. **How the extraction and SPICE performance target is judged.** The spec asks for single-threaded wall time no worse than Magic's (`hefesto-performance` "Batch throughput relative to Magic"). The same requirement already moved DRC and GDS to callgrind instruction counts, because the wall-clock ratio moves by about 0.1× between runs.
   - (a) *Recommended:* amend the requirement so that extraction and SPICE generation are judged like GDS: at most Magic's instruction count on the x86_64 runner, then the recorded baseline + 2 %, with wall time informational (D23 has the text). Workloads: `extract all`, flat and hierarchical `ext2spice`, and `ext2sim` on the `gds-bench` block. `extresist-bench` is gated at its own baseline + 2 %.
   - (b) Keep the wall-time rule as written, measured by repeated runs on one dedicated runner, with Magic run in the same job.
   - (c) Gate on both.

   **Decided 2026-11-21: (a).** `hefesto-performance` "Batch throughput relative to Magic" now judges extraction and SPICE generation by callgrind instruction counts, at most Magic's count and then baseline + 2 %, with the D23 text and workloads; P-24 (`docs/decisions/product.md`) is extended to match.

6. **How users run `extcheck`.** `netlist-extcheck` defines a standalone checker with an exit status. Magic builds it only on request (Makefile.in:128-130).
   - (a) *Recommended:* a small executable `hefesto-extcheck` in `src/cli/extcheck/`, installed next to `hefesto-cli` on desktop hosts, calling `netlist::extcheck::run`. It is not shipped on iPadOS, where no process runs it.
   - (b) A mode of the existing CLI, `hefesto-cli --extcheck <args>`. It needs one binary fewer, but scripts that call `extcheck` need a wrapper.
   - (c) Only an `extcheck` console command. That does not meet "Invocation" (exit status 1 on a usage error).

   **Decided 2026-11-21: (a).** A standalone `hefesto-extcheck` in `src/cli/extcheck/`, packaged next to `hefesto-cli` on desktop hosts (D2, D15).

7. **Desktop scope for M7.**
   - (a) *Recommended:* enable the top bar's "Extract" button (disabled since M1 with an M7 tooltip) to run `extract all` on the active window's root, with the existing progress panel and cancel. Netlisting, extresist and antenna checks stay console commands. No new boards.
   - (b) Also add a netlist dialog (format, hierarchy, output file) and an "Extraction" menu. DESIGN draws them first.
   - (c) Leave the button disabled: everything from the console.

   **Decided 2026-11-21: (a).** The existing "Extract" button is enabled, with progress and cancel; no new boards (D19).

8. **Threading.** `hefesto-performance` says multithreaded runs SHOULD be faster, and `hefesto-architecture` requires results equal to single-threaded runs.
   - (a) *Recommended:* single-threaded extraction in M7, with no globals in `ExtractContext` (D5), so that independent cells of the bottom-up stack can be extracted in parallel later without an interface change.
   - (b) Parallel per-cell extraction in M7: cells whose children are done run on a pool, and files and console output are ordered as in a serial run.
   - (c) Parallelism inside a cell, by step squares.

   *Why (a):* the port is large, the gate is single-threaded, and Magic's per-cell order fixes the console output and the incremental decisions.

   **Decided 2026-11-21: (a).** Single-threaded in M7, with no globals in `ExtractContext` (D5, D23).

9. **The DEF-write net cases once Hefesto extracts.** LEFDEF-317..328 and 330..334 take Magic-written `.ext` inputs (m6 Open Question 1 (a)).
   - (a) *Recommended:* keep those inputs, so the cases go on testing `def write` alone. Add one end-to-end case, LEFDEF-336, that runs `extract all` then `def write` on LEFDEF-317's layout.
   - (b) Switch the 17 cases to run `extract all` first. Each case then tests two features, and an extractor regression shows up as 17 DEF failures.

   **Decided 2026-11-21: (a).** The DEF-write cases keep their Magic-written `.ext` inputs, and LEFDEF-336 is added as the end-to-end case (D21, D25).

## Resolved Questions

- **The 56 open questions that blocked M7** were approved as drafted on 2026-11-13, with the three judgement items decided as option (a) (Q-EXTRACT-12 tile-exact path length, Q-EXTRACT-2 Magic's diode syntax, Q-NETLIST-37 compat). `docs/decisions/m7-decision-table.md` holds the table, the evidence corrections and the spec changes, and `check_milestone_readiness.py --milestone M7` reports ready. This design cites each decision where it shapes the code (D7–D17, D21, D22).
- **Entries resolved earlier** (2026-11-06/07, by X-1 as amended by D-1, X-2 tier 1, X-3, D-5 and D-14): Q-EXTRACT-5, 7, 8, 9, 13, 14, 15 and Q-NETLIST-1, 2, 3, 11, 14, 21, 22, 26, 27, 33, 36, 42, 45, 47. They are cited in D8–D16, D21 and D22.
- **M6's pull-forward** of the `.ext` subset and the style subset (m6 Open Questions 1 and 2, decided (a) on 2026-11-18) is the starting point of EXTFLAT and EXTTECH.
- **M10's extract subset** (m10 Open Question 4, decided (a) on 2026-11-13): there is one extract reader, and M7 extends it (D7).
- **The R1 follow-ups (2026-11-22).** The user approved, each with its recommended option: Q-EXTRACT-28 (fix), Q-NETLIST-49 and Q-MISC-91 (compat), Q-NETLIST-50, 51 and 52 (fix), the new rows Q-EXTRACT-29 and Q-NETLIST-53 (compat; RESNET, R4), and the plan items M7-3 (M4's `ext2spice` folded in, `scale` kept and moved into `SpiceSettings`), M7-9 (R7 option (a): NL-CHK-01 hand-derived), M7-10 (`spice-cap` with a node list), M7-11 (the synthetic tech additions), M7-12 (the +240 MB tile-slot bound; task 3.4 done) and M7-13 (D20 amended for the two hook lines in `tech/`). `docs/decisions/m7-decision-table.md` "Entries approved 2026-11-22" lists them with the spec changes.
- **This plan's Open Questions 1–9 and Q-EXTRACT-27** were approved by the user on 2026-11-21, each with its recommended option: (a) for the nine questions, fix in both modes for Q-EXTRACT-27. The spec edits they imply (`hefesto-performance`, `extract-commands`, `extresist-output`) are applied, and `docs/decisions/m7-decision-table.md` records them.

## Decisions this design depends on

| Decision | Option | Design sections it shapes |
|---|---|---|
| Q-EXTRACT-5 (tile order exact) | compat | D6, D8, D22 |
| Q-EXTRACT-7 (`merge` hash order, `cap` canonical order) | X-1 | D8, D21, D22 |
| Q-EXTRACT-1, 3, 19, 21, 25 | compat / replicate | D7 |
| Q-EXTRACT-4, 6, 10, 23 | compat / fix / replicate | D8 |
| Q-EXTRACT-17, 18, 20, 22, 24 | compat / replicate | D9 |
| Q-EXTRACT-2, 8, 9, 13, 15, 16 | replicate / fix / platform | D7, D10 |
| Q-EXTRACT-26 | replicate | D11 |
| Q-EXTRACT-11, 12 | platform / replicate | D12 |
| Q-EXTRACT-27 (approved 2026-11-21; covers `.ext`, `.res.ext`, `.res.lump`, `.fh`) | fix | D12, D16, D18, D21, D24 |
| Q-NETLIST-8, 9, 10, 12, 16, 30, 42 | compat / replicate / fix | D13 |
| Q-NETLIST-1, 4, 5, 6, 7, 13, 19, 20, 31, 32, 34, 35, 37, 41 | fix / compat | D14, D21 |
| Q-NETLIST-14, 15, 17, 18, 36, 48 | fix / compat / n/a / other | D15 |
| Q-NETLIST-21, 23, 24, 25, 26, 27, 28, 29, 39, 40, 43, 44, 45, 46, 47 | fix / replicate / compat | D16, D21 |
| Q-MISC-1, 3, 4, 7 | compat / fix | D7, D17 |
| Q-MISC-2, 5 (X-2 tier 1) | fix (both modes) | D7, D21 |
| Q-MISC-6 (X-2 tier 1) | fix (both modes) | D17, D21 |
| Q-NETLIST-11, 33 (D-14) | platform | D5, D10, D13 |
| m6 Open Questions 1, 2 | (a) | Context, D7, D13 |
| m10 Open Question 4 | (a) | D7 |
| Open Question 1 (decided 2026-11-21) | (a) | D5, D6, D16, D20, D23 |
| Open Question 2 (decided 2026-11-21) | (a) | D1, D3, D5, D7, D20 |
| Open Question 3 (decided 2026-11-21) | (a) | D4, D9, D22 |
| Open Question 4 (decided 2026-11-21) | (a) | D21, D25 |
| Open Question 5 (decided 2026-11-21) | (a) | D23 |
| Open Question 6 (decided 2026-11-21) | (a) | D2, D15 |
| Open Question 7 (decided 2026-11-21) | (a) | D19 |
| Open Question 8 (decided 2026-11-21) | (a) | D5, D23 |
| Open Question 9 (decided 2026-11-21) | (a) | D21, D25 |
| m10 scaffold as landed (`HeightView`, no loader hand-off; 73d4d195) | landed | Context, D5, D7, D20 |
| Q-EXTRACT-28, Q-NETLIST-50, 51, 52 (approved 2026-11-22) | fix | D12, D13, D15 |
| Q-NETLIST-49, Q-MISC-91 (approved 2026-11-22) | compat | D15, D17 |
| Q-EXTRACT-29, Q-NETLIST-53 (approved 2026-11-22) | compat | D6, D16 |
| M7-3, M7-9 to M7-13 (approved 2026-11-22) | as recommended | D14, D15, D20, D21, R7, task 3.4 |
