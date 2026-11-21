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
    - `ext-flattening-hierarchy` (5): "Container iteration order", "Flat node construction", "Node merge semantics", "Kills", "Global name merging";
    - `ext-flattening-names` (5): "Hierarchical name structure", "Component hash", "Instance and array element names", "Name preference order", "Global and ground detection".
  - **covered in part (10):**
    - `ext-flattening-read` "Device records" (node side only) and "Parameters records" (messages only);
    - `ext-flattening-hierarchy` "Visit orders for output" (nodes only);
    - `extract-tech-section` (7): "Extract section line grammar and keyword matching", "Extraction styles and variants", "Style defaults", "Substrate declaration", "Device declaration parsing" (class and types), "Non-electrical types", "Antenna keywords parse-level reference" (`tiedown`).

  M6's open task 9.2 note (the visit-order comparison with Magic's probe lines) moves to EXTFLAT if M6 has not closed it.
- **M10 3D P1 (in flight).** TECH (m10 tasks 3.1, 3.2) adds `style` and variants, `scale`, `units`, `planeorder`, `noplaneordering`, `height`, the end-of-style plane-order check, the defaults and the micron conversion and grid rescale of h and t to the same `extract/tech/` reader, plus the `extract` raw-line hand-off in `tech/loader.cpp`. M10's contract names `ExtractStyleSubset` and `parse_extract_subset`. When it lands it touches five more `extract-tech-section` requirements, in part: "Units, lambda, cscale and rscale", "Plane ordering", "Layer height and thickness defaults", "Micron conversion", "Scaling to internal units". M10 P3 (overlays) waits for M7.
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
  - IHP ships no `.mag` standard cells. `sg13g2_stdcell` (84 cells) is in the `ihp-gds` set (M5).
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
| R0 DECIDE | now | the user's answers to Open Questions 1–9 and Q-EXTRACT-27; the spec edits they imply (Open Question 5, Q-EXTRACT-27) | every proposed row has a `Resolution:`; `check_milestone_readiness.py --milestone M7` ready; `openspec validate --all --strict` |
| R1 SCAFFOLD | R0 | areas, headers and stubs (D2, D5); command stubs; the plane client slot and stitch header (D6); `extcheck` in the capture image; the capture modes and comparators; the NETLIST cases authored and captured; the first EX cases authored | the build passes with `-Werror` on every preset; `[m7-scaffold]` tests list every option; NETLIST expected outputs committed |
| R2 CORE | R1 merged | EXTTECH (parsers in new files; wiring per Open Question 2), EXTNODE, EXTDEV, EXTCAP on single cells; **in parallel** EXTFLAT, SPICE and SIM against the captured NETLIST cases | the flat EX cases (no subcells) replay; NL-FLAT, NL-SPICE and NL-SIM replay |
| R3 HIER | R2 merged | EXTHIER, EXTCMD, ANTENNA; NL-HIER, NL-CHK | every EX and MISC-ANT case replays, except those that need extresist |
| R4 RESIST | R3 merged (EXTNODE's regions and EXTFLAT's overlay read needed) | RESNET, RESCMD; SHELL | NL-RES and NL-RESNET replay |
| R5 GOLDEN | R4 merged | the PDK cases (Open Question 4), the deferred SCRIPT cases, the catalogue audit; PERF benches and gates (Open Question 5) | D21 counts all pass; the benches meet their gate |
| R6 EXIT | R5 | fuzz, security review, determinism areas, coverage, `CORE-NOTIMPL` retirement, exit check | D25's exit list; `m7_cases.py --exit-check` clean |

M7's start does not wait for M6's exit work (benches, fuzz, the security review; m6 tasks 16 and 17). Those tasks do not edit `extflat/` or `extract/tech/`, except `fuzz_ext_reader` (17.2) and the `ReadLimits` proposals (17.3). M7 extends that target once it lands, and inherits the decided caps. M7 does not wait for M10 P1 either (Open Question 2, recommended (a)). The user asked on 2026-11-20 that M7 run alongside the 3D work to gain time, and D3 orders the work so that only EXTTECH's wiring into the shared reader files waits for M10's TECH merge.

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
| EXTTECH | `extract-tech-section` (22) | `src/core/{include/hefesto,src}/extract/tech/`, `tests/unit/extract/tech/`, `tests/data/extract/tech/`, `extract.md` (STYLE section); the `extract` style hook in `tech/loader.cpp`/`tech/commands.cpp` and `tech/scale_layout.cpp`, shared with M10's TECH (Open Question 2) |
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
- EXTTECH starts at once, in new files (Open Question 2 (a)). It wires its parsers into the shared reader files after M10's TECH merges, or at once if TECH has merged by then.
- EXTNODE needs the client slot (D6). EXTCAP and EXTDEV need EXTNODE's regions and writer. They start on the scaffold's stubs and integrate in R2.
- EXTHIER needs EXTNODE, EXTCAP and EXTDEV on single cells. EXTCMD needs EXTHIER's `extract_cell` for the run driver; its option parsing and incremental logic start in R2.
- EXTFLAT starts at once, on M6's code and the captured NETLIST inputs. SPICE and SIM need EXTFLAT's visitors (device, cap, resist and hierarchical) and start on its stubs.
- ANTENNA needs EXTTECH's antenna rules, EXTFLAT's device visits and `connect::search_hierarchical`.
- RESNET needs EXTNODE's regions, the stitch header (D6) and `connect`. RESCMD needs EXTFLAT's tokenizer (its own record handling, ResReadExt) and RESNET.
- SHELL needs EXTCMD's run driver with progress.
- GOLDEN authors inputs from R1. The NETLIST cases are captured in R1; the EX and MISC-ANT cases as they are authored; the PDK cases in R5.
- PERF builds the benches once EXTCMD and SPICE land (R5).

### D4. Build and stub contract

- **No new library dependency**, unless Open Question 3 is answered (a) with the vendored option, which adds one MIT-licensed source file and a `THIRD_PARTY_NOTICES.md` entry (`hefesto-licensing`).
- **Floating point.** `extract`, `extflat`, `netlist`, `extresist` and `antenna` build with `-ffp-contract=off`.
  - Numbers are printed with Magic's formats (`%lg`, `%g`, `%.*f`, `esSIvalue`) through one formatting helper per area, with the C locale.
  - Q-EXTRACT-20's single-precision sidewall denominator and Q-NETLIST-25's integer heap keys (ON) are kept as Magic's types.
  - `sqrt` is IEEE-exact. `atan` (ExtCouple.c:718-737, 1298, 1492-1508, 1874-1875) is Open Question 3.
  - D22 lists the pinned-value tests.
- **Stubs.** The scaffold registers `extract` and `ext`, `ext2spice` and `exttospice`, `ext2sim` and `exttosim`, `extresist` and `antennacheck`, each with its spec's full option table. Options are matched as `extract-commands` "Command syntax and option matching" and `netlist-spice-options` "Command options" define (Q-NETLIST-13, Q-NETLIST-41 per mode).
  - Each option that is not implemented yet fails with the next free core code, `CORE-NOTIMPL-007` unless M8 or M10 has taken it first ("not implemented yet: <command> <option>"). Completion, help, scripts and golden inputs therefore see the final names.
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

  It also provides `StyleView`'s four queries, and the h, t and plane-order accessors M10's `LayerStack` reads. `extract::StyleTable load_styles(const tech::RawSection&, const tech::Technology&, diag::DiagnosticSink&, const compat::CompatSettings&)`, `ExtStyle::rescale(n, d, compat)` (ExtTechScale) and `micron_convert(const mask::OutputStyle*)` (ExtTechFinal, with the cifoutput scale) are its builders (EXTTECH). `StyleView` and M10's `ExtractStyleSubset` keep their interfaces as views over it.

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
- **Client slot (recommended, Open Question 1 (a)).** `detail::Tile` gains one pointer-sized `client` field, the field Magic's `Tile` has (tiles/tile.h:65-78), initialised to `kClientDefault`. `Plane::client`, `set_client` and `reset_clients` reach it through `TileView::tile`. It is not part of the plane's value: `clone()`, `operator==`, undo and the `.mag` writer ignore it.
- **Ownership of the slot.** A run takes the cell's planes under the shared lock and owns their client fields for the run. The fields are run-scoped scratch: they are written only by the thread that holds the extraction run, and every pass resets them before it ends, as Magic does. A debug check verifies that the slots are all `kClientDefault` when the run releases the lock. Two extraction runs never overlap, because each holds the run token of `ExtractSession`. DRC, rendering and connectivity never read the slot.
- **Stitch edits.** extresist fractures a scratch copy of the net (ResFract.c:64-251) with TiSplitX and TiJoinY1. `db::StitchEditor` exposes `detail::TilePlane::split_x` and `join_y` on a plane that no cell owns (a yank buffer). The restricted header keeps every other caller on the paint API, which preserves the maximal-strip invariant.
- **Connectivity.** antennacheck and extresist yank nets with `connect::search_hierarchical` into scratch cells (DBTreeCopyConnect), and the extractor's interaction yanks use `db::flatten_to_new` and the `UseIndex` order (ExtYank.c). `connect/` keeps its area-keyed visited set. Changing it to the client slot is a performance follow-up, not M7 scope.

### D7. The extraction style (EXTTECH; `extract-tech-section`; Open Question 2)

- **One reader.** EXTTECH completes the reader M6 and M10 started, in its home `extract/tech/` with its tags. The keyword table (`kKeywords`, `style_reader.cpp:38`) becomes a dispatch table of `{keyword, min, max, usage, handler}`. Each handler group lives in its own source file:
  - `cap_rules.cpp`: areacap, perimc, overlap, sidewall, sideoverlap, the defaults, fringeshieldhalo;
  - `resist_rules.cpp`: resist, contact;
  - `device_rules.cpp`: device and fet, with parameters and ranges;
  - `antenna_rules.cpp`: antenna, model, tiedown;
  - `conversion.cpp`: micron conversion and rescale.

  M10's TECH keeps the style framework and its keywords (`style`, `variants`, `scale`, `units`, `planeorder`, `noplaneordering`, `height`). Adding rows to the dispatch table is the only edit both sides make to the same file.
- **Parallel start (Open Question 2 (a)).** EXTTECH writes and unit-tests its handlers in R2 against a section fixture, through a scaffold hook that runs one handler on one line. Once M10's TECH has merged, it moves the remaining keywords from "accepted silently" to their handlers and wires the table. Whichever of EXTTECH and TECH merges second rebases the table. M10's `ExtractStyleSubset` becomes an alias of, or a view over, `ExtStyle`, and `LayerStack` does not change.
- **Load and rescale.** The style table is built at tech load (one hook in the tech loader, shared with M10's hand-off). `extract style <name>` parses the named style (ExtLoadStyle). Micron conversion runs with the current cifoutput scale (ExtTechFinal, ExtTech.c:3962-3966; Q-EXTRACT-3 per mode). `scalegrid` and finer-grid reads rescale the current style and the session overrides (ExtTechScale, ExtTech.c:4046-4093; Q-EXTRACT-19, Q-EXTRACT-25).
- **Decisions applied.** Q-EXTRACT-1 (shield direction by plane index ON, plane order OFF), Q-EXTRACT-2 (diode OTHER is terminal and substrate types), Q-EXTRACT-3 (gscap scaling per mode), Q-EXTRACT-19 (step and halo truncation ON, rounding OFF), Q-EXTRACT-21 (limits /1000 ON, milli-units OFF), and Q-MISC-3 (an antenna line without a calculation type stores "none" in both modes).
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

- **Substrate and overlap.** Area and perimeter caps, then overlap coupling with shield masks in plane order (ExtBasic.c, ExtCouple.c).
- **Sidewall.** Edge enumeration, nearest-neighbour shadowing and the coupling value, with the single-precision denominator (Q-EXTRACT-20, replicate).
- **Sideoverlap.** The legacy model (Q-EXTRACT-18 per mode) and the fringe-halo model (Q-EXTRACT-17 per mode). The fringe fractions are `0.6366 · atan(mult · d)` (ExtCouple.c:718-737, 1298, 1492-1508, 1874-1875). The fringe-halo model is on by default for tech format 34 and above ("Fringe model default chosen from tech format"), and the three PDK tech files are format 35, so it runs on every PDK cell. The `atan` must give Magic's bits on every platform (Open Question 3).
- **Corners and substrate coupling.** Corner fringe to substrate and corner coupling (Q-EXTRACT-24 per mode), coupling to the substrate node, and coupling record output (Q-EXTRACT-7).
- **Lumped resistance.** Per-class area and perimeter, the formula (ExtPerim.c's `sqrt` correction), rounding, the record values and the class header. The shorted-ports warning follows Q-EXTRACT-22 per mode.
- **One branch per decision.** Each compat split is a named function in `cap/compat_branches.cpp` taking the compat setting, with a unit test per mode.

### D10. Devices and substrate (EXTDEV)

- **Devices.** Device regions and subcell priority, then terminal discovery on and off the device plane, with terminal area, perimeter and sharing.
  - Record selection by identifier layers and substrate variants, and model selection by parameter ranges (Q-EXTRACT-21 per mode). Missing terminals copy or reject as Magic does.
  - MOSFET-family length and width from boundary lists, with the effective length and width of non-rectangular gates (ExtBasic.c:601-641, the `sqrt` estimate).
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
- **Atomic output (Q-EXTRACT-27, proposed; recommended fix).** Magic opens `<cell>.ext` for writing, writes the header first and stops writing on interrupt (ExtCell.c:99-116, 515-531). The truncated file keeps a valid `timestamp` line, so the next `extract` treats it as current (ExtMain.c:295-296, 984-986), and the netlisters read a partial netlist. Hefesto writes each file to a temporary name in the same directory and renames it when the cell is complete. A cancelled run keeps the previous file. Uninterrupted runs give the same bytes as Magic.
- **Path length.** The flood of ExtLength.c, ported exactly (Q-EXTRACT-12), so `distance` records are byte-identical.
- **Locking (three phases, as M5's `gds write`).**
  - *Prepare*, under the exclusive lock: load unread subcells, compute the bottom-up stack and the incremental set, and apply the temporary unique labels.
  - *Extract*, under the shared lock: `extract_cell` for each cell, writing files.
  - *Finish*, under the exclusive lock: revert unique labels (Q-EXTRACT-6), update flags and timestamps, and print the summary.

  Rendering continues during the extract phase; edits wait, as for every M5 write.
- **Progress and summary.** "Extracting <cell> into <file>:" and the error and warning totals as Magic prints them ("Progress and summary messages", informative).

### D13. Flattening (EXTFLAT)

EXTFLAT extends M6's code; nothing is rewritten. It adds:
- **Read side.** The full device records (every class of NL-FLAT-04, parameters and attributes), `parameters` records with scale and offset (Q-NETLIST-8 per mode, reversed order per Q-NETLIST-9), the `resist` value offset (Q-NETLIST-12 per mode), and the `.res.ext` overlay ("Resistance-extraction overlay").
- **Flattening.** Coupling capacitance and resistor flattening with the thresholds, lumped node resistance, device length and width, the device, cap and resistor visit orders, and one-level flattening for the hierarchical netlister (EFhier.c).
- **Common arguments.** EFArgs (`-c -C -r -R -p -s -S -t -T -v -o`), shared by ext2spice, ext2sim, extcheck and antennacheck. `-o` applies to one run (Q-NETLIST-42, fix).
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
- **extcheck.** The library part is `netlist::extcheck::run(args, out, err) -> int`, which returns the exit status and prints the counts in Magic's format with the defaults of Q-NETLIST-36. Open Question 6 decides how users run it. GOLDEN builds Magic's `extcheck` in the capture image (`make -C extcheck` after the main build, at the same pin; its Makefile.in links only `extflat` and `utils`).

### D16. extresist (RESNET, RESCMD)

- **RESNET.** For each net that RESCMD selects, RESNET builds the network:
  - it yanks the net's geometry (`connect::search_hierarchical` into a scratch cell, minus the `skip` types) and fractures it with the stitch editor (ResFracture);
  - it annotates devices, chooses the start tile and origin, places breakpoints and processes tiles in ResMain's order;
  - it builds tile resistors, cuts near devices (Q-NETLIST-43 per mode) and adds contact resistors;
  - it merges nodes and applies the construction-time reductions (Q-NETLIST-44 per mode; Q-NETLIST-45 resistor-list order in both modes);
  - it distributes path resistance and capacitance (Q-NETLIST-24 replicate; Q-NETLIST-25 and 46 per mode), prunes by Elmore delay, simplifies, and runs the final scrunch with a stable sort (Q-NETLIST-26, Q-NETLIST-47 guards).

  The region callbacks reuse EXTNODE's `RegionFinder` on the scratch cell.
- **RESCMD.**
  - The command and its options (Q-NETLIST-23 comma lists, fixed in both modes; Q-NETLIST-28 `mindelay` in ps).
  - The run flow, and the input model from the `.ext` file. ResReadExt keeps its own record handling over EXTFLAT's tokenizer, with Q-NETLIST-29 per mode and its guards in both.
  - The first cutoff, the second cutoff after extraction, and the lumped file.
  - The `.res.ext` overlay grammar and order (Q-NETLIST-39 per mode), node naming, FastHenry output (Q-NETLIST-40 per mode; Q-NETLIST-27 and Q-NETLIST-21 fixed) and the geometry centerlines.
  - `ext2spice extresist on` and `ext2sim extresist on` consume the overlay through EXTFLAT ("Consumption by netlisters").

### D17. Antenna check (ANTENNA)

`antennacheck` follows EFantenna.c:
- it requires a plane order (EFantenna.c:184-189), reads and flattens with `EF_FLATNODES | EF_WARNABSTRACT`, and builds the device-type table from the style (:227-252);
- it visits each gate net once and yanks it with `connect::search_hierarchical`, cumulatively and per plane;
- it measures surface and sidewall areas, counts contact cuts (Q-MISC-4 per mode, with the no-rule and zero-pitch guards in both), applies the connectivity restriction (Q-MISC-7 per mode) and the cumulative model (Q-MISC-1 per mode);
- it reports violations as feedback with Magic's text, and prints progress every 100 gates.

`antenna::RuleView` exposes the per-type rules and the tie-down set. `lef write` reads ANTENNAGATEAREA and ANTENNADIFFAREA inputs through it ("Shared antenna definitions for LEF output"), replacing M6's StyleView query (D20).

### D18. How M9 and M10 consume the output

- **LVS** (`lvs-command` "Layout netlist from extraction"): `extract::ensure_extracted` brings the hierarchy up to date as `extract all` would, then `netlist::spice::render` with the `lvs` preset produces the layout netlist in memory. M9 never parses Hefesto's console output, and M7's golden cases fix the text it reads.
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
| extract style built at tech load, rescaled on grid change | `tech/loader.cpp` or `tech/commands.cpp` (the hook M10's TECH adds), `tech/scale_layout.cpp` | EXTTECH, coordinated with M10 TECH |
| `StyleView` re-sourced from `ExtStyle` | `extract/tech/style_view.{hpp,cpp}` | EXTTECH |
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
  | `EXTRACT-PDK-<lib>` (Open Question 4 (a)) | 3 | every cell of `sky130_fd_sc_hd` (446), `gf180mcu_fd_sc_mcu7t5v0` (229), `sg13g2_stdcell` (84, read from the `ihp-gds` set) | one `.ext` per cell |
  | `NETLIST-PDK-<lib>` (Open Question 4 (a)) | 3 | the `.ext` files of the matching EXTRACT-PDK case | `ext2spice lvs` (flat) and `ext2spice hierarchy on format ngspice` per cell, `ext2sim` of the whole library |
  | SCRIPT-API-16, SCRIPT-API-20 | 2 | existing cases | their deferred outputs |

  The total is 154 cases. GOLDEN also audits the catalogues against the 204 exact and behavioural requirements. Each requirement whose behaviour Magic shows gets a case or a pinned unit test. Missing rows are added as EX-071 on and NL-<family>-<n+1> (task 17.2).
- **Authoring.**
  - `tools/golden/authoring/{extract_cases,netlist_cases,antenna_cases,ext_pdk_cases}.py` build the cases, and `m7_cases.py` drives them (`--list`, `--not-authored`, `--exit-check`).
  - Cases are `input/commands.tcl` files that both Magic and `hefesto-cli --magic` run, with `exttest.tech` committed once under `tools/golden/authoring/`.
  - The PDK cases run with `--trust-pdk` and the PDK's default extract style.
- **Session-state pinning.** D18 of M5 applies: a fresh session per case (Q-NETLIST-16 and Q-NETLIST-46 depend on it), `drc off`, `TZ=UTC`, `LC_ALL=C`, and `random seed 1` for PDK cases.
- **Capture.** `golden-capture.yml` gains the outputs above, collected by name pattern (`*.ext`, `*.spice`, `*.sim`, `*.al`, `*.nodes`, `*.res.ext`, `*.res.lump`, `*.fh`). The image builds Magic's `extcheck` (D15). The NETLIST cases need no Hefesto extraction, so they are captured in R1, and SPICE and SIM develop against them from the first day.
- **Comparators.**
  - `ext-cap` (exists): `.ext` files with canonical `cap` records, every other line byte for byte (Q-EXTRACT-7).
  - `spice-cap` (new): flat coupling-capacitor lines, the `C` lines whose two nodes are both non-ground, are compared as a multiset. Terminals are swapped so the smaller name comes first, and the device index is masked. Every other line, the order of device and node lines included, is compared byte for byte (Q-NETLIST-1).
  - `sim-cap` (new): the same rule for `.sim` coupling lines.
  - `res-ext` (new): the resistor records within one net of a `.res.ext` are compared in canonical order (by terminal names, then value), since Q-NETLIST-26 and Q-NETLIST-45 fixed Magic's unstable or address-keyed orders. Node, device and attribute records are compared byte for byte.
  - `magic-console` (exists) for stdout, with the timing lines of ExtTimes masked.
  - PDK cases: `ext-cap` on every `.ext`, `spice-cap` on every netlist. A cell that differs fails the case and is named in the report.
- **DEF write cases (Open Question 9 (a)).** LEFDEF-317..328 and 330..334 keep their Magic-written `.ext` inputs. One new end-to-end case, LEFDEF-336, runs `extract all` then `def write` on LEFDEF-317's layout, so Hefesto's extractor feeds Hefesto's DEF writer once.
- **Deviations.** These need an approved decision:
  - the both-modes fixes whose output Magic can produce in a case: Q-EXTRACT-6 (EX-024's layout labels after the run), Q-EXTRACT-8, Q-EXTRACT-13, Q-EXTRACT-15, Q-NETLIST-6, Q-NETLIST-14, Q-NETLIST-23, Q-NETLIST-42, Q-NETLIST-47 and Q-MISC-3. Each case that exercises one records it in `deviations.toml` with the decision;
  - Q-NETLIST-21 and Q-NETLIST-27 change outputs only where Magic reads uninitialised memory, so the cases avoid those inputs (NL-RES-07 sets the frequency);
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
  - Pinned-value tests run on every leg for: `esSIvalue` and the `%lg`/`%g` outputs, the fringe fractions (Open Question 3), the float32 sidewall denominator, the lumped-resistance `sqrt` correction, the non-rectangular gate estimate, the Elmore delays and the delta-star arms.

### D23. Performance (PERF; Open Questions 5 and 8)

`hefesto-performance` requires extraction and SPICE generation to be no slower than Magic in single-threaded wall time on the same machine. Open Question 5 (a) proposes measuring them as DRC and GDS are measured. The proposed requirement text:

> **Extraction and SPICE generation: instruction counts.** Each reference workload SHALL take at most Magic's callgrind instruction count on the same x86_64 runner. Once Hefesto's count for a workload is recorded as its baseline, the workload SHALL take at most that baseline + 2 %. Wall time is reported for information.

Workloads:
- `extract-bench`: `extract all` of the reference all-cells block of `sky130_fd_sc_hd` (the `gds-bench` block), `ext2spice` flat with `lvs`, `ext2spice hierarchy on`, and `ext2sim`. Hefesto and Magic both run under callgrind.
- `extresist-bench`: `extresist all` on a routed sky130 block. Gated at its own baseline + 2 %, with the Magic count reported (extresist has no spec target).

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
- `m7_cases.py --exit-check` reports the 154 cases passing, or deviating with an approved decision;
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
- `StyleView` (unchanged interface), M10's `ExtractStyleSubset` view
- `testing::StyleBuilder` (scaffold test helper)

### EXTNODE: `hefesto/extract/node/`, `hefesto/extract/file/`
- `RegionFinder::find(const db::CellDef&, const Rect&, const tech::TypeMask&, const ConnectTable&, RegionCallbacks&)`, `NodeRegion`, `LabelRegion`, `reset(const db::CellDef&)`
- `node_name(const NodeRegion&, const compat::CompatSettings&) -> std::string`, `generated_name(...)`
- `UniqueRenamer` (`apply(db::CellDef&, UniqueMode)`, `revert()`)
- `ExtWriter` (`header`, `node`, `attr`, `equiv`, `port`, `substrate`, `device`, `parameters`, `cap`, `resist`, `use`, `merge`, `killnode`, `distance`, `finish`), `format_value(double) -> std::string`

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

## Risks / Trade-offs

- **R1. Port size.** About 58,000 lines of Magic C, twice M6. → Seven rounds, with the netlisters in parallel with the extractor on Magic-written inputs. One dispatch table per reader. One helper per compat branch, and flagged exceptions of at most 25.
- **R2. Parallel work with M10 on `extract/tech/`.** → Open Question 2 (a): the handler files are separate, and only the dispatch table is shared. M10's subset becomes a view over `ExtStyle`, so `LayerStack` is untouched.
- **R3. Memory and speed of the tile client slot.** One more field on every tile of every plane, on desktop and on iPad (`hefesto-performance` "Memory bound on iPad"). → Same layout as Magic's `Tile`. `drc-profile`, `gds-bench` and the iPad memory check run before R2 starts. The fallback, Open Question 1 (b), is kept as an option.
- **R4. Floating-point identity.** `atan` in the fringe-halo model, `%lg` output, float32 sidewalls. → Open Question 3, `-ffp-contract=off` and pinned values per leg. The PDK cases exercise the fringe-halo model on 759 cells.
- **R5. Hash and address order.** `merge` order (hash), `cap` and coupling order (address). → `MagicHashTable`, already validated against Magic's hash, and the `ext-cap`, `spice-cap`, `sim-cap` and `res-ext` canonicalizers, each documented in its module.
- **R6. extresist's direct stitch surgery.** ResFracture breaks the maximal-strip invariant on purpose. → Only on scratch planes, through a restricted header (D6). Debug builds check that no cell plane is passed.
- **R7. `extcheck` absent from Magic's default build.** → GOLDEN builds it in the image (D15). If it no longer builds at the pin, NL-CHK-01 is hand-derived from the source and marked so, with an approved deviation.
- **R8. Malformed input.** → D22 caps, D24 review, fuzz targets.
- **R9. Decisions changing later.** The plan assumes the recommended answers to Open Questions 1–9. → The table "Decisions this design depends on" names the D sections each answer shapes.

## Migration Plan

User data does not change: `.mag` files are read and written as before.
- Technologies load the same way. The extract section is now parsed in full, and PDK sections give no new diagnostics (D7).
- `.ext` files written by Hefesto are read by Magic and netgen, as the goldens check.
- Scripts that call `extract`, `ext2spice`, `ext2sim`, `extresist` or `antennacheck` now run instead of failing as unknown commands.
- `lef write` takes its antenna rules from the shared view. The output is unchanged, as the LEFDEF antenna cases check.
- `CORE-NOTIMPL-007` (or the code the scaffold takes) exists only during M7.
- CI gains the `extcheck` build in the capture image and the `extract-bench` and `extresist-bench` jobs.

## Open Questions

These shape or block the work. **All nine are proposed and wait for the user's decision.** Options are listed with the recommended one first. The decision table holds the same rows ("Planning questions raised by the M7 plan"), and the behaviour question Q-EXTRACT-27 is a proposed entry in `docs/decisions/open-questions/EXTRACT.md` (D12).

1. **Where the extractor keeps its per-tile state.** Magic's extractor stores a region pointer in `ti_client` on every tile it visits and resets it after each pass (ExtRegion.c:71-88, 142-209, 533; about 40 uses in `extract/`). extresist uses it on about 60 lines and calls TiSplitX and TiJoinY1 on its fracture plane (ResFract.c:436-488). Hefesto's `detail::Tile` has no client field, and `split_x`/`join_y` are private.
   - (a) *Recommended:* add one pointer-sized `client` field to `detail::Tile` (Magic's `Tile` has it, tiles/tile.h:73), reached through `Plane::client`, `set_client` and `reset_clients`. Add the restricted header `db/plane_stitch.hpp` for split and join on scratch planes (D6). One field per tile; O(1) access, as in Magic. This is the only way to stay within the "no slower than Magic" target with a literal port.
   - (b) A side table per run, keyed by `TileView::tile` (a hash map), so no tile changes size. The extractor visits every tile several times per pass, which adds a hash lookup to each visit (the cost `connect/search.cpp`'s `std::set` already shows). Stitch edits still need (a)'s header.
   - (c) An extractor-private copy of the tile module with a client field, used for yank buffers and cell planes copied in. The tile code would exist twice, and the copy would cost a pass over every cell.

   *Why (a):* it matches the port one to one, keeps Q-EXTRACT-5's order free (no copy), and its cost (one field) is measured by the existing benches before R2.
2. **Starting M7 while M10's 3D P1 extends `extract/tech/`.** The user asked on 2026-11-20 that M7 run with the 3D work to gain time. M10's TECH (tasks 3.1, 3.2) is writing the style framework and `scale`/`units`/`planeorder`/`height` in the same reader.
   - (a) *Recommended:* start M7 now. EXTTECH writes its handlers in new files (D7) and tests them through a scaffold hook. It wires them into the shared dispatch table after TECH merges, or at once if TECH has merged. The other M7 workstreams build on the scaffold's `ExtStyle` and `StyleBuilder`, not on parsed tech files. M10's `ExtractStyleSubset` becomes a view of `ExtStyle`.
   - (b) Start M7 now except EXTTECH, which waits for TECH's merge. This is safer for M10, but EXTTECH's 22 requirements move to R3, and EXTCAP's and EXTDEV's PDK tests wait with it.
   - (c) Hand `extract/tech/` to M7 now and have M10's TECH consume M7's reader. That stops in-flight M10 work.

   *Why (a):* it gains the most time and limits the conflict to adding rows to one table.
3. **Bit-identical `atan` for the fringe-halo model.** The fringe fractions are `0.6366 · atan(mult · d)` (ExtCouple.c:718-737, 1298, 1492-1508, 1874-1875). The model is on by default for tech format 34 and above, and sky130A, gf180mcuD and ihp-sg13g2 are format 35, so it runs on every PDK cell. `.ext` values must match Magic's (glibc on Linux x86_64) byte for byte after `%lg`, on Linux x86_64, Linux aarch64 and macOS arm64 (`hefesto-output-determinism`). The platform `atan` functions are not guaranteed to agree to the last bit.
   - (a) *Recommended:* a correctly rounded binary64 `atan`, either vendored from CORE-MATH (MIT; a licence check and a `THIRD_PARTY_NOTICES.md` entry) or written in-house from IEEE basic operations as M6 did for the OASIS CIRCLE sine and cosine (Q-OASIS-31). A pinned-value test on the capture image compares it with glibc's `atan` on every argument the EX and PDK cases produce, and any difference is reported before R5.
   - (b) The platform `atan`, accepting rare last-digit differences in `%lg` output on non-glibc platforms, with a per-case tolerance where they appear. This breaks the byte-identity rule.
   - (c) Port glibc's `atan`. It is LGPL, which `hefesto-licensing` does not allow in the core.
4. **Scope of the PDK standard-cell golden cases (the exit criterion).** The roadmap requires `.ext` and SPICE to match Magic's after canonicalization "on the PDK standard cells", without naming cells.
   - (a) *Recommended:* every cell of one standard-cell library per PDK. `sky130_fd_sc_hd` (446 `.mag` cells) and `gf180mcu_fd_sc_mcu7t5v0` (229) come from the existing `.mag` sets, and `sg13g2_stdcell` (84) from the `ihp-gds` set via `gds read`. That gives `EXTRACT-PDK-<lib>` (`.ext` per cell) and `NETLIST-PDK-<lib>` (`ext2spice lvs` flat, `ext2spice hierarchy on` in ngspice format, and `ext2sim`): 6 cases on 759 cells. No new data set is needed.
   - (b) A sample of about 20 cells per library (inverters, NAND/NOR, flops, a mux, a tap, a fill), which captures faster but leaves most device shapes untested.
   - (c) sky130 only.

   *Why (a):* Magic extracts a standard cell in milliseconds, so the full libraries cost little capture time. They cover the device variants, the substrate and well handling and the fringe-halo model of three different tech files.
5. **How the extraction and SPICE performance target is judged.** The spec asks for single-threaded wall time no worse than Magic's (`hefesto-performance` "Batch throughput relative to Magic"). The same requirement already moved DRC and GDS to callgrind instruction counts, because the wall-clock ratio moves by about 0.1× between runs.
   - (a) *Recommended:* amend the requirement so that extraction and SPICE generation are judged like GDS: at most Magic's instruction count on the x86_64 runner, then the recorded baseline + 2 %, with wall time informational (D23 has the text). Workloads: `extract all`, flat and hierarchical `ext2spice`, and `ext2sim` on the `gds-bench` block. `extresist-bench` is gated at its own baseline + 2 %.
   - (b) Keep the wall-time rule as written, measured by repeated runs on one dedicated runner, with Magic run in the same job.
   - (c) Gate on both.
6. **How users run `extcheck`.** `netlist-extcheck` defines a standalone checker with an exit status. Magic builds it only on request (Makefile.in:128-130).
   - (a) *Recommended:* a small executable `hefesto-extcheck` in `src/cli/extcheck/`, installed next to `hefesto-cli` on desktop hosts, calling `netlist::extcheck::run`. It is not shipped on iPadOS, where no process runs it.
   - (b) A mode of the existing CLI, `hefesto-cli --extcheck <args>`. It needs one binary fewer, but scripts that call `extcheck` need a wrapper.
   - (c) Only an `extcheck` console command. That does not meet "Invocation" (exit status 1 on a usage error).
7. **Desktop scope for M7.**
   - (a) *Recommended:* enable the top bar's "Extract" button (disabled since M1 with an M7 tooltip) to run `extract all` on the active window's root, with the existing progress panel and cancel. Netlisting, extresist and antenna checks stay console commands. No new boards.
   - (b) Also add a netlist dialog (format, hierarchy, output file) and an "Extraction" menu. DESIGN draws them first.
   - (c) Leave the button disabled: everything from the console.
8. **Threading.** `hefesto-performance` says multithreaded runs SHOULD be faster, and `hefesto-architecture` requires results equal to single-threaded runs.
   - (a) *Recommended:* single-threaded extraction in M7, with no globals in `ExtractContext` (D5), so that independent cells of the bottom-up stack can be extracted in parallel later without an interface change.
   - (b) Parallel per-cell extraction in M7: cells whose children are done run on a pool, and files and console output are ordered as in a serial run.
   - (c) Parallelism inside a cell, by step squares.

   *Why (a):* the port is large, the gate is single-threaded, and Magic's per-cell order fixes the console output and the incremental decisions.
9. **The DEF-write net cases once Hefesto extracts.** LEFDEF-317..328 and 330..334 take Magic-written `.ext` inputs (m6 Open Question 1 (a)).
   - (a) *Recommended:* keep those inputs, so the cases go on testing `def write` alone. Add one end-to-end case, LEFDEF-336, that runs `extract all` then `def write` on LEFDEF-317's layout.
   - (b) Switch the 17 cases to run `extract all` first. Each case then tests two features, and an extractor regression shows up as 17 DEF failures.

## Resolved Questions

- **The 56 open questions that blocked M7** were approved as drafted on 2026-11-13, with the three judgement items decided as option (a) (Q-EXTRACT-12 tile-exact path length, Q-EXTRACT-2 Magic's diode syntax, Q-NETLIST-37 compat). `docs/decisions/m7-decision-table.md` holds the table, the evidence corrections and the spec changes, and `check_milestone_readiness.py --milestone M7` reports ready. This design cites each decision where it shapes the code (D7–D17, D21, D22).
- **Entries resolved earlier** (2026-11-06/07, by X-1 as amended by D-1, X-2 tier 1, X-3, D-5 and D-14): Q-EXTRACT-5, 7, 8, 9, 13, 14, 15 and Q-NETLIST-1, 2, 3, 11, 14, 21, 22, 26, 27, 33, 36, 42, 45, 47. They are cited in D8–D16, D21 and D22.
- **M6's pull-forward** of the `.ext` subset and the style subset (m6 Open Questions 1 and 2, decided (a) on 2026-11-18) is the starting point of EXTFLAT and EXTTECH.
- **M10's extract subset** (m10 Open Question 4, decided (a) on 2026-11-13): there is one extract reader, and M7 extends it (D7).

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
| Q-EXTRACT-27 (proposed) | fix recommended | D12, D21, D24 |
| Q-NETLIST-8, 9, 10, 12, 16, 30, 42 | compat / replicate / fix | D13 |
| Q-NETLIST-1, 4, 5, 6, 7, 13, 19, 20, 31, 32, 34, 35, 37, 41 | fix / compat | D14, D21 |
| Q-NETLIST-14, 15, 17, 18, 36, 48 | fix / compat / n/a / other | D15 |
| Q-NETLIST-21, 23, 24, 25, 26, 27, 28, 29, 39, 40, 43, 44, 45, 46, 47 | fix / replicate / compat | D16, D21 |
| Q-MISC-1, 3, 4, 7 | compat / fix | D7, D17 |
| Q-NETLIST-11, 33 (D-14) | platform | D5, D10, D13 |
| m6 Open Questions 1, 2 | (a) | Context, D7, D13 |
| m10 Open Question 4 | (a) | D7 |
| Open Question 1 (proposed) | (a) recommended | D5, D6, D16, D20, D23 |
| Open Question 2 (proposed) | (a) recommended | D1, D3, D7 |
| Open Question 3 (proposed) | (a) recommended | D4, D9, D22 |
| Open Question 4 (proposed) | (a) recommended | D21, D25 |
| Open Question 5 (proposed) | (a) recommended | D23 |
| Open Question 6 (proposed) | (a) recommended | D2, D15 |
| Open Question 7 (proposed) | (a) recommended | D19 |
| Open Question 8 (proposed) | (a) recommended | D5, D23 |
| Open Question 9 (proposed) | (a) recommended | D21, D25 |
