# Proposal: M3 Design rule checking

## Why

M2 made Hefesto a usable editor, but nothing checks the layout: the technology `drc` section is kept as raw lines, the DRC check and error planes exist only as built-in planes that `.mag` files read and write, no edit queues an area for checking, `drc` is not a command, `tech drc` reports `TECH-CMD-005`, and the desktop top bar shows a static "DRC not run" chip. M3 (docs/roadmap.md, "M3") adds continuous and on-demand DRC whose errors are identical to Magic's: every rule type, background checking that never blocks the UI, interaction checks across the hierarchy, the `drc` command set, mask hints, and a DRC results browser. Mask generation (M5), extraction (M7) and the AI agent's DRC fix loop (M11) build on the rule table, the check kernel and the error planes delivered here. The 24 open questions that blocked M3 are resolved (commit "Resolve the 24 open questions blocking M3") and the M3 specs are aligned with them (commit "Align the M3 specs with the approved decisions"); `scripts/check_milestone_readiness.py --milestone M3` reports ready.

## What Changes

- **Rule language (RULES).** A compiler from the technology `drc` section to Magic's primitive rule table: grammar and unique-prefix keywords, styles and variants, scale factor and rounding, the why-text table, every rule keyword (width, spacing in all adjacencies, widespacing, edge/edge4way, extend, overhang, surround, rect_only, rectangle, area, maxwidth, off_grid, angles, no_overlap, exact_overlap, exception/exemption, option, stepsize), rule ordering, illegal-edge pruning and covered-rule elimination, halo and step size, grid rescaling, the DRC paint table, the mask-layer statements (`cifstyle`, `cif*`) parsed and bound, a read-only catalog of the `cifoutput` styles DRC needs (names, scale, layer names, contact `squares` borders), tech-file lint warnings, and the `tech drc` queries.
- **Rule semantics (CHECK).** The basic check over one flat cell or flattened buffer: edge walk, check regions and corner extensions, Manhattan and Euclidean corners, triggers and the wide-spacing maximal-rectangle search, maxwidth, area, rectangle, off-grid, angles, diagonal edges, and exception/exemption scoping by mask hints, each with both `magic-compat` modes where a decision says so.
- **Engine (ENGINE).** The check and error planes, marking changed areas from every editing path Magic marks, the pending list and step squares, interaction areas, flattening with the DRC paint table, copied child errors, array and exact-overlap checks, `drc check`, the flat check, the halo override, and the background checker on a worker thread (Q-DRC-12), with `drc catchup` synchronous and `magic::drcstate busy|idle` events.
- **Commands (COMMANDS).** `drc` with every option and Magic's text, list and listall results: `on`, `off`, `status`, `catchup`, `check`, `count`, `find`, `why`, `ignore`, `euclidean`, `style`, `printrules`, `rulestats`, `statistics`, `help`, and the wizard options `*halo`, `*stepsize`, `*flatcheck`, `*showint`; why-text substitution in the current units.
- **Mask hints (HINTS).** The hint region in Magic's strip decomposition, the `property` command forms, `.mag` read/write (prefix type keywords, `plane` per magic-compat), flatten/transform/rescale, the inside test for DRC in both modes, and the hint operations the M5 mask readers and writers will call.
- **Desktop DRC (DESIGN, SHELL).** Boards for the M3 states of the DRC results browser and status chip, then the browser (errors grouped by why-text, stepping in `drc find` order, re-check, ignore, style), a live DRC status chip and status-bar state, and the worker integrated with the app loop.
- **Reference PDK data (PDK).** `scripts/fetch_pdk.py` gains a pinned gf180mcuD subset (Magic tech files and the `gf180mcu_fd_sc_mcu7t5v0` layout library) and an ihp-sg13g2 tech subset, installed by Hefesto's own PDK manager from the same ciel release set as sky130, with tree digests, per-set CI cache keys and documentation.
- **Golden and exit checks (GOLDEN, INTEGRATION).** The 113 DRC golden cases and the authorable MISC-MH cases (MISC-MH-08 seeded from the Magic runs behind Q-MISC-13), the cases of earlier areas that waited for DRC, retirement of the `mag-checkpaint` mask, the standard-cell exit check (every `sky130_fd_sc_hd` and `gf180mcu_fd_sc_mcu7t5v0` cell has Magic's error set), a full-hierarchy DRC benchmark against Magic, fuzz targets for the new readers, and the docs.

### Non-goals

- Checking mask-layer rules (`drc-cif-rules`: generating mask layers and checking them) and the cifoutput/cifinput operator pipelines: M5. M3 parses and binds the mask-layer statements so halo, why-texts and load messages match, and leaves the `drc(full)` mask-layer checks to M5 (design D13).
- GDS/CIF reading and writing, and so the `mask-hints` cases that need them (MISC-MH-03..06): M5. M3 provides the hint operations those readers and writers call.
- Tcl: `magic::drcstate` is delivered to the scripting layer as an event now and becomes a Tcl callback with the M4 binding; list results are already Tcl-list values.
- The AI DRC fix loop, net names in the browser, and sign-off items shown on the DRC browser board: M11, M7 and M13.
- iPadOS: the engine is platform-neutral and the worker model is the same on iPadOS (Q-DRC-12); the iPad UI is M14.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

None. M3 implements the existing requirements of the 5 capabilities listed in docs/roadmap.md ("M3"): `drc-rule-language`, `drc-rule-semantics`, `drc-engine`, `drc-commands`, `mask-hints` (72 requirements, 69 of them exact or behavioral). Every open question cited by these specs is resolved and the specs were aligned in the preceding commit. The change therefore sets `skip_specs: true`, following the M0, M1 and M2 precedent (m0-foundations design D10). A workstream that finds a requirement that must change raises it as an open item; INTEGRATION then edits the living spec in this change's branch.

## Impact

- **New code:** `src/core/{include/hefesto,src}/drc/{rules,check,engine,commands}/` and `src/core/{include/hefesto,src}/hints/`; DRC marking calls in the existing `db`, `select`, `cell`, `undo` and `wiring` paths; `src/shell/desktop/` (DRC browser, status chip, worker hosting); `tests/unit/{drc,hints}/`, `tests/data/{drc,hints}/`; `tests/fuzz/fuzz_drc_section.cpp`.
- **Shared changes:** `editor::EditorState` gains the DRC session; a database reader/writer lock implements the `hefesto-architecture` concurrency model (one writer, the command thread; concurrent readers); `db::PlaneValue` becomes a hint region in strip form; `tech drc` and the wiring tool's default width read the compiled rule set; the core is built with `-ffp-contract=off`.
- **Dependencies:** none. The worker uses the standard library threads.
- **Data:** gf180mcuD and ihp-sg13g2 subsets of the ciel release set already pinned for sky130 (open_pdks `0fe599b2`), never committed.
- **Golden cases:** DRC (113), MISC-MH-01, 02, 07, 08 and a new MISC-MH-09, MODEL-G-006, TECH-G44, CMDEDIT-LBL-07 (deferred to M3 by M2), two standard-cell corpus cases; 138 existing cases lose the `mag-checkpaint` mask.
- **Docs:** `docs/design/m3-ui-conformance.md`, the DRC board updates, diagnostic code documents, building (PDK data sets, DRC settings), contributing, roadmap status.
