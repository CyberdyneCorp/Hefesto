# Design: M5 Mask generation, GDS and CIF

## Context

See proposal.md for motivation. M4 (openspec/changes/m4-scripting) delivered the embedded sandboxed Tcl, the `magic::` binding, the PDK startup scripts with consent, the PCell framework, the GUI managers and the Desktop Alpha. The conventions of M0–M4 still hold:
- SPDX headers;
- Catch2 tags `[cap:<capability>][req:<exact requirement heading>]`;
- per-area diagnostic code tables `AREA-TOPIC-NNN` in `docs/diagnostics/codes/<area>.md`;
- cognitive complexity at most 15 per function (flag irreducible ones);
- no UI, GPU, Raven or process headers in the core;
- golden expected outputs captured only in CI (`.github/workflows/golden-capture.yml`);
- Magic 8.3.694 at fc5daa9e as the behavioral reference;
- the clean-room rule for netgen, xschem and KLayout;
- `native_only` diagnostics dropped by the `magic` diagnostics format;
- golden cases run with magic-compat ON and the OFF behaviors are unit-tested.

What M4 left that M5 builds on (from the code, 2026-11-13):

- **Technology.** `cifoutput` and `cifinput` are kept verbatim as `RawSection` lines (`tech/technology.hpp`, `tech_file.cpp:42-43`). `cif_output_grid()` holds the first output style's grid (TECH-GRID-002). No cifinput semantics are parsed, and there are no CIF-reader tech diagnostics.
- **DRC catalog.** `drc::CifCatalog` (`drc/rules/cif_catalog.{hpp,cpp}`, 806 lines) is a quiet partial cifoutput reader. It has the style/variant state machine (CIFTechLine), CIFTechFinal and CIFTechOutputScale. It stores style names and order, the current style, scale factor, expander and reducer, layer names, per-layer numeric operands, `or_paint`, `keeps_cuts` and per-type contact borders, and offers `select()`, `rescale(n,d)` and `default_step_size()`. Its header promises that M5's reader replaces its source without changing its interface.
  - `drc/rules/keywords_cif.cpp` parses `cifstyle` and the `cif*` rules into `drc::MaskLayerRule`. Those rules are stored but never checked.
  - `drc/check/basic_check.cpp` has no mask-layer step.
  - `CheckInput.cif` already points to the catalog.
  - `DrcStatistics::cif_tiles` exists.
- **Mask hints.** `hints::HintRegion` is a `db::Plane` painted with Magic's CIF paint table, with AND-NOT. `hints::output_region`, `add_input_region`, `gds_maskhints_region`, `flat_copy_hints` and `HintBatch` are unit-tested M5 entry points (m3-drc design D13).
- **Commands.** The M4 `cif` command (`script/binding/cif_command.cpp`) has Magic's 23-entry option table, `cif help`, `cif [list|listall] ostyle|istyle` and `cif scale|lambda in|out`. Every other option fails with SCRIPT-BIND-002, and its istyle answers come from its own small parse of the raw `cifinput` lines. No `gds` or `calma` command is registered. UI-G-005's deviation lists them as later-milestone commands.
- **Database.** `db::uses_in_search_order(def)` returns the unbinned LIFO order only. `db::decompose_polygon` (a CIFrdpoly.c port) is labeled "shared with CIF/GDS polygon input, M5". Split tiles (`db/nm_split.cpp`, `nm_paint.cpp`) are complete. `CellDef::vendor_gds` and the `GDS_START`, `GDS_END` and `GDS_FILE` properties exist, and setting `GDS_FILE` makes a cell read-only. `cell/timestamps.cpp:111-113` leaves a hook for vendor content stamps. `cell/mag_input.cpp` inflates gzip input, concatenated members included. A glibc TYPE_3 `random()` exists for the `random` command (`db/commands_misc.cpp`).
- **Desktop.** The menu bar has "Read GDS" and "Write GDS" (`magic::promptload gds`, `promptsave gds`) disabled with the tooltip "arrives in M5". `platform::FilePicker` and the progress and interrupt card of the Tcl console exist.
- **Golden.** `tests/golden/MASK/CASES.md` has 233 rows: OS 17, OP 39, HG 25, CI 25, GR 60, GW 40, CF 27. None is authored.
  - DRC-CIF-1..8 are captured. DRC-CIF-4..7 are `deferred = "M5"`, and DRC-CIF-2 and DRC-CIF-3 pass only because no mask-layer error is generated.
  - MISC-MH-03..06, MISC-FL-05 and CELL-TS-07 are listed as not authored for M5. 490 cases carry the `magic-tech-sections` mask, which also removes cifinput's `Input style …: scaleFactor=…` line.
  - The `gds-dates` mask exists. No decoded-GDS, gzip or CIF-preamble comparator exists, and nothing in the repository writes GDS.
- **Data, perf, fuzz.** The PDK data sets keep `libs.tech/magic` and `.mag` libraries only, with no `gds/` directory. `perf.yml` has `drc-bench`, `drc-profile` (the 12.38-billion-instruction gate) and `golden-slow`. There are 19 fuzz targets, none for GDS, CIF or the cif sections.

M5 runs as follows:
1. an early PDK data step;
2. a scaffold step (interfaces, `EditorState` members, command stubs, the build contract);
3. twelve workstreams after the scaffold, ordered by the dependencies of D2;
4. an integration step.

No new screen needs a board (D16).

## Goals / Non-Goals

**Goals:**
- The M5 exit criteria of docs/roadmap.md:
  - the common exit criteria for the 8 M5 capabilities: tagged unit tests for the 194 exact and behavioral requirements, fuzz targets for the new readers, CI green on every leg, and byte-identical output on Linux x86_64, Linux aarch64 and macOS arm64;
  - the MASK golden cases pass (D18);
  - DRC-CIF-2..7 pass (D14);
  - GDS output is byte-identical to Magic's apart from the masked time fields (D12);
  - every sky130, gf180mcu and IHP library GDS round-trips (D19).
- One cifoutput parser, one operator engine and one use-order service, each shared by every consumer: DRC, GDS, CIF, and later OASIS and density.
- Every compat decision has one branch point, testable in both modes.

**Non-Goals:**
- OASIS, LEF/DEF (M6), extraction (M7) and tapeout fill (M13), as listed in proposal.md "Non-goals".
- Rewriting M1–M4 code that meets its spec. M5 changes earlier code only where an M5 requirement or decision needs it (D17).

## Decisions

### D1. Layout of the M5 code

| Area | Public headers | Sources | Unit tests | Namespace |
|---|---|---|---|---|
| mask planes (CIF paint table, AND-NOT, scratch pool) | `src/core/include/hefesto/mask/plane/` | `src/core/src/mask/plane/` | `tests/unit/mask/plane/` | `hefesto::mask` |
| output styles (cifoutput reader, IR, rescale, queries) | `src/core/include/hefesto/mask/style/` | `src/core/src/mask/style/` | `tests/unit/mask/style/` | `hefesto::mask` |
| input styles and the read engine | `src/core/include/hefesto/mask/input/` | `src/core/src/mask/input/` | `tests/unit/mask/input/` | `hefesto::mask` |
| operator kernels | `src/core/include/hefesto/mask/ops/` | `src/core/src/mask/ops/` | `tests/unit/mask/ops/` | `hefesto::mask` |
| generation (flat, hierarchical, see, coverage, statistics) | `src/core/include/hefesto/mask/gen/` | `src/core/src/mask/gen/` | `tests/unit/mask/gen/` | `hefesto::mask` |
| use index (bplane emulation) | `src/core/include/hefesto/db/use_index.hpp` | `src/core/src/db/use_index*.cpp` | `tests/unit/db/use_index/` | `hefesto::db` |
| GDS stream layer (records, reals, chars, dates, gzip sinks) | `src/core/include/hefesto/gds/stream/` | `src/core/src/gds/stream/` | `tests/unit/gds/stream/` | `hefesto::gds` |
| GDS reader, vendor views, content hash | `src/core/include/hefesto/gds/read/` | `src/core/src/gds/read/` | `tests/unit/gds/read/` | `hefesto::gds` |
| GDS writer, verbatim copy, full dump | `src/core/include/hefesto/gds/write/` | `src/core/src/gds/write/` | `tests/unit/gds/write/` | `hefesto::gds` |
| `gds`/`calma` command and settings | `src/core/include/hefesto/gds/command/` | `src/core/src/gds/command/` | `tests/unit/gds/command/` | `hefesto::gds` |
| CIF reader | `src/core/include/hefesto/cif/read/` | `src/core/src/cif/read/` | `tests/unit/cif/read/` | `hefesto::cif` |
| CIF writer | `src/core/include/hefesto/cif/write/` | `src/core/src/cif/write/` | `tests/unit/cif/write/` | `hefesto::cif` |
| `cif` command | `src/core/include/hefesto/cif/command/` | `src/core/src/cif/command/` | `tests/unit/cif/command/` | `hefesto::cif` |
| DRC mask-layer check | `src/core/include/hefesto/drc/check/mask_check.hpp` | `src/core/src/drc/check/mask_check*.cpp` | `tests/unit/drc/check/` | `hefesto::drc` |

`src/core/CMakeLists.txt` adds `mask gds cif` to `HEFESTO_CORE_AREAS`. Each area uses per-subdirectory `sources.cmake` and `codes.inc` fragments, assembled by the INTEGRATION-owned `{mask,gds,cif}/{sources.cmake,codes.cpp}`, as `drc/` and `script/` did.

Diagnostic prefixes:

| Document | Topics |
|---|---|
| `mask.md` | `MASK-STYLE-*` (STYLES), `MASK-INPUT-*` (INPUT), `MASK-OPER-*` (OPS), `MASK-GEN-*` (GEN) |
| `gds.md` | `GDS-STREAM-*`, `GDS-READ-*` (GDSR), `GDS-WRITE-*`, `GDS-CMD-*` (GDSW) |
| `cif.md` | `CIF-READ-*`, `CIF-WRITE-*`, `CIF-CMD-*` (CIF) |
| `drc.md` | `DRC-MASK-*` (DRCCIF) |
| `db.md` | `DB-INDEX-*` (INDEX) |

These topics are chosen so that no code reads like a golden case ID (MASK-OS-001, DRC-CIF-1).

### D2. Workstreams and ownership

| Workstream (tasks.md section) | Capabilities | Owns (may edit) |
|---|---|---|
| PDK | none (reference data for the round-trip criterion) | `scripts/fetch_pdk.py`, `scripts/tests/test_fetch_pdk.py`, `tests/unit/common/reference_data.hpp` (GDS paths), the reference-data steps of `.github/workflows/{ci,golden-capture,perf}.yml`, `docs/building.md` "Reference data for tests" |
| STYLES | `mask-output-styles`; the shared style/variant/scale core used by INPUT | `src/core/{include/hefesto,src}/mask/style/`, `tests/unit/mask/style/`, `tests/data/mask/style/`, `mask.md` (STYLES section); the re-sourcing of `drc/rules/cif_catalog.{hpp,cpp}` onto `OutputStyle` (interface unchanged) and `tech/loader.cpp`'s first-style grid read |
| OPS | `mask-output-operators` | `src/core/{include/hefesto,src}/mask/{plane,ops}/`, `tests/unit/mask/{plane,ops}/`, `tests/data/mask/ops/`, `mask.md` (OPS section); `hints/region.*` only for moving the CIF paint table into `mask/plane/` (the hints API is unchanged) |
| INDEX | `gds-write` "Structure ordering" use enumeration order (the shared service); the Q-MASK-400 seed in `drc-engine` "Interaction area" | `src/core/{include/hefesto,src}/db/use_index*`, `db/hierarchy.{hpp,cpp}` (`uses_in_search_order` and the use search functions), `db/cell.*` (index member and re-placement hooks), `tests/unit/db/use_index/`, `db.md` (`DB-INDEX-*`); `drc/engine/interaction.cpp` for Q-MASK-400 OFF |
| GEN | `mask-hierarchical-generation` | `src/core/{include/hefesto,src}/mask/gen/`, `tests/unit/mask/gen/`, `tests/data/mask/gen/`, `mask.md` (GEN section); `hints/hints.cpp` call sites for the generation wiring (m3 D13) |
| INPUT | `mask-input-styles` | `src/core/{include/hefesto,src}/mask/input/`, `tests/unit/mask/input/`, `tests/data/mask/input/`, `mask.md` (INPUT section) |
| GDSR | `gds-read`; the GDS stream layer used by GDSW | `src/core/{include/hefesto,src}/gds/{stream,read}/`, `tests/unit/gds/{stream,read}/`, `tests/data/gds/read/`, `gds.md` (STREAM, READ sections); the vendor-stamp hook in `cell/timestamps.cpp` |
| GDSW | `gds-write`; the `gds`/`calma` command | `src/core/{include/hefesto,src}/gds/{write,command}/`, `tests/unit/gds/{write,command}/`, `tests/data/gds/write/`, `gds.md` (WRITE, CMD sections) |
| CIF | `cif-format-io` | `src/core/{include/hefesto,src}/cif/`, `tests/unit/cif/`, `tests/data/cif/`, `cif.md`; removal of `script/binding/cif_command.cpp` and its registration line |
| DRCCIF | `drc-cif-rules` | `src/core/{include/hefesto,src}/drc/check/mask_check*`, the mask-layer step in `drc/check/basic_check.cpp`, `drc/rules/keywords_cif.cpp` (only if binding needs changes), `tests/unit/drc/check/mask_check*`, `drc.md` (`DRC-MASK-*`) |
| SHELL | `hefesto-app-ux` (GDS menu items, progress and cancel); `ui-gui-managers` "File prompts" (the `gds` and `cif` kinds) | `src/shell/desktop/src/menu_bar_model.cpp` (the M5 entries), `src/core/src/gui/prompts.*` (the `gds` and `cif` prompt kinds), `tests/unit/gui/prompts_test.cpp`, the progress card's GDS/CIF host, `tests/unit/shell/` (M5 tests), `docs/design/m5-ui-conformance.md` |
| GOLDEN | the MASK cases, the earlier cases that waited for GDS, DRC-CIF-4..7 | `tests/golden/`, `tests/data/db/use_index/` (the use-order traces of task 1.5; INDEX only reads them), `tools/golden/` (`authoring/{mask_cases,m5_cases,gdsgen}.py`, additions to `misc_cases.py`, `cell_cases.py`; the new comparators and masks; the `magic-tech-sections` mask change) |
| PERF | `hefesto-performance` "Batch throughput relative to Magic" (GDS read and write) | `bench/gds/`, the `gds-bench` and `gds-roundtrip` jobs of `perf.yml`, `docs/design/m5-ui-conformance.md` "Performance" |
| INTEGRATION | none | everything shared: root and `src/core/CMakeLists.txt`, `{mask,gds,cif}/{sources.cmake,codes.cpp}`, the shared headers of D4, `CMakePresets.json`, `cmake/`, `tests/unit/CMakeLists.txt`, `tests/fuzz/`, `src/core/{include/hefesto,src}/{cmd,diag,platform,settings,compat,util,editor,app}/`, `src/cli/`, `.github/` (other steps), `scripts/` (other files), `README.md`, `CHANGELOG.md`, `docs/roadmap.md`, `docs/building.md` (other sections), `docs/contributing/`, `docs/decisions/`, `docs/diagnostics/codes.md`, `openspec/specs/` (open items), this change's artifacts |

Rules (unchanged from M1–M4 D2):
- A workstream edits only what it owns. A needed change to a shared file or to another workstream's file is an `open_item` (what, why, and the exact edit if small); the owner or INTEGRATION applies it.
- GOLDEN authors new golden case directories from the requirement and the owning workstream's notes. `expected/` is captured in CI.
- Only the integration step commits and pushes, to `origin m5-mask-gds-cif` (or the branch its prompt names).

Dependencies:
- PDK (section 1) lands first. The round-trip cases and the GDS bench need its data, and its size check may change D19.
- The scaffold (section 2) publishes the interfaces of D4 with stubs.
- GOLDEN's use-order probe (task 1.5) lands with the PDK step, before the scaffold ends, because INDEX's trace test (task 5.1) replays its traces. INDEX depends on GOLDEN for it.
- STYLES, OPS, INDEX, GDSR (stream layer, records, content hash, vendor views) and the CIF parser start at once. OPS works against a hand-built `OutputStyle`, and the CIF parser works against a recording `ReadSink`.
- GEN needs STYLES and OPS.
- INPUT's tech reader needs STYLES' shared core. Its read engine needs OPS.
- DRCCIF needs only GEN's flat generation, so it can land before the hierarchy.
- GDSR's geometry needs INPUT's read engine.
- GDSW and the CIF writer need GEN (hierarchical) and INDEX.
- SHELL needs the commands.
- GOLDEN authors inputs from day one. PERF builds the bench as soon as GDSR or GDSW lands.

### D3. Build and stub contract

- **No new dependency.** zlib 1.3.1 is already bundled and linked (`hefesto::zlib`). Inflate input reuses the `cell/mag_input.cpp` approach (`inflateInit2(15+32)`, concatenated members). Deflate output uses the same library. zstd stays decoder-only for PDK assets.
- **Floating point.** The `mask`, `gds` and `cif` areas build with the M3 flags `-ffp-contract=off`. They use Magic's types and operation order for every float path: Euclidean grow, the grow-min `sqrt`, path outlines, eight-byte reals, rescale ratios and wire ends (D22).
- **Stubs.** The scaffold registers `gds` and `calma` (Magic's option table, CmdCD.c:151-187, a bare `gds` meaning write). It moves `cif` to `cif/command/` with every option. Each option that is not yet implemented fails with a new code `CORE-NOTIMPL-005` ("not implemented yet: <command> <option>"). Completion, help, scripts and golden inputs therefore see the final names. INTEGRATION retires `CORE-NOTIMPL-005` at M5 exit. The M4 answers (`cif scale`, `lambda`, `list`/`listall`) keep working throughout.
- **Scaffold tests** carry `[m5-scaffold]` and no `[cap:][req:]` tags. Owners replace them.

### D4. Session state and shared interfaces

`editor::EditorState` gains `mask` (`mask::MaskSession`). INTEGRATION owns it as a struct whose members belong to the named workstreams:

| Member | Holds | Workstream |
|---|---|---|
| `output_styles` | the parsed cifoutput styles, as `std::shared_ptr<const OutputStyle>` per style, and the current one | STYLES |
| `input_styles` | the parsed cifinput styles and the current one | INPUT |
| `gds` | `gds::GdsSettings`: every `gds` option, the session date stamp, the Q-MASK-111 rewound flag under ON, warning level and redirect | GDSW (read options GDSR) |
| `cif` | `cif::CifSettings`: `*hier`/`*array` switches, prefix, polygon/unfracture, limit/rescale, warning, arealabels and idcell | CIF |
| `random` | the shared glibc TYPE_3 generator of `random`/`random seed`, which Q-MASK-211 ON reads | INTEGRATION, moved from `db/commands_misc.cpp` |

A style changes only when the technology reloads, a variant or style is selected, or a rescale happens. Each change builds a new immutable object and swaps the pointer, as `drc::RuleSet` does. The DRC worker keeps the pointer it started with.

Shared interfaces created by the scaffold (INTEGRATION owns the files; the named workstream fills in the implementation):

**Mask core**
- `mask::MaskPlane` (`mask/plane/plane.hpp`): a `db::Plane` with the CIF paint and erase tables, AND-NOT, and a scratch pool (`PlanePool`) reused across generations (OPS).
- `mask::OutputStyle` and `mask::LayerOp` (`mask/style/style.hpp`): an immutable IR of layers in order, each a list of operators with typed operands. It also holds the scale (`scale_factor`, `expander`, `reducer`), the label map, the GDS layer map, options, the halo radius and contact sizes (STYLES). `OutputStyle::rescale(n, d) -> std::shared_ptr<const OutputStyle>`.
- `mask::InputStyle` (`mask/input/style.hpp`): the parsed values in both their raw and resolved forms. The mode-dependent cifinput decisions (Q-MASK-300, 302, 306, 307) are resolved when a style is selected, and again when `magic-compat` changes (INPUT).
- `mask::OperatorContext` and `mask::evaluate_layer(const OutputStyle&, std::size_t layer, const LayerInputs&, Rect area, OperatorContext&) -> MaskPlane&` (`mask/ops/evaluate.hpp`): the CIFGenLayer equivalent (OPS). `OperatorContext` carries `compat::CompatSettings`, the cell being generated (for bbox, tagged, net and mask-hints), the scratch pool, a cancel token and statistics.

**Generation**
- `mask::GenerationContext` (`mask/gen/context.hpp`): scratch planes, a progress callback, a cancel token and statistics. It has no globals, and one instance per caller makes generation reentrant (GEN).
- `mask::generate_flat(...)` (CIFGen/CIFsee over an area of a flattened cell) and `mask::generate_hierarchical(...)` (CIFGenSubcells and CIFGenArrays) (GEN).

**Use order**
- `db::UseIndex` (`db/use_index.hpp`) (INDEX), with these operations:
  - `place(use)`, `remove(use)`;
  - `search(area, visitor)`, Magic's `BPE_OVERLAP` search (area shrunk by 1), which rebuilds bins when it is non-surrounding (D7);
  - `find_equal(rect, visitor)`, Magic's `BPE_EQUAL` lookup, which never rebuilds;
  - `enumerate_all(visitor)`, the whole-plane enumeration of the writers and `.mag` write, which never rebuilds;
  - `order(compat) -> std::vector<CellUse*>`.

**Input**
- `mask::ReadSink` (`mask/input/sink.hpp`): the interface between the format parsers and the read engine (INPUT), with these operations:
  - `begin_cell(name)`;
  - `paint(cif_layer, polygon|rect)`;
  - `label(...)`;
  - `call(child, transform, array)`;
  - `property(...)`;
  - `end_cell()`.

  The CIF and GDS parsers drive it. Tests use a recording sink.

**GDS**
- `gds::ByteSource` and `gds::ByteSink` (`gds/stream/io.hpp`): plain, gzip and in-memory. Each reports positions in the decompressed stream (GDSR).
- `gds::RecordReader` with one-record look-ahead, and `gds::RecordWriter` (GDSR).

**Commands and progress**
- `gds::read(EditorState&, path, const GdsReadOptions&)`, `gds::write(EditorState&, root, path, const GdsWriteOptions&)`, `cif::read(...)` and `cif::write(...)`. Each returns a `cmd::CommandResult` and takes an optional `mask::Progress` (callback and cancel). The commands, the CLI and the shell all call these (GDSR, GDSW, CIF).
- Each write has two phases, because Magic's write mutates the database before it generates anything:
  - `prepare_write(EditorState&, root, options) -> PreparedWrite` runs on the command thread under the exclusive database lock. It checks the style (Q-MASK-219), loads the subtree with Magic's recursive area search through `UseIndex::search` (which can rebuild bins), resolves timestamp mismatches with the DBReComputeBbox re-placement, and returns the checked options and the root;
  - `emit_write(const PreparedWrite&, path, Progress*)` generates and writes under the shared lock. It mutates nothing in the layout; the only state it changes is the use index's bins, under the index mutex (D7).

  `gds::write` and `cif::write` call both in sequence; the shell runs the second on a worker (D16).
- `platform::FilePicker` is reused (M4).

### D5. Output styles and the catalog (STYLES; `mask-output-styles`)

- **One parser.** The cifoutput reader ports CIFtech.c (`CIFTechLine`, `CIFTechFinal`, `CIFTechOutputScale`, `cifParseLayers`) into `OutputStyle`. It covers:
  - every statement: style, variant, scalefactor, units, stepsize, gridlimit, layer, templayer, labellayer, labels, calma, render, min-width and options;
  - every operator with argument validation;
  - the tech-load messages of Magic, printed at the same points with the same text.

  `drc::CifCatalog` keeps its interface and becomes a thin view over the current style's `OutputStyle`, so DRC's Euclidean rule code and step size are untouched. Its own parse code is removed. The catalog's tests and DRC golden cases must pass unchanged before the old parser is removed.
- **Decisions.**
  - Q-MASK-10 (fix): exact-name and base+variant style matching, with a warning.
  - Q-MASK-8 (replicate): `gridlimit` as written, with a warning when the expander is not 10.
  - Q-MASK-11 (replicate): `labels … text` is the same as `labels …`.
  - Q-MASK-13 (replicate): `bloat-all` counts as 1 in both GCDs.
  - Q-MASK-18 (compat): ON keeps the stale halo after a non-reducing rescale; OFF recomputes it.
  - Q-MASK-401 (replicate): `see-no-vendor` is parsed and stored but has no effect.
- **Queries.** The halo radius (`mask-output-styles` "Interaction radius") and the contact cut size are methods of `OutputStyle`. `cif` and DRC use them, and LEF (M6) will too.
- **Shared core.** The style/variant/scale state machine and `CIFParseScale` are a small library (`mask/style/core.hpp`) that INPUT also uses. They are written once.

### D6. Mask planes and operator kernels (OPS; `mask-output-operators`)

- **Planes.** A mask plane is a `db::Plane` with Magic's CIF paint table (`CIFPaintTable`, `CIFEraseTable`). The table moves from `hints/region` into `mask/plane/` and `hints` uses it from there. Painting order and tile structure follow Magic, because merged-polygon output (Q-MASK-217) and area and maxwidth results depend on tile order.
- **One kernel per operator**, each a function of the style operands, the input planes, the area, `OperatorContext` and the output plane. The kernels are ported from CIFgen.c (5,971 lines) in four groups, each with its own source files:
  1. boolean and grow: `or`, `and`, `and-not`, `grow`, `shrink`, Euclidean, `grow-grid`, `grow-min`;
  2. contact cuts: `squares`, `squares-grid`, `slots`, with the contact strip pre-pass;
  3. bridge and bloat: `bridge`, `bridge-lim`, `bloat-or`, `bloat-min`, `bloat-max`, `bloat-all`, `close`;
  4. topology: `maxrect`, `orthogonal`, `not-square`, `(non)overlapping`, `(non)interacting`, `net`, `tagged`, `bbox`, `boundary`, `mask-hints`.

  Split tiles are handled through `db/nm_*`.
- **Compat branch points.** Each compat decision is one small helper that takes `const CompatSettings&` and is called from the kernel. The OFF path is therefore a separate, testable function, and the ON path stays a line-for-line port:

  | Decision | ON (Magic) | OFF |
  |---|---|---|
  | Q-MASK-1 | grow-grid moves edges by \|c\| mod g | outward rounding to the grid |
  | Q-MASK-2 | close threshold scaled linearly | quadratic scaling, exact 64-bit compare |
  | Q-MASK-3 | bridge SE corner tested against the bottom-left neighbour | bottom neighbour at the right end, as pass 2 |
  | Q-MASK-5 | bridge-lim searches with mask-unit coordinates | converted units, warning when scaleFactor ≠ 1 |
  | Q-MASK-6 | tagged mixes units | label rectangle converted to plane units |
  | Q-MASK-7 | not-square drops split tiles | triangles copied |
  | Q-MASK-21 | interacting divides by truncation | outward rounding |
  | Q-MASK-27 | squares grid-limit shrink is sign-blind | sign-aware correction |
  | Q-MASK-228 | bridge-lim enumerates the plane it paints | a snapshot |
  | Q-MASK-229 | bridge-lim erases whole limiting tiles, unscaled | the clipped part, in mask units |
  | Q-MASK-230 | bloat-all leaves stale trigger marks | every searched plane reset |
  | Q-MASK-231 | bloat-all seeds the first tile only when d = 0 | every overlapping region seeded |
  | Q-MASK-232 | bloat-all converts triggers by truncation | outward rounding |

  The replicate rows are ported exactly in both modes: Q-MASK-15 (grow-min with int product, double `sqrt`, truncation), Q-MASK-20 (bloat-or on split tiles), Q-MASK-22 (`net` without layers adds nothing), Q-MASK-23 (the cell bbox of the cell being generated: paint, subcell bboxes and label anchors, without text extents) and Q-MASK-26 (truncation). Q-MASK-227 is a fix in both modes: the bridge `sqrt` argument is clamped to 0.
- **Complexity.** CIFgen.c's bridge, bridge-lim, bloat-all and squares functions are long. OPS splits them into helpers per pass and per corner case. Any function that cannot get under 15 without obscuring the port is flagged in the task note, with at most 25 allowed (as M4 allowed for generators).

### D7. Use enumeration order: the bplane emulation (INDEX; Q-MASK-204, Q-MASK-323)

The highest-risk decision of M5. GDS SREF order, structure order, CIF symbol numbering and CIF `C` call order all follow it. The byte-identity criterion fails without it on any golden case that writes a cell after its use index has been binned.

- **One service in `db`.** Each `CellDef` owns a `db::UseIndex` that models Magic's bplane state:
  - the unbinned `inBox` list, LIFO;
  - the bin array with bin sizing, recursive sub-binning (minimum population 10, average population 1.0) and the oversized list;
  - the "bins exist" flag;
  - the doubled bounding box on rebuild.

  The model is a direct port of `bplane/bpMain.c`, `bpBins.c`, `bpEnum.h` and `bpEnum.c`. Every operation that Magic routes through BPAdd/BPDelete goes through `place`/`remove`:
  - use creation, read, move and delete;
  - the re-placement in DBReComputeBbox, in the child's parent-use order (DBcellbox.c:728-764).

  Every area search over uses goes through `search(area)`: DRC interactions, flattening, `select`, hierarchical generation, the writers' subtree loading, and later extraction. The search is Magic's `BPE_OVERLAP`: the area is first shrunk by 1 on each side (bpEnum.c:104-106), and the search is *non-surrounding* when the shrunk area does not surround the index's bounding box (touching allowed). A non-surrounding search rebuilds the bins at 10 or more unbinned uses (bpBins.c:651), and so do sub-bins built during a search. Because of the shrink, a search over a cell's own bounding box is non-surrounding whenever a use touches that box's edge. Exact-match lookups (Magic's `BPE_EQUAL`, `DBCellFindDup`, DBcell.c:107) go through `find_equal(rect)`, which never rebuilds. Whole-plane enumeration (`DBCellEnum` over `TiPlaneRect`, used by `.mag` write and by both writers' enumeration of a cell's uses) is `enumerate_all`, which never rebuilds.
- **Mode.**
  - magic-compat ON: `order()` is the bin walk of the current state, as the spec describes.
  - magic-compat OFF: `order()` is the reverse of insertion order: the reverse of the `.mag` `use` order, or of the SREF/AREF order for a cell read from GDS, independent of history.

  The state is maintained in both modes, so toggling the mode never needs a rebuild.
- **Writers' searches and sampling points.** Each writer makes Magic's searches, in Magic's order, before it enumerates:
  1. subtree loading (`DBCellReadArea`, CalmaWrite.c:316, CIFwrite.c:137): `search` over the root's bounding box, then recursively over each child with the area transformed into the child's coordinates, each array element in turn, stopping at the remaining elements of an array once the transformed area surrounds the child's bounding box (DBexpand.c:329-337). In a fresh session this bins the root, and any cell whose transformed area does not surround its uses, if it has 10 or more uses;
  2. `DBFixMismatch`, whose DBReComputeBbox re-places the uses of each child whose bounding box changed;
  3. the GDS writer takes the SREF order before the cell's CIFGenSubcells (CalmaWrite.c:1486, :1510-1514); the CIF writer numbers children before CIFGenSubcells (CIFwrite.c:333) and writes `C` calls after it (:467).

  Steps 1 and 2 are the prepare phase of D4. GEN's interaction searches are non-surrounding searches, so they can bin a cell between the two CIF sampling points, as in Magic. A trace test covers a fresh session with one write and a cell of 10 or more uses touching its bounding box.
- **Concurrency.** Rebuilding bins is a logically const mutation that happens during reads, from the command thread or from the DRC worker under the shared database lock. `UseIndex` therefore guards its state with its own mutex, and `order()` returns a snapshot vector, so an enumeration is never invalidated by a concurrent rebuild. In batch CLI mode there is no worker, so the history, and with it the ON order, is deterministic.

  On the desktop, a background DRC pass that ran before a write can bin a cell, exactly as Magic's idle-time DRC does. Golden cases run in the CLI. The OFF order never depends on history.
- **Verification.**
  - A trace test replays recorded sequences of place, search and enumerate against orders captured from Magic. GOLDEN adds a small Tcl probe (task 1.5), which reads the order through GDS and CIF writes after controlled searches (the SREF use-id property, PROPATTR 61, CalmaWrite.c:1834-1836, and CIF `91`/`C` lines). `cellname list childinst` walks the def's use-id hash (DBcellname.c:524-534), not the bplane, so the probe records it only as a separate check line. The probe also records the orders after a plain fresh-session `gds write` and `cif write`, the case where subtree loading bins the root.
  - Golden cases cover ≥10-use cells with an interaction for CIF and GDS, a second write in one session, and a write after `drc check`.
  - The M1–M4 golden suite and the DRC corpus cases must pass unchanged. The DRC results do not depend on the order, but the regression risk is real.

### D8. Generation (GEN; `mask-hierarchical-generation`)

- **Flat.** `generate_flat` follows Magic in three steps. It flattens the area of a cell into a yank cell, copying the hints in a `HintBatch` (m3 D13). It evaluates every layer in style order with `evaluate_layer`. It returns one plane per layer. It serves `cif see`, `cif paint`, `cif coverage`, DRC (D14) and the flat CIF writer.
- **Hierarchical.** `generate_hierarchical` ports CIFhier.c (1,790 lines):
  - the halo radius;
  - classification of hierarchically processed layers;
  - interaction-area discovery, which shares the finder with hierarchical DRC (`drc-engine` "Interaction area");
  - flattening for interaction processing and the residual subtraction;
  - array interaction regions and correction;
  - sliver growth;
  - the hierarchy and array output switches (`CIFhier`, `CIFarray`).

  The compat splits:

  | Decision | ON (Magic) | OFF |
  |---|---|---|
  | Q-MASK-400 | the (0,0) seed adds a phantom area | no phantom area |
  | Q-MASK-402 | a stopped layer's partial plane joins the residual | a stopped layer has no residual |
  | Q-MASK-403 | layers that depend on bbox/boundary layers stay hierarchical | they are removed transitively |
  | Q-MASK-404 | Magic's halo radius | bloat and bridge widths are counted too |
  | Q-MASK-405 | Magic's force set | grow-min, not-square, orthogonal and maxrect also force |
  | Q-MASK-406 | signed array index filter | absolute distance from the corner |
  | Q-MASK-409 | Magic's sliver geometry | rescaled min-width, full growth on both axes |
  | Q-MASK-410 | the CIF writer ignores `CIFhier` | the CIF writer honours `CIFhier` |
  | Q-MASK-411 | flat CIF without labels or hints | flat CIF with labels and hints |
  | Q-MASK-413 | `cif unfracture` uses the fourth word only | `cif unfracture` uses the third word |

  Each split is a helper in the GEN area. Q-MASK-400 is implemented once in the shared interaction finder, which INDEX's D17 change wires into DRC.
- **Q-MASK-414 (platform).**
  - `mask::Progress` reports the fraction of interaction squares done, rate-limited to about 5 s, and offers cancellation.
  - The console host may print `Completed N%`. That text is informative and masked in golden output.
  - A cancelled GDS or CIF write leaves no output file. The writers write to a temporary file in the target directory and rename it on success.
  - A write that fails where Magic would leave a partial, terminated file (Q-MASK-226 ON, "Write result and error propagation") is still renamed into place, so the observable file matches Magic. Only a cancellation discards.
  - Snapshot isolation uses the M3 interpretation (Q-DRC-12, m3 D8). A write's prepare phase (D4: subtree loading, mismatch fix-up and the Magic-order pre-search) mutates the database, so it runs on the command thread under the exclusive lock. Generation and output then run under the shared database lock. On the desktop that phase runs on a worker with progress (D16), while edits issued during it queue until it ends. "An edit made during the write does not appear in the written file" holds because nothing in the layout mutates under the shared lock; use-index rebuilds during generation are guarded by the index mutex (D7). A TSan test runs a write while a background DRC pass runs.
- **Hints.** The generation wiring of m3-drc design D13 (`output_region`, `flat_copy_hints`, `add_input_region`, `gds_maskhints_region`) is done by GEN for output and by INPUT and GDSR for input. MISC-MH-03..06 are authored then.

### D9. Input styles and the shared read engine (INPUT; `mask-input-styles`)

- **Reader.** CIFrdtech.c (1,516 lines) is ported on top of STYLES' shared core. It covers:
  - styles with the prefix and exact rules (Q-MASK-302 compat, Q-MASK-303 fix) and variants;
  - scalefactor with the multiplier (Q-MASK-300 compat), gridlimit, layer and templayer rules, and the input operators;
  - the `calma` mapping with wildcards (Q-MASK-326 compat: the 255-entry limit with ON only);
  - `labels` and `sticky` (Q-MASK-301), and labels under a templayer (Q-MASK-311 compat, with the invalid-index guard in both modes);
  - the CIF-reader tech messages.

  The resolved values that depend on the mode are recomputed when `magic-compat` changes, because magic-compat is a session setting and the tech file is read once.

  The M4 `cif` command's ad-hoc istyle parse is replaced by this reader, and `cif scale`/`lambda in` answer from it. Magic's `Input style …: scaleFactor=…, multiplier=…` line is printed at tech load, so GOLDEN removes that line from the `magic-tech-sections` mask.
- **Read engine.** One engine (CIFrdcl.c, CIFrdutils.c, CIFrdpt.c, CIFrdpoly.c) serves both parsers through `ReadSink`:
  - per-cell read planes and the scale S;
  - CIFScalePlanes and CIFInputRescale, the mid-read subdivision under `cif|gds limit` and `rescale`;
  - CIFPaintCurrent through `evaluate_layer` with the input-mode differences;
  - `boundary` to `FIXED_BBOX`, `mask-hints` to `MASKHINTS_` properties, and copyup;
  - `tagged` with explicit unit types, so the Q-MASK-6/310 ON unit mix is one deliberate branch.

  The input-mode differences are Q-MASK-306 (input grow ON follows the output style's grow-euclidean; OFF is always square), Q-MASK-307 (boundary and mask-hints in regular layer rules: ON uses the output OR; OFF is a no-op), and Q-MASK-330 and 334.

  Mid-read rescale goes through the existing `DBScaleEverything` path under the exclusive lock, with undo disabled for the read, as Magic does.

### D10. GDS stream layer and compressed files (GDSR; `gds-read`, `gds-write`)

- **Reader.** `RecordReader` reads records with one-record look-ahead and reports offsets in the decompressed stream. `GDS_START`/`GDS_END` and the Q-MASK-126 seek therefore count decompressed bytes, for `.gds` and `.gds.gz` alike. Seeking back in a gzip source re-inflates from the start or from a checkpoint, never from a byte offset of the compressed file. `PaZOpen`'s lookup order for `<name>`, `<name>.gz` and the search path follows `gds-read` "Read command and file lookup".
- **Writer.** `RecordWriter` and the encoders cover:
  - the eight-byte real (the `gds-write` "Eight-byte real encoding" algorithm, with the MASK-GW-040 vectors as a unit test);
  - the strict and permissive character tables (Q-MASK-200 compat: ON swaps `%` and `&`, which IHP output depends on);
  - string-limit (Q-MASK-207);
  - dates in local time (Q-MASK-203), with the pinned-zone override setting of Q-MASK-132 for tests;
  - 64-bit coordinates, where a value outside I4 fails the write (Q-MASK-222).
- **Compressed output** (Q-MASK-223, platform). One gzip member at deflate level z through zlib. Conformance and golden comparison use the decompressed bytes. The gzip header and deflate choices are informative.

### D11. GDS read, read-only vendor views and content-hash timestamps (GDSR; `gds-read`)

- **Reader.** It ports CalmaRead.c, CalmaRdcl.c, CalmaRdpt.c and CalmaRdio.c (about 4,900 lines) as a dispatch table per record type, with one handler per element kind, to keep complexity down. It covers:
  - structures and post-order reading with file repositioning;
  - BOX and BOUNDARY, with non-Manhattan decomposition through `db::decompose_polygon`;
  - PATH and path-to-polygon;
  - TEXT, label layers, pins and ports;
  - SREF and AREF with transforms, magnification and placement;
  - properties;
  - flatten, flatglob, polygon subcells and path subcells;
  - duplicates and `noduplicates`/`unique`;
  - end-of-read cleanup.

  Each compat row of the "GDS read" table (Q-MASK-110..134) is a local ON/OFF helper. Session-global Magic state that ON reproduces (the Q-MASK-111 rewound flag) is an explicit field of `GdsSettings`, never a static.
- **Read-only vendor views.** With `gds readonly yes`:
  - each structure carries `GDS_START`, `GDS_END` and `GDS_FILE`;
  - the units check is made against 1 nm or 1 Å;
  - the rest follows the spec.

  `GDS_FILE` is the search-path name (ON) or the canonical path (OFF), per Q-MASK-129. PDK-variable substitution is then applied, and finally `~` substitution on the GDS path itself (Q-MASK-128, fix). Read-only cells are not queued for DRC. The background checker already skips `vendor_gds` cells.
- **Content-hash timestamps.** The structure table and the FNV-1a stamp are a pure function over a byte span, `gds::content_stamps(std::span<const std::byte>) -> StampTable`. It is unit-tested with the spec's three vectors (143581310, 1837575894, 1595775465) and fuzzed (D21).

  `cell/timestamps.cpp`'s M1 hook calls it when a read-only cell is created and when its `GDS_FILE` is re-resolved (`gds-read` "When content-hash timestamps are assigned"). This closes CELL-TS-07.

### D12. GDS write and byte identity (GDSW; `gds-write`)

- **Writer.** It ports CalmaWrite.c (3,343 lines) in this order:
  1. the header and trailer, with UNITS from the current style on every write (Q-MASK-201, fix);
  2. the preconditions, checked before the file is opened (Q-MASK-219, fix), and subtree loading with mismatch fix-up (the prepare phase of D4, with Magic's searches of D7);
  3. structure ordering (D7), with naming and the Q-MASK-209 uniquification (`XXXXX<n>`, `_k` on collision);
  4. output scale factors (Q-MASK-225 compat: ON writes mis-scaled output with Magic's errors; OFF refuses before opening);
  5. per structure, the mask geometry from `generate_hierarchical`, the tile decomposition and enumeration, unmerged boundaries, or merged polygons (`merge yes`).

  The merge and collinear passes are ported exactly, slits included (Q-MASK-217). The rest of the write covers:
  - labels and TEXT records: Q-MASK-205 is a stable sort of the reversed list, using our own merge sort with glibc's order on every host (never `std::stable_sort` assumptions about the libc); Q-MASK-206 port labels; Q-MASK-221 MAG ignoring `magscale` under ON; the three font-size formulas of Q-MASK-122;
  - references and arrays, and array flattening;
  - verbatim copy of read-only cells (Q-MASK-216 compat for SNAME rewriting);
  - full dump (Q-MASK-211 compat prefix: ON uses the shared `random()`, OFF a file-name hash; Q-MASK-212 fix: always check units; Q-MASK-214 and 215 compat);
  - addendum, and abstract views (Q-MASK-226 compat);
  - the write result.

  The writer never mutates the layout (Q-MASK-208). Magic rewrites label text in place.
- **Byte identity and masked time fields.**
  - GDS output is compared byte for byte after the `gds-dates` mask, which zeroes the 24 date bytes of BGNLIB and BGNSTR. Under ON the modification date is the current time (Q-MASK-202), so it is the only field that is never reproducible.
  - A new narrower mask, `gds-mod-dates`, zeroes only the 12 modification-date bytes. Cases that set `gds datestamp <n>`, or whose creation dates come from `.mag` timestamps that Hefesto reproduces, use it, so creation dates are compared too. Most GW cases do this.
  - Every case runs with `TZ=UTC` and `LC_ALL=C`, which the capture image already sets (Q-MASK-203).
  - Compressed output is compared after decompression (`gzip` canonicalizer).
  - A decoded record view (`gds-records`) is used only for readable diffs. Pass or fail is always the masked bytes.
- **OFF mode.** It writes `t = 0` for modification dates. Dates are converted in the host's local time zone in both modes (Q-MASK-203, replicate), so `t = 0` encodes as 1970-01-01 00:00:00 under UTC and as 1969-12-31 in western zones. OFF output is therefore reproducible under a fixed time zone (`TZ`, or the `gds-time-zone` setting of Q-MASK-132), not across zones. Unit tests check the OFF bytes directly under UTC and under one pinned non-UTC zone.

### D13. CIF read symbol table and CIF I/O (CIF; `cif-format-io`)

- **Parser.** A table-driven lexer and command dispatcher (DS, DF, DD, C, L, B, R, P, W, E, and the user extensions 9, 91, 94 and 95) drives `ReadSink`. It covers:
  - the error limit and the warning redirect (Q-MASK-331 compat);
  - top-level `9` (Q-MASK-317 compat);
  - odd box lengths (Q-MASK-315 compat: OFF subdivides read units);
  - the round flash as its bounding square (Q-MASK-316);
  - square half-width wire ends (Q-MASK-333);
  - the malformed call transform (Q-MASK-335 compat);
  - copyup on `C` (Q-MASK-309 compat: OFF applies the child's cached copyup planes);
  - the 95 rescale, which doubles the symbol numerator (Q-MASK-314, fix);
  - label extensions (Q-MASK-327 compat).
- **Symbol table (Q-MASK-318, fix in both modes).** A `cif::SymbolTable` belongs to one read (`ReadSession`), never to the library:
  - Symbol n, in `DS n` and in a forward `C n` alike, always maps to a cell created by this read.
  - A name collision with an existing cell gives `n_k`.
  - No cell already in memory is cleared or redefined.

  `DD n` is compat: ON deletes one symbol, OFF deletes every symbol ≥ n, as CIF 2.0 says. Tests cover forward calls, `DD` then redefinition, and collisions with library cells. The resulting deviation from Magic is recorded in `deviations.toml` with Q-MASK-318.
- **Writer.** It ports CIFwrite.c:
  - the preamble (Q-MASK-324 platform: the same lines, `@@tool` is Hefesto, and the six host and build lines are masked by a new `cif-preamble` mask);
  - numbering and `C` call order from INDEX at Magic's two sampling points (Q-MASK-323);
  - DS, 9, L, B and P lines; 94 and 95 labels (Q-MASK-322 compat: OFF replaces whitespace and `;` with `_`);
  - C and 91 arrays;
  - flat `__CIF2__` (Q-MASK-321, 411);
  - prefix, idcell and arealabels;
  - the `.cif` suffix (Q-MASK-320 compat).
- **`cif` command.** Every remaining option: `read`, `write`, `flat`, `ostyle`, `istyle`, `see`, `paint`, `coverage` (Q-MASK-325 compat: OFF counts half area), `statistics`, `drccheck`, `warning`, `limit`, `rescale`, `polygon`, `unfracture` (Q-MASK-413 = D-20), `*hier`, `*array` and `prefix`.

### D14. DRC on generated mask layers (DRCCIF; `drc-cif-rules`)

- **Hook.** At the end of `basic_check(input, K, L, sink)`, the check runs the Magic drcCifCheck port when the current DRC style binds a cifoutput style (`cifstyle`) and has at least one mask-layer rule. The steps are:
  1. call `mask::generate_flat` on the yanked cell over the check area grown by the mask halo, for the bound style's layers that the rules use;
  2. run the width and spacing edge checks, the area flood (`cifarea`) and the maxwidth flood (`cifmaxwidth`) on those planes;
  3. map each error back to layout coordinates.
- **Q-DRC-13 (compat).** ON truncates toward zero, widens degenerate errors, and drops errors that collapse, as Magic does. OFF rounds outward. In both modes a `DRC-MASK-*` diagnostic counts dropped errors, as `native_only` when it is not Magic's text.
- **Cost.** Generation runs only when a mask-layer rule applies, so styles without them, including the drc-profile workload's default style, pay nothing. The scratch planes come from the worker's `GenerationContext`, so no allocation happens per square. `DrcStatistics::cif_tiles` counts generated tiles.

  PERF checks that `drc-profile` stays at or below 12.38 billion instructions. A per-pass generation cache keyed by the yank area is added only if the profile shows a need.
- **Threading.** The worker owns its own `GenerationContext` and holds the style through `shared_ptr<const OutputStyle>` (D4), so generation on the worker under the shared lock is safe. TSan covers it.
- **`cif ostyle`.** `cif ostyle` reselects the catalog's current style through the existing `select_cif_style`, so DRC-CIF-6 and DRC-CIF-7 replay. DRC-CIF-4..7 lose `deferred = "M5"`, and DRC-CIF-2 and DRC-CIF-3 must still pass once errors are generated.

### D15. Commands and settings (GDSW, CIF)

- **`gds` and `calma`.** Magic's CmdCalma (CmdCD.c:81-1600) is ported as a table of option handlers. It covers:
  - `help`, `abstract`, `addendum`, `arrays`, `compress`, `contacts` (accepted, no effect), `datestamp`, `drccheck`, `flatten [yes|no|limit]`, `flatglob`, `labels`, `library`, `lower`, `magscale`, `maskhints`, `merge`, `nodatestamp`, `noduplicates`, `ordering`, `read`, `readonly`, `rescale`, `savepaths`, `warning`, `write`, `polygon subcells`, `path subcells`, `undefined` and `unique`;
  - a bare `gds`, which means write;
  - the option settings and their defaults, held in `GdsSettings`.

  Q-MASK-130 compat: under ON, `gds rescale no` also sets warning level `limit`. Q-MASK-210 compat: under OFF, the extension test looks at the basename only.
- **Settings.** The options are session state, as in Magic. The Q-MASK-132 pinned time zone is a `setting` key (`gds-time-zone`), used by tests and empty by default.
- **Seed (Q-SCRIPT-44 with Q-MASK-211).** `random seed` and the ON full-dump prefix share one generator per project. No project setting pins the seed unless the user asks for one (Open Question 4). OFF output does not depend on the seed.

### D16. Desktop (SHELL)

- **Menu items.** The wrapper's "Read GDS" and "Write GDS" items (`gui/wrapper.cpp`) are enabled. `later_milestone()` in `menu_bar_model.cpp` stops returning M5. The items issue the logged `magic::promptload gds` and `magic::promptsave gds`, which the prompt procedures (`gui/prompts.*`, SHELL in M5) complete for the `gds` and `cif` kinds through `platform::FilePicker`, as Magic's wrapper.tcl does. For `cif` this includes the `cif_temp` load-and-delete sequence of `promptload cif`. The pickers pass the root name, and the extension rule applies (Q-MASK-320, 210). Magic's wrapper has no CIF menu item, so none is added; `promptload cif` is reachable from the console and scripts. The M4 GUI-manager scenario "GDS with a single top cell" (`ui-gui-managers` "File prompts", m4 task 9.5) is completed here.
- **Progress and cancel (Q-MASK-414).** A long read or write shows progress and can be cancelled. Both use the existing patterns: the Tcl console's running header and the canvas Interrupt card after 2 s (m4 D5). A write's prepare phase (D4) runs on the command thread under the exclusive lock, with frames pumped; its generate-and-emit phase runs on a worker under the shared lock and the UI keeps drawing. Reads run on the command thread with progress pumping frames, as Tcl evaluation does, because they take the exclusive lock.
- **No new board.** Both items reuse approved patterns (`M4-Menu-Bar`, `M4-Script-Consent`'s long-running state). If SHELL finds that an element is missing from every board, DESIGN draws it first, following the M2–M4 rule. `docs/design/m5-ui-conformance.md` records the items.

### D17. Ownership of the M1–M4 changes M5 makes

| Change | Owner | Decision or requirement |
|---|---|---|
| `uses_in_search_order` and every use area search through `UseIndex`; DBReComputeBbox re-placement | INDEX | Q-MASK-204, `gds-write` "Structure ordering" |
| Q-MASK-400 OFF in the DRC interaction finder (`drc/engine/interaction.cpp`) | INDEX (with GEN sharing the finder) | `drc-engine` "Interaction area" |
| `drc::CifCatalog` source moves to `OutputStyle`; old parse code removed | STYLES | m3 D5/D13 |
| mask-layer step in `basic_check` | DRCCIF | `drc-cif-rules` |
| CIF paint table moved from `hints/region` to `mask/plane` | OPS | m3 D10 |
| hints wiring in generation and reading | GEN, INPUT, GDSR | m3 D13 |
| vendor content stamps in `cell/timestamps.cpp` | GDSR | `gds-read` "When content-hash timestamps are assigned", `cell-timestamps` |
| `cif` command moves out of `script/binding/` | CIF (BINDING's file retired) | m4 D6 |
| `random()` generator moved into `EditorState` and shared | INTEGRATION | Q-MASK-211 |
| `magic-tech-sections` mask loses the cifinput line; the deviation text of UI-G-005 drops `calma, gds, cif` | GOLDEN | D9, D18 |
| desktop GDS menu items enabled; `promptload`/`promptsave` `gds` and `cif` completed | SHELL | D16, m4 task 9.5 |

### D18. Golden cases and Magic session-state pinning (GOLDEN)

- **Authoring.**
  - `tools/golden/authoring/mask_cases.py` covers the 233 MASK rows and the `MASK-RT-*` rows.
  - `m5_cases.py` is the driver, as `m4_cases.py` is: `--list` and `--not-authored`.
  - Cases are single `input/commands.tcl` files that both Magic and `hefesto-cli --magic` run (`tclcase.py`). They use small synthetic technology files committed per case family.
  - `authoring/gdsgen.py` is a small GDS record writer, with unit tests in `tools/golden/tests/`, for hand-built inputs: bad headers, corrupt `GDS_END`, PATHTYPE, AREF, properties, odd TEXT. Inputs under 64 KB are committed as binary. Larger ones are generated at authoring time with a digest.
  - MASK-GW-040 (the eight-byte real vectors) becomes a unit test.
  - MASK-GW-032 and 034 use `.mag` cells with `LEFview`/`GDS_FILE` properties instead of `lef read`.
- **Session-state pinning** (Q-MASK-204, 211, 202, 203). Each case runs in a fresh Magic session and a fresh `hefesto-cli` process. It makes one write, with no DRC, extraction or select-area search before it, unless the case tests history on purpose. A history case names its steps: a second write in the same session, a write after `drc check`, ≥10 uses with an interaction for CIF and GDS. Each case also does the following:
  - PDK cases pass `--trust-pdk` and run `random seed 1` after startup;
  - background DRC is off (`drc off`) unless the case is about DRC;
  - the capture image's `TZ=UTC` and `LC_ALL=C` apply to both tools.

  Cases are written so that `drc off` never hides a search Magic would make at idle. The capture runs `magic -dnull`, where idle DRC does not run during a script.
- **Comparators and masks.**
  - `gds-mod-dates` (D12);
  - the existing `gds-dates`;
  - a `gzip` canonicalizer (decompress, then compare);
  - `gds-records` (decoded diff, for display only);
  - `cif-preamble` (the six Q-MASK-324 lines);
  - a polygon-set canonicalizer for `cif see`/feedback output where Magic's order is incidental;
  - the `Completed N%` console mask.

  The cifinput line leaves `magic-tech-sections`. The 490 cases that carry that mask are replayed and must still pass.
- **Other areas.**
  - DRC-CIF-4..7 lose `deferred = "M5"`.
  - MISC-MH-03..06 and MISC-FL-05 are authored in `misc_cases.py`, and CELL-TS-07 in `cell_cases.py`.
  - `UI-G-005`'s deviation text drops `calma, gds, cif`. The case stays deferred to M12 for the other commands.
- **Deviations.** Only with an approved decision:
  - the fix rows whose output differs from Magic: Q-MASK-201 (only in a case with an angstrom write first), 208 (label text after a write, MASK-GW-005), 209, 219, 222, 227, 314, 318, and Q-MASK-10 for any listing case where Magic's prefix shadowing drops a style;
  - the platform rows: Q-MASK-223 (handled by the canonicalizer) and 324 (by the mask).

  MASK-OP-032 ("per platform") is captured on the x86_64 image. Under the Q-MASK-227 fix, Hefesto's replay must be identical on every leg.

### D19. PDK library GDS data and the round-trip check (PDK, GOLDEN, PERF)

- **Data sets.** `fetch_pdk.py` gains `sky130-gds`, `gf180mcu-gds` and `ihp-gds`. They use the same pins as the M4 sets. Each `-only` subset is `<variant>/libs.ref/<lib>/gds/` for every library in the release, and each set has its own tree digest and cache key (`pdk-<set>-<pin>-<digest prefix>`).
  - Each set installs the libraries' assets with `-lib`. The ciel library tarballs already hold `gds/`, so the M4 `.mag` sets keep their digests.
  - Task 1.1 lists the libraries, their download size, the kept size and the peak disk use (m3 D15: `-only` does not stream). The facts are recorded here.
  - GitHub's 10 GB cache limit per repository is the constraint. If the kept GDS exceeds about 4 GB across the three PDKs, or the cold fetch exceeds the job's disk, the PDK step stops and raises Open Question 1 with the measured numbers.
  - **Task 1.1 facts (measured 2026-11-13, local install into a scratch root).** The GDS sets install under their own PDK root, `.cache/pdks/gds`: `pdk install` skips a library whose `libs.ref/<lib>` already exists, so installing GDS into the M4 version directories would silently skip `sky130_fd_sc_hd`, `sky130_fd_pr`, `gf180mcu_fd_sc_mcu7t5v0` and `gf180mcu_fd_pr`, and the cache paths would overlap.

    | Set | Libraries | Files | Downloaded | Kept | Peak disk |
    |---|---|---|---|---|---|
    | `sky130-gds` (sky130A) | 10 | 21 | 2.94 GB + 6.6 MB common | 211 MB | 4.73 GB (`sky130_fd_sc_ls`: 743 MB archive, 3.6 GB tar; one install per library) |
    | `gf180mcu-gds` (gf180mcuD) | 12 | 35 | 514 MB + 170 MB common | 130 MB | 1.95 GB |
    | `ihp-gds` (ihp-sg13g2) | 4 | 33 | 22 MB + 434 MB common | 90 MB | 1.20 GB |

    Kept in all: about 431 MB, about 50 MB compressed as `actions/cache` stores it (zstd `--long=30`), far below 4 GB, so every set is cached and nothing is fetched uncached. The libraries and per-set sizes are in `docs/building.md` "Library GDS data sets (M5)". Left out, with no GDS for the variant: `sky130_ml_xx_hd` and `gf180mcu_ocd_alpha_{large,misc,small}` (`.mag` only), and `sky130_fd_pr_reram` (sky130B only; `pdk install sky130A` rejects it). Several libraries hold more than one file: `sky130_fd_io` 9, `sky130_sram_macros` 4, `gf180mcu_fd_pr` 11, `gf180mcu_re_efuse` 7, `gf180mcu_fd_ip_sram` and `gf180mcu_ocd_ip_sram` 4 each, `gf180mcu_fd_io` 2, `sg13g2_sram` 30. A `MASK-RT-<lib>` case covers every file of its library (one round trip per file, in one case); GOLDEN may split a library into `MASK-RT-<lib>-<suffix>` cases, which `scripts/gds_rt_shards.py` assigns to the library's shard. Magic's read and write of each file, and its time, are measured by the dispatch-only `d19-magic-timing` job of `golden-capture.yml` (`scripts/d19/`); tasks.md 1.1 records the result. Magic reads all 89 files with the PDK's rc. 85 of them round-trip on their own. The four `ef_io` wrapper files (`sky130_ef_io`, `sky130_ef_io__analog`, `sky130_ef_io__gpiov2_pad_wrapped`, `gf180mcu_ef_io`) place cells defined in their library's main file, so on their own Magic's write stops with "Failure to read entire subtree" and leaves an empty file. Their round trip reads the defining file (`sky130_fd_io.gds`, `gf180mcu_fd_io.gds`) first, in the same session. Magic's read+write takes about 18 minutes for sky130, 8 for gf180mcu and 83 for IHP, almost all of it in the SRAM macro writes.
- **Round-trip definition (proposed, Open Question 1).** For each library GDS file L, both tools run the same script in a fresh session, with the PDK's rc trusted and `random seed 1`:
  1. `gds read L` with the PDK's default read options;
  2. create a wrapper cell that places every top structure, then pin its timestamp with `cellname timestamp <wrapper> 0` (Magic's `cellname` timestamp option, commands/CmdCD.c:1554-1561);
  3. `gds library yes; gds write out`.

  Library mode omits the wrapper's own structure and writes every structure below it. LIBNAME is still the wrapper, and BGNLIB's creation date is the wrapper's `cd_timestamp` when no `gds datestamp` is set (calma/CalmaWrite.c:3075-3078). `gds-mod-dates` does not mask it, so step 2 pins it to 0, and both tools must write it as 1970-01-01 00:00:00 under the capture image's `TZ=UTC`. Structure creation dates keep the BGNSTR dates of the read (calma/CalmaRdcl.c:504-510), so they are compared too. The wrapper has many uses, one of which touches its bounding box, so with 10 or more top structures the write's subtree-loading search bins it (D7) and the round trip exercises the binned Q-MASK-204 order; libraries with fewer top structures exercise the unbinned order.

  Pass means Hefesto's `out.gds` equals Magic's after `gds-mod-dates`. Expected outputs of this size are not committed. `expected/` stores the SHA-256 of the masked, decompressed output and a compact decoded summary (structure names, record counts and the BGNLIB creation date) for diagnosis, as DRC-PR-1's printrules digest did. A library that Magic itself cannot read or write correctly is recorded with an approved decision.

  There is no read/write fixed-point gate. A GDS read pushes each SREF at the head of the unbinned list and the next write walks it from the head, so SREF order reverses on each cycle for a cell with 2 to 9 uses under ON and always under OFF, and regenerating masks from read-back layers is not guaranteed idempotent (cuts, merges, labels). Instead a Hefesto-only *equivalence report* reads `out.gds` back, writes it again, and compares the two writes by decoded content, insensitive to order: structures as a set by name, SREF/AREF records as a multiset per structure, and the geometry of each layer and datatype as a canonical polygon set. Differences are listed in the job summary for review. They do not fail the job.
- **Where it runs.**
  - One `MASK-RT-<lib>` case per library. The three standard-cell cases (`sky130_fd_sc_hd`, `gf180mcu_fd_sc_mcu7t5v0`, `sg13g2_stdcell`) are small and carry the ordinary `golden` label. `ci.yml`'s golden legs fetch the three standard-cell GDS subsets (their own cache keys) and replay them on every leg, so the round trip is checked byte for byte on Linux x86_64, Linux aarch64 and macOS arm64. A missing input in those legs is an error, not a skip.
  - Every other `MASK-RT-*` case carries a new label `golden-gds-rt`. `replay.py` excludes that label by default (`--include-gds-rt` opts in), and `ci.yml` does not run it (its ctest legs pass `-LE 'golden-slow|golden-gds-rt'`).
  - A new `gds-roundtrip` job in `perf.yml` runs every `MASK-RT-*` case on main, on the schedule, on dispatch, and on pull requests that touch the GDS, mask or CIF paths. `perf.yml`'s `push` and `pull_request` path filters gain `src/core/{include/hefesto,src}/{gds,mask,cif}/**`, `tests/golden/MASK/**`, `bench/gds/**` and `scripts/fetch_pdk.py` (`src/core/src/db/**` is already there).
  - Capture of the `MASK-RT-*` cases runs in a separate, sharded job of `golden-capture.yml` (`capture-gds-rt`, one shard per PDK, with large libraries such as `sky130_fd_io` and the SRAM macros in shards of their own if task 1.1's timings need it), each with its own timeout sized from task 1.1's measured Magic read+write times plus a margin. The existing 120-minute `capture` job keeps the other cases. INTEGRATION owns the job; GOLDEN owns the case list it shards.
    - As landed (task 1.6): a `plan` job splits the selected cases; `capture-gds-rt` is a matrix over the shards that have cases. The shard table is `scripts/gds_rt_shards.py`: `sky130`, `sky130-io` (`sky130_fd_io`), `sky130-sram` (`sky130_sram_macros`), `gf180mcu`, `gf180mcu-io` (`gf180mcu_fd_io`, `gf180mcu_ocd_io`), `ihp`, `ihp-io` (`sg13g2_io`) and `ihp-sram` (`sg13g2_sram`, 82 minutes in Magic), each with its PDK's tech and GDS sets and a `timeout-minutes` of about 35 minutes plus twice task 1.1's Magic time (tasks.md 1.1 and 1.6). A case's library is the part of its directory name after `MASK-RT-` up to the next `-`; a library in no shard fails the plan.

### D20. Performance targets and gates (PERF)

- **Target.** `hefesto-performance` "Batch throughput relative to Magic": single-threaded GDS read and GDS write no slower than Magic 8.3.694 on the same workloads and machine.
- **Workloads** (`bench/gds/`):
  1. read `sky130_fd_sc_hd.gds`;
  2. write the M3 all-cells block (every `sky130_fd_sc_hd` cell in rows) to GDS with the `gdsii` style;
  3. write the same block with `merge yes`.
- **Gate (proposed, Open Question 2).** As the user decided for DRC on 2026-11-12, `gds-bench` measures callgrind instructions per workload on the x86_64 runner for both tools. It gates Hefesto at or below Magic's count for each workload. Once the baseline is recorded, the gate is the baseline + 2 %, as `drc-profile` does. The timed ratio is reported as information. The job runs on pull requests that touch `gds/`, `mask/`, `cif/` or `db/use_index*`, and on main, schedule and dispatch. If Open Question 2 is answered (a), task 14.5 amends `hefesto-performance` "Batch throughput relative to Magic", which still names wall time.
- **DRC.** `drc-profile`'s 12.38-billion gate must still pass after D14 and D7, because the use index sits on the DRC interaction path.
- **Data layout.** The operator kernels reuse pooled planes. The record writer buffers 64 KB. The reader inflates in 64 KB blocks. The content hash runs in one linear pass.

### D21. Fuzz targets (INTEGRATION with GDSR, CIF, STYLES, INPUT)

`hefesto-testing-strategy` "Parser fuzzing" requires a target for every external reader. M5 adds:

| Target | Input | Covers |
|---|---|---|
| `fuzz_gds_reader` | raw bytes; a leading flag byte selects plain or gzip-wrapped | the record reader, the full GDS read into a scratch session with a fixed synthetic tech, the content-hash scanner and the read-only view path |
| `fuzz_cif_reader` | CIF text | lexer, parser, symbol table and read engine |
| `fuzz_cifoutput_section` | a technology file whose `cifoutput` section is the fuzz input | the reader, operator argument validation, rescale |
| `fuzz_cifinput_section` | a technology file whose `cifinput` section is the fuzz input | the input reader |

- Seeds come from the MASK case inputs, the PDK tech sections and `gdsgen.py` outputs.
- Each target has resource caps (structure count, recursion depth, plane tile count) so that time-outs are real hangs, not large inputs.
- The 60 s CI run and the seed replay on every leg follow M3/M4.
- A write after the read (`fuzz_gds_reader` also writes to an in-memory sink) checks that read-then-write never crashes.

### D22. Floating point and cross-platform determinism

The float paths that can differ between x86_64 and aarch64 are:
- Euclidean grow;
- the grow-min `sqrt`;
- the Q-MASK-227 bridge distance, now clamped;
- path outlines (Q-MASK-118: ON uses H = 1.5708);
- the eight-byte real;
- rescale ratios;
- CIF wire ends.

They are written with explicit `double` operations in Magic's order, `-ffp-contract=off`, and no `long double` or `std::fma`.

A pinned-value unit test per path runs on every leg. The determinism job of `ci.yml` gains `--require-area MASK` once MASK expected outputs exist. Q-MASK-205's sort is our own stable merge sort, so the output does not depend on the libc.

### D23. Items deferred to M5 from earlier milestones

| Item (source) | M5 home |
|---|---|
| DRC-CIF-4..7 `deferred = "M5"`; DRC-CIF-2/3 regression | DRCCIF + GOLDEN |
| MISC-MH-03..06 (m3 D12/D13), mask-hints operator wiring | GEN, INPUT, GDSR + GOLDEN |
| MISC-FL-05 (needs `gds write`/`read`) | GOLDEN |
| CELL-TS-07 (needs `gds readonly`); vendor content stamps hook (`cell/timestamps.cpp`) | GDSR + GOLDEN |
| m4 task 9.5 scenario "GDS with a single top cell"; menu "Read GDS: arrives in M5" | SHELL |
| M1 coverage gap "GDS font attributes" (`font-labels` GDS attributes, M1 246/247) | GDSW (Q-MASK-122 formulas) |
| `cif` options beyond the M4 queries (m4 D6) | CIF |
| Q-SCRIPT-44 seed-pinning setting (with Q-MASK-211) | INTEGRATION (Open Question 4) |

## Area contracts

The headers are the contract, and each one is documented. The key types and functions per area (the scaffold writes them with stubs):

### STYLES: `hefesto/mask/style/`
- `read_output_styles(const tech::RawSection&, diag::Sink&) -> OutputStyleSet`
- `OutputStyle` (layers, ops, scale, labels, GDS map, options, `halo()`, `contact_size(type)`, `rescale(n, d)`)
- `select_output_style(set, name, compat) -> Result`
- the shared core: `StyleStateMachine`, `parse_scale`

### OPS: `hefesto/mask/plane/`, `hefesto/mask/ops/`
- `MaskPlane`, `PlanePool`, `cif_paint_table()`
- `evaluate_layer(...)`, and one `op_<name>(const OpArgs&, OperatorContext&, MaskPlane& out)` per operator
- `CompatSettings` helpers `q_mask_<n>_*`

### INDEX: `hefesto/db/use_index.hpp`
- `UseIndex` (`place`, `remove`, `search`, `find_equal`, `enumerate_all`, `order(compat)`, `state_for_test()`)
- `uses_in_search_order(def, compat)`
- `uses_overlapping(def, area, visitor)`

### GEN: `hefesto/mask/gen/`
- `GenerationContext`, `Progress`
- `generate_flat(EditorState&, const CellDef&, Rect, const OutputStyle&, GenerationContext&) -> LayerPlanes`
- `generate_hierarchical(...)`
- `see`, `paint`, `coverage`, `statistics`
- `interaction_areas(...)` (shared with DRC)

### INPUT: `hefesto/mask/input/`
- `read_input_styles(...) -> InputStyleSet`, `InputStyle::resolve(compat)`
- `ReadSink`, `ReadEngine` (implements `ReadSink`; planes, scale, rescale, paint-current)

### GDSR: `hefesto/gds/stream/`, `hefesto/gds/read/`
- `ByteSource`, `ByteSink` (plain, gzip, memory), `RecordReader`, `RecordWriter`
- `encode_real8`, `decode_real8`, `CharTable`, `encode_date`
- `read(EditorState&, path, GdsReadOptions, Progress*) -> CommandResult`
- `content_stamps(bytes) -> StampTable`

### GDSW: `hefesto/gds/write/`, `hefesto/gds/command/`
- `prepare_write(EditorState&, CellDef& root, GdsWriteOptions) -> Result<PreparedWrite>` (exclusive lock), `emit_write(const PreparedWrite&, path, Progress*) -> CommandResult` (shared lock), and `write(...)`, which calls both
- `merge_polygons(const MaskPlane&) -> std::vector<Polygon>`
- `GdsSettings`, `register_gds_command(CommandRegistry&)`

### CIF: `hefesto/cif/`
- `read(EditorState&, path, CifReadOptions, Progress*)`, `SymbolTable`, `ReadSession`
- `prepare_write(...)`, `emit_write(...)` and `write(EditorState&, CellDef& root, path, CifWriteOptions, Progress*)`, split as GDSW's (D4)
- `CifSettings`, `register_cif_command(CommandRegistry&)`

### DRCCIF: `hefesto/drc/check/mask_check.hpp`
- `mask_layer_check(const CheckInput&, const Rect& K, ErrorSink&, GenerationContext&)`
- `map_mask_error(Rect mask_rect, const OutputStyle&, compat) -> std::optional<Rect>`

## As-landed scaffold contracts

The scaffold (tasks.md 2.2–2.6, branch `m5-r1-scaffold`) landed the areas, headers and stubs below. The headers are the contract; this section records where they are, who fills them in, and where they differ from "Area contracts" and D4.

### Files and owners

| Path | Contents | Owner (fills in) |
|---|---|---|
| `src/core/src/{mask,gds,cif}/{sources.cmake,codes.cpp}` | area sources and code tables assembled from `<sub>/codes.inc` (sorted at compile time) | INTEGRATION |
| `src/core/src/mask/{plane,ops}/`, `include/hefesto/mask/plane/plane.hpp`, `mask/ops/{evaluate,operators}.hpp` | `MaskPlane`, `PlanePool`, `cif_paint_table`, `cif_erase_table`, `mask::Polygon`; `OperatorContext`, `LayerInputs`, `evaluate_layer`, `OpArgs` and one `op_<name>` per operator | OPS |
| `src/core/src/mask/style/`, `include/hefesto/mask/style/{core,style}.hpp` | `ScaleValue`, `parse_scale`, `StyleStateMachine`; `OpKind`, `LayerOp`, `OutputLayer`, `GdsLayer`, `StyleScale`, `OutputStyle`, `OutputStyleSet`, `read_output_styles`, `select_output_style` | STYLES |
| `src/core/src/mask/input/`, `include/hefesto/mask/input/{style,sink,engine,warnings}.hpp` | `InputLayer`, `InputResolved`, `InputStyle`, `InputStyleSet`, `read_input_styles`, `select_input_style`; `ReadSink`, `ReadShape`, `ReadLabel`; `ReadEngine`; `WarningLevel`, `WarningSettings` | INPUT |
| `src/core/src/mask/gen/`, `include/hefesto/mask/gen/{progress,context,generate}.hpp` | `Progress`, `GenerationStatistics`; `GenerationContext`; `LayerPlanes`, `generate_flat`, `generate_hierarchical`, `see`, `paint`, `coverage`, `statistics`, `interaction_areas` | GEN |
| `include/hefesto/mask/session.hpp` | `MaskSession` (`EditorState::mask`) | INTEGRATION (members as D4) |
| `src/core/src/db/use_index.cpp`, `include/hefesto/db/use_index.hpp` | `UseIndex`, `UseIndexState`, `UseIndexVisitor`, `uses_in_search_order(def, compat)`, `uses_overlapping` | INDEX |
| `src/core/src/gds/{stream,read}/`, `include/hefesto/gds/stream/{io,records,encode}.hpp`, `gds/read/{read,stamps}.hpp` | `ByteSource`, `ByteSink`, `MemorySource`, `MemorySink`, `open_source`, `open_sink`; `RecordType`, `DataType`, `Record`, `RecordReader`, `RecordWriter`; `encode_real8`, `decode_real8`, `CharTable`, `encode_date`; `GdsReadOptions`, `read`; `StructureStamp`, `StampTable`, `content_stamps` | GDSR |
| `src/core/src/gds/{write,command}/`, `include/hefesto/gds/write/write.hpp`, `gds/command/{command,settings}.hpp` | `GdsWriteOptions`, `PreparedWrite`, `prepare_write`, `emit_write`, `write`, `merge_polygons`; `GdsSettings`, `PolygonSubcells`, `gds_options`, `register_gds_command` | GDSW (read fields of `GdsSettings`: GDSR) |
| `src/core/src/cif/`, `include/hefesto/cif/{read/read,write/write,command/command,command/settings}.hpp` | `CifReadOptions`, `SymbolTable`, `ReadSession`, `read`; `CifWriteOptions`, `PreparedWrite`, `prepare_write`, `emit_write`, `write`; `CifSettings`, `register_cif_command` (the M4 command, moved) | CIF |
| `src/core/src/drc/check/mask_check.cpp`, `include/hefesto/drc/check/mask_check.hpp` | `mask_layer_check`, `map_mask_error` | DRCCIF |
| `include/hefesto/cmd/stubs.hpp`, `src/core/src/cmd/stubs.cpp` | `not_implemented`, `not_implemented_status`, `not_implemented_diagnostic` (`CORE-NOTIMPL-005`) | INTEGRATION (removed at M5 exit, task 15.5) |
| `include/hefesto/util/libc_random.hpp`, `src/core/src/util/libc_random.cpp` | `util::LibcRandom` (glibc TYPE_3 `random()`) | INTEGRATION |
| `tests/unit/{mask,gds,cif}/` and their subdirectories, `tests/unit/db/use_index/` | one executable per subdirectory once it has a `*_test.cpp` (`hefesto_add_area_tests` skips an empty directory) | as the source subdirectory |
| `tests/unit/core/m5_scaffold_test.cpp` | the `[m5-scaffold]` tests | INTEGRATION; owners replace them with `[cap:][req:]` tests |
| `docs/diagnostics/codes/{mask,gds,cif}.md` | one section per owning workstream and the generated table | INTEGRATION (sections: their workstreams) |
| `docs/design/m5-ui-conformance.md` | seeded with the D16 items | SHELL ("Performance": PERF) |

### Differences from "Area contracts" and D4

- **Read warnings.** Magic keeps one `CIFWarningLevel` for both readers, set by `cif warning` and `gds warning`. It is one `mask::WarningSettings` in `MaskSession::warnings`, not a field of `GdsSettings` and `CifSettings`. The read options carry a copy.
- **Progress.** `mask::Progress` (with `GenerationStatistics`) is in `mask/gen/progress.hpp`, so the GDS and CIF headers include it without the generators. Its cancel token is a `const std::atomic<bool>*`; `GenerationContext` and `OperatorContext` point to it.
- **Return types.** `read_output_styles`, `read_input_styles`, `generate_flat`, `generate_hierarchical`, `evaluate_layer` and `mask_layer_check` return their value and report errors to the caller's sink, as the contract's signatures require. `select_output_style` returns `diag::Status` (the contract's "Result"). `select_input_style` is added for `cif istyle`. `prepare_write` returns `diag::Outcome<PreparedWrite>`. Each `op_<name>` returns `diag::Status`.
- **Signatures.** `generate_hierarchical(editor, cell, style, ctx)`; `see`, `paint` and `coverage` take the editor, the cell, the layer name, the area and the context; `interaction_areas(cell, halo, compat)`. The read and write entry points take `platform::Path` and an optional `const mask::Progress*`. `GdsReadOptions`/`GdsWriteOptions` and `CifReadOptions`/`CifWriteOptions` carry a snapshot of the session settings and the typed name; `CifWriteOptions::flat` selects `cif flat`.
- **Use index.** `UseIndex` is a standalone class for now. INDEX adds the `CellDef` member and the re-placement hooks (D2). The visitor type is `db::UseIndexVisitor`, because `db::UseVisitor` already names the hierarchy walker of `db/hierarchy.hpp`. The scaffold version keeps only the unbinned LIFO list, so `order()` gives today's `uses_in_search_order` order in both modes and no search rebuilds. `search` already applies the shrink by 1 of `BPE_OVERLAP`.
- **Working parts.** Some scaffold pieces are not stubs, because the stubs and tests need them: `cif_paint_table`/`cif_erase_table`, `MaskPlane::paint`/`erase`/`clear`/`empty`, `PlanePool`, `MemorySource`/`MemorySink`, `SymbolTable` (`DD` in both modes), `GenerationContext::operator_context`, and `write()` calling `prepare_write` then `emit_write`. The owners may replace them.
- **Queries before their owners land.** Some stubs return a neutral value instead of `CORE-NOTIMPL-005`: `OutputStyle::halo()` returns 0, `contact_size` and `parse_scale` return nullopt, `content_stamps` returns an empty table, `encode_real8`/`encode_date` return zero bytes, `CharTable` is the identity, `merge_polygons` and `statistics` return nothing, and `map_mask_error` returns nullopt. Nothing calls them yet: `basic_check` does not call `mask_layer_check` until DRCCIF wires it.
- **Commands.** `gds` and `calma` are registered with Magic's 29-entry table, Magic's lookup and invalid-option message, the window check of `read`, `write` and a bare `gds`, and `gds help`. Every other option answers `CORE-NOTIMPL-005` ("not implemented yet: gds <option>"). `cif` moved from `script/binding/cif_command.cpp` to `cif/command/command.cpp` with its M4 answers unchanged. Its other options answer `CORE-NOTIMPL-005` ("not implemented yet: cif <option>"; a bare `cif` is `cif write`), and `SCRIPT-BIND-002` is retired.
- **random.** The generator is `util::LibcRandom` in `MaskSession::random`, one per open project (it was one per process). A command context without an editor uses a process-wide generator. A fresh session starts seeded with 1, so `random seed 1; random` gives 1804289383 as in M4.
- **Edits outside the new areas.** `src/core/src/db/sources.cmake` (`use_index.cpp`), `src/core/src/drc/check/sources.cmake` (`mask_check.cpp`), `src/core/src/db/commands_misc.cpp` (the generator), `src/core/src/script/binding/{commands.cpp,sources.cmake,codes.inc}` and `include/hefesto/script/binding/commands.hpp` (the `cif` move), `tests/unit/db/CMakeLists.txt` (`use_index/`), and `docs/diagnostics/codes/script.md`.

### Round 2 contract updates

Round 2 (branch `m5-round2`) changed these contracts; the headers are authoritative.

- **Output styles (STYLES).** `read_output_styles(const tech::Technology&[, const tech::RawSection&], sink)` takes the technology. New: `read_first_output_style` (the tech loader's grid), `RescaleStep`/`rescale_step`, `rescale_output_styles(set, n, d, compat)`, `select_output_style(set, name, lambda_n, lambda_d, compat, sink)`, `OutputStyleSet::base` (the styles as read, which a selection reloads from) and `OutputStyleSet::section_failed` (Q-MASK-17), and `MaskErrorReporter` (`mask/style/errors.hpp`). `OutputStyle` gained `label_layer`, `port_text`, `port_layer`, `radius`, `load_messages` and the `style_option` flags.
- **LayerOp (STYLES, read by OPS).** `distance` is `co_distance` (grow, shrink, grow-grid, grow-min, close threshold, bridge and bridge-lim spacing, bloat-all clip). `values` holds only the client data, indexed by `squares_value`, `slots_value` and `bridge_value`; for `close` OPS reads an optional `{declared, D, L}` record there (Q-MASK-2 OFF), which the reader does not fill yet. `bloat` (`BloatTable`: `plane` and `distance[]`) serves bloat-or/min/max/all. `option` is the `co_client` flag (maxrect `external`, orthogonal `fill`, bbox `top`). The operand table in `mask/ops/operators.hpp` follows this layout.
- **Operators (OPS).** New public header `mask/ops/compat.hpp` (`q_mask_*` helpers, D22 float paths); `LayerInputs::technology`, `OperatorContext::{hierarchical, stale_marks}` and `OpArgs::layer` were added, all defaulted.
- **Session.** Tech load fills `EditorState::mask.output_styles` (emitting the first style's messages before the drc section's) and fails the load when the cifoutput section failed; the styles follow every grid change (tech load lambda, `scalegrid`, a finer-grid `.mag` read) through `rescale_output_styles`.
- **Use index (INDEX).** `CellDef::use_index()`; `db::place`/`remove_use`/`recompute_bbox` drive it; `db::search_subtree` (DBCellReadArea) and `db::rebuild_use_index` (DBMoveCell, DBScaleEverything). Direct edits of `CellDef::uses()` are reconciled on the next index read. The DRC context's overlap search goes through `db::uses_overlapping`.
- **GDS (GDSR).** New headers `gds/stream/{gzip,values,dates}.hpp`, `gds/read/{library,vendor}.hpp`. `cell::update_timestamps` takes a defaulted `VendorStampFn`.
- **CIF read (CIF).** `cif/read/{lexer,messages,parser,wire,symbols}.hpp`: the parser drives `cif::CifReadTarget` (`mask::ReadSink` plus `rescale` and the other CIF operations); `SymbolTable` moved from `read.hpp` to `symbols.hpp`.
- **db::Plane.** The split-tile merge after a paint stays inside the usable plane (a paint touching the plane edge crashed).

## Risks / Trade-offs

- **R1. Use-order emulation (Q-MASK-204).** A small mistake in bin sizing or the walk changes SREF and `C` order only in binned cells. That shows up late, in large designs and second writes. → INDEX lands early with trace tests against Magic orders (the probe of task 1.5, which lands first), targeted golden history cases, and the round-trip corpus (wrapper cells with hundreds of uses). The OFF order is history-free, so users who switch compat off are never exposed.
- **R2. Byte identity of merged polygons and tiles (Q-MASK-217).** Point order and split points depend on the mask planes' tile structure and paint order. → Planes use Magic's CIF paint table and paint order. The merge pass is a line-for-line port. MASK-GW merge cases and the round trips (with `merge` as the PDK sets it) exercise it.
- **R3. Port size.** About 12,000 lines of Magic C (CIFgen, CIFhier, CIFrd*, CIFwrite, Calma*, DRCcif, bplane) carry about 70 compat branches under a complexity limit of 15. → Four independent operator groups. Dispatch tables for the GDS records, CIF commands and `gds` options. One helper per compat branch. Flagged exceptions of at most 25.
- **R4. DRC cost.** Mask generation inside the basic check could break the `drc-profile` gate. → Generation is skipped without applicable rules, the planes are pooled, and the gate runs on every DRC-path PR.
- **R5. Concurrency.** Generation on the DRC worker and index rebuilds under the shared lock, and a write that loads and fixes up the hierarchy before generating. → The write's prepare phase under the exclusive lock (D4), reentrant contexts with no globals, the index mutex with snapshot orders, immutable styles, and the TSan leg.
- **R6. Data volume and CI time of the round trips.** Several GB of library GDS, a 10 GB cache limit, and Magic's own time on large libraries. → Separate GDS sets, a size and Magic-time check first (task 1.1), a sharded capture job (task 1.6), digests instead of committed outputs, the full corpus off the per-PR CI legs (D19), and Open Question 1.
- **R7. Floating point.** Path outlines and Euclidean operators on arm64 against x86_64. → D22, pinned-value tests, and the determinism job.
- **R8. Mid-read rescale.** `DBScaleEverything` during a read touches the whole database, undo and the DRC worker. → The read holds the exclusive lock, undo is disabled for the read as in Magic, and the DRC worker waits on the exclusive lock, as it does for every command.
- **R9. Malformed input.** GDS and CIF are attacker-controlled files. → Fuzz targets with caps (D21), 64-bit coordinate arithmetic (Q-MASK-222), bounded recursion in post-order reading, and symbol and structure count limits reported as diagnostics.
- **R10. Native modes without a reference.** About 70 OFF behaviors have no Magic output to compare with. → Unit tests from the spec scenarios, one per mode for every compat requirement, as in M3/M4.

## Migration Plan

User data does not change: `.mag` files are read and written as before.
- The cell-use index changes how uses are stored. The `.mag` writer stays alphabetical, so saved files are unchanged.
- Cells that carry `GDS_FILE` get content-hash timestamps when their GDS is found, as `cell-timestamps` already specifies. Before M5 their stamp came from the `.mag` file.
- With `magic-compat` OFF, the DRC interaction area no longer includes the Q-MASK-400 phantom area. This can remove spurious interaction checks, but the errors are the same for designs without a real interaction there.
- DRC now reports mask-layer errors for styles with `cif*` rules (sky130 `drc(full)` and others). That is a visible change, documented in CHANGELOG.md.
- `CORE-NOTIMPL-005` exists only during M5.
- The CI reference data gains the GDS cache keys.

## Open Questions

These are product and scope decisions. **Decided 2026-11-13: the user chose the recommended option (a) for all four.** The options are kept for the record.

1. **Library round-trip scope and definition (exit criterion "every sky130, gf180mcu and IHP library GDS round-trips").** Decision: (a).
   - (a) *Recommended:* every library GDS in the three pinned releases, round-tripped as in D19 (read, wrapper with a pinned timestamp, `gds library yes`, write; equal to Magic's own read-and-write after `gds-mod-dates`; plus a non-gating, order-insensitive equivalence report of a second Hefesto read and write). The full corpus runs in a `gds-roundtrip` job on main, schedule, dispatch and PRs that touch GDS/mask/CIF paths. One standard-cell library per PDK replays on every CI leg. If task 1.1 finds the data over about 4 GB cached, the largest libraries (SRAM macros) are fetched in the job without caching.
   - (b) Only the standard-cell and primitive libraries (`sky130_fd_sc_hd`, `sky130_fd_pr`, `gf180mcu_fd_sc_mcu7t5v0`, `gf180mcu_fd_pr`, `sg13g2_stdcell`, `sg13g2_pr`) in CI. The other libraries run once, manually, at exit, with the results recorded.
   - (c) Compare against the source file's geometry (decoded polygon equivalence), not against Magic's output. This is cheaper to capture, but it does not prove byte identity.
2. **GDS throughput gate.** Decision: (a); task 14.5 amends `hefesto-performance`.
   - (a) *Recommended:* callgrind instructions per workload at or below Magic's count, then the baseline + 2 % once recorded, with wall time as information. This matches the user's DRC decision of 2026-11-12. `hefesto-performance` "Batch throughput relative to Magic" still says single-threaded wall time, and the DRC decision was recorded only in m3-drc design D16, so answering (a) also amends that requirement for DRC and GDS read/write and records the decision (task 14.5).
   - (b) The spec's single-threaded wall time against Magic on the same runner, as a hard gate. It is noisy across runner types.
   - (c) Record only, with no gate in M5.
3. **Desktop scope for M5.** Decision: (a).
   - (a) *Recommended:* enable the GDS menu items and the `gds`/`cif` prompts with the file pickers and the existing progress and cancel patterns (D16), with no new board.
   - (b) Also add a GDS import/export dialog exposing the main `gds` options (readonly, flatten, merge, compress). DESIGN draws a board first.
   - (c) CLI and console only, with the menu items left disabled.
4. **Seed-pinning setting (Q-SCRIPT-44, deferred to M5 with Q-MASK-211).** Decision: (a), no setting.
   - (a) *Recommended:* no setting. With magic-compat OFF the full-dump prefix comes from a file-name hash, so output does not depend on the seed. With it ON, Magic's behavior is kept, and the golden harness runs `random seed 1`.
   - (b) A project setting `random-seed` applied after startup scripts, for reproducible ON-mode full dumps.

## Resolved Questions

All 105 open questions that blocked M5 were approved as drafted on 2026-11-13. `docs/decisions/m5-decision-table.md` holds the table, the counts (74 compat, 16 replicate, 11 fix, 4 platform), the evidence corrections, and the spec changes made after approval. The entries' `Resolution:` lines are in `docs/decisions/open-questions/MASK.md` (Q-MASK-*) and `DRC.md` (Q-DRC-13). The specs were aligned in the commit "Align the M5 specs with the approved decisions". This design cites each decision where it shapes the code: D5–D15 and D22.

- **D-20** (audit, `cif unfracture` argument) is settled by Q-MASK-413 (compat), reversing D-20's draft. It is recorded in `docs/decisions/audit/DECISIONS.md` and the decision table, section "Audit decision D-20".
- **Q-MASK-204 and Q-MASK-318** were judgement items in the first draft. They were decided by the standing rules: the compat rule and the byte-identical exit criterion for 204, and X-2 tier 1 for 318. The cost notes in the decision table's "Effort and scope notes" are the basis of D7 and D13.
