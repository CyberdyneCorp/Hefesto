# Design: M3 Design rule checking

## Context

See proposal.md for motivation. M2 (openspec/changes/m2-interactive-editing) delivered the editing command set, layout undo, selection and connectivity, dispatch with macros and tools, highlights, the wiring tool and the desktop editor. The conventions of M0-M2 still hold: SPDX headers, Catch2 tags `[cap:<capability>][req:<exact requirement heading>]`, per-area diagnostic code tables `AREA-TOPIC-NNN` documented in `docs/diagnostics/codes/<area>.md`, cognitive complexity at most 15 per function (flag irreducible ones), no UI/GPU/Raven/process headers in the core, golden expected outputs captured only in CI (`.github/workflows/golden-capture.yml`), Magic 8.3.694 at fc5daa9e as the behavioral reference (`build/ref/magic`, gitignored; a full clone is needed for porting), clean-room rule for netgen/xschem/KLayout.

What M2 left that M3 builds on (from the code, 2026-11-10):

- **Technology.** The `drc` and `cifoutput` sections are kept verbatim as `tech::RawSection` lines; only the first cifoutput style's grid values are read (`Technology::cif_output_grid`). The built-in types `checkpaint`, `checksubcell`, `error_p`, `error_s`, `error_ps` and the planes `designRuleCheck`/`designRuleError` exist (`tech/sections.cpp`). `tech drc` fails with `TECH-CMD-005`. The wiring tool parses the raw `drc` section itself for its default width (`wiring/wiring.cpp`, DRCGetDefaultLayerWidth).
- **Database.** The DRC planes are ordinary planes of `db::CellDef`; `.mag` read/write handles `<< checkpaint >>` and error sections like paint, and `db/tech_masks.hpp` has `all_but_space_and_drc()`. Mask hints are `db::PlaneValue` = a vector of rectangles, not a region with Magic's strip decomposition. No edit queues a DRC area: `cmd-paint-erase` item 7 and Magic's 55 `DRCCheckThis` call sites outside `drc/` have no counterpart (comments in `undo/undo_log.cpp`, `wiring/session.cpp`).
- **Commands.** `drc` is not registered (it is in the `help` text table, `ui/magic_help.cpp`). The default macros already bind `?` to `drc why` and `!` to `drc find; findbox zoom; zoom 16` (`ui/bindings.cpp`). Command results carry `cmd::Value`, which can be a nested list (used by `what -list`).
- **Concurrency.** Everything runs on the command thread; `platform::TaskExecutor` exists; there is no database lock. `hefesto-architecture` "Concurrency model" asks for one writer (the command thread) and concurrent readers (rendering, background DRC).
- **Diagnostics.** `--diagnostics=magic` prints every diagnostic's text, so a Hefesto-only warning would show up in golden console captures.
- **Display and desktop.** The scene draws the root cell's error plane ("DRC error display", `display/scene.cpp`). The top bar shows a static `DRC not run` chip (`shell/desktop/src/view_models.cpp`, `drc_chip_m1`). The board `docs/design/mockups/DRC-Browser.dc.html` (M3) shows the browser with later-milestone parts (agent fix, nets, sign-off).
- **Golden.** 138 captured cases carry the `mag-checkpaint` mask because Magic's edits leave check areas that Hefesto did not write; CMDEDIT-LBL-07 is `deferred = "M3"` (it needs the check plane); MODEL-G-006 and TECH-G44 are not authored (need `drc`, `tech drc`); `tests/golden/DRC/` has only `CASES.md` (113 rows); no MISC-MH case is authored.
- **PDK data.** `scripts/fetch_pdk.py` installs one sky130A subset (open_pdks `0fe599b2`, ciel release `sky130-0fe599b2…`) through `hefesto-cli pdk install … -only …`, verified by a tree digest; CI caches it keyed on the script's hash.

M3 runs as a scaffolding step (interfaces, `EditorState` members, the `drc` command stub, the build contract), a design step for the DRC screens, a PDK data step, six parallel workstreams and an integration step. The directory layout, public headers, ownership table and shared-file rules below are the contract that lets the workstreams work without editing each other's files.

## Goals / Non-Goals

**Goals:**
- The M3 exit criteria of docs/roadmap.md: the common exit criteria for the 5 M3 capabilities (unit tests tagged for every exact/behavioral requirement, 69 of them; fuzz targets for new readers; CI green on every leg; byte-identical output on Linux x86_64, Linux aarch64 and macOS arm64), the DRC golden cases pass (D12, Resolved Question 1 for the mask-layer cases), and the sky130 and gf180mcu standard cells have exactly Magic's error sets (D14).
- Background DRC that never blocks input and whose final result equals a single-threaded run (`hefesto-architecture` "Concurrency model"), starting on an edited area within 100 ms (`hefesto-performance` "Edit latency"), and full-hierarchy DRC no slower than Magic single-threaded ("Batch throughput relative to Magic").
- Interfaces that M5 (mask-layer rules, hint operators, GDS reading with check marking), M7 (extraction reusing the flattening and the paint tables) and M11 (agent DRC loop reading errors and why-texts) extend without reshaping them.
- The items earlier milestones deferred to M3 are closed (D17).

**Non-Goals:**
- Mask generation and mask-layer checks (`drc-cif-rules`, M5); GDS/CIF I/O (M5); Tcl (M4); net names, agent fixes and sign-off in the browser (M7, M11, M13); the iPad UI (M14).
- Rewriting M1/M2 code that meets its spec. M3 changes earlier code only where an M3 requirement needs it: marking calls, the hint region type, the database lock, `tech drc`, the wiring default width.

## Decisions

### D1. Layout of the M3 code

| Area | Public headers | Sources | Unit tests | Namespace |
|---|---|---|---|---|
| rule compiler, rule table, cifoutput catalog | `src/core/include/hefesto/drc/rules/` | `src/core/src/drc/rules/` | `tests/unit/drc/rules/` | `hefesto::drc` |
| basic check kernel | `src/core/include/hefesto/drc/check/` | `src/core/src/drc/check/` | `tests/unit/drc/check/` | `hefesto::drc` |
| engine: planes, marking, squares, hierarchy, worker | `src/core/include/hefesto/drc/engine/` | `src/core/src/drc/engine/` | `tests/unit/drc/engine/` | `hefesto::drc` |
| `drc` command and reporting | `src/core/include/hefesto/drc/commands/` | `src/core/src/drc/commands/` | `tests/unit/drc/commands/` | `hefesto::drc` |
| shared DRC types (session, sink, events) | `src/core/include/hefesto/drc/{session,types,events}.hpp` | `src/core/src/drc/` | `tests/unit/drc/` | `hefesto::drc` |
| mask hints | `src/core/include/hefesto/hints/` | `src/core/src/hints/` | `tests/unit/hints/` | `hefesto::hints` |
| desktop DRC browser and status | `src/shell/desktop/include/hefesto/shell/` | `src/shell/desktop/src/` | `tests/unit/shell/` | `hefesto::shell` |

All new core code is part of `hefesto_core`. `src/core/CMakeLists.txt` adds `drc hints` to `HEFESTO_CORE_AREAS`. `src/core/src/drc/sources.cmake` (INTEGRATION) lists the shared files and includes one `sources.cmake` per subdirectory, owned by its workstream; `tests/unit/drc/CMakeLists.txt` does the same for the test subdirectories. Diagnostic prefixes: `DRC-` (one code table for the area, assembled by `drc/codes.cpp` from per-workstream fragments `drc/<sub>/codes.inc` with the topics `DRC-TECH-*`, `DRC-LINT-*` (RULES), `DRC-CHECK-*` (CHECK), `DRC-ENG-*` (ENGINE), `DRC-CMD-*` (COMMANDS); one document `docs/diagnostics/codes/drc.md` with one section per workstream) and `HINTS-` (`docs/diagnostics/codes/hints.md`).

### D2. Workstreams and ownership

| Workstream (tasks.md section) | Capabilities | Owns (may edit) |
|---|---|---|
| DESIGN | `hefesto-design-system` (screen patterns), `hefesto-app-ux` (DRC browser) | `docs/design/mockups/DRC-Browser.dc.html`, new `docs/design/mockups/M3-*.dc.html`, `docs/images/ui/m3-*`, `docs/design/mockups/{README.md,canvas.json}` |
| PDK | — (test data for every M3 capability) | `scripts/fetch_pdk.py`, `scripts/tests/test_fetch_pdk.py`, `tests/unit/common/reference_data.hpp`, the reference-data steps of `.github/workflows/{ci,golden-capture}.yml`, `docs/building.md` "Reference data for tests" |
| RULES | `drc-rule-language`; `tech-command` "tech drc" | `src/core/{include/hefesto,src}/drc/rules/`, `tests/unit/drc/rules/`, `tests/data/drc/rules/`, `docs/diagnostics/codes/drc.md` (RULES section), the DRC-default-width functions in `src/core/src/wiring/wiring.cpp`, `kDrc` in `src/core/src/tech/commands.cpp` |
| CHECK | `drc-rule-semantics` | `src/core/{include/hefesto,src}/drc/check/`, `tests/unit/drc/check/`, `tests/data/drc/check/`, `drc.md` (CHECK section) |
| ENGINE | `drc-engine` | `src/core/{include/hefesto,src}/drc/engine/`, `tests/unit/drc/engine/`, `tests/data/drc/engine/`, `drc.md` (ENGINE section); the one-line `drc::check_this` calls at the marking sites in `src/core/src/{db,select,cell,undo,wiring}/` (D7), the `checkpaint` scheduling and format-error marking in `src/core/src/cell/mag_reader*.cpp` |
| COMMANDS | `drc-commands` | `src/core/{include/hefesto,src}/drc/commands/`, `tests/unit/drc/commands/`, `tests/data/drc/commands/`, `drc.md` (COMMANDS section) |
| HINTS | `mask-hints` | `src/core/{include/hefesto,src}/hints/`, `src/core/{include/hefesto,src}/db/property.*` (the `PlaneValue` replacement), the hint paths of `db/flatten.cpp`, `db/commands_labels.cpp` (`property` with `MASKHINTS_`) and `cell/mag_reader_records.cpp`/`cell/mag_writer.cpp` (hint lines, type keywords), `tests/unit/hints/`, `tests/data/hints/`, `docs/diagnostics/codes/hints.md` |
| SHELL | `hefesto-app-ux` (DRC browser, status), desktop parts of `drc-engine`/`drc-commands` | `src/shell/desktop/`, `tests/unit/shell/`, `docs/design/m3-ui-conformance.md` |
| GOLDEN | the golden cases of the 5 capabilities | `tools/golden/` (authoring modules `drc_cases.py`, `m3_cases.py`, additions to `misc_cases.py`, `model_cases.py`, `tech_cases.py`, `cmdedit_cases.py`; comparators and masks), `tests/golden/` (`CASES.md` rows, `cases/`, `expected/`, `deviations.toml`), `openspec/changes/m3-drc/seeds/` |
| INTEGRATION | — | everything shared: root and `src/core/CMakeLists.txt`, `src/core/src/drc/{sources.cmake,codes.cpp}` and the shared `drc/` headers, `CMakePresets.json`, `cmake/`, `tests/unit/CMakeLists.txt`, `tests/fuzz/`, `src/core/{include/hefesto,src}/{cmd,diag,platform,settings,compat,util,editor,app,tech (other files),pdk}/`, `src/cli/`, `.github/` (other steps), `scripts/` (other files), `README.md`, `docs/roadmap.md`, `docs/building.md` (other sections), `docs/contributing/`, `docs/decisions/`, `docs/diagnostics/codes.md`, `THIRD_PARTY_NOTICES.md`, `openspec/specs/` (open items), this change's artifacts |

Rules (unchanged from M1/M2 D2):
- A workstream edits only what it owns. A needed change to a shared or another workstream's file is an `open_item` (what, why, the exact edit if small); the owner or INTEGRATION applies it.
- New golden case directories are authored by GOLDEN from the requirement and the owning workstream's notes; `expected/` is captured in CI.
- Only the integration step commits and pushes, to `origin m3-drc` (or the branch its prompt names).

Dependencies: the scaffold (section 1) lands first and publishes the interfaces of D4 with stubs. RULES produces the rule table CHECK reads, but CHECK starts at once on hand-built rule tables in its tests (the rule types are scaffold headers). ENGINE starts on the stub kernel and a fake rule set. COMMANDS starts on the engine's stub API. HINTS has no dependency. DESIGN boards land before SHELL builds the browser. GOLDEN authors inputs from day one; replay is meaningful once RULES, CHECK and ENGINE land.

### D3. Build and stub contract

- **Sources and tests.** As M2 D3, with the per-subdirectory `sources.cmake` of D1.
- **Stubs.** The `drc` command is registered at scaffold time in COMMANDS with Magic's option table and a stub handler per option that fails with a new code `CORE-NOTIMPL-003` ("not implemented yet: drc <option>"; `CORE-NOTIMPL-001/002` stay retired, never reused), so `help`, completion, macros (`?`, `!`) and golden scripts see the final command from day one. INTEGRATION retires `CORE-NOTIMPL-003` at M3 exit.
- **Scaffold tests** carry `[m3-scaffold]` and no `[cap:][req:]` tags; owners replace them.
- **Floating point.** The core is compiled with `-ffp-contract=off` (GCC and Clang) so that the Euclidean corner filter (single-precision projection) and the diagonal offsets (`sqrt` in double) round exactly as Magic's x86_64 build and identically on x86_64 and aarch64 (GCC in GNU mode defaults to `-ffp-contract=fast` and Clang to `-ffp-contract=on`; both fuse multiply-adds into FMA instructions on aarch64, which the x86_64 baseline does not have). A determinism unit test pins the results of these formulas on edge values.

### D4. Session state and shared interfaces

`editor::EditorState` gains `drc` (`drc::DrcSession`, owned by INTEGRATION as a struct whose members are owned by the named workstream):
- `rules` (`std::shared_ptr<const drc::RuleSet>`: the compiled current style; replaced, never mutated, on `drc style`, `tech load`, grid rescale and cifoutput style change, so the worker keeps a consistent copy) and `styles` (declared style names) — RULES;
- `background` (off / on / not set), `pending` (`drc::PendingList`), `worker` (`drc::BackgroundChecker`), `halo_override`, `step_size`, `statistics` — ENGINE;
- `euclidean`, `ignore` (why indices), `find_counter` (initial value per magic-compat, Q-DRC-15) — COMMANDS.

Shared interfaces created by the scaffold (INTEGRATION owns the files; the named workstream fills the implementation):
- `drc::RuleSet`, `drc::Rule`, `drc::RuleFlags`, `drc::WhyTable` (`drc/types.hpp`): the compiled table CHECK and COMMANDS read — RULES.
- `drc::ErrorSink` (`drc/types.hpp`): `report(Rect, const Rule&)` plus the error counter; implementations paint a temporary error plane (engine), collect why-texts (`drc why`), count (`*flatcheck`) — used by CHECK, implemented by ENGINE/COMMANDS.
- `drc::CheckInput` and `drc::basic_check(const CheckInput&, Rect K, Rect L, ErrorSink&)` (`drc/check/basic_check.hpp`): the kernel entry ENGINE calls. `CheckInput` names a read-only cell (its planes, its hint regions), the rule set, the Euclidean flag, the compat settings and the cifoutput catalog — CHECK.
- `drc::check_this(EditorState&, db::CellDef&, std::optional<db::Rect>)` (`drc/engine/marking.hpp`): Magic's `DRCCheckThis` (paint `checkpaint` over the area ⊕ halo, enqueue, propagate to parents); a no-op stub until ENGINE fills it. Every marking site calls only this function (D7).
- `drc::DrcEvents` (`drc/events.hpp`): an observer list for `busy`/`idle` transitions, committed squares (redisplay area) and style changes, consumed by the shell, the CLI and, in M4, the Tcl binding (`magic::drcstate`).
- `db::DatabaseLock` (`db/lock.hpp`): a reader/writer lock over the cell library. The dispatcher holds it exclusively for every command; readers (scene building in the shell, the DRC worker) hold it shared. A writer that wants the lock first asks the worker to yield (D8).
- `hints::HintRegion` (`hints/region.hpp`): replaces `db::PlaneValue` behind the same property API (D10) — HINTS.
- `drc::CifCatalog` (`drc/rules/cif_catalog.hpp`): the read-only view of the cifoutput styles DRC needs (D5) — RULES.
- **Hefesto-only diagnostics.** `diag::Diagnostic` gains a `native_only` flag; the `magic` diagnostics format (golden replay, `--diagnostics=magic`) drops such diagnostics and `text`/`json` print them. The tech-file lint warnings of Q-DRC-3/18/19 (`DRC-LINT-*`) use it, so golden console captures stay identical to Magic's.

### D5. Rule compilation (RULES)

- **Port, not reinvention.** The compiler is a port of `drc/DRCtech.c` keyword by keyword, with the spec's primitive-rule notation as the test oracle (each spec scenario becomes a test on the compiled table) and `drc printrules` output compared with Magic's in golden cases.
- **Table layout.** While a style loads, rules for edge pair (A,B) live in insertion-ordered lists that implement the spec's ordering rules (trigger pairs, the appended coincidence rule, the rectangle size rule). Finalization (pruning, covered-rule elimination with the Q-DRC-5 mode switch) freezes them into one contiguous `std::vector<Rule>` plus a CSR offset table indexed by A·N+B, which the kernel walks without pointer chasing; the frozen `RuleSet` is immutable and shared (D4).
- **Values.** Each rule keeps its tech-file values, its compiled values and Magic's one-byte remainders, so rescaling can follow Magic's restore-and-rescale (magic-compat ON) or recompute from the tech-file values (OFF), Q-DRC-1. The halo keeps tech-file and mask-layer contributions apart so both Q-DRC-2 modes are computable.
- **Mask-layer statements.** `cifstyle` binding, `cifwidth/cifspacing/cifarea/cifmaxwidth` token counts, why registration, layer lookup in the bound style and halo contribution are implemented now (so load messages, why indices and halo match Magic, golden DRC-CIF-1 and DRC-CIF-8); the compiled mask-layer rules are kept in the `RuleSet` and checked from M5 (D13).
- **cifoutput catalog.** RULES reads, from the raw `cifoutput` section, what DRC needs before M5: style names in declaration order and the current style, each style's scalefactor and reducer, its layer names, and for each contact type the `squares` operator border (Magic's `CIFGetContactSize`) used by the Euclidean derived borders and by step size (`6400/cifScale`). M5's `mask-output-styles` reader replaces the catalog's source without changing its interface.
- **Lint.** Q-DRC-3 (dropped plane pairs), Q-DRC-18 (cross-plane `exact_width`) and Q-DRC-19 (exclude types with `bend_illegal`/`both`) emit `DRC-LINT-*` native-only warnings in both modes (D4).
- **Consumers.** `tech drc` (`tech-command` "tech drc"), the wiring tool's default width and the step size come from the `RuleSet`; `wiring/wiring.cpp` stops parsing the raw section.

### D6. Check kernel (CHECK)

- **Port of `drc/DRCbasic.c`, `drc/DRCextend.c` and `utils/maxrect.c`** over `db::Plane`'s read-only tile views, which already reproduce Magic's tiling and enumeration order (X-1). Area, maxwidth, exception and diagonal results depend on tile structure, so the kernel never re-tiles or clips its input.
- **Modes.** Every compat quirk of `drc-rule-semantics` (Q-DRC-7, 9, 10, 11/Q-MISC-13, 20) is one branch on `CheckInput::compat.magic_compat`, with both modes tested; replicated quirks (Q-DRC-6, 8, 19, 21) have no switch.
- **Euclidean and diagonal arithmetic** use the exact types of Magic's code (`float` for the projection term, `double` for `sqrt`, `long` sums) under D3's floating-point rule; a table-driven test pins edge cases (`sqx·k+b` overflow range, s at 4095/4096, equal distances).
- **Statistics.** The kernel counts tiles, edge pieces, constraint areas and multi-tile constraints for `drc statistics` through the `ErrorSink` context.
- **Complexity.** `drcTile` is a single 900-line function in Magic; the port splits it into edge walk, rule dispatch, region build, corner extension and candidate search so every function stays at most 15, except the edge-walk dispatcher if it is genuinely irreducible (flagged, at most 25).

### D7. Engine (ENGINE)

- **Planes.** The check and error planes are the existing built-in planes; the error-plane paint table composes P and S into PS as specified. Error tiles stay out of paint commands, selection, extraction and mask generation (`all_but_space_and_drc` everywhere); `.mag` read of a `<< checkpaint >>` section enqueues the cell, and a format error marks the whole cell (`DRCCheckThis` in `dbCellReadDef`'s `badfile` path).
- **Marking sites.** ENGINE ports every Magic `DRCCheckThis` call site that has a Hefesto counterpart, with Magic's area at each: paint/erase/fill/corner/contact/polygon/splitpaint/drop and the wire paths (`commands/`, `wiring/wireOps.c`), selection delete/copy/move/stretch/transforms and `getcell`/`dump` placement (`select/selOps.c`, `selCreate.c`), cell use place/delete/array/identify and `flatten` (`commands/CmdE.c`, `CmdFI.c`, `CmdLQ.c`, `CmdRS.c`, `CmdSubrs.c`, `CmdTZ.c`), undo/redo replay (`database/DBundo.c`), timestamp-mismatch bounding-box changes (`database/DBtimestmp.c`), `.mag` reads (`database/DBio.c`), and `drc check`/`DRCCheckThis` itself. Sites of later milestones (CIF/GDS/LEF read, plowing, routers, netlist wiring) are listed in `drc.md` with their milestone. A `[m3]` checklist test greps the call-site table so none is missed.
- **Squares and hierarchy.** Step squares, the interaction grid, interaction areas, flattening into an internal yank cell with the DRC paint table (subcell-error type, why 2; non-Manhattan off-grid overlap, why 5), hint flattening, copied child errors (why 4), array regions and the exact-overlap check are ports of `drc/DRCcontin.c`, `DRCsubcell.c`, `DRCarray.c`, `DRCmain.c`. The yank cell is a library-internal `db::CellDef` never listed or saved.
- **Modified flag.** A committed square marks the cell modified unless it is non-editable or a vendor-GDS cell (property `GDS_FILE`, `CDVENDORGDS`), without changing its timestamp.

### D8. Background checker and concurrency (Q-DRC-12)

- **Worker over a frozen database.** The resolution asks for a worker checking one square at a time "over an immutable snapshot", committing only if the edit generation is unchanged. Copying the hierarchy per square would change tile structure at the copy boundary (area and maxwidth floods, exception tests and diagonals depend on it) or cost O(cell) per square, so the snapshot is realized by the `db::DatabaseLock` of D4: the worker holds the lock shared while it checks one square into a private temporary error plane; nothing can change the database meanwhile, so the worker reads an immutable state with Magic's exact tiling. This is the "one writer (the command thread), concurrent readers" model of `hefesto-architecture`.
- **Yield and discard.** Before a command takes the lock exclusively it sets the worker's cancel flag; the worker polls it between tiles (bounded wait well under 1 ms) and releases the lock, dropping its temporary result (Magic's `DRC_BREAK_PENDING`). Each completed square records the database's edit generation at its start; the commit (erase `checkpaint`/`error_p`/`error_s`, paint the new errors, one atomic step on the command thread between commands) is applied only if the generation is unchanged, otherwise the square's check tiles stay for a retry. Commands that make no edit do not bump the generation.
- **Order.** The worker takes squares in Magic's order: pending cells in list order (children before parents) and the first `checkpaint` tile in area-enumeration order. Final error planes are therefore identical to a single-threaded run, which a stress test checks (random edits racing the worker vs. `drc catchup` on the same edit sequence) under ThreadSanitizer (a new `linux-tsan` preset and CI leg).
- **Hosts.** `drc catchup` runs the same square loop synchronously on the command thread. The desktop app runs the worker whenever background DRC is on and commits from its event loop between commands. `hefesto-cli` never runs the worker while executing a script or `-c` commands, matching Magic in batch mode, where the idle loop never runs inside a script (golden `.mag` captures contain the `checkpaint` areas edits leave); its interactive prompt runs the worker while waiting for input. `magic::drcstate busy|idle` are `drc::DrcEvents` at Magic's transitions.
- **Latency.** Marking is synchronous with the edit; the worker is woken at the end of the command, so checking of the edited area starts within the 100 ms of `hefesto-performance` "Edit latency" (benchmarked, D16).

### D9. Commands and results (COMMANDS)

- **Option table and text.** A port of `CmdDrc` (`commands/CmdCD.c`) and `drc/DRCmain.c` reporting: Magic's option table and help, unique-prefix lookup with the wizard `*` names, wrong-argument messages, `drc list` alone prints help (Q-DRC-22).
- **Values.** `count`, `find`, `why`, `listall why`, `style`, `status` return Magic's Tcl structures as `cmd::Value` lists (nested lists of strings and integers); the CLI prints them in Tcl list syntax, and the M4 binding hands them to Tcl unchanged (`drc-commands` "Scripting results on iPadOS and non-Tcl hosts").
- **Determinism.** `drc count` lists cells by name (X-1, `hefesto-output-determinism`); `drc listall why` texts follow Magic's hash order only informatively, so Hefesto emits them in first-discovery order and the golden comparator canonicalizes both (D12).
- **Modes.** Q-DRC-14 (total line), Q-DRC-15 (find counter) and Q-DRC-17 (scan range) switch on magic-compat; Q-DRC-16 copies unknown `%x` literally in both modes. Why-text values use `cmd::format_value` and `editor::coordinate_units` (D-units of `cmd-coordinates-and-units`).

### D10. Mask hints (HINTS)

- **Region type.** `hints::HintRegion` stores a hint as a `db::Plane` of one hint type painted with Magic's CIF paint table, so its tiles are Magic's maximal horizontal strips (needed by the compat inside test, Q-DRC-11/Q-MISC-13, and by the `.mag` strip order). It replaces `db::PlaneValue` inside `db::PropertyValue`; the property store API keeps its shape.
- **I/O and modes.** Read: type keyword by case-insensitive unique prefix, any keyword for `MASKHINTS_` keys (Q-MISC-10), `plane` per magic-compat (Q-MISC-12: format error and rest of file dropped with it ON, accepted with it OFF); write: `string` (or `dimension` with `property compat false`), never `plane`.
- **Inside test.** `hints::inside(const HintRegion&, Segment, bool magic_compat)` implements both modes; CHECK calls it.
- **M5 handoff.** The `mask-hints` requirements "Mask-hint output operator", "Mask hints from cifinput templayers" (union, Q-MASK-308/Q-MISC-11) and "Mask hints generated by gds maskhints" are implemented as library operations (`hints::output_region(cell, name, scale)`, `hints::add_input_region(cell, name, region)`, `hints::gds_maskhints_region(input, generated, scale)`) with unit tests tagged by those requirements; M5's operator pipelines and GDS reader call them, and MISC-MH-03..06 are authored in M5.

### D11. Desktop DRC (DESIGN, SHELL)

- **Board first.** DESIGN updates `DRC-Browser.dc.html` with the M3 states and adds `M3-DRC-Status.dc.html`: the top-bar chip and status-bar DRC state (not run, checking, clean, N errors, off), the browser's empty, running and stale states, errors grouped by why-text with counts, stepping (`#k of n`), the error view on the canvas, ignored rules, the style selector, and a `drc why` popover. Elements of later milestones on the existing board (Fix with agent, nets, measured-versus-required values, sign-off panel) are shown disabled with their milestone in the tooltip, or hidden, as M1/M2 did for later actions.
- **Commands only.** Every browser action is a command through the dispatcher (`drc find n`, `drc why`, `drc check` + `drc catchup`, `drc ignore`, `drc style`), so it is logged and replayable; the list itself is built from `drc listall why` over the root cell (virtual Raven Table, 200,000-row scenario of `hefesto-app-ux`).
- **Status.** The chip and status bar follow `drc::DrcEvents` and the root cell's error-tile count; the browser marks itself stale after edits until the worker goes idle.
- **Conformance record.** `docs/design/m3-ui-conformance.md`, same format as M2's.

### D12. Golden cases

- **Authoring.** `tools/golden/authoring/drc_cases.py` (the 113 DRC cases, with the committed synthetic `drctest.tech` of `tests/golden/DRC/CASES.md`: planes well, active, metal1, via1, metal2, the listed types, a cifoutput style at 0.01 µm per unit with a `squares` contact layer, and per case the rule lines) and `m3_cases.py` (driver as `m2_cases.py`: `--list`, `--not-authored`). The capture procedure CAP of `CASES.md` (`drc off; drc check; drc catchup; drc listall why; drc list count total`, save, `drc printrules`) is one helper. Cases with a magic-compat dependence run with the default (ON); the native mode is covered by unit tests only, as in M2.
- **Other areas.** MISC-MH-01, 02, 07, 08 in `misc_cases.py`, with MISC-MH-08 built from `seeds/mask-hints/` (the Magic runs behind Q-MISC-13: left/right/above/below touch, L-shaped hint, far corner) and a new MISC-MH-09 (Q-MISC-12, `plane`/`PLANE`/`str` keywords, from the same seeds; GOLDEN adds its `CASES.md` row); MODEL-G-006, TECH-G44 and the DRC parts of CELL-TS-01..03, 05..08 and CELL-RD-10 that are now authorable; CMDEDIT-LBL-07 loses `deferred = "M3"`. MISC-MH-03..06 stay not authored with reason "needs GDS (M5)".
- **Comparators and masks.** A canonicalizer for `drc listall why` (texts sorted, rectangles kept in discovery order per text) and `drc count`/`drc listall count` cell order; a derived-output reducer that extracts only the `checkpaint`/`error_*` sections of a set of saved `.mag` files (used by the standard-cell corpus, D14). The `mag-checkpaint` mask is retired once marking is complete: the 138 cases that carry it are replayed without it and must still pass (D17).
- **Deviations.** DRC-CIF-2..7 (mask-layer checks) are `deferred = "M5"`; DRC-CIF-1 and DRC-CIF-8 (load messages, halo) pass in M3 (Resolved Question 1). Any other difference needs an approved decision.
- **PDK cases.** DRC-PDK-1 (sky130A), DRC-PDK-2 (gf180mcuD standard-cell row) and DRC-PDK-3 (ihp-sg13g2) use `[[external]] source = "pdk"` inputs (extended to name the data set). DRC-PDK-3's "cap_cmomf layout" is drawn with `box`/`paint` on the IHP layers (a MOM finger structure), because the IHP device generators are Tcl (M4); its row is reworded accordingly.

### D13. What M3 owns versus M5

| Item | M3 | M5 |
|---|---|---|
| `cifstyle`/`cif*` statements | parse, bind, why registration, halo (both Q-DRC-2 modes), load messages | mask-layer generation and checks (`drc-cif-rules`), Q-DRC-13 |
| cifoutput sections | read-only catalog: names, scale, layers, `squares` borders (D5) | full `mask-output-styles` reader |
| PDK styles | default styles (`drc(fast)` in sky130A and gf180mcuD) fully checked; `drc(full)`/`drc(routing)` load and check every non-mask rule | mask-layer rules of `drc(full)` |
| `mask-hints` | data model, `property`, `.mag`, flatten, DRC scoping, operator library functions | operator wiring in cifoutput/cifinput, `gds maskhints`, MISC-MH-03..06 |
| Marking sites | every site with an M1-M3 counterpart | CIF/GDS read sites (LEF M6, plowing/routers later) |

### D14. Standard-cell exit check

- **Mechanism: golden corpus cases.** Two cases, `DRC-STD-SKY130-HD` and `DRC-STD-GF180-7T5V0`, are generated by `drc_cases.py` from the installed library (one block of commands per `.mag` cell in sorted order: `load <cell>`, `select top cell`, `drc check`, `drc catchup`, `drc listall why`, `drc list count total`, `save` into the work directory), with the PDK's startup settings applied as commands (`drc euclidean on`; the default DRC style). The per-cell command list is generated because `hefesto-cli` has no Tcl loop before M4; the Magic reference script is the same list. Outputs: stdout (canonicalized why lists and counts, D12) and one derived file with the DRC sections of every saved cell (D12 reducer). Expected outputs are small: clean cells contribute one count line and no sections.
- **Why golden and not a separate comparison job:** the cases reuse capture in the pinned Magic container, replay on every CI leg (so the error sets are also checked for cross-platform determinism), and deviations go through `deviations.toml` like any other case. They are tagged `golden-slow` and run in the golden job, not in the per-leg unit run.
- **Scope.** All 441 `sky130_fd_sc_hd` cells and every cell of `gf180mcu_fd_sc_mcu7t5v0` under each PDK's default style. The exit criterion "DRC-clean exactly where Magic reports them clean, with identical error sets elsewhere" is met when both cases pass without a deviation.

### D15. Reference PDK data (PDK)

- **Same release set.** open_pdks `0fe599b2afb6708d281543108caf8310912f54af` (the sky130 pin) also has the ciel release `gf180mcu-0fe599b2…` (published 2025-04-17, minutes after `sky130-0fe599b2…`), and ciel published `ihp-sg13g2-ddb601a4a4473163e1ed6df416b885df18b4ac03` from IHP-Open-PDK the same day. Using that set keeps one consistent snapshot of the Magic tech generators and avoids a second open_pdks pin.
- **Subsets.** `gf180mcu`: `gf180mcuD/libs.tech/magic` and `gf180mcuD/libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag` (assets `common.tar.zst`, 389 MB, and `gf180mcu_fd_sc_mcu7t5v0.tar.zst`, 180 MB, from the GitHub release listing). `ihp`: `ihp-sg13g2/libs.tech/magic` only (asset `common.tar.zst`, 332 MB; the tech file is 137 KB). `gf180mcu_fd_sc_mcu9t5v0` (another 180 MB asset) is not fetched: the roadmap names "the gf180mcu standard cells" and the 7-track library covers the same rule set; it can be added later as a third corpus case.
- **Mechanism.** As for sky130: `hefesto-cli pdk install <variant> <pin> -lib … -only …` into `.cache/pdks/ciel/<family>/versions/<pin>/`, then a tree digest per set; `fetch_pdk.py [sky130] [gf180mcu] [ihp] [magic-ref]`, `--print-digest <set>`. CI caches each set under its own key (`pdk-<set>-<pin>-<digest prefix>`), so adding gf180 does not refetch sky130, and a cold cache downloads about 0.9 GB once.
- **To verify at implementation (PDK 3.1).** That the gf180 assets contain exactly those paths (`gf180mcuD.tech`, `gf180mcuD.magicrc`, the 7t5v0 `.mag` files) and how large the kept subset is; that `-only` filtering streams without keeping the 389 MB archive on disk beyond the download cache; that the ihp `common` asset contains `libs.tech/magic/ihp-sg13g2.tech` (present at that IHP commit on GitHub); that each tech loads in Magic 8.3.694 in the capture container without errors; and the digests.

### D16. Performance

- **Background start latency** (`hefesto-performance` "Edit latency": checking starts within 100 ms): measured from the end of a paint command to the first square started by the worker, on the 100 × 100 `sky130_fd_sc_hd__dfxtp_1` array of the M2 benchmark.
- **Full-hierarchy DRC throughput** ("Batch throughput relative to Magic"): `drc check; drc catchup` over (a) the dfxtp_1 array and (b) a block placing every `sky130_fd_sc_hd` cell in rows, single-threaded, in Hefesto and in Magic in the same job of the capture container (same machine). A CI benchmark job records both times and fails if Hefesto is slower by more than the measurement noise band; `docs/design/m3-ui-conformance.md` "Performance" records the numbers. A parallel mode (several independent squares on several workers) is not in M3: it would change commit order only within the rules of D8 and is left as a later optimization.
- **Kernel data layout.** The CSR rule table (D5) and tile views avoid allocation in the edge loop; the temporary error plane is reused per square.

### D17. Items deferred to M3 from earlier milestones

| Item (source) | M3 home |
|---|---|
| CMDEDIT-LBL-07 `deferred = "M3"` (needs the check plane) | ENGINE marking + GOLDEN |
| `mag-checkpaint` mask on 138 captured cases | ENGINE (all M1/M2 marking sites) + GOLDEN (mask retired) |
| `tech drc` (`TECH-CMD-005`), TECH-G44 | RULES + GOLDEN |
| MODEL-G-006 (needs `drc`) | GOLDEN |
| CELL-TS / CELL-RD-10 parts that need DRC (`drc why`) | GOLDEN |
| `cmd-paint-erase` "Common effects" item 7 (queue the changed area) | ENGINE |
| Undo replay re-queueing (`undo/undo_log.cpp` comment) | ENGINE |
| Wiring default width from the raw drc section | RULES |
| `DRC not run` chip (M1 conformance) | SHELL |
| `drc` registration (help table only) | COMMANDS |

## Area contracts

The headers are the contract; each is documented. Key types and functions per area (the scaffold writes them with stubs):

### RULES — `hefesto/drc/rules/`
- `CifCatalog` (styles, current style, scale, layers, `contact_border(type)`); `DrcStyleList` (declared names, default).
- `load_style(const tech::Technology&, std::string_view style, const GridScale&, const CifCatalog&, const compat::CompatSettings&, diag::Sink&) -> std::shared_ptr<const RuleSet>`; `rescale(RuleSet, n, d, compat)`.
- `RuleSet`: per-pair rule spans, why table, exception names, halo (tech/mask parts), step size, DRC paint table, exact-overlap types, illegal-overlap list, mask-layer rule records, rulestats histogram.
- `print_rules(const RuleSet&, std::ostream&)`; `tech_drc_query(const RuleSet&, rule, layer, layer2)`; `default_width(const RuleSet&, type)` (wiring).

### CHECK — `hefesto/drc/check/`
- `CheckInput`, `basic_check(...)` (D4); `CheckStats`; `maximal_rectangles(...)` (maxrect); internal: edge walk, region build, corner extension, Euclidean filter, area/maxwidth/rectangle/off-grid/angles, diagonal search, exception test via `hints::inside`.

### ENGINE — `hefesto/drc/engine/`
- `check_this`, `PendingList`, `process_square(EditorState&, CellDef&) -> SquareResult`, `commit(SquareResult)`, `interaction_area(...)`, `interaction_check(...)`, `flatten_for_check(...)`, `copy_child_errors(...)`, `array_checks(...)`, `exact_overlap_check(...)`, `force_recheck(...)` (`drc check`), `flat_check(...)`, `catchup(EditorState&)`, `BackgroundChecker` (start, stop, wake, yield, generation), error-plane paint table.

### COMMANDS — `hefesto/drc/commands/`
- `register_drc_command(cmd::CommandRegistry&)`; per-option handlers; `why_text(const RuleSet&, const Rule&, const coordinate_units&)` (substitution); `count_errors`, `find_error`, `why_errors` (recheck under the box, why-4 resolution), `IgnoreList`.

### HINTS — `hefesto/hints/`
- `HintRegion` (paint rectangles, union, strips, transform, scale, translate, equality), `parse_hint_value`/`format_hint_value`, `inside(...)`, `copy_hints(src, dst, transform, clip)`, the M5 operations of D10.

### SHELL — `hefesto/shell/`
- `DrcBrowserModel` (groups, counts, current index, stale flag), the browser panel, the status chip view model, the worker host in the app loop.

## Risks / Trade-offs

- **R1. Tile-structure-dependent results.** Area, maxwidth, exception (compat) and diagonal results depend on Magic's exact tiling, and the engine's flattening must reproduce Magic's yank-buffer tiling. → M1's database already reproduces tile order (X-1, verified by M1/M2 golden); the kernel never re-tiles (D6); the worker reads the live planes instead of copies (D8); golden cases DRC-AREA-2/3, DRC-EXC-3/4 and MISC-MH-08 target exactly these effects.
- **R2. Concurrency bugs.** A data race between the worker and the command thread would give nondeterministic error planes. → One lock (D4) with a single writer; the commit is on the command thread; the stress test and a TSan CI leg (D8); the CLI batch mode runs no worker, so golden replay is single-threaded and deterministic.
- **R3. Marking completeness.** Retiring the `mag-checkpaint` mask exposes every edit path whose marked area differs from Magic's. → ENGINE ports the call sites from Magic's source with a checklist test; GOLDEN replays the 138 cases early (as soon as marking lands) and keeps the mask per case only where a gap is recorded as an open item, never at exit.
- **R4. Floating-point determinism.** Euclidean and diagonal checks use float/double formulas whose results must match Magic's x86_64 build and be identical on aarch64. → `-ffp-contract=off`, Magic's exact types and operation order, a pinned-value test (D3, D6), and the cross-platform determinism job over the DRC golden outputs.
- **R5. PDK data volume.** gf180 and ihp add about 0.9 GB of downloads on a cold cache. → Per-set cache keys (D15); kept subsets are small; the network test stays on one leg.
- **R6. Throughput versus Magic.** Magic's checker is tuned C; a straightforward port with abstraction overhead could be slower. → CSR rule table and allocation-free loops (D16); the benchmark job runs from the first ENGINE milestone, not at exit.
- **R7. Scope.** 5 capabilities, 69 testable requirements, the 113 DRC golden cases plus about ten of other areas, 2 corpus cases, a worker thread and a desktop browser. → Workstreams are independent after the scaffold; the mask-layer checks, GDS and Tcl are explicit non-goals; the exit check is the coverage script plus golden replay.
- **R8. Native modes without a reference.** The magic-compat OFF behaviors (Q-DRC-10 diagonal check, Q-MISC-13 inside test, Q-DRC-5 cover rule) have no Magic output to compare with. → Unit tests from the spec scenarios; golden cases run in the default mode only.

## Migration Plan

No user data changes. `.mag` files written by M3 carry the `checkpaint` areas edits leave (as Magic's do) and error tiles once checked; files written by earlier Hefesto builds read unchanged. A `plane` keyword in a properties section now follows Q-MISC-12 (format error with magic-compat ON). Mask hints keep their `.mag` form; the in-memory `PlaneValue` type becomes `hints::HintRegion` behind the same property API. `TECH-CMD-005` is retired when `tech drc` works (kept in the table, never reused); `CORE-NOTIMPL-003` exists only during M3. The CI reference-data cache keys change once (per-set keys).

## Resolved Questions

Approved by the user on 2026-11-10:

1. **The 113-case exit criterion.** M3 authors and captures all 113 DRC cases; DRC-CIF-1 and DRC-CIF-8 pass in M3 and DRC-CIF-2..7 replay as `deferred = "M5"`. The roadmap's M3 exit criterion reads "all 113 DRC golden cases pass, except the six mask-layer check cases DRC-CIF-2..7, which pass in M5", and M5's exit criteria gain them.
2. **Native inside test for exceptions.** Q-MISC-13 sets the magic-compat OFF test for both `mask-hints` and `drc-rule-semantics` (a hint touching or straddling the edge from either side); Q-DRC-11 carries a note saying so.
3. **Background checker snapshot.** Q-DRC-12's "immutable snapshot" is the live database read under the reader/writer lock (design D8), with the edit-generation commit check; Q-DRC-12 carries a note saying so.
