# Proposal: M7 Extraction and netlisting

## Why

After M6, Hefesto reads and writes every layout format and exchanges designs with place-and-route flows. It still cannot tell what circuit a layout is:
- `extract`, `ext2spice`, `ext2sim`, `extresist` and `antennacheck` are not registered. Magic's help strings for them are printed, and the UI-G-005 deviation lists them as M7 commands.
- The `extract` section of the technology file is parsed only for the type sets LEF and DEF write need (M6 EXTSUB) and, once M10's 3D P1 lands, for plane order and layer heights. No capacitance, resistance or device rule is read.
- `.ext` files can be read and flattened (M6 EXTSUB), but only Magic writes them: the DEF-write net cases take Magic-written `.ext` inputs, and SCRIPT-API-16 and SCRIPT-API-20 are deferred to M7.
- M9 (LVS and simulation) needs a layout netlist from Hefesto's own extraction (`lvs-command` "Layout netlist from extraction", `simulation-netlist-preparation`), and M10's 3D P3 needs extracted parasitics for its overlays.

M7 (docs/roadmap.md, "M7") delivers:
- hierarchical extraction with parasitics, including the fringe-halo model, and device recognition;
- ext2spice in all five dialects, and ext2sim;
- full R-C extraction with extresist;
- antenna checks.

Its exit criteria are:
- the common exit criteria;
- the EXTRACT and NETLIST golden cases pass;
- `.ext` and SPICE output match Magic's after canonicalization on the PDK standard cells.

The 56 open questions that blocked M7 (19 Q-EXTRACT, 4 Q-MISC, 33 Q-NETLIST) were approved as drafted on 2026-11-13, with the three judgement items decided as option (a) (`docs/decisions/m7-decision-table.md`), and `scripts/check_milestone_readiness.py --milestone M7` reports ready. This plan raises nine planning questions and one behaviour question (Q-EXTRACT-27). All ten are proposed, not decided (design "Open Questions").

## What Changes

- **Decisions first (R0).** The user decides Open Questions 1–9 and Q-EXTRACT-27. If Open Question 5 is answered (a), `hefesto-performance` changes how the extraction and SPICE targets are measured.
- **Reference data and capture (PDK, GOLDEN).**
  - The capture image builds Magic's standalone `extcheck` (not in Magic's default build).
  - New capture modes for `.ext`, SPICE, `.sim`, `.res.ext` and FastHenry outputs. The NETLIST cases, whose inputs are hand-written `.ext` files, are captured in R1, before any Hefesto extraction exists.
  - New comparators `spice-cap`, `sim-cap` and `res-ext`, beside the existing `ext-cap`.
- **The tile database for extraction (INTEGRATION with the MODEL owner; Open Question 1).** `db::Plane` gains a per-tile client slot and a restricted stitch-edit interface (split and join), the two things Magic's extractor and extresist use that Hefesto's plane does not expose.
- **Extraction style (EXTTECH).** `extract-tech-section` is completed as one reader in `extract/tech/`. It builds an immutable `extract::ExtStyle` (the ExtCurStyle data model), loaded at tech load and on `extract style`, converted from microns and rescaled with the grid. `StyleView` (M6) and M10's subset become views of it (Open Question 2).
- **Nodes, labels and the `.ext` writer (EXTNODE).** The region finder over the plane client slot, node names, labels, ports, aliases and unique naming, and one `.ext` record writer that produces Magic's record order (Q-EXTRACT-5, 7).
- **Capacitance and lumped resistance (EXTCAP).** Area, perimeter, overlap with shielding, sidewall, legacy and fringe-halo sideoverlap, corner fringe, substrate coupling, and per-class lumped resistance. Every compat split is a single branch point (Q-EXTRACT-1, 17, 18, 24). The fringe-halo `atan` gives the same bits on every platform (Open Question 3).
- **Devices and substrate (EXTDEV).** Device regions, terminals, dimensions of non-rectangular gates, resistors, capacitors and diodes, model selection by parameter range, parameters, the `device` cell property, isolated substrate regions and substrate nodes.
- **Hierarchy (EXTHIER).** Bottom-up order, interaction halos and step squares, pass-1 connections, name unification, merges, hierarchical coupling, hard-way names, arrays and sticky labels.
- **Commands (EXTCMD).** `extract` with every option, incremental extraction, `extract unique`, output paths, path length, progress and cancel. A cancelled run leaves no truncated `.ext` file, and a cancelled `extresist` no truncated `.res.ext`, `.res.lump` or `.fh` (Q-EXTRACT-27, proposed).
- **Flattening (EXTFLAT).** M6's reader and flattener gain device parameters, coupling and resistor flattening, lumped node resistance, device L/W, one-level flattening for hierarchical output, the `.res.ext` overlay, the common netlister arguments and SPICE and sim name conversion.
- **Netlisters (SPICE, SIM).** `ext2spice` (spice2, spice3, hspice, ngspice, cdl; flat and hierarchical; parallel merging; `lvs`), `ext2sim` (`.sim`, `.al`, `.nodes`) and the standalone `extcheck` (Open Question 6).
- **R-C extraction (RESNET, RESCMD).** extresist's net collection, tile fracturing, breakpoints, tile and contact resistors, reductions, Elmore pruning and simplification; the command, the input model, the two cutoffs, `.res.ext`, `.res.lump`, FastHenry and geometry output.
- **Antenna check (ANTENNA).** `antennacheck` and the antenna rules that `lef write` then reads from the same place.
- **Desktop (SHELL; Open Question 7).** The top bar's disabled "Extract" button is enabled, with progress and cancel. No new boards.
- **Golden, performance, fuzz and exit (GOLDEN, PERF, INTEGRATION).**
  - GOLDEN authors the 70 EXTRACT, 67 NETLIST and 9 MISC-ANT cases, the PDK standard-cell cases of Open Question 4, and the deferred SCRIPT-API-16 and SCRIPT-API-20. It also checks the catalogues against the requirements.
  - PERF adds `extract-bench` (Open Question 5) and `extresist-bench`.
  - Fuzz targets for the extract section, the full `.ext` reader with the netlisters, the `.res.ext` input and the extractor itself.
  - A security review of the new readers and of extraction on hostile layouts.
  - The M7 exit check.

### Non-goals

- LVS, simulation and post-layout simulation (M9). M7 delivers the netlists and the entry points M9 calls (design D18).
- Parasitic and field overlays in the 3D view (M10 P3). M7 delivers the data they draw.
- The extractor debugging wizard `*extract` (`ui-wizard-and-debug` "Layout wizard commands" is informative and lets it be omitted).
- sim2simp (Q-NETLIST-48: not provided; `ext2sim merge` covers it).
- Multithreaded extraction, unless Open Question 8 is answered otherwise.
- An extraction panel or netlist dialog on the desktop (Open Question 7 (a)).
- iPadOS user interface for extraction. The core runs there, and the paths follow Q-EXTRACT-11.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

None as drafted. M7 implements the existing requirements of the 21 capabilities of docs/roadmap.md ("M7"): 207 requirements, 204 of them exact or behavioural and 3 informative only ("Progress and summary messages", "Annotated example", "Deprecated sim2simp filter").

Every open question these specs cite is resolved, and the specs are aligned, so the change sets `skip_specs: true`, as M0–M6 did. Two of the proposed answers would change spec text once decided:
- Open Question 5 (a): `hefesto-performance` "Batch throughput relative to Magic" measures extraction and SPICE generation by callgrind instruction counts, as it already does for GDS. The proposed text is in design D23.
- Q-EXTRACT-27 (fix): `extract-commands` "Incremental extraction" and "Output path and file location" say that a run writes each `.ext` file completely or not at all.

INTEGRATION makes these edits in this change's branch after the decisions. A workstream that finds another requirement that must change raises an open item, as before.

## Impact

- **New code:**
  - `src/core/{include/hefesto,src}/extract/{node,file,cap,device,hier,command,length}/`
  - `src/core/{include/hefesto,src}/extract/tech/` and `src/core/{include/hefesto,src}/extflat/` (extended, not replaced)
  - `src/core/{include/hefesto,src}/netlist/{spice,sim,extcheck}/`
  - `src/core/{include/hefesto,src}/extresist/{network,command,output}/`
  - `src/core/{include/hefesto,src}/antenna/`
  - `src/cli/extcheck/` (Open Question 6 (a))
  - `tests/unit/{extract,extflat,netlist,extresist,antenna}/`
  - `tests/fuzz/{fuzz_extract_section,fuzz_ext_netlist,fuzz_res_ext_reader,fuzz_extract_layout}.cpp`
  - `bench/extract/`, `bench/extresist/`
- **Shared changes:**
  - `db::Plane` gains the tile client slot and the restricted stitch-edit header (Open Question 1).
  - `EditorState` gains `extract`, `netlist` and `extresist`.
  - A new editor-side hook builds the extraction style when a technology loads into the session and rescales it on `scalegrid` and finer-grid reads. No such hook exists (M10's `HeightView` reads `Technology::raw_sections()`), and `tech/` is not changed.
  - `StyleView` (M6) and `HeightView` (M10) are re-sourced from `ExtStyle`; `HeightView::rescaled` follows the session's style, so there is one rescale path. `lef write` and `def write` use the selected extract style instead of the default one, as Magic's writers do.
  - `lef write` reads antenna rules from `antenna::RuleView` instead of its own StyleView query.
  - The command registry gains `extract` (`ext`), `ext2spice` (`exttospice`), `ext2sim` (`exttosim`), `extresist` and `antennacheck`, and `ui/magic_help.cpp`'s UI-G-005 deviation drops them (design D20).
- **Dependencies:** none new in the product, unless Open Question 3 is answered by vendoring a correctly rounded `atan` (MIT). The capture image builds Magic's `extcheck` from the pinned source.
- **Data:** none new for sky130 and gf180mcu (the `.mag` standard-cell sets exist). `sg13g2_stdcell` comes from the existing `ihp-gds-sc` set (Open Question 4 (a)).
- **Golden cases:**
  - EXTRACT: 70 rows in `tests/golden/EXTRACT/CASES.md`.
  - NETLIST: 67 rows in `tests/golden/NETLIST/CASES.md`.
  - MISC: the 9 MISC-ANT rows.
  - PDK: `EXTRACT-PDK-<lib>` and `NETLIST-PDK-<lib>` for the three standard-cell libraries (Open Question 4 (a)).
  - SCRIPT-API-16 and SCRIPT-API-20 leave their `deferred = "M7"` entries.
  - LEFDEF-336, one end-to-end `extract all` then `def write` case (Open Question 9 (a)).
  - 155 cases in all (design D21).
  - Deviations only for the both-modes fixes whose output Magic can produce (design D21).
- **Docs:**
  - `docs/diagnostics/codes/{extract,extflat,netlist,extresist,antenna}.md`;
  - `docs/contributing/` (areas, fuzz targets, golden modules, the security review);
  - `docs/design/m7-ui-conformance.md`;
  - `docs/building.md` (the `extcheck` build in the image, the `ihp-gds-sc` use);
  - the roadmap status and CHANGELOG.
