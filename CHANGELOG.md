# Changelog

User-visible changes to Hefesto, newest first. Milestone details live in
`openspec/changes/` and `docs/roadmap.md`; this file lists what a user of a
release notices.

## Unreleased (M10: 3D view, phase 1)

### Fixed

- **Huge `.ext` arrays fail at once.** A `use` array (or arrayed
  `merge`) larger than what is left of the 4 Mi instance (16 Mi merge
  element) cap now fails with EXTFLAT-READ-009 before any element is
  flattened, instead of flattening elements for seconds until the cap.
- **Huge arrays in hierarchical SPICE fail at once.** `ext2spice` with
  `hierarchy on` now fails with EXTFLAT-READ-009 when a `use` array has
  more elements than what is left of the 4 Mi instance cap, before any
  call is stored and without writing a netlist, instead of running out
  of memory.
- **3D view upside down in tilted views.** The isometric, front and side
  views showed the stack from below (wells on top, metals at the
  bottom); every tilted view now looks from above.

### Added

- **3D view of the layout layers.** `specialopen wind3d [cell]` (and the
  Tcl `magic::render3d`, Options > 3D Display, or the 3D tab of the
  desktop app) opens a 3D window of the cell with Magic's commands:
  `view`, `scroll`, `zoom`, `refresh`, `level`, `defaults`, `cif`,
  `render`, `see`, `cutbox`, `closewindow` and `help`, with Magic's
  messages. Heights and thicknesses come from the technology's extract
  `height` and `planeorder` statements; the cifoutput `render` statement
  is read. Higher layers are drawn in front, in both magic-compat modes
  (Magic's depth test puts lower layers in front in the top view).
- **Desktop 3D tab.** The 3D viewport builds the mesh in the background
  and draws it with Vulkan on Linux and macOS (MoltenVK). The side panel
  lists the layers with their µm ranges and show, hide, isolate and
  transparency toggles, the top, isometric, front and side presets, the
  perspective toggle, "Span connected layers", the level and the cut
  box. Clicking a face picks it: the 2D view selects the material (or
  the subcell use) under it, a 2D selection highlights its faces, and
  "Isolate selected net" highlights the picked net and dims the rest.
- **Board look in the desktop 3D tab.** The tab now draws the approved
  board look in both magic-compat modes: dark theme background, flat
  per-face shading (tops lighter than sides), opening on a framed
  isometric view from above with an x/y/z axis gizmo, and wells,
  implants and other non-conducting area layers ghosted at 20 % (each
  row can be changed). Mode, level, cut box and mesh status sit in a
  toolbar under the view.
  The isometric opening applies to the window the tab opens (`render3d`);
  a `specialopen wind3d` window keeps the top-down view. In mask-layer
  mode the opening z scale follows the output scale factor, so the stack
  keeps its proportions.
- **Script routing with magic-compat ON.** While a 3D window is open and
  current, the layout commands a script runs go to it, as in Magic; with
  magic-compat OFF they go to the layout window.
- **Mask layers in 3D (phase 2).** Mask-layer mode (`cif` in the 3D
  window, the default when an output style is current) now draws the mask
  layers the current output style generates for the cell, with their
  `render` heights, instead of the layout layers under a "phase 2"
  notice. An edit regenerates only the nearby mask geometry.
- **Cross-sections.** The desktop 3D tab's "Cross-section" button cuts
  A–A′ along the layout box (or two clicks in the view), clips the 3D
  view at the line and shows each crossed layer from its height to its
  top in µm in a "Cross-section A–A′" panel.
- **glTF and STL export.** `export3d <file> [-format gltf|glb|stl]
  [-merged] [-zscale k] [-visible|-all]` writes the 3D window's model in
  µm (one glTF node or STL file per layer, or one merged STL); the
  desktop tab's "Export glTF…" asks for the file. The output is
  byte-identical on Linux and macOS.
- **3D tab layout.** The view toolbar floats over the 3D view as on the
  board, the side panel fits a 1280 × 800 window, a substrate slab is
  drawn under the stack ("Substrate" toggle), and every control has a
  screen-reader name and state.

## Unreleased (M9: LVS and simulation, in progress)

### Added

- **M9 decisions (G0).** The M9 decision table (LVS, simulation, advanced
  simulation) is approved and its spec changes are in `openspec/specs/`.
  The simulation engine is pinned to ngspice-47 (`a80f6e3e`) and every
  ngspice trace of the specs was re-checked against it. SCHEM-LVS-01 is
  deferred to M9.
- **netgen clean room.** `docs/contributing/cleanroom.md` extends the
  two-team process to LVS; `scripts/check_cleanroom.py` rejects netgen's
  GPL header, `netgen ref:` traces and LVS golden cases without a valid
  `provenance`; the pull-request template has the LVS items.
- **netgen and ngspice data sets.** `scripts/fetch_pdk.py` installs
  `gf180mcu-netgen`, `ihp-netgen`, `sky130-ngspice`, `gf180mcu-ngspice` and
  `ihp-ngspice`, pinned and digest-checked.
- **Golden comparators** `lvs-canonical`, `raw-tolerance` and `meas-table`.
- **`lvs` and `sim` core areas.** The area-contract headers are in place;
  the `lvs` command exists and answers CORE-NOTIMPL-009 until the LVS
  engine lands. `scripts/check_engine_boundary.py` (run in CI) keeps
  ngspice behind its adapter plugin.
- **Simulation engine library.** `libngspice-hefesto` is built from
  ngspice-47 with the Hefesto patch series (static OSDI and code-model
  registries, portable seeding, deterministic initialisation) behind the
  `HEFESTO_BUILD_NGSPICE` option (default OFF). The golden capture image
  uses the same ngspice (`47-hefesto1`) and adds OpenVAF-reloaded.
- **Statistical `.param` drawn once per load.** Patch 0005 backports the
  upstream ngspice change after release 47: a top-level `.param` with
  `agauss` and similar functions is drawn once per circuit load (one draw per
  die), not at every reference. Engine and capture image are now
  `47-hefesto2`.
- **Simulation reference runs** (`golden-capture.yml` mode `sim-refruns`):
  `.param` precedence per PDK, sky130 `ng_nomodcheck` and the descriptor
  loads, gf180mcu `hs` against `hsa`.
- **Desktop Beta layout cell.** `beta_inv` is drawn with its own transistors
  for sky130, gf180mcu and IHP, DRC-clean with each PDK's Magic tech file and
  written to GDS (ctest `e2e.beta.<pdk>.layout`).

## Unreleased (M8: schematic capture, in progress)

### Added

- **xschem-compatible SPICE netlisting through the session.** `schematic
  netlist`, `xschem netlist` and `hefesto-cli --netlist` resolve symbols
  through the session's library search path and `xschemrc` files, so
  `tcleval(...)` values are evaluated. `xschem translate`,
  `abs_sym_path` and `rel_sym_path` are available to scripts.
- **System symbol library.** A clean-room `devices/` library (43 symbols)
  ships under `assets/schematic/xschem_library/devices/`, where the PDK
  `xschemrc` files look for it.
- **Tcl symbol generators** run in a sandboxed interpreter (10 s and
  256 MiB per run, 60 s in total until the symbols are reloaded);
  generators that run external programs are not run (`SCHEM-LIB-011`).
- **Schematic editing (M8 EDIT).** `schematic place`, `wire`, `label`,
  `line`/`box`/`arc`/`polygon`/`text`, `select`, `delete`, `move`,
  `copy`, `rotate`, `flip`, `align`, `setprop`, `replace`, `symbol`,
  `descend`, `ascend`, `hierarchy`, `hilight`, `undo` and `redo`, with a
  per-document undo history of 200 steps (at most 512 MiB of document
  states; consecutive steps share them). PDK and system-library symbols
  are read-only (`SCHEM-EDIT-001` to `-007`), also for `symbol make` and
  `symbol toschematic`. Non-finite coordinates are refused.
- **Desktop Schematic tab.** The active schematic is drawn read-only with
  its hierarchy breadcrumb; clicking an instance selects it. Headless
  view models for the library browser, properties, messages, netlist
  action and xschem-style key bindings.
- **`schem-bench`** times loading, saving and netlisting of a generated
  10,000-device hierarchy and of PDK testbenches (`bench/schem`).
- **Schematic object order and clipboard.** `schematic order <index>`,
  `clipboard [copy|set <text>]`, `paste <x> <y>` and `merge <file>
  [<x> <y>]`; each paste, merge or order is one undo step
  (`SCHEM-EDIT-001` on a read-only document; `SCHEM-EDIT-008` when the
  clipboard holds no schematic objects). A pasted or merged embedded
  symbol does not redefine a symbol the document's instances already use
  (proposed Q-SCHEM-51).
- **Schematic simulation decks.** The schematic deck is built from the
  netlist with its `.control` blocks translated: `write`/`wrdata` become
  capture points, `quit`/`exit` are removed and host commands (`shell`,
  `cd`, `load`, `source`, ...) are refused (`SCHEM-SIM-001` to `-004`).
  Running the deck arrives with M9.
- **Desktop Schematic tab panels.** Library browser, properties,
  breadcrumb, messages and netlist bar with Interrupt; pan, zoom and
  click-to-select; net highlights drawn in their colours. Symbols are
  resolved off the UI thread and shown as placeholders until ready.

### Changed

- **Input limits for hostile schematics, in both `xschem-compat` modes.**
  A flat netlist stops substituting at 64 levels or 256 MiB of output
  and is still written to its `.end` (`SCHEM-NET-017`); a schematic used as a symbol stops at 256 MiB when
  flattened (`SCHEM-LIB-009`); the total bus expansion of a netlist run or
  editing session is capped at 4 GiB (`SCHEM-NET-006`).
- **`CAD_ROOT` defaults to the install root.** `hefesto-cli` and the
  desktop app find the system symbol library without `CAD_ROOT`: the
  install root is the first of the executable's folder, `../Resources`
  and `..` that holds `assets/`, located by the operating system (not
  argv[0]). `CAD_ROOT` still overrides it. Project scripts may read only
  its `assets` and `magic` folders, not the whole install prefix.
- **Script limits and reporting in schematics.** A `tcleval` value stops
  after 10 s and the values of one netlist run share 60 s
  (`SCHEM-TCL-003`); sandbox denials in `tcleval` values and generators
  are reported, naming the instance. A generator file over 256 MiB is
  neither read nor run (`SCHEM-LIB-012`, was `SCHEM-LIB-011` without
  `.tcl`).
- **Unknown commands in an `xschemrc` file are Tcl errors**
  (`SCHEM-LIB-006`, earlier settings kept), except the GUI-only commands
  the sky130, GF180MCU and IHP `xschemrc` files call, which do nothing.
- **`schematic reload` and `close` drop the document's editing state**:
  selection, net highlights, undo history and launcher confirmation;
  `reload` also re-reads changed symbol files.
- **`schematic netlist` and `check` can be interrupted** (`SCHEM-CMD-011`,
  no file written).
- **Netlister fixes found by the SCHEM golden replay**: `@#n:net_name`,
  the `expr` error text, the `tcleval` line end, `#<p>#` name lists, save
  lines, the 130-character quote rule and empty `*.PININFO` lines now
  match xschem.

### Fixed

- **`schematic label` with a quote or backslash** in the name is escaped
  and reads back unchanged; so are the net labels a netlist run stores.
- **Schematic scripts cannot close or move the standard channels**
  (`SCRIPT-SANDBOX-007`).

## Unreleased (M7: extraction and netlisting, in progress)

### Added

- **Circuit extraction.** `extract all`, `extract cell`, `extract parents`,
  `extract showparents`, `extract unique` and the `do`/`no`, `warn`,
  `style`, `halo`, `stepsize` and `path` options write Magic's `.ext`
  files, flat and hierarchical (interactions, `merge` records,
  hierarchical coupling, hard-way names, arrays), with subcell device
  priority. A cancelled or failed run keeps the previous `.ext` files.
- **`extresist`** builds and reduces each net's resistor network and
  writes `.res.ext` and `.res.lump`; **`antennacheck`** checks gate
  antenna ratios against the style's rules.

### Changed

- **`extract`, `ext`, `ext2spice`, `exttospice`, `ext2sim`, `exttosim`,
  `extresist` and `antennacheck` are registered commands.** They take
  Magic's option tables and print Magic's help and invalid-option
  messages. Until extraction and the netlisters land, each option answers
  `CORE-NOTIMPL-008` ("not implemented yet: extract all"), so a script's
  `extract all` now fails with that message instead of "unknown command".
  `ext2spice scale [on|off]` keeps working as before; other `ext2spice`
  options answer `CORE-NOTIMPL-008` instead of `UI-CLIENT-002`.

## Unreleased (M6: LEF/DEF and OASIS, complete)

### Added

- **`lef read`, `lef write`, `lef writeall`, `def read` and `def write`.**
  They take Magic's options and print Magic's messages. `lef read` reads a
  technology LEF and cell LEF into abstract views (`-annotate` adds
  ports to existing cells), `lef write`/`writeall` write them back, `def
  read` builds a placed-and-routed layout (or annotates labels), and `def
  write` writes components, pins, vias and the nets it flattens from the
  `.ext` files of the cell. The technology file's `lef` section maps LEF
  layer names and vias. With `magic-compat` on Hefesto copies Magic's
  quirks; with it off it applies the documented fixes.
- **`oasis read` and `oasis write`.** OASIS (SEMI P39) read and write,
  equivalent to the GDS path: `oasis` takes the `gds` options plus
  `strict`, `validation`, `repetitions`, `layernames` and `circlepoints`,
  and shares the read settings with `gds`. Writes are deterministic, use
  CBLOCK compression and detect shape repetitions. The outputs are checked
  with KLayout and gdstk.
- **GDS and OASIS transcoding of read-only cells.** A read-only GDS cell
  can be written as OASIS and a read-only OASIS cell as GDS, so a vendor
  view keeps its content when it is written in the other format.
- **Opening an OASIS file.** The desktop app's File > Open and drag and
  drop recognise an OASIS file by its content, even when it is named
  `.gds`. OASIS reads and writes, `def read` and the LEF/DEF writes run on
  the Interrupt card (`def read` and OASIS show progress; `lef read` shows
  none yet).
- **`.ext` reading and flattening** (pulled forward from M7), used by `def
  write`.
- **LEF/DEF and OASIS benches.** `perf.yml` measures each workload in
  callgrind instructions (`lefdef-bench` against Magic, `oasis-bench`
  against Hefesto's GDS path) and gates it at its own baseline + 2 %
  (the baselines were recorded at the M6 exit round, from a Performance
  run on the round-6 branch).

### Changed

- **Startup prints the technology line after the command-line files.**
  `hefesto-cli` prints `Using technology "<name>", version <v>` once,
  after the files named on the command line, as Magic's Tcl startup does,
  including `Using technology "minimum", version 0.0` when nothing supplies
  a technology.
- **`lef write` and `def read -annotate` print the cells they read in
  Magic's order.** `Cell <c> read from path ...` now comes inside the
  macro's messages (after `Writing LEF output`) and between the DEF
  section counts, instead of before the command's other lines.
- **Technology lines of more than 30 words.** The first 30 words are read
  and the dropped words are reported as error `TECH-READ-002`; the load
  goes on (Magic overflows its word buffer there).
- **Input limits on hostile files.** An OASIS point list or explicit
  repetition of more than 2^24 entries drops its element
  (`OASIS-CODEC-030`); a `.ext` line of more than 128 tokens is ignored
  with warning `EXTFLAT-READ-011`; the other read and write caps are in
  `docs/contributing/security.md`.
- **`CORE-NOTIMPL-006` is retired.** Every `lef` and `def` option is
  implemented, and the code is never reused.

## Unreleased (M5: mask generation, GDS and CIF, complete)

### Added

- **Read GDS and Write GDS in the desktop app.** The File menu's Read GDS
  and Write GDS items are enabled. The native file dialogs filter
  `*.gds`, `*.strm`, `*.cal`, `*.cif` and `*.mag` (wrapper.tcl's Tk
  extension lists matched no file there). A long `gds`/`cif` read or write from the menu or the
  console keeps the window drawing: the Interrupt card shows "Writing
  chip.gds · N s" and "Completed N%", Interrupt stops the job without
  leaving an output file, and lines typed meanwhile wait until it ends.
  The card also covers a write's loading of unread cells, which Interrupt
  stops before any file is opened.
- **GDS throughput gate.** `perf.yml` measures `gds read` of
  `sky130_fd_sc_hd.gds` and the all-cells block write (with and without
  `gds merge`) in callgrind instructions against Magic (`bench/gds/`), and
  a `gds-roundtrip` job reads and writes every sky130, gf180mcu and IHP
  library GDS (decision P-24). Both tools write with the same outline
  fonts, and the gate fails when their outputs differ.

- **`gds write` and `cif write`.** `gds write` (and a bare `gds`) writes
  the window's root cell as GDS II, byte for byte as Magic does: structure
  order and names, SREF/AREF, merged polygons (`gds merge`), labels and
  label layers, read-only vendor cells copied verbatim from their source
  GDS, full library dumps, `addendum`, `abstract` and compressed output.
  `cif write` and `cif flat` write CIF with Magic's header, symbol numbering
  and call order. Neither writer changes the layout: with `magic-compat`
  OFF the UNITS follow each write's style (Q-MASK-201), colliding structure
  names are renamed with a warning (Q-MASK-209), no file is created without
  an output style (Q-MASK-219), and coordinates or array counts that do not
  fit 32 bits fail the write instead of wrapping (Q-MASK-222).
- **`gds read` and the read options.** `gds read` finds `<name>.gds`,
  `.gds.gz`, `.strm` or the name itself, and every `gds` read option
  (`flatten`, `flatglob`, `readonly`, `magscale`, `maskhints`, `rescale`,
  `warning`, `polygon subcells`, `path subcells`, `unique`, ...) works with
  Magic's argument forms. `cif read`, `cif see`, `cif paint`, `cif
  coverage`, `cif statistics` and the remaining `cif` options work too.
- **`cif unfracture`.** Merges the split tiles of each generated layer
  before output. With `magic-compat` ON the value is Magic's fourth word
  (a three-word `cif unfracture yes` only warns, Q-MASK-413) and the merge
  covers Magic's internal-unit search area; with it OFF `cif unfracture
  yes` sets it and the whole generated layer is merged (Q-MASK-415).
- **`gds-time-zone` setting.** A POSIX TZ string (for example
  `--setting gds-time-zone=UTC0`) pins the zone of GDS dates and the CIF
  header date; unset, the host zone is used, as in Magic (Q-MASK-132).

### Changed

- **Interaction operators with `magic-compat` ON.** `overlapping`,
  `nonoverlapping`, `interacting` and `noninteracting` give an empty layer,
  as Magic's cifInteractingRegions does (Q-MASK-417, compat); OFF keeps the
  region filter. MASK-OP-021 replays.
- **`squares`, `squares-grid` and `slots` with `magic-compat` ON** leave an
  earlier region's tiles marked after its cuts, as Magic's
  cifSquaresFillArea does, so a later ring-shaped region lays its own,
  overlapping cut grid over them (Q-MASK-418, compat). OFF clears each
  region. The sky130_fd_io round trips MASK-RT-sky130_fd_io-f01/-f09
  now match Magic's GDS.
- **`expand` and `unexpand` with `magic-compat` ON** follow Magic's
  dbExpandFunc: they act below instances that stay unexpanded, use its
  surround test, test only the first array element under the box, and
  `expand all toggle` expands (Q-MASK-419, compat). With OFF an expanded
  array is now searched under every element the box overlaps, not only its
  first, and each instance is acted on once per command, so `expand
  toggle` no longer undoes itself on an instance under two elements.
- **`maxrect` with `magic-compat` ON** leaves an earlier region's tiles
  marked after its rectangle, as Magic's cifRectBoundingBox does, so a
  later non-simple region's largest rectangle can lie on them (Q-MASK-420,
  compat). OFF clears each region, as before.
- **Selected instances below array elements** are found by `edit`,
  `expand selection`, `flatten -doinplace` and the `cellname` options, and
  with `magic-compat` ON they are taken in Magic's SelEnumCells order (the
  `array` queries report them in its reverse, as Magic does). With
  `magic-compat` OFF the instance id must match too, so of two coincident
  instances of one cell the selected one is used.
  `flatten`, the wiring commands and `select cell` search each cell's use
  index as Magic does.

- **File prompts and `writeall` with `magic-compat` OFF.** `promptload
  gds` reads the picked file when its root name does not find it
  (Q-MASK-136); `writeall` writes an undefined cell as an empty, stamped
  `.mag` (Q-MASK-235); and the copied SNAMEs of a renamed read-only vendor
  child are rewritten through the child's `GDS_FILE` name (Q-MASK-216).
  `promptsave gds` writes the chosen file (Q-MASK-320), and background
  DRC pauses while a GDS or CIF write runs (Q-MASK-414).
- **`gds rescale` and `cif rescale` are one setting**, as Magic's
  CIFRescaleAllow is, and a CIF read resets the once-per-read grid
  message.
- **Read messages match Magic.** GDS read errors give Magic's byte
  positions, polygons with fewer than four points and non-Manhattan
  polygons are reported (MASK-INPUT-020), labels moved onto their paint
  print Magic's "Moving label" lines (MASK-INPUT-019), and the split tiles
  of read polygons keep Magic's fragmentation (also after `scalegrid`).
- **`writeall` after a GDS or CIF read.** A modified `(UNNAMED)` cell is
  no longer written to `(UNNAMED).mag`: `writeall force` reports `Can't
  write file named '(UNNAMED)'`, and a chosen write asks for a file name.
  With `magic-compat` ON a used but undefined cell is written as Magic
  writes it (an empty cell whose timestamp stays 0).
- **Generation errors become feedback.** The errors of `gds write`, `cif
  write`, `cif flat` and the `cif` previews ("parent and child disagree on
  CIF") are added as feedback areas when the command finishes, and every
  write counts in `cif statistics`.
- **`cif see` and the other previews read unread subcells** within the box
  before generating, as Magic does.
- **Saving restamps read-only vendor cells.** `save` and `writeall`
  recompute a read-only cell's content-hash timestamp from the GDS file its
  `GDS_FILE` names, as Magic's DBUpdateStamps does.
- **Generated mask layers keep their split tiles apart.** Mask planes are
  painted without merging collinear triangles, as Magic's CIF planes are,
  so GDS boundary counts and CIF polygons of non-Manhattan layouts match
  Magic's.

- **Use enumeration order follows Magic's bplane.** Searches over cells
  with ten or more uses now enumerate them in Magic's binned order after a
  partial search, and a use whose child's bounding box changes is
  re-placed when its new box differs from the old extended box, as Magic
  does; GDS SREF and CIF call order will follow it. With `magic-compat` OFF
  uses enumerate in reverse creation order, and the DRC interaction area no
  longer includes Magic's phantom area at the origin (Q-MASK-400). The
  `.mag` read's id uniquification, flatten, selection, `what`, hints and
  the DRC reports now enumerate in the index order as well; connectivity
  searches still keep the M4 order.
- **The cifinput section is read at tech load.** Its messages are printed
  at load, the input styles follow every grid change (tech load lambda,
  `scalegrid`, a finer-grid `.mag` read), and `cif istyle <name>` selects
  an input style by unique prefix, or by exact name when it is a prefix of
  several (Q-MASK-303, a fixed deviation from Magic: `cif istyle gds`
  with a `gds2` style present selects `gds`). `cif list istyle`,
  `cif listall istyle` and `cif scale in` read the loaded styles. Changing
  `magic-compat` re-reads the section (Q-MASK-300, Q-MASK-302). A
  cifinput statement before the first `style` line now fails the load
  with `magic-compat` ON, as in Magic; with it OFF the statement is
  ignored with a warning (MASK-INPUT-018, Q-MASK-32).
- **DRC checks mask-layer rules.** The drc section's `cifwidth`,
  `cifspacing`, `cifarea` and `cifmaxwidth` rules (for example sky130's
  `cifmaxwidth` and `cifspacing` rules) now report errors, generated on
  the bound cifoutput style as Magic's drcCifCheck does. With
  `magic-compat` OFF error boxes are rounded outward to the internal grid
  instead of truncated (Q-DRC-13). Coordinate units in DRC output follow
  the current `cif ostyle`. Under `drc euclidean on` a mask-layer corner
  distance of 4096 mask units or more keeps its full value with
  `magic-compat` OFF; ON still wraps it modulo 4096, as Magic does
  (Q-DRC-27).
- **The cifoutput section is read at tech load.** Its messages are printed
  at load, before the drc section's, and a cifoutput statement before the
  first `style` line fails the load (Q-MASK-17), as in Magic. The DRC
  `cif*` style catalog is read through the same reader: a style without a
  `scalefactor` now has reducer 0 and the `slots` values count in the
  reducer, as in Magic. `cif ostyle <name>` reloads a DRC style whose
  `cifstyle` names the selected style, as Magic's DRCReloadCurStyle does.
  Grow, shrink, bridge and bloat distances past the usable plane
  (1073741818) are rejected with a message instead of overflowing, in both
  `magic-compat` settings (Q-MASK-30, a fixed deviation from Magic).
  The native gridlimit warning (MASK-STYLE-004) now applies only to
  styles that use `bloat-all`, so `cif ostyle density` and
  `cif ostyle metfill` on the PDKs are quiet (Q-MASK-8). A unit
  normalisation that truncates a style's scale factor, as Magic's does
  (a 10/10 `bloat-all` style under `scalegrid 1 3` becomes 3/10), is
  reported with `magic-compat` OFF (MASK-STYLE-007, Q-MASK-29).
- **GDS stream layer and vendor content stamps.** The GDS record layer
  (gzip input, `real8` and date encoding) and the content stamps of vendor
  GDS cells are in place for the coming `gds read`.

### Fixed

- **GDS and CIF reads of nearly parallel 45-degree edges** tile the read
  planes as Magic does: read-plane triangles get their own budget (up to
  1024 pieces each from a per-read pool) instead of the 512-piece database
  limit, which filled long chains as rectangles and changed the derived
  layers of gf180mcu_fd_io's POWER_RAIL_COR_1 (Q-MODEL-37, fix). A
  read-unit rescale and a flatten copy repaint the read planes' triangles
  with the same budget.
- **Operator errors of a write reach the feedback list.** A
  `no room for contacts in area!` from `squares`, `squares-grid` or `slots`
  during `gds write` or `cif write` is now a feedback entry, as in Magic,
  not only a line on the console.

- **GDS read of rotated or mirrored arrays.** Flatten copies and
  copy-up copies of an AREF land on the right elements: the element
  offset is applied in the child's coordinates, as Magic does. Before,
  a rotated array's copies went onto other paint (159 extra "no room for
  contacts" problems in sky130_fd_io's hvc_clampv2).
- **The `5000 uses` progress line of a GDS read** is printed for every
  structure that reaches 5000 uses, not only for the first.
- **`cif statistics` counts the tiles OR-ed into existing planes**, as
  Magic's CIFGen does, so its tile-operation counts match Magic's.
- **`writeall` and `save` of an unavailable cell read its existing
  `.mag` first**, so the written file keeps the cell's paint (Q-MASK-234,
  both `magic-compat` settings).
- **`expand` searches the use index under the box**, as Magic's
  DBExpandAll does, so after a partial `expand` `gds write` and `cif
  write` emit sibling structures in Magic's order. `select` at a point
  and `delete`, `copy` and `move` of selected cells make Magic's searches
  of the use index too.
- **Every hierarchical search goes through the use index of each cell it
  descends into**, as Magic's DBTreeSrTiles, DBTreeSrLabels and
  DBTreeSrCells do: `select area`, `what` and the connectivity searches
  bin a cell's index as Magic's do. `delete`, `move`, `identify` and the
  array report find a selected cell from its first element's corner,
  through every child there, as Magic's SelEnumCells does. With
  `magic-compat` ON, `expand` and `unexpand` also search below unexpanded
  instances, in the frame of the first array element under the box.
- **A GDS read finds a duplicate placement after a grid subdivision** in
  the same structure ("placed on top of itself"); it was kept before.
- **The desktop's background DRC link to GDS and CIF writes is undone
  before the DRC pump goes away**, so a write ending while the window
  closes no longer resumes a destroyed pump.
- **Array overlap no longer overflows.** The use index and the DRC
  computed the array elements under an area in 32 bits; one 64-bit
  routine now serves the use index, DRC, mask flattening and the writers.
- **Writing a deep hierarchy is no longer exponential.** Loading the cells
  under the written cell re-descended into every use, as Magic does, so a
  cell placed twice at every level of a deep hierarchy took 2^depth steps;
  `gds write` and `cif write` now skip visits that cannot change anything,
  with the same cells read in the same order. Placing a cell in such a
  hierarchy (the recursion check) was exponential too. A write can be
  cancelled while it loads cells, and the hierarchy depth limit of the
  writers is now 1024 levels, the mask generators' limit.
- **GDS writes fail cleanly on values outside 32 bits.** A merged polygon
  or label whose coordinates do not fit no longer leaves a partial element
  in the file, and the error names the cell and the exact value.
- **CIF label layers with a scale factor.** With `magic-compat` ON a
  `labellayer` tile is written as Magic writes it, multiplied by the
  style's scale factor (Q-MASK-338); with it OFF it is written in
  the units of the geometry. `cif flat` with `magic-compat` ON processes
  timestamp mismatches after the write, as Magic does.
- **Large inputs.** `cif read` refuses a file larger than 2 GiB before
  reading it (CIF-READ-010), and copying a read-only vendor cell streams its
  records instead of holding the whole structure in memory.
- **Reading many huge triangles no longer takes seconds or minutes.** A
  paint repainted each contact image under it as a paint of its own, each
  allowed the full 512 pieces over crossed triangles, and a record of a
  type-list section paints every type, so a 354-byte `.mag` file with a
  large `magscale` took about 12 s to read and a few kilobytes of such
  records over two minutes. A paint now shares its 512 pieces with the
  images it repaints, and the paints of one `.mag` read share a pool of 512
  pieces plus 16 per record; such files read in about 0.1 s. Ordinary
  triangles use at most 16 pieces and paint exactly as before (Q-MODEL-36,
  a fixed deviation from Magic).
- **Reading a huge triangle no longer takes seconds.** Each piece a
  triangle paint descends into over crossed triangles works next to the
  tiles the pieces before it left, so one paint cost the square of the
  4096-piece limit, and a `.mag` file with a large `magscale` that paints
  such triangles on several layers took about 10 s to read. The limit is
  now 512 pieces, and the merge after a paint no longer moves the plane's
  search hint onto a large tile far from the next contact image to
  repaint; the file reads in 0.3 s. Ordinary triangles never come near
  the limit and paint exactly as before (Q-MODEL-35, a fixed deviation
  from Magic).
- **Painting a huge triangle no longer overflows the stack.** Painting a
  triangle over existing triangles on the opposite diagonal recursed once
  per piece left on the diagonal, as Magic does, and that depth grows with
  the triangle's size: a `.mag` file with a large `magscale` crashed on
  read. The pieces are now kept on a heap stack and processed in the same
  order, so ordinary triangles paint exactly as before. One paint
  processes at most 4096 such pieces; past that a piece is filled as a
  rectangle and reported off grid, as for a piece too small to quarter
  (Q-MODEL-34, a fixed deviation from Magic).
- **Painting up to the edge of the plane no longer crashes.** A paint or
  erase that touched the plane's edge without covering the whole plane
  dereferenced the boundary tiles while merging split tiles.
- **Malformed CIF and GDS input stays bounded.** CIF integers saturate at
  2^61, so box, flash, area-label and call-translation sums cannot overflow;
  redefining one CIF symbol many times no longer takes quadratic time; a
  vendor `.gds.gz` is scanned for content stamps one record at a time
  instead of being inflated whole; a `GDS_START` of -2^63 and an oversized
  temporary read scale no longer overflow; and a gzip member boundary one
  byte before the edge of the 1 GiB inflate window no longer ends the
  stream early. Flatten copies of a GDS read share one budget of 2^26 tile
  paints, so a large `gds flatglob` cell in big arrays is skipped with
  GDS-READ-011 instead of repainting without bound.
- **Contact-cut values past the plane are refused.** A `squares`,
  `squares-grid` or `slots` value above 1073741818 in a `cifoutput` style
  is an error (`Square border must be at most 1073741818.` and the like)
  and the statement is dropped, rescaling keeps the values within that
  bound, and the cut operators compute their sums in 64 bits, so mask
  generation no longer overflows on them. A `cifoutput` `scalefactor`
  above 1073741818 is refused as a `cifinput` one is, and the style uses 1
  (Q-MASK-35, approved; Magic overflows there).
- **A huge `repeat` count no longer hangs a technology load.** One read
  spends at most 100000 lines in `repeat` passes; past that the load
  warns (TECH-REPEAT-003) and reads the rest of the file once. Real PDK
  files repeat about a hundred lines (Q-TECH-33, approved).
- **Grow distances past the plane are refused in `cifinput` styles.** A
  `grow`, `grow-grid` or `shrink` distance above 1073741818 is an error
  (`Grow/shrink distance must be at most 1073741818.`) and the statement
  is dropped, as in `cifoutput` styles; a read's grid subdivisions and
  rescales keep input distances within that bound, and the grow, shrink
  and grow-min operators compute their edges in 64 bits, so a GDS or CIF
  read no longer overflows growing a layer (Q-MASK-36, approved; Magic
  overflows there).

### Security

- **Deep hierarchies no longer overflow the stack in `expand` or the
  selection's instance search.** Both descend at most 1024 levels;
  `expand` and `unexpand` report VIEW-EXPAND-003 when they stop.
- **Compressed GDS input is capped.** A `.gds.gz` inflates to at most 256
  times its compressed size (at least 1 GiB); past that the read stops
  with GDS-STREAM-006 instead of filling memory. New fuzz targets cover
  the GDS and CIF readers and the `cifoutput`/`cifinput` sections
  (`docs/contributing/security.md` "Layout input").
- **Placing many uses in one cell is no longer quadratic in the use
  index.** Each placement made the index rescan the cell's whole use
  list (M5-S3): reading a GDS structure with about 105 000 uses took
  more than 1200 s. The GDS reader also checks duplicate placements
  against the uses it placed in the structure being read, and places a
  use without generating a default id first (Magic's reader makes the use
  with no id), so such a read now takes seconds.

## Unreleased (M4: scripting)

### Changed

- **Confirmations that cannot be asked follow `magic-compat`.** In a
  session with nobody to answer (a `-dnull` script, a headless run),
  `tech load` over painted layout, `save NAME` over another existing file,
  `flush` of a changed cell, losing an edited color map on `quit` or `load`,
  and `quit` with modified cells go ahead with it ON, as Magic does, and are
  declined with it OFF. An answered "no" declines in both.
- **`quit` asks before losing modified cells.** Without `-noprompt` it
  asks `N Magic cells have been modified. Do you want to exit magic and
  lose them?`, as Magic does; "no" keeps the session (on iPadOS, the
  project stays open).
- **`cellname list childinst` and the `save` file-name answer follow
  `magic-compat`.** With it ON the children are listed in Magic's
  instance-identifier hash order and an answered file name keeps its first
  7 characters, as in Magic; with it OFF the list is in placement order and
  the whole answer is the name.
- **DRC throughput is gated on instruction count.** CI fails when one DRC
  pass of the reference block takes more than the M3 baseline of 12.14
  billion instructions + 2 %; the time ratio to Magic is reported for
  information.
- **`cellname list filepath` and `readspice` bus brackets follow
  `magic-compat`.** With it ON, `cellname list filepath` returns the whole
  stored path, as Magic does, and a netlist pin `D[0]` never matches the
  layout port `D<0>` (Magic's bracket-map typo). With it OFF, the first
  returns the directory and the bracket map works as intended.
- **The eight IHP sg13g2 fixed devices are native generators.** With
  `magic-compat` ON the result is Magic's; with it OFF the device is placed
  in the edit cell and the window, edit cell and box are kept.
- **GUI scripts run headless in `hefesto-cli --magic -d X11`.** A model of
  Tk's widget tree stands in for the wrapper window, so wrapper.tcl's tags,
  menus and dialogs work without a display.

- **Desktop: Place device has a filter field; the label popover closes on
  a click outside.** Both are now Raven popovers (Raven is pinned at
  v0.2.3). In Place device, typing filters the PDK's devices, Up/Down move
  the highlighted device and Return places it; Escape or a click outside
  closes it. The label popover also closes on a click outside, issuing
  nothing, like Escape and Cancel.
- **Desktop: dragging a panel header shows where it will dock.** A dashed
  band at the dock edge ("Drop to dock on the right"), the outline of a
  floating target, and the dock the panel came from. A panel docked again
  after floating now takes its dock's size and position (it sat offset in
  an oversized dock before).
- **Desktop: the script consent prompt is an alert sheet.** It has the
  shield icon, the PDK's origin in an inset block, and Don't run on the
  left. The Interrupt card sits at the top centre of the canvas. In the
  PDK panel, each row's install state no longer covers its scripts state.
- **Desktop: the PCell dialog, text helper, reorder dialog and Cells
  manager follow their design boards** (compact keys, monospaced values,
  buttons on the right; the Cells list fits the cell name).
- **Desktop: scripted input and captures for checks.** `--input 'move X,Y;
  down X,Y; up X,Y; key [MOD+]NAME; text T; wait MS'` plays pointer and key
  events and prints where each panel ended up. `--press X,Y` holds the left
  button (to open a menu). `--menu-bar-in-window` draws the menu bar inside
  the window on macOS. `--capture-window pcell|texthelper|reorder` saves a
  utility window.

- **Desktop: the undo history names each step after the command that made
  it** ("↶ paint metal1", "Undid paint metal1 · 3 more steps"), and pressing
  and holding Undo for half a second opens it (a shorter press still
  undoes).
- **Desktop: Tab completes in the Tcl console** the command, procedure or
  `$variable` name before the cursor from the project's interpreter; several
  matches are listed in the transcript.
- **Desktop: a script that runs longer than 2 s dims the canvas** under its
  Interrupt card, and the canvas shows the script's edits as it goes. The
  script-consent prompt names what the startup file runs next ("sky130A.tcl +
  8 part files") and whether the store release's digest verified.

- **`hefesto-cli` runs `-c`, script files, standard input and
  `--interactive` as Tcl outside `--magic` mode too.** Each complete command
  runs in the project's sandboxed interpreter and its non-empty result is
  printed, as in a Tcl console (`-c "tech name"` prints the name). Before M4
  these lines went through Hefesto's own command reader with no Tcl
  substitution or result printing. In `--magic` mode only `puts` prints, as
  in Magic's batch mode. A Tcl error stops that source at its line, and a
  command that fails quietly still makes the run exit 1. See
  `docs/building.md`, `hefesto-cli`.
- **Lines typed in the desktop command bar and console are Tcl.** Braces,
  double quotes, `$` and `[...]` follow Tcl's quoting rules, as in Magic's
  Tcl console: `label {two words}` labels with two words, and a `$` or `[`
  meant literally needs braces or a backslash.
- **`shell` runs nothing, from any origin.** Magic's `shell` runs a command
  in an operating-system shell. Hefesto runs no external program (decision
  X-3), so `shell` now fails with `SCRIPT-SANDBOX-001` ("process execution
  is not permitted") when you type it in the console, the command bar or
  hefesto-cli, as it already did for scripts, key macros and plugins. The
  default `L` key macro (`shell ls`) reports the same error.
- **Key and button macros, key-binding files, menu items and manager
  buttons are Tcl too.** `$`, `[...]` and backslashes follow Tcl's rules in
  them. A key-binding file runs only its `macro` and `imacro` lines; any
  other command in it is reported (`UI-USAGE-001`) and does nothing. A
  quiet engine failure still shows the line as failed.
- **While a script runs, the layout takes no input.** A long PDK script, a
  menu callback or a console command now draws frames as it runs; after 2 s
  an Interrupt card (also Ctrl-C) stops it. Clicks, key macros and menu
  items in the meantime run nothing (`SCRIPT-TCL-004`); lines typed in the
  console or command bar wait and run after it. The edits of an
  interrupted console command are undone in one step.
- **A layout whose PDK scripts you declined is read-only.** It opens with
  the `minimum` technology, the editing tools and File › Save are off, and
  nothing writes it (`SCRIPT-TRUST-005`). "Review scripts…" asks again and
  reopens it with its technology when you allow the scripts.
- **Layer toolbar reorder dialog.** "Group selected" makes the selected
  layers adjacent in the first one's toolbar row, and "Move layer" moves a
  layer to a new position; Magic's two buttons only print the selection.
  Groups affect the order only, and the dialog selects one row at a time.
- **PCell dialog.** Create with the default instance name issues
  `magic::gencell_create <type> <lib> {k v …}`, because Magic's
  `magic::gencell <lib>::<type> {} k v …` only opens the dialog. A named
  instance issues `magic::gencell <lib>::<type> <inst> k v …`. The dialog
  stays above every window, not only Hefesto's, until the Raven toolkit has
  owned utility windows.

### Fixed

- **A `.mag` subcell placed outside the usable plane is ignored with an
  error** instead of overflowing coordinates. A `use` block whose
  `transform` translation or `box` lies beyond ±1073741818 (after scaling)
  is skipped with `Ignoring subcell C of P: its transform or box lies
  outside the usable plane`, and the rest of the cell is read.
- **The color-map size limit is checked by the reader itself.** A `.cmap`
  that declares more entries than the limit (2^24, or a smaller limit the
  caller sets) reports `Color map file "F.cmap" has too many colors` before
  any memory is allocated, including when the index sits on the second
  part of a line longer than 127 characters.
- **Background DRC no longer stalls after `drc on`.** If the checker found
  nothing to do (for example with DRC off) at the same moment a command woke
  it, it could go to sleep with squares still pending, until the next edit.
- **"(UNNAMED)" no longer stays in the Cells manager** when the desktop
  app is started with a cell on the command line: the window's empty
  startup cell is replaced by the loaded cell, as in Magic.
- **The canvas keeps its view when it is resized after you start working
  in it.** Opening the console (for example on an error) between a wiring
  press and the pointer moves refitted the view, so the leg went south
  instead of east. The view is now refitted on resize only until the first
  press in the canvas; after that the layout under every pixel stays put.
- **DRC on sky130 standard cells is fast again.** Copying mask hints into
  the DRC's interaction area cost quadratic time in the number of hinted
  cells; a block check is back to about 0.7 × Magic's time on x86.
- **Tag scripts run in the frame of the code that ran the command**, as in
  Magic: a tag fired from a procedure sees `info level` and its locals.
  GUI sessions no longer print "Box tool must be present" twice when a
  PCell is placed without a box.
- **`array`, `delete`, `copy`, `move`, `stretch`, `clockwise`, the flips and
  `orient` without a box print "Box must be present"**, as Magic does.

- **Moving, copying, deleting, rotating or arraying before anything was
  ever selected no longer crashes.** `box 0 0 0 0; delete` in a fresh
  session (or a device script that runs `array` first) crashed
  `hefesto-cli`; these commands now see an empty selection, as in Magic.
- **`cellname create` of an existing cell does nothing**, as in Magic,
  instead of failing with `cell "..." already exists` (a second generated
  device with the same parameters hit it).
- **Commands that need the box in the edit cell print Magic's text.**
  `paint`, `erase`, `label`, `port` and the other editing commands say `Box
  must be present` when there is no box; `box`, `select box` and `element
  inbox` keep `Box tool must be present`.
- **`readspice` and `mazeroute` are global Tcl commands**, as Magic's
  `readspice.tcl` and `mazeroute.tcl` define them, so scripts can call them
  unqualified.
- **`--diagnostics=magic` no longer prints the native-generator fallback
  note** (`PCELL-NATIVE-001`), which Magic has no equivalent of.
- **A cell named on the command line that does not exist is reported as
  Magic reports it**, without the extra "The cell was already read and
  points to conflicting location" message, and a `.mag.gz` name is shown
  without its extensions (`File /w/inv couldn't be read`).
- **`load` removes the untouched `(UNNAMED)` cell when a display is
  running**, as Magic does under X11, so the desktop's Cells manager no
  longer lists it; `hefesto-cli` (no display) keeps it, as `magic -dnull`
  does.
- **`hefesto-cli --interactive` with standard input from a character
  device such as `/dev/null` ends at end of input on macOS** instead of
  waiting forever.
- **`file link NAME` and `file readlink NAME` with a NUL character in NAME
  fail with "no such file or directory"** instead of crashing: Tcl 8.6.16
  passes a NULL path to `readlink()` there.
- **Outline fonts with far-apart curve control points** no longer trigger
  undefined integer overflow while flattening Bézier curves; the points
  wrap as Magic's C arithmetic does.

### Security

- **A script can no longer corrupt memory with `catch`.** In the embedded
  Tcl 8.6.16, `catch body var` where `var` cannot be set (for example
  `catch nosuchcmd nosuchns::v`) read the interpreter result after it had
  been freed. Hefesto now carries a fix for Tcl
  (`third_party/patches/tcl/`); the script gets Tcl's usual
  `can't set "nosuchns::v": parent namespace doesn't exist` error.
- `--trust-pdk <family|variant>` also trusts a copy of that PDK found
  outside the PDK store, for example a tree named by `-rcfile` or
  `PDKPATH`. This is a documented, accepted limit (S-12 in
  `docs/contributing/security.md`): name only PDKs you mean to run.
