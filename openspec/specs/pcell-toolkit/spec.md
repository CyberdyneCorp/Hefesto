# Parameterized Cell (gencell) Toolkit

## Purpose
Specifies the "gencell" framework that Magic provides in `toolkit.tcl`. Every open PDK (sky130, gf180mcu, IHP sg13g2) builds its device generators ("PCells") on this framework. The spec covers the procedure-naming contract between a PDK library and the framework, the parameter dictionary model, the deterministic naming of generated cells, the cell properties that record how a cell was generated, the create/change/regenerate algorithms, the parameter-dialog builder API, menu hooks, and SPICE-netlist-driven layout seeding.

Generated cells are ordinary layout cells. They are saved, read and extracted like any other cell. Hefesto MUST interoperate with them at the data level (names, properties, geometry) whether or not it can run the PDK's generator code (see Q-SCRIPT-1). The legacy framework `toolkit_rev0.tcl` and the menu helpers in `bsitools.tcl` are installed but not loaded by default (see capability `scripting-auxiliary-tools`).

## Requirements

### Requirement: Library and device procedure contract
A device library SHALL be a script namespace `<library>`. For each device type `<type>`, the library SHALL provide these procedures in that namespace:
- `<type>_defaults {}`: returns the default parameter dictionary.
- `<type>_convert {parameters}`: maps SPICE-netlist parameter names and values to dialog parameters, and returns a dictionary.
- `<type>_dialog {parameters}`: populates the parameter dialog using the builder API.
- `<type>_check {parameters}`: validates and corrects the parameters, and returns a dictionary.
- `<type>_draw {parameters}`: draws the device into the current edit cell. For `nocell` devices it instead places an instance and returns that instance's name.

The full device name is `<library>::<type>`. If a name has no `::`, the library SHALL default to `toolkit`. A device SHALL be recognized as a gencell if and only if `<library>::<type>_convert` exists as a command. Otherwise `magic::gencell` SHALL raise the error `No import routine for <library> library cell <type>!`. See Q-SCRIPT-20.
Compatibility: exact

(Magic ref: tcltk/toolkit.tcl:23-55,714-723)

#### Scenario: Unknown device
- **WHEN** a script runs `magic::gencell sky130::nosuchdev x1`
- **THEN** a script error `No import routine for sky130 library cell nosuchdev!` SHALL be raised

#### Scenario: Default library
- **WHEN** a script runs `magic::gencell mycap`
- **THEN** the library SHALL be `toolkit` and the device type `mycap`

### Requirement: Parameter dictionary model
A parameter set SHALL be an ordered key/value dictionary of strings, using Tcl dict semantics. The effective parameters of a device SHALL be `merge(<type>_defaults, given)`, where later values override earlier ones and default keys keep their positions. The arguments to `magic::gencell` after the instance name SHALL be key/value pairs. An odd count is an error (the dict-creation error). Two keys are special:
- `gencell`: if present after merging, it SHALL change the device type to its value before the type's `_defaults`, `_check` and `_draw` are used. It SHALL be removed from the dictionary before naming and storage.
- `nocell`: if present, the device is a "fixed" device. Its `_draw` procedure places an instance of a pre-existing cell and returns the instance name. No generated cell or properties are created. The keys `nx`, `ny`, `pitchx` and `pitchy` describe the instance array. When an existing nocell instance is edited, `nx = xhi−xlo+1`, `ny = yhi−ylo+1` and `pitchx`/`pitchy` are filled from the instance's array information.
Compatibility: exact

(Magic ref: tcltk/toolkit.tcl:686,1528-1532,1218-1246,1253-1258,1642-1653)

#### Scenario: Defaults merge
- **GIVEN** `_defaults` returns `w 1 l 0.15 nf 1`
- **WHEN** parameters `nf 4` are given
- **THEN** the effective parameters SHALL be `w 1 l 0.15 nf 4`

#### Scenario: Type switch
- **GIVEN** the dialog parameters contain `gencell nfet_03v3`
- **WHEN** a create is executed for type `nfet_01v8`
- **THEN** `nfet_03v3_defaults`, `nfet_03v3_check` and `nfet_03v3_draw` SHALL be used, and the stored parameters SHALL NOT contain `gencell`

### Requirement: Parameter value normalization for naming
For naming, each parameter value v SHALL be normalized as follows:
1. If v is a strict double string (Tcl `string is double -strict`: decimal, exponent and hexadecimal-integer forms with optional surrounding whitespace), let x = double(v). If |x − round(x)| < 1e−9, the result is `%d` of round(x). Rounding is half away from zero.
2. Otherwise, format x with `%.3f` and then delete the leftmost match of the regular expression `\.?0+$`.
3. A non-numeric v is used verbatim.
See Q-SCRIPT-21 for values that are infinite or NaN.
Compatibility: exact

(Magic ref: tcltk/toolkit.tcl:1848-1869)

#### Scenario: Numeric normalization table
- **WHEN** values `2`, `2.0`, `2e0`, `0.1500`, `1.23456`, `-0.5`, `0.0004`, `10.5`, `0x10`, ` 3 `, `abc` are normalized
- **THEN** the results SHALL be `2`, `2`, `2`, `0.15`, `1.235`, `-0.5`, `0`, `10.5`, `16`, `3`, `abc`

### Requirement: Deterministic generated-cell naming
The generated cell for device type T with final parameters P SHALL be named `T_<suffix>`. The suffix is computed as follows:
1. Sort the keys of P in ascending code-point order (Tcl `lsort`, ASCII). Build S by concatenating `normalize(value) + ";"` for each key in that order. Keys themselves do not take part.
2. Compute h = MurmurHash3_x86_32(S, seed 0) as specified below.
3. For i = 0..5, let c = (h >> 5i) & 31 and map it to a character: c<8 → `'2'+c`; 8≤c<16 → `'A'+(c−8)`; 16≤c<21 → `'J'+(c−16)`; c≥21 → `'P'+(c−21)`. The alphabet is `23456789ABCDEFGHJKLMNPQRSTUVWXYZ`, which excludes 0, 1, I and O. The characters are appended in order i = 0..5, least-significant group first. Bits 30-31 of h are unused.

The MurmurHash3 input SHALL be S taken as a sequence of characters, each contributing its code point modulo 256 as one byte (Q-SCRIPT-22). The length is the character count. The algorithm:
- Process blocks of 4 bytes as little-endian 32-bit k: k·=0xcc9e2d51; k=rotl(k,15); k·=0x1b873593; h^=k; h=rotl(h,13); h=h·5+0xe6546b64.
- Process the tail of 1-3 bytes in the standard way.
- Finalize: h^=len; h^=h>>16; h·=0x85ebca6b; h^=h>>13; h·=0xc2b2ae35; h^=h>>16.
- All arithmetic is mod 2³².
Compatibility: exact

(Magic ref: tcltk/toolkit.tcl:1167-1198,1778-1832)

#### Scenario: Golden hash values
- **WHEN** P = `w 1.0 l 0.15 nf 1` for type `nfet`
- **THEN** S SHALL be `0.15;1;1;`, h SHALL be `0x5f8a415d`, and the cell name SHALL be `nfet_XCJNSH`

#### Scenario: Equivalent numeric spellings share a cell
- **WHEN** P1 = `w 1.0 l 0.15 nf 1` and P2 = `w 1 l 0.15 nf 1`
- **THEN** both SHALL produce suffix `XCJNSH`

#### Scenario: Empty parameter set
- **WHEN** P is empty
- **THEN** S is the empty string, h = 0, and the suffix SHALL be `222222`

#### Scenario: Four-key example
- **WHEN** P = `nf 2 w 0.42 l 0.15 m 1`
- **THEN** S SHALL be `0.15;1;2;0.42;`, h SHALL be `0x2cea7c7f`, and the suffix SHALL be `Z5ZNGQ`

#### Scenario: Murmur reference vectors
- **THEN** murmur3_32 of `hello` SHALL be `0x248bfa47`, of `abc` SHALL be `0xb3dd93fa`, of `a` SHALL be `0x3c2569b2`, and of the empty string SHALL be `0x00000000`

### Requirement: Generated cell properties
After drawing, a generated cell SHALL carry these string properties: `library` = library name, `gencell` = device type (after any `gencell` override), and `parameters` = the final parameter dictionary in its Tcl string form (space-separated key/value list, braced as Tcl requires). These properties SHALL be written to and read from the cell file like any other string property (see capability `cell-file-format`), so that a generated cell can be re-edited after a save and reload. When a cell is selected but has no `gencell` property, its type SHALL be derived from a cell name of the form `<type>_<digits>`. The derivation uses the regular expression `^(.*)_[0-9]*$` and is a legacy fallback. Its library defaults to `toolkit`.
Compatibility: exact

(Magic ref: tcltk/toolkit.tcl:48-55,696-709,803-805,1267-1269)

#### Scenario: Properties after creation
- **WHEN** `magic::gencell sky130::nfet x1 w 1 l 0.15 nf 1` creates cell `nfet_XCJNSH` (given defaults `w 1 l 0.15 nf 1`)
- **THEN** `cellname list property nfet_XCJNSH library` SHALL be `sky130`, `... gencell` SHALL be `nfet`, and `... parameters` SHALL be `w 1 l 0.15 nf 1`

### Requirement: gencell entry point dispatch
`magic::gencell <name> ?<instname>? ?args...?` SHALL behave as follows. A leading `-spice` in args selects SPICE mode, and the flag is removed.

| `<name>` | instname | args | action |
|---|---|---|---|
| empty | — | — | edit the first selected instance in the dialog. If the selected cell has no gencell type, raise `No gencell device is selected!` |
| `lib::type` | empty | any | interactive: open the dialog for a new device. In SPICE mode, args are first passed through `_convert` if it exists |
| `lib::type` | names an existing instance in the edit cell | empty | interactive: select it and open the dialog to edit it |
| `lib::type` | names an existing instance | non-empty | non-interactive change: select it; P = merge(defaults, stored parameters of its cell); in SPICE mode args = `_convert(args)`; P = merge(P, args); run the change algorithm |
| `lib::type` | not an existing instance | any | non-interactive create: in SPICE mode args = `_convert(args)`; P = merge(defaults, args); run the create algorithm; select the new instance and rename it to `<instname>` |

The procedure SHALL return `0` on success. See Q-SCRIPT-23 for the undefined variables in the nocell change path.
Compatibility: behavioral

(Magic ref: tcltk/toolkit.tcl:641-779)

#### Scenario: Scripted creation
- **GIVEN** no instance `M1` exists in the edit cell
- **WHEN** `magic::gencell sky130::nfet M1 w 2` runs
- **THEN** an instance named `M1` of cell `nfet_<hash(merge(defaults, {w 2}) after check)>` SHALL be placed with its lower-left at the cursor box's lower-left, and the result SHALL be `0`

#### Scenario: SPICE parameters
- **WHEN** `magic::gencell sky130::nfet M2 -spice w 1.0 l 0.15 mult 2` runs
- **THEN** `sky130::nfet_convert` SHALL receive `w 1.0 l 0.15 mult 2` and its returned dictionary SHALL be merged over the defaults

### Requirement: Create algorithm
Creating a gencell with type T, library L, parameters P and orientation O (default `0`) SHALL proceed as follows:
1. Suspend window redraw (nested; see `ui-box-stack-and-measure`).
2. If P is empty, take P from the dialog (merged over defaults) and the instance name from the dialog's instance entry. An entry of `(default)`, or a name that already exists, means no explicit name.
3. Otherwise apply the `gencell` override, then P = merge(`T_defaults`, P).
4. P = `T_check(P)`. If check raises an error, print the message to stderr and continue with the unchecked P.
5. Push P back to the dialog if one is open. Remove the `gencell` key.
6. Save the units and set units to internal. Save the box values. Set the pointer to the focused window if one is known.
7. If `nocell` ∈ P: inst = `T_draw(P)`; restore the box.
8. Otherwise: name N = `T_<suffix(P)>`. Create cell N if it does not exist (an existing in-memory cell is reused and drawn into again; see Q-SCRIPT-24). Push the edit stack into N. Call `T_draw(P)`. Errors are printed to stderr and do not abort. Set the properties `library` = L, `gencell` = T and `parameters` = P on N. Pop the edit stack. Restore the box. inst = `getcell N O`. Expand the new instance.
9. If an explicit instance name was given, rename inst to it.
10. Restore units, resume redraw, redraw, and return the instance name.
Compatibility: behavioral

(Magic ref: tcltk/toolkit.tcl:1200-1283)

#### Scenario: Draw errors do not abort
- **GIVEN** `T_draw` raises `bad layer`
- **WHEN** a create runs
- **THEN** `bad layer` SHALL be printed on stderr, cell N SHALL still exist with the three properties, and an instance SHALL still be placed

#### Scenario: Box preserved
- **GIVEN** the box is (100,200)-(150,260) in internal units
- **WHEN** a create completes
- **THEN** the box SHALL again be (100,200)-(150,260)

### Requirement: Change algorithm
Changing instance I, of type T and library L, to parameters P SHALL proceed as follows:
1. Suspend redraw. If P is empty, P = merge(`T_defaults`, dialog values).
2. Read the dialog's instance-name entry. Magic does this unconditionally, even when no dialog exists (Q-SCRIPT-29). If it is `(default)` or names an existing instance, keep I. Otherwise the new name is that entry.
3. Apply the `gencell` override. P = `T_check(P)`, with errors printed. Push P to the dialog. Remove `gencell`.
4. Let G_old = I's cell and G = `T_<suffix(P)>`. If the instance name changed, rename I.
5. If G is empty or G == G_old, resume and return. Only the name changed, if anything.
6. Save units and set internal. Save the box. Set the pointer to the focused window.
7. Then, depending on the device:
   - nocell: record the abutment box, delete I, set the box to the abutment box, inst = `T_draw(P)`, and select it.
   - G already exists in memory: record I's orientation and abutment box, delete I, set the box to the abutment box, place `getcell G <orientation>`, and expand it.
   - Otherwise: the same, but create G with the create algorithm, preserving orientation.
8. For the two non-nocell branches:
   - If I's name did not start with G_old (it was user-assigned), the new instance SHALL keep the name I. Otherwise it keeps the default name given at placement.
   - If G_old has no remaining parents, delete cell G_old from memory without prompting.
9. Rename to the final name, restore the box and units, resume, run a DRC check of the box area, and redraw.

The old cell SHALL be retained if any other instance still uses it.
Compatibility: behavioral

(Magic ref: tcltk/toolkit.tcl:867-1042)

#### Scenario: Shared cell retained
- **GIVEN** instances `x1` and `x2` both use `nfet_XCJNSH`
- **WHEN** `x1` is changed to `w 2`
- **THEN** `nfet_XCJNSH` SHALL remain, with parent count 1, and `x1` SHALL use the new cell

#### Scenario: Last user removes old cell
- **GIVEN** only `x1` uses `nfet_XCJNSH`
- **WHEN** `x1` is changed to `w 2`
- **THEN** `nfet_XCJNSH` SHALL no longer be loaded (`cellname list exists nfet_XCJNSH` returns `0`)

#### Scenario: Orientation preserved
- **GIVEN** `x1` has orientation `90h`
- **WHEN** it is changed
- **THEN** the replacement instance SHALL have orientation `90h` and the same abutment-box lower-left

### Requirement: Non-placing cell generation
`magic::gencell_makecell <lib>::<type> ?key value ...?` SHALL compute P = merge(defaults, args) without running `_check` or the `gencell` override. It SHALL create N = `<type>_<suffix(P)>`, draw it with the edit stack pushed into N, set the three properties, pop, and return N without placing an instance. Redraw is suspended for the duration.
Compatibility: behavioral

(Magic ref: tcltk/toolkit.tcl:789-809)

#### Scenario: Makecell returns name only
- **WHEN** `magic::gencell_makecell sky130::nfet w 1 l 0.15 nf 1` runs with defaults `w 1 l 0.15 nf 1`
- **THEN** the result SHALL be `nfet_XCJNSH` and no new instance SHALL exist in the edit cell

### Requirement: Legacy random cell names
`magic::get_gencell_name <type>` SHALL return `<type>_<6 chars>`. The characters are drawn uniformly from `0-9a-z` (36 symbols, using code points 48-57 and 97-122). A name is redrawn while a cell of that name is loaded. The framework no longer uses this procedure, but it remains available to PDK scripts.
Compatibility: informative

(Magic ref: tcltk/toolkit.tcl:1133-1144)

#### Scenario: Name shape
- **WHEN** `magic::get_gencell_name res` runs
- **THEN** the result SHALL match `^res_[0-9a-z]{6}$`

### Requirement: Parameter dialog structure
The dialog SHALL be a single top-level window `.params`. It has a title area with these items:
- device-mode label `New device:` or `Edit device:`
- device type
- `Library:` and library name
- `Cellname:` and the cell name, or `(default)` for new devices
- `Instance:` and an editable instance-name entry, `(default)` for new devices

Below them is a scrollable parameter area `.params.body.area.edits` and a button row. Buttons:
- New devices: `Create` (create, then switch the dialog to edit the new instance) and `Create and Close`.
- Existing instances: `Apply` and `Okay` (apply and close). These invoke the change algorithm with empty P.
- Always: `Reset` (rebuild the dialog for a new device of the same type with defaults) and `Close`.

Re-opening SHALL reuse the window and replace its contents. After building, the device's `_dialog` SHALL be called with P. Then a default bounds-check callback SHALL be attached to every parameter widget, and the window raised. With an empty type and no instance, the dialog SHALL reuse the type and library shown in the current dialog if that dialog is in `Edit device:` mode, and otherwise do nothing. Array subscripts `[i]` or `[i,j]` at the end of an instance name SHALL be stripped to locate the cell.
Compatibility: behavioral

(Magic ref: tcltk/toolkit.tcl:1570-1771)

#### Scenario: Edit dialog for selected device
- **GIVEN** instance `x1` of `nfet_XCJNSH` with properties library `sky130`, gencell `nfet`
- **WHEN** `magic::gencell_dialog x1 {} {} {}` runs
- **THEN** the dialog SHALL show `Edit device:` `nfet`, library `sky130`, cellname `nfet_XCJNSH`, instance `x1`, and `sky130::nfet_dialog` SHALL be called with the stored parameters

### Requirement: Dialog builder API
The framework SHALL provide these builders. Each appends a row (label in column 0, widget in column 1) and binds the widget to a variable `magic::<pname>_val`. A parameter missing from P uses the fallback shown.
- `magic::add_entry pname text P`: text entry, fallback empty. Widget `<pname>_ent`.
- `magic::add_checkbox pname text P`: checkbox, fallback empty. Widget `<pname>_chk`.
- `magic::add_selectlist pname text values P ?initial?`: choice menu whose variable holds the chosen value, fallback `initial`. Widget `<pname>_sel`.
- `magic::add_selectindex pname text values P ?ival 0?`: choice menu whose variable holds the 0-based index, fallback `ival`. Widget `<pname>_sel`.
- `magic::add_message pname text P ?color blue?`: read-only text. Widget `<pname>_txt`.

`magic::gencell_getparams` SHALL return a dictionary of `pname → magic::<pname>_val` for every `_ent`, `_chk` and `_sel` widget in grid order. `magic::gencell_setparams P` SHALL write P into those widgets. It writes `_txt` only if the key exists, skips the pseudo-widget `gencell_sel`, and does nothing if no dialog exists.
Compatibility: behavioral

(Magic ref: tcltk/toolkit.tcl:1285-1306,1414-1517,811-865)

#### Scenario: Select index
- **GIVEN** `add_selectindex topc "Top contact" {none full half} P` with P lacking `topc`
- **THEN** the menu SHALL show `none` and `magic::topc_val` SHALL be `0`

### Requirement: Dialog dependencies and validation
`magic::add_dependency callback type lib ?pname ...?` SHALL attach `magic::update_dialog callback pname type lib` to each named parameter's widget, appending to any existing handlers:
- Entries: on Return and on focus-out.
- Checkboxes and choice items: on change.

Without pnames, the intent is to attach to every default parameter (Q-SCRIPT-25). `magic::update_dialog` SHALL compute P = merge(defaults, dialog) with the `gencell` override applied, then P = `_check(P)` with errors printed, then P = `callback(pname, P)` if a callback is given, and finally push P to the dialog. The default bounds check SHALL be attached to every parameter widget after the device dialog is built. Its callback is empty.
Compatibility: behavioral

(Magic ref: tcltk/toolkit.tcl:1308-1412)

#### Scenario: Dependent recomputation
- **GIVEN** a dependency on `nf` with callback `sky130::mos_dep`
- **WHEN** the user changes `nf` and presses Return
- **THEN** `_check` SHALL run first and `sky130::mos_dep nf <P>` second, and the dialog SHALL show the callback's returned values

### Requirement: Selection-driven dialog refresh and key binding
The framework SHALL bind key Control-P in layout windows to `magic::gencell {} ; raise .params`. It SHALL append the tag `magic::gencell_update %1` to the `select` command. When a top-level `select cell ...` completes while `.params` is in normal (shown) state, `magic::gencell_update` SHALL open the edit dialog for the first instance in the selection. Nested invocations (call depth > 1) SHALL be ignored.
Compatibility: behavioral

(Magic ref: tcltk/toolkit.tcl:107-117,1534-1554)

#### Scenario: Selecting refreshes the dialog
- **GIVEN** the parameter dialog is open
- **WHEN** the user runs `select cell x2`
- **THEN** the dialog SHALL switch to editing `x2`

### Requirement: Toolkit menu hooks
The framework SHALL provide these procedures for PDK menus inside a wrapper window frame F:
- `magic::add_toolkit_menu F text ?library toolkit?`: creates a menu button at `F.titlebar.mbuttons.<library>` with a non-tearoff menu `...<library>.toolmenu`, packed left.
- `magic::add_toolkit_button F text type ?library? ?args?`: adds a menu item that runs `magic::gencell <library>::<type> {} <args>`.
- `magic::add_toolkit_command F text command ?library? ?args?`: adds an item that runs `<command> <args>`.
- `magic::add_toolkit_separator F ?library?`: adds a separator.

F is the layout frame `.layoutN.pane.top` (see `ui-gui-managers`).
Compatibility: behavioral

(Magic ref: tcltk/toolkit.tcl:61-105)

#### Scenario: PDK device menu entry
- **WHEN** `magic::add_toolkit_menu .layout1.pane.top "Devices 1" pdk1` and `magic::add_toolkit_command .layout1.pane.top "nmos (MOSFET)" "magic::gencell sky130::sky130_fd_pr__nfet_01v8" pdk1` run
- **THEN** a `Devices 1` menu SHALL exist whose `nmos (MOSFET)` item opens the new-device dialog for that type

### Requirement: SPICE netlist to layout seeding
`magic::netlist_to_layout <file> <library>` SHALL seed layouts from a SPICE or CDL netlist. If the file does not exist, it SHALL print `No such file <file>` and return. Otherwise it SHALL:
1. Read lines and drop those starting with `*`. For `.cdl` files, a `*.PININFO` line (case-insensitive) becomes `.PININFO`. A line starting with `+` is a continuation: its text after the `+` is appended to the previous kept line, with one space inserted first unless the continuation line itself ends with a space (as in capability `readspice`).
2. Record every `.subckt` name. For each name that equals the current window cell or the file root name, load it. Otherwise try loading it from disk without creating it. If it exists, mark it pre-existing and expand it. If not, create an empty placeholder.
3. Within each `.subckt ... .ends` block, collect element lines whose first letter is one of x m c r l d q (case-insensitive). Lines starting with i v b e are ignored. `.command ... .endc` blocks are skipped. `.global`, `.ic`, `.option` and `.end` are ignored.
4. For each block whose cell was not pre-existing, run the seeding procedure below. Top-level element lines are seeded into the cell named after the file root.
5. Finally, load the first newly created top cell (when there are no top-level elements) and clear the selection.
Compatibility: behavioral

(Magic ref: tcltk/toolkit.tcl:448-639)

#### Scenario: Existing layout is skipped
- **GIVEN** `inv.mag` exists on the search path and the netlist defines `.subckt inv A Y VDD VSS`
- **WHEN** `magic::netlist_to_layout top.spice sky130` runs
- **THEN** `Cell inv already contains layout. Skipping generation.` SHALL be printed, and `inv` SHALL be unchanged

### Requirement: Seeding a subcircuit cell
For cell C with pin list Π and element list E, the seeding procedure SHALL:
1. Load C. Delete instances of child cells that have the `not-found` flag, and delete those cells.
2. Set the box to 0 0 0 0. Create pins in Π that are absent from C's ports; each is a 1 µm × 1 µm square of `m1` at the box, labelled with the pin name (font FreeSans, size 1, rotation 0, offset 0 0, centered, on m1), made a port with index 0, 1, 2, … in creation order. After each pin, the box moves 2 µm south.
3. Remove ports of C that are not in Π: go to the pin, select the area on its layer, and erase labels.
4. Set the box size to 0 and its position to (0, round(3 / cifOutputScale)) internal units.
5. For each element line: tokens up to the first `name=value` are pins. The last pin token is the device type and the first is the instance name. Parameters accept `name='expr'` and `name={expr}`, both stored braced, and `name=value`. A device type starting with a digit or `.` denotes an ideal element: instance letter c/r/l gives type `capacitor`/`resistor`/`inductor` with parameter `value`. Parameter `M` (case-insensitive) is the multiplier.
6. Resolve the type against `<library>`, or `$PDKNAMESPACE` when the library is empty. If `<type>_defaults` exists exactly, use it. Otherwise match case-insensitively and adopt the library's spelling (the part before the first `_`).
7. If an instance of that name already exists with the same cell, skip. If it exists with a different cell, delete it.
8. Otherwise evaluate `magic::gencell <lib>::<type> <inst> -spice <lowercased params...>`. Brackets in the instance name are escaped. On success, move the box right by the instance width plus x-pitch × xhi. On error, place `getcell <type>` instead, name it, array it 1×M if M > 1, and move the box to the instance's right edge.
9. Save C and restore units.
Compatibility: behavioral

(Magic ref: tcltk/toolkit.tcl:119-446)

#### Scenario: Ideal capacitor
- **WHEN** the element line is `C1 a b 10f`
- **THEN** the call SHALL be `magic::gencell <lib>::capacitor C1 -spice value 10f`

#### Scenario: Bus pin name preserved
- **WHEN** Π contains `vout[0]`
- **THEN** the created label text SHALL be exactly `vout[0]` with no backslashes

### Requirement: Native generator equivalence
On a platform where PDK generator scripts cannot run (see Q-SCRIPT-1), Hefesto SHALL still:
- read, display, edit, save and extract cells that carry `library`/`gencell`/`parameters` properties;
- preserve those properties byte-for-byte;
- compute the cell names of this spec exactly.

If Hefesto provides native re-implementations of a PDK's generators, then for every parameter set the generated cell SHALL be geometrically identical (same paint, labels, ports and properties) to the PDK script's output, and SHALL have the identical cell name.
Compatibility: behavioral

(Magic ref: tcltk/toolkit.tcl:1-55)

#### Scenario: Round trip without scripts
- **GIVEN** a layout containing `nfet_XCJNSH` created on a desktop with the PDK script
- **WHEN** it is opened, moved and saved on a platform without scripting
- **THEN** the saved cell SHALL contain the same `library`, `gencell` and `parameters` property values
