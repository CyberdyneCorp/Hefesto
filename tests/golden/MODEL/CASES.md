# Golden cases — MODEL (layout database)

To be captured later by running Magic `fc5daa9e` headless (`magic -dnull -noconsole`). Unless stated otherwise the technology is a minimal test tech `mt` with planes `active, metal1, metal2, metal3`, types `ndiff pdiff poly nfet (active)`, `metal1`, `metal2`, `metal3`, contacts `pdc (pdiff metal1)`, `via1 (metal1 metal2)`, `via2 (metal2 metal3)` declared `stackable via1 via2`, compose `nfet poly ndiff`, connect `metal1 via1`, `metal2 via1,via2`, `metal3 via2`. Capture means: the saved `.mag` file (paint, labels, properties sections), plus any listed command output. Coordinates are internal units with `snap internal`.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| MODEL-G-001 | layout-geometry-model | paint metal1 `0 0 10 10` then `10 0 20 10` | `.mag` metal1 section | Canonical maximal-horizontal-strip decomposition |
| MODEL-G-002 | layout-geometry-model | paint metal1 `0 0 10 10`, `0 10 10 20`; second file with `5 10 15 20` | `.mag` metal1 sections | Canonical decomposition |
| MODEL-G-003 | layout-geometry-model | cell A: `0 0 20 10` then `0 10 10 20`; cell B: `0 0 10 20` then `10 0 20 10` | both `.mag` files (must be identical in paint) | Canonical decomposition (history independence) |
| MODEL-G-004 | layout-geometry-model | metal1 at `20 0 30 30`, `0 20 10 30`, `0 0 10 10` | `.mag` rect order | Tile enumeration order |
| MODEL-G-005 | layout-geometry-model | 30 random non-overlapping and overlapping metal1 rectangles (fixed seed list) | `.mag` rect order | Tile enumeration order (stress) |
| MODEL-G-006 | layout-geometry-model | DRC: two spacing violations producing L-shaped error regions; `drc count` | `drc count` output and error tiles via `drc listall why` | Canonical decomposition (DRC count) |
| MODEL-G-007 | layout-geometry-model | `splitpaint nw metal1` over box `0 0 10 10` | `.mag` `tri` line | Non-Manhattan geometry |
| MODEL-G-008 | layout-geometry-model | split nw metal1 then split se metal1 on same box | `.mag` (expect plain rect) | Non-Manhattan geometry (collapse) |
| MODEL-G-009 | layout-geometry-model | place child (metal1 `0 0 10 20`, label N) with each of the 8 orientations (`getcell` + `rotate`/`upsidedown`/`sideways`) | parent `.mag` `transform` lines, `box` lines; flattened result | Manhattan transforms |
| MODEL-G-010 | layout-geometry-model | child with label justifications 1..8; place rotated 90 and upside-down; flatten | flattened label justification values | Transforming positions |
| MODEL-G-011 | layout-geometry-model | child with `tri` nw/sw/se/ne; place in 8 orientations; flatten | flattened `tri` lines | Transforming triangles |
| MODEL-G-012 | layout-geometry-model | child with `flabel` rotations 0,90,180,270; place rotated 90 cw; flatten -dotoplabels / select copy | resulting flabel rotation values | Transforming angles (Q-MODEL-1, Q-MODEL-2) |
| MODEL-G-013 | layout-paint-semantics | paint ndiff `0 0 10 10`, pdiff `5 0 15 10` | `.mag` active sections | Result-table model |
| MODEL-G-014 | layout-paint-semantics | paint metal1 `0 0 20 10`, via1 `0 0 10 10` | `.mag` metal1/metal2/via1 sections | Default contact painting rules |
| MODEL-G-015 | layout-paint-semantics | via1 `0 0 10 10`, then metal1 `0 0 20 10` | `.mag` | Default contact painting rules (transparency) |
| MODEL-G-016 | layout-paint-semantics | via1 `0 0 10 10`, erase metal1 over it | `.mag` (expect metal2 only) | Default contact painting rules (dissolve) |
| MODEL-G-017 | layout-paint-semantics | pdc `0 0 4 4`, paint ndiff over it | `.mag` | Default contact painting rules (incompatible residue) |
| MODEL-G-018 | layout-paint-semantics | via1 `0 0 4 4`, via2 `0 0 4 4`; then erase via2 | `.mag` after each step | Stacked contacts |
| MODEL-G-019 | layout-paint-semantics | ndiff `0 0 10 10`, poly `4 -2 6 12`, then erase poly `4 0 6 10` | `.mag` after each step | Technology compose rules at runtime |
| MODEL-G-020 | layout-paint-semantics | metal1 `0 5 10 10`; `splitpaint nw metal1` on `0 0 10 10` | `.mag` | Non-Manhattan painting (verify scenario) |
| MODEL-G-021 | layout-paint-semantics | metal1 stripe `0 1 10 2` (creates metal1-plane tile boundaries at y=1 and y=2); `splitpaint` metal1 nw, and separately ne, over box `0 0 10 3` | `.mag` tri/rect lines | Non-Manhattan painting rounding (Q-MODEL-3) |
| MODEL-G-022 | layout-paint-semantics | split nw `0 0 10 10` metal1, then split ne `0 0 10 10` metal1 (opposite diagonal); also 1x5 box | `.mag` | Non-Manhattan painting quartering (Q-MODEL-4) |
| MODEL-G-023 | layout-paint-semantics | split nw `0 0 5 5`, split nw `5 5 10 10`, metal1 `0 5 5 10` | `.mag` (expect one tri) | Split tile merging |
| MODEL-G-024 | layout-paint-semantics | child via1 `0 0 4 4`; parent metal2 `100 0 120 4`; place child at 100 0; `flatten` | flattened `.mag` | Copying paint between cells |
| MODEL-G-025 | layout-paint-semantics | `tech lock metal2`; paint via1 | `.mag` | Active (locked) layers |
| MODEL-G-026 | layout-paint-semantics | save cell; paint metal1 where metal1 exists; `cellname list modified` (or `writeall` prompt) | modified status | Paint operation / modification marking (Q-MODEL-23) |
| MODEL-G-027 | layout-hierarchy | `getcell inv` three times; delete `inv_1`; getcell again | parent `.mag` use ids | Instance identifier assignment |
| MODEL-G-028 | layout-hierarchy | child named `lib/nand2` (loaded via path) placed twice | use ids | Instance identifier assignment (Q-MODEL-8) |
| MODEL-G-029 | layout-hierarchy | `.mag` with two uses having the same id and one with none; load and save | rewritten ids | Instance identifier assignment (read uniquification) |
| MODEL-G-030 | layout-hierarchy | array `0 3 20 0 1 15` of child bbox `0 0 10 10`; also negative xsep; rotated 90 parent box arrays | `.mag` `array`/`box` lines, `instance list`/`what` names | Arrays, Array element naming, Bounding box |
| MODEL-G-031 | layout-hierarchy | cells: paint + point label; label only; empty; with flabel | `.mag` bbox lines (`<< checkpaint >>` / `timestamp` area) and `box` of parents | Bounding box and extended bounding box (Q-MODEL-6, Q-MODEL-19) |
| MODEL-G-032 | layout-hierarchy | lock `inv_0`; try `move`, `delete`; save | command errors; `.mag` `*` prefix; modified flag | Locked instances (Q-MODEL-13) |
| MODEL-G-033 | layout-hierarchy | `flatten -nosubcircuits` with ported/unported children | flattened `.mag` | Descend rules |
| MODEL-G-034 | layout-hierarchy | 3-level hierarchy with labels and ports; `flatten flat` | flattened labels (text, port flags) | Flattening into a new cell |
| MODEL-G-035 | layout-hierarchy | 2x2 array with child label `out`; `flatten -doinplace inv_0` | parent `.mag` labels | Flattening an instance in place (Q-MODEL-7) |
| MODEL-G-036 | layout-hierarchy | `cellname rename a b` (b exists); rename vendor-GDS cell with/without force | errors; properties after force | Renaming a cell definition |
| MODEL-G-037 | layout-hierarchy | `cellname delete inv` while instantiated; after removing instances | errors / success | Deleting a cell definition |
| MODEL-G-038 | layout-labels-ports | labels at `2 2`, `15 15`, `28 28`, `2 28` in cell `0 0 30 30` with no justification | `.mag` rlabel justification numbers | Automatic justification |
| MODEL-G-039 | layout-labels-ports | label A on metal1 `2 2 8 8`; erase metal1 `0 0 5 10`; erase rest; sticky variant | `.mag` labels after each step; console messages (verbose) | Label adjustment after paint changes |
| MODEL-G-040 | layout-undo | paint, save, undo | modified flag; `.mag` after re-save | Undo and redo semantics (Q-MODEL-12) |
| MODEL-G-041 | layout-labels-ports | label on metal1 `2 2 4 4`; paint via1 `0 0 6 6` | label type in `.mag` | Choosing a label's layer |
| MODEL-G-042 | layout-labels-ports | `.mag` with label on `via1+via2` | label type after load/save | Labels on contacts and stacked contacts |
| MODEL-G-043 | layout-labels-ports | ports `b`, `A10`, `A2`, `VDD`, `vdd`, `VDD`, non-port `c`; `port renumber` | `.mag` port indices | Port renumbering (Q-MODEL-9, Q-MODEL-10) |
| MODEL-G-044 | layout-labels-ports | flabel with font `FreeSans`, sizes 8/64/128, all justifications, rotation 45 | parent use `box` / extended bbox in `.mag`, LEF | Rendered text box (Q-MODEL-6) |
| MODEL-G-045 | layout-labels-ports | port with use reset/scan/tieoff set from LEF read; save .mag | `.mag` port line | Label data model (Q-MODEL-18) |
| MODEL-G-046 | layout-properties | `property FIXED_BBOX 0 0 100 200`, `property integer n 7`, `property compat false`; save; `scalegrid 1 2`; save | `.mag` properties both modes and after scaling | Typed property store, compat mode, geometric properties |
| MODEL-G-047 | layout-properties | `.mag` with `string GDS_START 1234`, `string FIXED_BBOX 0 0 10 10`, `string MASKHINTS_X 0 0 4 4` | `property` outputs (types) | Reserved-name type coercion |
| MODEL-G-048 | layout-properties | `property GDS_FILE x.gds`; `cellname rename`; remove property | `cellname readonly` query result, rename errors | Reserved property names |
| MODEL-G-049 | layout-properties | cell with `noextract`, `device`, `parameter`; `extract all` | `.ext` files | Reserved property names (extraction) |
| MODEL-G-050 | layout-properties | `property x 1`; `undo` | `property x` | Property changes and modification/undo |
| MODEL-G-051 | layout-connectivity | metal1 corner-touching squares; `select net` at 1 1 | selection contents (`select list` / `what`) | Connectivity within one cell |
| MODEL-G-052 | layout-connectivity | metal1 + via1 + metal2 run; `select net` | selection contents | Connectivity within one cell (through via) |
| MODEL-G-053 | layout-connectivity | parent metal1 abutting child metal1; child port pair VDD index 1 unconnected | `select net` contents | Hierarchical connectivity |
| MODEL-G-054 | layout-connectivity | metal1 + via1 + diagonal metal2; DEF write (flat search) | DEF NETS section | Flat search cross-plane quirk (Q-MODEL-17) |
| MODEL-G-055 | layout-undo | one Tcl proc (single command) that paints two metal1 rects and adds a label | state after one `undo` | Event log and command grouping |
| MODEL-G-056 | layout-undo | move use, `undo`; move box, `undo` | use transform, box | Recorded event kinds |
| MODEL-G-057 | layout-undo | load cell, `undo`; `flatten -doinplace`, `undo` | cell content | Non-recorded operations (Q-MODEL-28) |
| MODEL-G-058 | layout-undo | 3 paints, undo x2, paint, redo | state, `redo` result | Undo and redo semantics |
| MODEL-G-059 | layout-undo | 1001 paints via script; `undo 1001` | remaining paint | History limit |

## Authoring status (M1)

Case directories under `cases/` hold the inputs; `expected/` comes from the CI capture (golden-capture.yml).
Generated by `tools/golden/authoring/m1_cases.py` (45): MODEL-G-001, MODEL-G-002, MODEL-G-003, MODEL-G-004, MODEL-G-005, MODEL-G-007, MODEL-G-008, MODEL-G-009, MODEL-G-010, MODEL-G-011, MODEL-G-012, MODEL-G-013, MODEL-G-014, MODEL-G-015, MODEL-G-016, MODEL-G-017, MODEL-G-018, MODEL-G-019, MODEL-G-020, MODEL-G-021, MODEL-G-022, MODEL-G-023, MODEL-G-024, MODEL-G-025, MODEL-G-026, MODEL-G-027, MODEL-G-028, MODEL-G-029, MODEL-G-030, MODEL-G-031, MODEL-G-032, MODEL-G-033, MODEL-G-034, MODEL-G-035, MODEL-G-036, MODEL-G-037, MODEL-G-038, MODEL-G-039, MODEL-G-041, MODEL-G-042, MODEL-G-043, MODEL-G-044, MODEL-G-046, MODEL-G-047, MODEL-G-048.

Not authored (reason):

- MODEL-G-006: needs drc
- MODEL-G-040: needs undo
- MODEL-G-045: needs lef read
- MODEL-G-049: needs extract
- MODEL-G-050: needs undo
- MODEL-G-051: needs select/what
- MODEL-G-052: needs select/what
- MODEL-G-053: needs select/what
- MODEL-G-054: needs def write
- MODEL-G-055: needs Tcl proc + undo
- MODEL-G-056: needs move + undo
- MODEL-G-057: needs undo
- MODEL-G-058: needs undo/redo
- MODEL-G-059: needs undo
- Partially authored: G-027 (no instance deletion), G-032 (no move/delete), G-039 (no sticky variant), G-036/G-037 use cellname only; MODEL-G-010/011/024/033/034 compare the flattened cell only.
