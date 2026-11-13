# Proposal: M5 Mask generation, GDS and CIF

## Why

After M4, Hefesto opens, edits, DRC-checks and generates PCells in real PDK designs, but it cannot produce or read a mask file. The technology `cifoutput` and `cifinput` sections are kept as raw lines. Only a quiet partial reader of `cifoutput` exists: the M3 `drc::CifCatalog`, which supplies DRC with style names, scale and contact borders. No operator, mask plane or generation code exists. There is no `gds`/`calma` command. The M4 `cif` command answers only the scale and style queries. The desktop's "Read GDS" menu item says "arrives in M5". The DRC mask-layer rules (`cifwidth`, `cifspacing`, `cifarea`, `cifmaxwidth`) are parsed and bound, but never checked, so DRC-CIF-4..7 are `deferred = "M5"`. `uses_in_search_order` gives only the unbinned order of Magic's cell-use index. Vendor-GDS cells have no content-hash stamp. Waiting for GDS are MISC-MH-03..06, MISC-FL-05, CELL-TS-07 and the M4 GUI-manager scenario "GDS with a single top cell".

M5 (docs/roadmap.md, "M5") delivers:
- mask generation with every cifoutput and cifinput operator;
- GDSII read and write, with compressed files, read-only vendor views and content-hash timestamps;
- CIF read and write;
- DRC rules that run on generated mask layers.

Its exit criteria: GDS output byte-identical to Magic's apart from the masked time fields, every sky130, gf180mcu and IHP library GDS round-trips, the MASK golden cases pass, and DRC-CIF-2..7 pass.

The 105 open questions that blocked M5 are resolved. All 105 were approved as drafted on 2026-11-13 (commit "Resolve the 105 open questions blocking M5", `docs/decisions/m5-decision-table.md`), and audit decision D-20 is settled by Q-MASK-413. The M5 specs are aligned with them (commit "Align the M5 specs with the approved decisions"). `scripts/check_milestone_readiness.py --milestone M5` reports ready.

## What Changes

- **Library GDS data first (PDK).** `scripts/fetch_pdk.py` gains one GDS data set per PDK. Each holds `libs.ref/<lib>/gds` of every library in the pinned ciel releases (open_pdks `5740c4a8…` standing in for `c6df28c1`, IHP `5e6d592e`), with sizes and digests verified before any round-trip case is authored. The sets have their own CI cache keys and are fetched only by the jobs that use them (design D19).
- **Output styles (STYLES).** `mask-output-styles` adds the full `cifoutput` reader: every statement, operator argument validation, tech messages, unit normalisation (Q-MASK-8, 10, 11, 13, 18) and the halo and contact-size queries. Its output is an immutable `mask::OutputStyle`. `drc::CifCatalog` becomes a view over it, with its interface unchanged, so there is one cifoutput parser.
- **Operators (OPS).** `mask-output-operators` adds mask planes with Magic's CIF paint table and one kernel per operator:
  - boolean, grow/shrink (square and Euclidean), grow-grid, grow-min;
  - bloat-or/min/max/all;
  - squares, squares-grid, slots;
  - close, bridge, bridge-lim, maxrect, orthogonal, not-square;
  - (non)overlapping and (non)interacting, net, tagged, bbox, boundary, mask-hints.

  Each kernel has its compat branch points (Q-MASK-1..27, 228..232).
- **Use enumeration order (INDEX).** A cell-use index in `db` emulates Magic's bplane for `magic-compat` ON: LIFO insertion, DBReComputeBbox re-placement, the bin rebuild at 10 or more uses on a non-surrounding search, and the bin walk. With magic-compat OFF it gives the canonical reverse-insertion order. GDS, CIF, DRC and later OASIS share it (Q-MASK-204, 323; design D7).
- **Generation (GEN).** `mask-hierarchical-generation` adds:
  - flat and hierarchical generation: the halo, interaction areas (Q-MASK-400, shared with hierarchical DRC), array correction, the residual, hierarchy-stopping operators and grow-slivers;
  - the compat splits Q-MASK-400..411;
  - progress and cancellation without partial output (Q-MASK-414);
  - the `mask-hints` wiring promised in m3-drc design D13;
  - `cif see`, `cif paint`, `cif coverage` and the statistics.
- **Input styles and the read engine (INPUT).** `mask-input-styles` adds:
  - the `cifinput` reader, sharing the style, variant and scale core with STYLES (Q-MASK-300..311, 326, 330, 334);
  - the read engine shared by CIF and GDS: read planes, scaling and mid-read rescale, painting through the operator kernels, boundary, mask-hints, copyup, tagged and labels.
- **GDS read (GDSR).** `gds-read` adds:
  - the stream layer: records with look-ahead and offsets into the decompressed stream, gzip input, eight-byte reals;
  - structures, BOX, BOUNDARY with non-Manhattan decomposition, PATH-to-polygon, TEXT and pins, SREF/AREF, properties, post-order reading, flatten and flatglob, polygon and path subcells;
  - read-only vendor views with the FNV-1a content-hash timestamps;
  - the compat splits Q-MASK-110..134.
- **GDS write (GDSW).** `gds-write` adds:
  - record and date encoding, the character tables, UNITS on every write, structure order and naming, tile and merged-polygon output (Q-MASK-217), labels, references and arrays;
  - verbatim copy, full dump, addendum and abstract views;
  - compressed output, and the `gds`/`calma` command with every option.
- **CIF (CIF).** `cif-format-io` adds:
  - the CIF lexer and parser with a symbol table scoped to one read (Q-MASK-318) and the user extensions;
  - the CIF writer: header, numbering and call order from INDEX, labels, arrays, flat output;
  - every remaining `cif` option, so `cif ostyle` and `cif see` work.

  `cif_command.cpp` moves out of `script/binding/`.
- **DRC on mask layers (DRCCIF).** `drc-cif-rules` adds the mask-layer check (Magic drcCifCheck) at the end of each basic check. It generates the bound style's layers for the checked area, runs the width, spacing, area and maxwidth checks, and maps errors back to layout coordinates per Q-DRC-13, with the drop count reported in both modes. DRC-CIF-4..7 lose `deferred = "M5"`.
- **Desktop (SHELL).** The wrapper's Read GDS and Write GDS menu items are enabled. They and the `promptload`/`promptsave` `gds` and `cif` prompts use the shell's file pickers, with the root name (Q-MASK-320). Long reads and writes show progress and can be cancelled (Q-MASK-414). This uses the existing progress and interrupt patterns, so no new screen is needed (design D16).
- **Golden, performance and exit (GOLDEN, PERF, INTEGRATION).** GOLDEN authors:
  - the 233 MASK cases, using a GDS record writer for hand-built inputs;
  - the library round-trip cases;
  - MISC-MH-03..06, MISC-FL-05 and CELL-TS-07.

  The decoded-record, gzip and CIF-preamble comparators are added. The cifinput/cifoutput part of the `magic-tech-sections` mask is retired. A `gds-bench` job measures GDS read and write against Magic. Fuzz targets are added for the GDS and CIF readers and for the cifoutput and cifinput sections. The M5 exit check closes the milestone.

### Non-goals

- OASIS (M6). `oasis-read`/`oasis-write` cite the shared decisions (Q-MASK-204, 207, 209, 211, 216, 114). M5 builds the use-order service and the stream helpers so that M6 can reuse them, but writes no OASIS code.
- `lef read`/`def read` (M6). GOLDEN builds abstract-view cases (MASK-GW-032, 034) from `.mag` cells that carry the `LEFview` and `GDS_FILE` properties. A case that needs a real LEF read is deferred to M6.
- Extraction (M7), tapeout fill and density (M13). M5 provides `cif coverage` and the density computation's generation path (Q-MASK-325). It does not provide the `tapeout-fill-and-density` commands.
- A GDS import dialog with per-option controls, and a mask-layer viewer beyond `cif see` feedback. These are left for later, unless the user asks for them (design Open Question 3).
- The full-chip frame-rate run of `hefesto-performance` "Interactive frame rate". M5 makes the reference GDS readable. The run itself stays with the Raven confirmation checks.
- Multithreaded generation. Generation is single-threaded and reentrant (design D8). A parallel mode is a later optimisation.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

None. M5 implements the existing requirements of the 8 capabilities listed in docs/roadmap.md ("M5"): `mask-output-styles`, `mask-output-operators`, `mask-hierarchical-generation`, `mask-input-styles`, `gds-read`, `gds-write`, `cif-format-io` and `drc-cif-rules`. These hold 203 requirements, 194 of them exact or behavioral.

M5 also makes three decided changes to code built in earlier milestones (design D17):
- the Q-MASK-400 seed rule in `drc-engine` "Interaction area";
- the use enumeration order behind `uses_in_search_order`;
- the half-area coverage of `tapeout-fill-and-density` "Density computation", which only becomes reachable through `cif coverage`.

Every open question these specs cite is resolved, and the specs were aligned in the preceding commit. The change therefore sets `skip_specs: true`, following the M0–M4 precedent (m0-foundations design D10). A workstream that finds a requirement that must change raises it as an open item. INTEGRATION then edits the living spec in this change's branch.

## Impact

- **New code:**
  - `src/core/{include/hefesto,src}/mask/{plane,style,input,ops,gen}/`
  - `src/core/{include/hefesto,src}/gds/{stream,read,write,command}/`
  - `src/core/{include/hefesto,src}/cif/{read,write,command}/`
  - `src/core/{include/hefesto,src}/db/use_index.*`
  - `src/core/src/drc/check/mask_check.cpp`
  - `tests/unit/{mask,gds,cif}/`
  - `tests/fuzz/{fuzz_gds_reader,fuzz_cif_reader,fuzz_cifoutput_section,fuzz_cifinput_section}.cpp`
  - `bench/gds/`
- **Shared changes:**
  - `drc::CifCatalog` is re-sourced from `mask::OutputStyle`.
  - `db::uses_in_search_order` and every area search over uses go through the use index (design D7).
  - `cell/timestamps.cpp` takes vendor stamps from the content hash.
  - `basic_check` calls the mask-layer check.
  - `cif_command.cpp` moves from `script/binding/` to `cif/command/`.
  - `EditorState` gains the mask session (current styles, `gds`/`cif` settings, the shared `random()` of Q-MASK-211).
  - The desktop menu bar enables the GDS/CIF items.
- **Dependencies:** none new. zlib (bundled, `hefesto::zlib`) already provides inflate and deflate.
- **Data:** the GDS data sets `sky130-gds`, `gf180mcu-gds` and `ihp-gds` (sizes verified in task 1.1). They are never committed and are cached per set.
- **Golden cases:**
  - MASK: 233 rows in `tests/golden/MASK/CASES.md`, plus the library round-trip rows `MASK-RT-*`.
  - DRC-CIF-4..7 un-deferred. DRC-CIF-2 and DRC-CIF-3 must still pass once errors are generated.
  - MISC-MH-03..06, MISC-FL-05 and CELL-TS-07.
  - Deviations for the fix rows whose output differs from Magic: Q-MASK-201, 209, 219, 222, 227, 314, 318 and Q-MASK-10 where a listing differs.
- **Docs:**
  - the diagnostic code documents `docs/diagnostics/codes/{mask,gds,cif}.md` and the new DRC codes;
  - `docs/building.md` (GDS data sets);
  - `docs/contributing/` (areas, fuzz targets, golden modules, the GDS bench);
  - `docs/design/m5-ui-conformance.md` (menu items, progress);
  - the roadmap status and CHANGELOG.
