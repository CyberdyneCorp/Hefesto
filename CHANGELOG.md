# Changelog

User-visible changes to Hefesto, newest first. Milestone details live in
`openspec/changes/` and `docs/roadmap.md`; this file lists what a user of a
release notices.

## Unreleased (M5: mask generation, GDS and CIF; in progress)

### Added

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
- **`cif unfracture`.** With `magic-compat` OFF it merges split tiles of
  each generated layer before output; with it ON it prints a warning and
  keeps Magic's behaviour (Q-MASK-413).

### Changed

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
  `magic-compat` re-reads the section (Q-MASK-300, Q-MASK-302).
- **DRC checks mask-layer rules.** The drc section's `cifwidth`,
  `cifspacing`, `cifarea` and `cifmaxwidth` rules (for example sky130's
  `cifmaxwidth` and `cifspacing` rules) now report errors, generated on
  the bound cifoutput style as Magic's drcCifCheck does. With
  `magic-compat` OFF error boxes are rounded outward to the internal grid
  instead of truncated (Q-DRC-13). Coordinate units in DRC output follow
  the current `cif ostyle`.
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
  style's scale factor (Q-MASK-338, proposed); with it OFF it is written in
  the units of the geometry. `cif flat` with `magic-compat` ON processes
  timestamp mismatches after the write, as Magic does.
- **Large inputs.** `cif read` refuses a file larger than 2 GiB before
  reading it (CIF-READ-010), and copying a read-only vendor cell streams its
  records instead of holding the whole structure in memory.
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
