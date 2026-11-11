# GUI Wrapper and Managers

## Purpose
Specifies Magic's Tcl/Tk GUI layer (`wrapper.tcl` and the scripts it sources). It covers:
- the wrapper layout window, its menus, title-bar captions and the hook points that PDK scripts use;
- the cell, library, technology and DRC managers;
- the text helper;
- the layer toolbar and its reordering presets;
- file prompts, the crash-backup timer and tool (button-binding) modes.

Most visual details are `informative`. Hefesto's GUI on Linux/macOS/iPadOS MAY look and behave differently, but the commands these components issue, the hooks they expose to scripts, and any files they read or write are specified so that scripts and data remain compatible. Hefesto's managers, menus and dialogs are native in both shells (desktop and iPad). They provide the `magic::add_toolkit_*` hooks and the dialog builder API natively and do not emulate Tk path names in general; the widget operations of PDK menu callbacks are intercepted as specified in capability `scripting-pdk-integration`, Requirement "Wrapper widget operations in PDK menu callbacks" (Q-SCRIPT-34, resolution: platform; Q-SCRIPT-42).

## Requirements

### Requirement: Wrapper window construction
When a windowing toolkit is available, `magic::openwrapper ?cell? ?frame?` SHALL create a top-level frame. The default frame name is `.layoutN`, where N is a session counter starting at 1. The first call SHALL turn off the engine's own window caption, scrollbars and border. The window is built as follows:
- The frame contains a vertical paned window `F.pane` with a top pane `F.pane.top` (the "layout frame" L) and a hidden bottom pane `F.pane.bot`.
- L contains `L.titlebar` (menu buttons, a DRC checkbutton, caption, message and position labels), scrollbars `L.xscroll`/`L.yscroll`, a zoom button `L.zb`, the layout window `L.magic`, and an initially hidden toolbar `L.toolbar`.
- The GUI scale `Opts(scale)` is set once, if unset, to max(1, floor(width of "M" in the default font / 10)).
- The scrollbar width is floor(13 × scale).
- The frame geometry is taken from `Winopts(F,geometry)`, or else `Opts(geometry)`. The default is 3/4 of the screen width × 3/4 of the screen height at +100+100.
- The layout window is opened with `magic::openwindow cell L.magic`.

The command SHALL return the layout window path `F.pane.top.magic`. The commands `openwrapper` and `closewrapper` SHALL be imported globally.
Compatibility: behavioral

(Magic ref: tcltk/wrapper.tcl:1050-1205,1455-1510,1581-1584)

#### Scenario: First wrapper
- **WHEN** magic starts with the wrapper enabled and no cell
- **THEN** the window `.layout1.pane.top.magic` SHALL exist and `windownames layout` SHALL return `.layout1.pane.top.magic`

### Requirement: Wrapper hook points for scripts
After building a wrapper, `openwrapper` SHALL evaluate the following in its own scope, where the local variables `framename`, `layoutframe`, `winname` and `cell` are visible:
1. The script in `Opts(callback)`, if defined. Errors are caught and ignored.
2. The script in the per-window element `Winopts(<frame>, callback)`, with a space in the key, as Magic reads it; then the script in `Winopts(<frame>,callback)` if that element exists (Q-SCRIPT-13, resolution: fix; Magic never runs the key without the space).

Then, if `Opts(autobuttontext)` is true (default 1), it SHALL set each menu item's accelerator text to `(<key>)`, where `<key>` is the key macro whose text equals the item's command after stripping `magic::` and surrounding blanks. The key names are mapped: `Control_`→`^`, `XK_`→`` (removed), `less`→`<`, `more`→`>`, `comma`→`,`, `question`→`?`. PDK scripts use `Opts(callback)` to add device menus to every new wrapper window. The script typically references `$framename` or `$layoutframe`.
Compatibility: behavioral

(Magic ref: tcltk/wrapper.tcl:1495-1508, tcltk/tools.tcl:267-295)

#### Scenario: PDK menu callback
- **GIVEN** `set Opts(callback) {sky130::addtechmenu $framename}`
- **WHEN** `openwrapper` creates `.layout2`
- **THEN** `sky130::addtechmenu .layout2` SHALL be evaluated

#### Scenario: Per-window callback
- **GIVEN** `set Winopts(.layout2,callback) {set ::seen $framename}`
- **WHEN** `openwrapper` creates `.layout2`
- **THEN** `::seen` SHALL be `.layout2`

### Requirement: Wrapper menus
The wrapper SHALL provide menu buttons File, Edit, Cell, Window, Layers, Drc and Options. Each item issues the listed command:

| Menu | Items (label → command) |
|---|---|
| File | Open... → `promptload magic`; Save... → `promptsave magic`; Flush changes → `magic::flush`; Read GDS → `promptload gds`; Write GDS → `promptsave gds`; Save All and Quit → `saveallandquit`; Quit → `magic::quit` |
| Edit | Delete → `delete`; Select Area → `select area`; Select Clear → `select clear`; Undo/Redo → `magic::undo`/`magic::redo`; Rotate 90 degree → `magic::clockwise 90` (Q-SCRIPT-36, resolution: fix; Magic's item runs the non-existent `magic::clock`); Mirror Up/Down → `upsidedown`; Mirror Left/Right → `sideways`; Move Right/Left/Up/Down → `move <dir> 1`; Stretch Right/Left/Up/Down → `stretch <dir> 1`; Text ... → open the text helper |
| Cell | New... → `prompt_dialog new`; Save as... → `prompt_dialog save`; Select → `select cell`; Place Instance → `promptload getcell`; Down/Up hierarchy → `pushstack`/`popstack`; Edit → `edit`; Delete → `cellname delete [cellname list window]`; Expand Toggle → `expand toggle`; Expand → `expand`; Unexpand → `unexpand`; Lock/Unlock Cell → `instance lock`/`instance unlock` |
| Window | Clone → `openwrapper [cellname list window]`; New → `openwrapper`; Set Editable → `pushbox; select top cell; edit; popbox`; Close → `closewrapper F`; Full View → `view`; Redraw → `redraw`; Zoom Out → `zoom 2`; Zoom In → `zoom 0.5`; Zoom Box → `findbox zoom`; Grid on/off → `grid on|off`; Snap-to-grid on/off → `snap on|off`; Measure box → `box`; Report internal/lambda/micron/grid units → `units internal|lambda|microns|grid`; Display units (check) → `units print|noprint`; Set grid 0.05um … 10.0um → `grid <v>um` for v ∈ {0.05, 0.10, 0.50, 1.00, 5.00, 10.0} |
| Layers | Protect Base Layers → `tech revert`; Unlock Base Layers → `tech unlock *`; Clear Feedback → `feedback clear`; Reorder layers → layer-reorder dialog |
| Drc | DRC update → `drc check; drc why`; DRC report → `drc why`; DRC Find next error → `drc find; findbox zoom`; DRC Fast → `drc style drc(fast)`; DRC Complete → `drc style drc(full)` |
| Options | check items: Toolbar, Toolbar Hide Locked, Cell Manager, Library Manager, Tech Manager, DRC Manager, Netlist Window, Colormap Window, 3D Display, Window Command Entry, Crosshair, and Maze Router if the maze helper is loaded |

The DRC checkbutton SHALL be bound to `Opts(drc)` and SHALL run `drc on` or `drc off`. The file menus SHALL only add and remove items through the hook API, so that PDK menus coexist with them.
Compatibility: informative

(Magic ref: tcltk/wrapper.tcl:1131-1175,1223-1453)

#### Scenario: Menu dispatch
- **WHEN** the user selects Window → Zoom Box
- **THEN** the command `magic::findbox zoom` SHALL be executed

### Requirement: Wrapper tag callbacks and caption formats
The wrapper SHALL install these tags (see `scripting-api`):
- `load`, `edit`, `save`, `down` → refresh captions;
- `box` → `boxview %W %1`;
- `move` → `boxview %W`;
- `scroll`, `view`, `zoom`, `findbox` → scrollbar update;
- `see` → `toolupdate %W %1 %2`;
- `tech` → `techrebuild %W %1; captions %1`;
- `drc` → `drcupdate %1`;
- `path` → tech manager update;
- `cellname`, `cif`, `gds` → `mgrupdate %W %1`.

Text formats:
- Caption: `Loaded: <root> Editing: <edit> Tool: <Opts(tool)>    Technology: <tech name>`. Root and edit come from `windowcaption`: element 0 is the root; if element 1 is `EDITING`, the edit cell is element 2, otherwise it is the whole caption.
- Box position, computed in microns: `box (%+g %+g) to (%+g %+g) microns`, with integral values printed as integers. `box remove` shows `no box`. Informational `box` forms (no argument, `exists`, `help`) do not update it.
- Pointer motion: `(%+g %+g) %+g %+g microns` (pointer and offset from the box lower-left), or `(%+g %+g) microns` without a box.
- DRC button: when idle, `DRC=<total count>` with the indicator red if the count > 0, else green. While busy, yellow.

Nested invocations (call depth > 1) of boxview and scrollupdate SHALL be ignored.
Compatibility: informative

(Magic ref: tcltk/wrapper.tcl:233-268,533-729,823-844,563-583)

#### Scenario: Box caption
- **GIVEN** the box is (0,0)-(1.5,2) µm
- **WHEN** `box 0 0 1.5um 2um` runs
- **THEN** the position label SHALL read `box (+0 +0) to (+1.5 +2) microns`

### Requirement: Wrapper option variables
The wrapper SHALL initialize these globals: `Opts(techmgr)=0`, `Opts(target)=default`, `Opts(netlist)=0`, `Opts(colormap)=0`, `Opts(wind3d)=0`, `Opts(crosshair)=0`, `Opts(hidelocked)=0`, `Opts(hidespecial)=0`, `Opts(toolbar)=0`, `Opts(toolscale)=1.0`, `Opts(drc)=1`, `Opts(autobuttontext)=1` and `Opts(cmdentry)=0`. Per window: `Winopts(F,toolbar)` defaults to 1 and `Winopts(F,cmdentry)` defaults to `Opts(cmdentry)`. `Opts(focus)` SHALL be set to the frame name when the pointer enters a layout window. `Opts(target)` selects the window that the managers act on: `default` means the first layout window.
Compatibility: informative

(Magic ref: tcltk/wrapper.tcl:585-601,1205-1221)

#### Scenario: Toolbar default on
- **WHEN** a wrapper opens and `Winopts(.layout1,toolbar)` was not set
- **THEN** the toolbar SHALL be built and shown

### Requirement: Window close and key overrides
`closewrapper F` SHALL do the following:
1. Reset `Opts(target)` to `default` if it pointed at F's window.
2. Remove F from the window list menu.
3. Close the layout window.
4. Destroy F.

Closing F through the window manager SHALL be equivalent. When the key macro `o` is exactly `openwindow`, the wrapper SHALL rebind it to open a 500×500 wrapper `.owindowK` at the pointer, showing the focused window's cell, with the view set to the box. When `O` is exactly `closewindow`, it SHALL be rebound to `closewrapper $Opts(focus)`.
Compatibility: informative

(Magic ref: tcltk/wrapper.tcl:1473-1493,1512-1536)

#### Scenario: Close via menu
- **WHEN** Window → Close is chosen in `.layout2`
- **THEN** `.layout2` SHALL be destroyed and `windownames layout` SHALL no longer list it

### Requirement: Per-window command entry
`addcommandentry F` SHALL show the bottom pane with a scrolled text console that evaluates commands directed at `F.pane.top.magic`, with prompt `F>`. While the pointer is in the layout window, it SHALL bind `:` to focus that console and alias `puts` to the console's output. `deletecommandentry F` SHALL hide the pane, rebind `:` to the default `:` imacro, and re-alias `puts` to the tkcon `puts`.
Compatibility: informative

(Magic ref: tcltk/wrapper.tcl:1538-1579, tcltk/tkshell.tcl:61-202)

#### Scenario: Directed console
- **WHEN** `paint m1` is typed in the console of `.layout1`
- **THEN** it SHALL execute as `.layout1.pane.top.magic paint m1`

### Requirement: Technology manager
`techmanager ?create|init|update?` SHALL maintain the `.techmgr` window. `create` builds it once (withdrawn). `init` rebuilds the menus. Any option is ignored when the window does not exist, except `create`. The window shows:
- the tech name, `(version <v>)` and the description;
- `Microns per lambda (CIF)` = `%g` of `cif scale output × λden/λnum`;
- the internal units per lambda (`λden / λnum`), with two editable entries;
- the CIF input and output styles with their microns-per-lambda values;
- the extract style and DRC style.

Its tech-name menu SHALL list:
- `<tech>` for every `*.tech` and `*.tech27` file in the system path (`path sys`), which runs `tech load <tech>`;
- `<dir>` for every entry of `$PDK_ROOT` (Tcl variable first, then environment), which runs `source <PDK_ROOT>/<dir>/libs.tech/magic/<dir>.magicrc`.

Editing the units-per-lambda entries SHALL call `scalegrid (lval0·λnum) (lval1·λden)`. The arguments are passed in the order shown.
Compatibility: informative

(Magic ref: tcltk/wrapper.tcl:286-385,411-531)

#### Scenario: PDK listed in tech menu
- **GIVEN** `PDK_ROOT=/pdk` containing `sky130A/`
- **WHEN** `techmanager init` runs
- **THEN** the tech menu SHALL contain `sky130A`, which sources `/pdk/sky130A/libs.tech/magic/sky130A.magicrc`

### Requirement: Layer toolbar
`maketoolbar L` SHALL rebuild the vertical, scrollable layer toolbar of layout frame L, unless the frame's redraw is suspended. The steps are:
1. The layer list is `errors labels subcell` (unless `Opts(hidespecial)`) followed by `tech layer *`, or by `tech unlocked` when `Opts(hidelocked)` is set.
2. If `./<techname>_toolbars.tcl` exists in the current directory, it SHALL be read as data and never evaluated: only lines of the form `set <name> {<layer> …}` or `set <name> "<layers>"` are read, every other statement is reported with a diagnostic naming the file and line and ignored, and the `current_toolbar` list, if the file sets it, is used instead (Q-SCRIPT-11, resolution: fix in both `magic-compat` settings; Magic sources the file as a script).
3. Each layer row has an icon (the layer's image, or a pale image if locked) and its name.

Row bindings, which issue commands to `L.magic`:
- Button-1 → `see <layer>`; Button-3 → `see no <layer>`.
- Unlocked rows only: Button-2 → `paint <layer>`; Shift-Button-2 → `erase <layer>`; key p → paint; key e → erase; key s → `select more area <layer>`; key S → `select less area <layer>`; key l → `tech lock <layer>`.
- Locked rows: key u → `tech unlock <layer>`.
- Entering a row shows `<layer>` or `<layer> (locked)` in the title-bar message.

`see` commands SHALL update the icons (visible: normal image; hidden: blank). The aliases `none`, `allSame`, `*`, `$` and `connect` are ignored.
Compatibility: informative

(Magic ref: tcltk/toolbar.tcl:7-241, tcltk/wrapper.tcl:731-818)

#### Scenario: Paint from toolbar
- **WHEN** the user middle-clicks the `poly` row in `.layout1`
- **THEN** `.layout1.pane.top.magic paint poly` SHALL execute

#### Scenario: Toolbar file is not executed
- **GIVEN** `sky130A_toolbars.tcl` contains the lines `set current_toolbar {m1 m2}` and `file delete x.mag`
- **WHEN** the toolbar of a sky130A window is built
- **THEN** the order SHALL be `m1 m2`, `x.mag` SHALL still exist, and a diagnostic SHALL name the second line

### Requirement: Toolbar order presets file
The file `<techname>_toolbars.tcl` in the current directory SHALL be a file of `set <preset> {layer ...}` lines, read as data as in "Layer toolbar". `current_toolbar` is the active order. The reorder dialog SHALL list presets as the names the file sets, or `default_toolbar` if the file is absent. Load SHALL read the chosen preset from that file through the host file API, and its error text SHALL name `<techname>_toolbars.tcl`. Saving a chosen preset SHALL rewrite every line matching the regex `set current_toolbar \{.*\}` (per line) with `set current_toolbar {<layers>}`. Without a file, the order applies only in memory. The file format and the rewrite are Magic's (Q-SCRIPT-28, resolution: fix; Magic's Load sources `$CAD_ROOT/magic/tcl/toolbar.tcl` instead and always fails, names `<tech>_toolbar.tcl` in its error, and reads the file with `exec cat`, which the sandbox denies).
Compatibility: informative

(Magic ref: tcltk/reorderLayers.tcl:1-316)

#### Scenario: Preset persisted
- **GIVEN** `sky130A_toolbars.tcl` contains `set current_toolbar {m1 m2}` and `set mine {poly m1}`
- **WHEN** preset `mine` is loaded
- **THEN** the file's `current_toolbar` line SHALL become `set current_toolbar {poly m1}`

### Requirement: Cell manager
When the toolkit supports tree views (Tk ≥ 8.5), `cellmanager ?create|update?` SHALL maintain `.cellmgr`, a tree of top-level cells (`cellname list topcells`) and their children (`cellname list children`). Each item's column shows the first instance name. Item ids are `/`-joined definition paths. Buttons act on the target window:
- Load → `load <def>`.
- Edit → select the instance path, then `expand; edit`.
- Expand → `expand toggle` (`expand` for a top cell).
- Place → `getcell <def>`.
- Zoom → `findbox zoom` (`view` for a top cell).
- Refresh.

The manager SHALL tag `select` with `mgrselect %r`, which highlights the selected instance (a 5-element result means a top cell). It SHALL tag `load` with `clearstack; cellmanager` and `getcell` with `cellmanager`.
Compatibility: informative

(Magic ref: tcltk/cellmgr.tcl:1-364)

#### Scenario: Load clears edit stack
- **GIVEN** the edit stack is non-empty
- **WHEN** the user runs `load foo` with the cell manager script loaded
- **THEN** the edit stack SHALL be empty

### Requirement: Library manager
`libmanager ?create|update?` SHALL maintain `.libmgr` with one tree node per directory in `path search` and then in `path cell`. The `path cell` entries have their path variables expanded from the path-variable table (Q-CELL-17), never by script substitution. Directory nodes are labelled `<dir>/`, and only the first is opened. Under each directory it lists the `*.mag` files, sorted, whose first line is exactly `magic` and whose second line is exactly `tech <name>`, with `<name>` shown in the Technology column. When the `Filter` option is on (default), only files whose tech equals `tech name` are listed. Buttons:
- Load → `load <cell>`.
- Place → `getcell <cell> child 0 0`.
- Pick → switch to the pick tool, place, and start a move.
- Refresh.

It SHALL refresh after `addpath`, `path`, `save` and `writeall`. Nested calls are ignored. A refresh compares normalized directory paths, so directories that did not change keep their nodes, and every file opened for the header check is closed (Q-SCRIPT-27, resolution: fix).
Compatibility: informative

(Magic ref: tcltk/libmgr.tcl:1-249)

#### Scenario: Filter by technology
- **GIVEN** directory `lib/` with `a.mag` (`tech sky130A`) and `b.mag` (`tech scmos`), and current tech `sky130A`
- **THEN** only `a` SHALL be listed while Filter is on

### Requirement: DRC manager and DRC report file
`drcmanager ?create|update?` SHALL list `drc listall why` for the top cell as one tree node per rule, with that rule's error boxes as children. Buttons:
- Update.
- Last/Next: select the previous/next item and zoom.
- Zoom: set the box to the error in internal units, `findbox zoom`, `zoom 2`.
- Save: `drc_save_report`.

`magic::drc_save_report ?cell? ?file drc.out?` SHALL write the file as follows:
1. Optionally load `cell`, select the top cell, and run `drc check`.
2. Line 1: `<cell> <drc list count result>`.
3. A separator line of 40 `-`.
4. For each rule in `drc listall why` order (capability `scripting-api`; Q-SCRIPT-8): the rule text; the separator; one line per box formatted as ` %.3f %.3f %.3f %.3f` (leading space), giving each coordinate in internal units × `cif scale out`; the separator. The coordinates are collected with units set to internal, and the user's units are restored afterwards.
5. A final empty line.
6. Reload the original cell if one was given.

Line 1 keeps Magic's form: the cell name, then the `drc list count` result, which is the list of `{cell n}` pairs in the cell order of capability `drc-commands` (decision X-1). The file is byte-identical to Magic's whenever the units are default or internal (Q-SCRIPT-26, resolution: fix the units, keep the format; Magic multiplies coordinates printed in the current units, so its values are wrong with lambda, micron or user units, and the report fails with the units print flag).

`magic::drc_load_report ?file drc.out?` SHALL read such a file:
- Every line beginning with a space and holding four tokens adds feedback `<last rule text>` with style `vert_highlights`, as the polygon (llx,lly) (llx,ury) (urx,ury) (urx,lly) in µm.
- Lines starting with neither a space nor `-` set the rule text.
Compatibility: exact

(Magic ref: tcltk/drcmgr.tcl:1-300)

#### Scenario: Report file format
- **GIVEN** cell `inv`, units default, `cif scale out` 0.005, and one error of rule `Metal1 spacing < 0.14um` at (0,0)-(28,40)
- **WHEN** `drc_save_report` runs
- **THEN** `drc.out` SHALL contain the lines `inv {inv 1}`, 40×`-`, `Metal1 spacing < 0.14um`, 40×`-`, ` 0.000 0.000 0.140 0.200`, 40×`-` and an empty line (verify against Magic)

#### Scenario: Report with micron units
- **GIVEN** the cell and error of the previous scenario, with units `microns` and the units print flag on
- **WHEN** `drc_save_report` runs
- **THEN** `drc.out` SHALL be identical to the file of the previous scenario, and the units SHALL again be `microns` with the print flag on

### Requirement: Text helper
The text helper `.texthelper` SHALL edit label attributes. Its defaults are: text empty, font `FreeSans`, size 1, justification center, rotation 0, offset `0 0`, port 0. It reads the selected label with `setlabel text|justify|font|rotate|offset|layer|sticky|size` and `port exists|index`, in microns. Applying to a selected label SHALL issue `setlabel` for each non-empty field (`default` font or justification → `-1`), set sticky, and then `port make <n>`, `port index <n>` or `port remove`. Creating a new label SHALL issue one of:
- `label <text> <just>` when the font is default. `<just>` may be the word `default`, which `label` reads as the font name `default`, the default font (capability `cmd-labels-properties`), so the label gets the default font and the automatic justification, as intended (Q-SCRIPT-31, resolution: replicate; there is no defect);
- `label <text> <font> <size>um <rot> <offx offy> ?<just>?`;
- `label <text> <font> <size>um <rot> <offx offy> <just|center> <layer>` when a layer is given (prefixed `-` when sticky).

Size `0` or empty becomes 1; rotation empty becomes 0; offset empty becomes `0 0`. After creating a port, the port entry SHALL advance to `port last + 1`. The `select` command SHALL be tagged to refresh the helper. The font menu lists `setlabel fontlist`.
Compatibility: informative

(Magic ref: tcltk/texthelper.tcl:1-420)

#### Scenario: New port label
- **GIVEN** text `VDD`, font `FreeSans`, size `0.5`, layer `m1`, default-layer unchecked, port enabled with index 3
- **WHEN** Apply is pressed with no label selected
- **THEN** `label VDD FreeSans 0.5um 0 {0 0} center m1` and `port make 3` SHALL execute (verify against Magic)

### Requirement: File prompts
`promptload magic|gds|cif|getcell` and `promptsave magic|force|gds|cif` SHALL use file dialogs:
- Load magic: `load <root of chosen file>`.
- Load gds: run `gds read`. If exactly one new top cell appears, load it. Otherwise print `Top-level cells read from GDS file: <list>`.
- Load cif: read into a temporary cell `cif_temp`, load its first child, and delete `cif_temp`.
- getcell: add the file's directory to `path search` if absent, `getcell <file tail>`, then `path cell +<dir>/`.
- Save magic/force: if the window cell is `(UNNAMED)`, prompt for a name (adding its directory to `path cell` if it differs from the working directory) and `save` it. Then `writeall`, or `writeall force`.
- Save gds/cif: `gds write <file>` / `cif write <file>`.

`saveallandquit` SHALL run `promptsave force` and then `quit -noprompt` if `cellname list modified` is empty, else `quit`. `prompt_dialog new|save` SHALL ask for a cell name and run `load <name>` (initial `(UNNAMED)`) or `save <name>` (initial: the window cell).
Compatibility: behavioral

(Magic ref: tcltk/wrapper.tcl:66-231,1033-1048)

#### Scenario: GDS with a single top cell
- **GIVEN** `chip.gds` contains top cell `chip` and no cells are loaded
- **WHEN** `promptload gds` reads it
- **THEN** `chip` SHALL be loaded into the window

### Requirement: Crash backup timer
`crashbackups ?start|resume|stop|cancel|disable?` SHALL manage periodic crash-recovery saves:
- `start` (default): sets `Opts(backupinterval)` to 600000 ms if unset, and schedules a backup after that interval if it is > 0.
- `resume`: schedules a backup only if an interval is set and > 0.
- `stop` / `cancel`: cancels the pending backup.
- `disable`: cancels and unsets the interval.

Each backup SHALL run `*bypass crash save`. A failure is printed as `Crash backup save error:  <msg>`. The next backup is rescheduled only if the interval is set and > 0. Otherwise a warning is printed (`Warning:  Crash backups halted due to interval of <n>` or `...due to no interval being set.`). The tool setup run from the system startup file SHALL set the interval to 60000 ms and start backups (Q-SCRIPT-18). The backups are written to Hefesto's app-private crash backup directory, never to a temporary or caches directory (capability `cell-io-safety`, requirement "Crash backup location and session lock"). On iPadOS the autosave service also writes a backup when the app moves to the background (capability `hefesto-files-and-pdks`, requirement "Locking and recovery replacements"; Q-SCRIPT-35 and Q-CELL-30, resolved by decision D-8).
Compatibility: behavioral

(Magic ref: tcltk/tools.tcl:52-120,582-586, doc/html/crashbackups.html)

#### Scenario: Disable then resume
- **WHEN** `crashbackups disable` and then `crashbackups resume` run
- **THEN** no backup SHALL be scheduled

### Requirement: Tool modes
`tool ?next|type|info|box|wiring|nettool|pick?` SHALL switch the active button-binding table by setting `Opts(tool)`. The engine reads `Opts(tool)` as the macro-table name. `next` cycles box → wiring → nettool → pick → box. `type` returns the current tool. `info` prints the current bindings. Switching prints `Switching to BOX|WIRING|NETLIST|PICK tool.` and sets cursor glyph 0, 19, 18 or 22 respectively. Switching SHALL be refused, silently, while a drag is in progress (`Opts(motion)` non-empty).

The tool setup SHALL:
- bind `>` to `pushstack [cellname list self]` and `<` to `popstack`, space to `tool`, Shift-space to `tool box`;
- copy the box bindings to the wiring, nettool and pick tables, then override them:
  - wiring: Button1 pick/start wire; Button2 done; Button3 cancel; Control-Button1 start from current wire; Shift-Button1/3 next/previous wire type; Button4/5 wire width ±;
  - nettool: Button1/2/3 → `netlist select|join|terminal`; Button4/5 scroll;
  - pick: Button1 keep or move the box; Button2 pick and drag; Shift-Button2 copy-drag; Button3 cancel or move the box corner.
Compatibility: behavioral

(Magic ref: tcltk/tools.tcl:512-788, dbwind/DBWbuttons.c:265-277)

#### Scenario: Tool cycle
- **GIVEN** `Opts(tool)` is `box`
- **WHEN** `tool` runs twice
- **THEN** `tool type` SHALL return `nettool`

### Requirement: 3-D view wrapper
`render3d ?cell?` SHALL create top-level `.render` containing a 3-D window `.render.magic` (`specialopen wind3d cell .render.magic`) with cut-box set to the box, a `Cutbox` checkbutton toggling `cutbox box|none`, and the caption `3D Rendering window  Cell: <cell|default>`.
Compatibility: informative

(Magic ref: tcltk/wrapper.tcl:1586-1615)

#### Scenario: Open 3-D view
- **WHEN** `render3d inv` runs
- **THEN** a window `.render.magic` showing `inv` SHALL exist
