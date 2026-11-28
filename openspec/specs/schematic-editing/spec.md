# Schematic Editing

## Purpose
Defines the editing model of Hefesto's schematic and symbol editor (decision P-12): the document and object model, snapping, selection, placing and changing wires, instances, labels, texts and graphics, bus notation in the editor, hierarchy navigation, net highlighting, undo and the clipboard, and how all of this works with a mouse and keyboard on desktop and with touch and Apple Pencil on iPadOS.

The behaviour follows xschem so that users and PDK tutorials written for xschem carry over. Key bindings and dialogs are the interaction surface and MAY differ; the results of operations SHALL match.

**Compatibility adaptation.** This capability is about xschem, not Magic. The compatibility classes in `docs/spec-conventions.md` therefore apply relative to **xschem** (`StefanSchippers/xschem` @ `4142e6d6f1eb5cd0e271bb183f14202af374ce27`, version `3.4.8RC`):
- `exact`: the edited data is identical to what xschem produces for the same operation
- `behavioral`: same result and semantics; interaction MAY differ
- `informative`: xschem detail that Hefesto MAY differ from
- `native`: Hefesto-only, and cites `(Hefesto decision: docs/decisions/product.md#P-12)` or another decision

Traces use `(xschem ref: path/file.c:line)`, relative to the xschem `src/` directory, or `keys.help` for xschem's key binding list. They exist for traceability only. Implementers SHALL work from this spec and SHALL NOT consult the GPL sources (capability `hefesto-licensing`).

Other capabilities own related parts:
- File format, orientation transform and property strings: capability `schematic-file-format`.
- Symbols, templates, instance names and library search: capability `schematic-symbols-and-libraries`.
- Connectivity and net names: capability `schematic-netlisting`.
- Commands behind every action: capability `schematic-command`.
- Platform input mapping: capability `hefesto-input-and-touch`. Window layout: capability `hefesto-app-ux`.

Open questions are in `docs/decisions/open-questions/SCHEM.md` (Q-SCHEM-n).

## Requirements

### Requirement: Document model
Each open schematic or symbol SHALL be a document holding, in order lists: lines, boxes, arcs and polygons per layer, texts, wires and instances, plus the global property strings and header text of capability `schematic-file-format`. The order of each list SHALL be the creation order, except as changed by Requirement "Object order". Every editing operation SHALL go through the command API (capability `schematic-command`, decision X-3), so that it can be scripted, logged and replayed. A schematic and a symbol with the same base name SHALL be separate documents.

Coordinates, angles and scales given to an editing operation SHALL be finite numbers; `inf`, `nan` or a number out of the `double` range (such as `1e999`) SHALL be refused as a usage error and change nothing, in both modes (review of the M8 round-4 editing code; Q-SCHEM-48, approved by the user 2026-11-25). A rotation SHALL be 0 to 3 and a flip 0 or 1 for instances, labels and texts alike. The selection of a document SHALL only name objects that exist: when the document changes under it (for example by `schematic reload`), selected objects that no longer exist SHALL be dropped before the next operation runs.

Compatibility: behavioral

(xschem ref: xschem.h:960-1210, save.c:3223-3281)

#### Scenario: Created objects appended
- **GIVEN** a schematic with wires w1 and w2
- **WHEN** a wire w3 is drawn and the file is saved
- **THEN** the `N` records SHALL be in the order w1, w2, w3

#### Scenario: Non-finite coordinate refused
- **WHEN** a script runs `schematic wire 0 0 1e999 0`
- **THEN** the option SHALL fail with a usage error and no wire SHALL be added

#### Scenario: Selection after reload
- **GIVEN** a schematic with 10 wires of which wire 9 is selected
- **WHEN** the file is changed on disk to hold 2 wires, the document is reloaded and the user deletes the selection
- **THEN** nothing SHALL be deleted and the selection SHALL be empty

### Requirement: Snap and grid
All placed and moved coordinates SHALL be snapped to the nearest multiple of the snap value, default 10 units. The visible grid SHALL default to 20 units. The user SHALL be able to halve the snap, double it and set it to any positive value. Snapping SHALL apply to the pointer position; objects SHALL move by snapped deltas, so an off-grid object stays off-grid by the same amount unless the user runs "align to grid", which SHALL round every coordinate of the selected objects to the nearest multiple of the snap.

Compatibility: behavioral

(xschem ref: xschem.h:160-163, save.c:5960-6036, keys.help)

#### Scenario: Halve snap
- **GIVEN** the default snap of 10
- **WHEN** the user halves the snap and places a wire endpoint at pointer x = 7.4
- **THEN** the endpoint SHALL be at x = 5

### Requirement: Selection
Selection SHALL behave as follows:
- Clicking an object SHALL select it and clear the previous selection; adding a modifier (Shift on desktop) SHALL add it instead.
- Clicking empty space SHALL clear the selection.
- Dragging a rectangle SHALL select every object entirely inside it. If the "select touched" setting is on (default on), dragging right-to-left SHALL also select objects that the rectangle only touches.
- Hefesto SHALL be able to remove one object or a rectangle of objects from the selection (Ctrl-click or a Ctrl-drag on desktop; `schematic select ... -remove` in scripts). A `schematic select` without `-remove` adds to the selection; `schematic select clear` empties it.
- "Select all" SHALL select every object.
- "Select connected" SHALL select every wire, label and port on the net of the selected wire, label or pin, across junctions; a variant (`schematic select connected -stopjunction`) SHALL stop at wire junctions: starting from the selected objects it adds the wires that touch them end to end and stops at any point where three or more wire ends meet or a wire end meets another wire's interior, without adding the wires beyond it.
- A selected instance SHALL optionally bring its floater texts (capability `schematic-symbols-and-libraries`) into the selection.
- Only objects on visible layers SHALL be selectable.

Hit testing on desktop SHALL use a tolerance of a few screen pixels around the pointer; on touch, the tolerance SHALL scale with the contact size (capability `hefesto-input-and-touch`).

Compatibility: behavioral

(xschem ref: select.c:93-200, select.c:1387-1500, keys.help)

#### Scenario: Select connected stops at a junction
- **GIVEN** wires W1 (0,0)-(50,0), W2 (50,0)-(100,0) and W3 (50,0)-(50,50), with W1 selected
- **WHEN** `schematic select connected -stopjunction` and, after `schematic select clear` and selecting W1 again, `schematic select connected` are executed
- **THEN** the first SHALL leave only W1 selected, and the second SHALL select W1, W2 and W3

#### Scenario: Right-to-left touch selection
- **GIVEN** a wire from (0,0) to (100,0) and the "select touched" setting on
- **WHEN** the user drags a rectangle from (60,−10) to (40,10)
- **THEN** the wire SHALL be selected

### Requirement: Wires
Placing a wire SHALL start at the snapped pointer position and end at a second snapped position. The route between the two points SHALL be one of: horizontal then vertical, vertical then horizontal, or a single oblique segment; a key or gesture SHALL cycle these, and an "orthogonal routing" setting SHALL choose H-V or V-H automatically from the pointer motion. A Manhattan route SHALL create two `N` records (one if the points share a coordinate). Wire placement SHALL continue from the last endpoint until the user ends it. A variant SHALL snap the endpoint to the nearest pin or wire endpoint.

The editor SHALL show a junction dot at a wire endpoint where, counting each other wire whose interior holds the endpoint as 2 and each other wire with a coincident endpoint as 1, the count is greater than 1.

The editor SHALL provide:
- **Break wires**: split selected wires at every point where another wire's endpoint or an instance pin touches their interior.
- **Cut wire**: split a wire at the pointer, snapped or not.
- **Auto-trim** (setting, default off): after each wire edit, merge collinear wires that overlap or touch end to end, and split wires at T junctions.

Compatibility: behavioral

(xschem ref: check.c:48-120, check.c:154-600, xschem.tcl:11988, xschem.tcl:11993, keys.help)

#### Scenario: Manhattan wire
- **GIVEN** H-V routing
- **WHEN** a wire is placed from (0,0) to (50,30)
- **THEN** two wires SHALL be created: (0,0)-(50,0) and (50,0)-(50,30)

#### Scenario: T junction dot
- **GIVEN** wires (0,0)-(100,0) and (50,0)-(50,50)
- **WHEN** they are drawn
- **THEN** a junction dot SHALL be drawn at (50,0)

### Requirement: Placing instances
The user SHALL place a symbol by choosing it from a library browser that lists the folders of the search path (capability `schematic-symbols-and-libraries`) and shows a preview. Placement SHALL:
- create a `C` record at the snapped pointer position with rotation 0 and flip 0, unless the user rotated or flipped it before clicking
- set the properties from the symbol template and make the name unique (capability `schematic-symbols-and-libraries`, Requirement "Instance names")
- write the symbol reference in the shortest form (capability `schematic-symbols-and-libraries`, Requirement "Symbol references")
- stay in placement mode for further copies if the "persistent command" setting is on (default off)

Replacing the symbol of selected instances SHALL change only the reference and keep the properties.

Compatibility: behavioral

(xschem ref: actions.c:1586-1700, token.c:800-858, xschem.tcl:11985, keys.help)

#### Scenario: Place two resistors
- **GIVEN** an empty schematic
- **WHEN** the user places `res.sym` twice
- **THEN** the instances SHALL be named `R1` and `R2`, each with `value=1k` and the other template attributes

### Requirement: Move, copy and stretch
- **Move** SHALL translate the selected objects by the snapped pointer displacement. A constrained variant SHALL allow only horizontal or only vertical motion.
- **Stretch move** SHALL also move the endpoints of unselected wires that touch the moved objects' pins or wire endpoints, so connections are kept. A setting SHALL swap plain and stretch move.
- **Move with wire insertion** SHALL insert new wires to keep connections where moved instance pins separate from pins or wires they touched.
- **Copy** SHALL duplicate the selected objects, displaced, with instance names made unique. A variant SHALL insert wires like the move variant.
- Rotation and flip SHALL be available during a move or copy and SHALL apply around the pointer position.

Compatibility: behavioral

(xschem ref: move.c:608-1150, move.c:1151-1596, keys.help)

#### Scenario: Stretch keeps connection
- **GIVEN** a resistor whose pin at (0,−30) is the endpoint of a wire (0,−30)-(0,−100), with only the resistor selected
- **WHEN** the user stretch-moves it by (20,0)
- **THEN** the wire SHALL become (0,−100)-(20,−30) or be re-routed orthogonally, and SHALL still touch the pin

### Requirement: Rotate and flip
Rotation SHALL turn the selected objects by 90° clockwise on screen around the snapped pointer position (pivot). A command that names a pivot (`schematic rotate <x> <y>`, `schematic flip <x> <y>`) SHALL use that point. A command that names none and is not `-inplace` SHALL use the snapped pointer position in the editor; in a run with no pointer (a script without the editor, the headless CLI) xschem uses its last snapped pointer position, which is the origin (0, 0) when no pointer event has happened, and Hefesto SHALL do the same with `xschem-compat` ON, and SHALL use the centre of the selection's bounding box, snapped, with `xschem-compat` OFF (standing rule for xschem behaviour no Q-row covers; reported to the user 2026-11-26). A variant SHALL rotate each object around its own anchor (instance origin, text anchor, wire or line first point, box corner). Horizontal flip SHALL mirror around the vertical line through the pivot; vertical flip SHALL be a horizontal flip combined with two rotation steps. For an instance, rotating SHALL add 1 (mod 4) to its rotation when its flip is 0, and SHALL change it consistently with the transform of capability `schematic-file-format` when its flip is 1, so that the drawn result turns clockwise; the origin SHALL be rotated around the pivot.

Compatibility: exact

(xschem ref: move.c:1151-1596, callback.c:3640-3680, xschem.h:351-358)

#### Scenario: Rotate resistor in place
- **GIVEN** an instance at (100,100) with rotation 0, flip 0
- **WHEN** it is rotated around its anchor
- **THEN** its record SHALL be `C {res.sym} 100 100 1 0 {...}`

#### Scenario: Rotate wire around pivot
- **GIVEN** a wire (0,0)-(10,0) and the pointer at (0,0)
- **WHEN** it is rotated
- **THEN** it SHALL become (0,0)-(0,10)

#### Scenario: Script rotation without a pivot
- **GIVEN** a script run with no editor, and a selected wire (100,100)-(120,100)
- **WHEN** `schematic rotate` is executed
- **THEN** with `xschem-compat` ON the pivot SHALL be (0,0) and the wire SHALL become (-100,100)-(-100,120); with `xschem-compat` OFF the pivot SHALL be (110,100) and the wire SHALL become (110,90)-(110,110)

### Requirement: Labels and ports
The editor SHALL place net labels (`lab_pin.sym`, `lab_wire.sym`) and ports (`ipin.sym`, `opin.sym`, `iopin.sym`) from the system library with a `lab` value entered by the user. It SHALL provide:
- **Attach labels to instance**: for the selected instances, place one `lab_pin` at each unconnected pin, named after the pin (optionally with a prefix), oriented away from the symbol.
- **Create symbol pins from schematic ports**: in a symbol document, add pin boxes on layer 5 named after the ports of the matching schematic.
- **Make symbol from schematic**: generate `<name>.sym` with a box outline, one pin per port (inputs left, outputs right, inouts right), `type=subcircuit`, `format="@name @pinlist @symname"` and `template="name=x1"`. It SHALL NOT overwrite an existing symbol without confirmation.
- **Make schematic from symbol**: generate `<name>.sch` with one port per symbol pin.

A pin or port name with a blank or `;` SHALL be written quoted, and a `"` or `\` in it escaped, so that the made file reads back the same name; so SHALL the `lab` of a placed label or port and the `lab` values a netlist run stores (capability `schematic-file-format`, Requirement "Property strings"). Making a symbol or a schematic SHALL NOT write into the PDK store or the system library, even when overwriting was confirmed (Requirement "Read-only libraries").

Compatibility: behavioral

(xschem ref: save.c:3776-3822, save.c:5658-5810, actions.c:1371-1585, xschem.tcl:6560-6600, keys.help)

#### Scenario: Make symbol
- **GIVEN** `amp.sch` with ports `INP` (ipin), `INM` (ipin), `OUT` (opin)
- **WHEN** the user makes a symbol
- **THEN** `amp.sym` SHALL have three pins `INP`, `INM`, `OUT` with `dir` `in`, `in`, `out`, and SHALL netlist as `x1 <nets> amp`

#### Scenario: Label name with a quote
- **WHEN** `schematic label 0 0 {a"b}` is executed and the schematic is saved and read again
- **THEN** the label's decoded `lab` value SHALL be `a"b`

### Requirement: Bus editing
Wires and labels SHALL accept the bus notation of capability `schematic-netlisting` (for example `DATA[7:0]`, `A,B,C`, `2*X`). A wire whose net name expands to more than one bit SHALL get `bus=1` and be drawn thick; when a net stops being a bus, `bus` SHALL be removed. Instance names MAY be vectors (`x1[3:0]`); each element is netlisted as a separate instance (capability `schematic-netlisting`, Requirement "Element line post-processing"). The editor SHALL warn when the widths of a pin, its instance vector and the attached net are inconsistent (net width not equal to pin width or to pin width × instance width).

Compatibility: behavioral

(xschem ref: netlist.c:1663-1712, hilight.c:553-700)

#### Scenario: Bus wire drawn thick
- **GIVEN** a wire attached to a label `lab=D[3:0]`
- **WHEN** connectivity is updated
- **THEN** the wire SHALL get `bus=1` and be drawn thick

### Requirement: Property editing
Each object SHALL have an editable property string (capability `schematic-file-format`, Requirement "Property strings"). The property editor SHALL:
- show the instance property string as text, with the symbol's `template` available as reference
- offer a field view for `key=value` pairs that writes back through the property update rules
- allow the instance name, symbol reference and the text of `T` objects to be edited
- keep instance names unique after editing (capability `schematic-symbols-and-libraries`, Requirement "Instance names")
- apply a change to all selected objects of the same kind when the user asks for it

Editing the global properties (`K`, `S`, ... records) and the header text SHALL be possible.

**Field view** (specified in M8 round 7 for task 9.2; both modes of `xschem-compat`). The field view of the selected instance (or wire) SHALL show one row per attribute of its property string, in the order the attributes appear in it, each row holding the key and the decoded value (quotes removed, escapes resolved, as `schematic getprop instance <name> <key>` returns it); an attribute given twice shows once, with the value that `getprop` returns. Changing a row's value SHALL run `schematic setprop instance <name> <key> <value>` (`setprop wire <n> ...` for a wire), and clearing it SHALL run the same command without `<value>`, which removes the attribute; each change is one command line in the command log and one undo step. Changing the `name` row renames the instance, made unique as `setprop` makes it. A row added with a new key SHALL run `setprop` with that key, which appends the attribute. With "apply to all selected" chosen, the change SHALL run `schematic setprop selection <key> [<value>]` instead (one undo step for all). The field view SHALL be refreshed after every command that changes the document.

**Scope in M8.** The field view, "apply to all selected" and the `schematic setprop schematic` command are M8. The whole-string text view of the property string with the symbol's `template` shown beside it, and the desktop dialogs for the global properties and the header text, are proposed to move past M8 (proposed Q-SCHEM-68, awaiting the user); until then they are reached through `schematic getprop`/`setprop` in the command bar.

Compatibility: behavioral

(xschem ref: editprop.c:236-260, xschem.tcl:7553-7700)

#### Scenario: Change value of two resistors
- **GIVEN** resistors `R1` and `R2` both selected
- **WHEN** the user sets `value=10k` for all selected
- **THEN** both property strings SHALL have `value=10k` and keep their other attributes

#### Scenario: Field view edits through setprop
- **GIVEN** the instance `R1` with the property string `name=R1 value=1k footprint="0603 smd"` shown in the field view on desktop, with `xschem-compat` ON or OFF
- **WHEN** the user changes the `value` row to `2k`, then clears the `footprint` row
- **THEN** the field view SHALL first show the rows `name` `R1`, `value` `1k`, `footprint` `0603 smd`; the command lines `schematic setprop instance R1 value 2k` and `schematic setprop instance R1 footprint` SHALL be run and logged; the property string SHALL be `name=R1 value=2k`; and two undo steps SHALL be recorded

### Requirement: Graphics and text
The editor SHALL draw lines, boxes, polygons (ended by placing the last point on the first, or by a double click / double tap), arcs and circles on the current layer (default 4, as in xschem; changed by the layer chooser or `schematic layer`, and used by `schematic line`/`box`/`arc`/`polygon` when `-layer` is not given), and texts with size, rotation, alignment and the text attributes of capability `schematic-file-format`. Symbol documents SHALL add pins as boxes on layer 5 with `name` and `dir`. Images and graphs embedded as boxes SHALL be preserved and drawn (graphs: capability `schematic-simulation-integration`).

Compatibility: behavioral

(xschem ref: keys.help)

#### Scenario: Default current layer
- **GIVEN** a new document whose current layer was never changed
- **WHEN** `schematic line 0 0 10 0` is executed
- **THEN** the line SHALL be on layer 4

#### Scenario: Closed polygon
- **WHEN** the user places points (0,0), (20,0), (20,20) and then (0,0)
- **THEN** a closed polygon `P <layer> 4 0 0 20 0 20 20 0 0 {}` SHALL be created

### Requirement: Object order
The editor SHALL let the user change the position of a selected object in its list (xschem's "change element order"), which changes the file order, the netlist instance order and the automatic net and instance numbering.

The command is `schematic order <index>` (capability `schematic-command`): the selected objects of each kind move, as one block keeping their order, to position `<index>` (from 0) of their kind's list, or to its end when `<index>` is past it; they stay selected. It is one undo step (none when nothing moves) and is refused on a read-only document (Requirement "Read-only libraries").

Compatibility: behavioral

(xschem ref: editprop.c:1818-1900, keys.help)

#### Scenario: Reorder instance
- **GIVEN** instances `R1` (index 0) and `R2` (index 1)
- **WHEN** `R2` is moved to index 0
- **THEN** the saved file and the netlist SHALL list `R2` before `R1`

#### Scenario: Reorder a block of wires
- **GIVEN** wires 0 to 4 with wires 1 and 3 selected
- **WHEN** `schematic order 99` is executed
- **THEN** the former wires 1 and 3 SHALL be wires 3 and 4, in that order, and stay selected, and one `schematic undo` SHALL restore the former order

### Requirement: Clipboard
Copy SHALL put the selected objects on the clipboard as text in the record format of capability `schematic-file-format` (a `v` record, the global records and the selected objects' records), with coordinates relative to the selection's anchor. Paste SHALL insert the records at the pointer, make instance names unique and select the pasted objects. Text from xschem's clipboard SHALL paste into Hefesto and the reverse. Merging a file SHALL insert its objects like a paste.

The commands are `schematic clipboard copy`, `schematic clipboard [set <text>]`, `schematic paste <x> <y>` and `schematic merge <file> [<x> <y>]` (capability `schematic-command`). The selection's anchor SHALL be the lower-left corner of the selection's bounding box, snapped to the snap; copied instances SHALL carry their embedded symbols. The clipboard text is session state, exchanged with the system clipboard by the front end. Paste and merge SHALL take only the objects (and the embedded symbols the document does not have yet), not the global records, keep symbol references as written, select the inserted objects, be one undo step each and be refused on a read-only document; copying SHALL be allowed on any document. Pasting a clipboard that holds no schematic objects SHALL fail with `schematic: the clipboard holds no schematic objects`.

An embedded symbol of the inserted objects SHALL NOT change the instances the document already has: when an instance of the document already uses the same symbol reference (from a library file or its own embedded block), the inserted block SHALL NOT be added and the inserted instances SHALL resolve that reference as the document's existing instances do, in both modes (Q-SCHEM-51 option (a), approved by the user 2026-11-25).

**System clipboard size limit** (crafted-input resource limit, approved under the user's standing rule of 2026-11-25, both modes): the desktop front end's Paste SHALL refuse system clipboard text larger than 16 MiB (16,777,216 bytes): it SHALL run no command and SHALL show the message `Paste: the clipboard text is too large (<n> bytes, limit 16777216)`. xschem has no such bound. The console SHALL echo a clipboard command line (`schematic clipboard set <text>`, and a Copy's result) shortened to its first line and its size in bytes, not the whole text.

Compatibility: behavioral

(xschem ref: save.c:6038-6120, paste.c:1-478)

#### Scenario: Paste from xschem
- **GIVEN** the system clipboard holds the text of an xschem copy containing `C {res.sym} 0 0 0 0 {name=R1}`
- **WHEN** the user pastes into a schematic that already has `R1`
- **THEN** a resistor SHALL be inserted at the pointer and named `R2`

#### Scenario: Copy and paste
- **GIVEN** a wire, a resistor `R1` and a text selected in a schematic
- **WHEN** `schematic clipboard copy` and then `schematic paste 500 500` are executed
- **THEN** the clipboard text SHALL hold the three records with the anchor at the origin, the pasted copies SHALL be moved by (500, 500), the resistor SHALL be named `R2`, the copies SHALL be selected, and one `schematic undo` SHALL remove them

#### Scenario: Pasted embedded symbol does not redefine the document's symbol
- **GIVEN** a schematic with an instance of the library `res.sym`, and a clipboard holding an instance of `res.sym` with its embedded block
- **WHEN** `schematic paste 0 0` is executed
- **THEN** the document SHALL get no embedded block for `res.sym`, and every instance of `res.sym`, the existing one included, SHALL resolve to the library file (Q-SCHEM-51)

#### Scenario: System clipboard text above the size limit
- **GIVEN** the system clipboard holds 16,777,217 bytes of text
- **WHEN** the user pastes into a schematic on desktop
- **THEN** no command SHALL run, the document SHALL be unchanged, and the message `Paste: the clipboard text is too large (16777217 bytes, limit 16777216)` SHALL be shown, in both modes

#### Scenario: Large clipboard text in the Console
- **GIVEN** the system clipboard holds 1,000 lines of schematic records
- **WHEN** the user pastes them on desktop
- **THEN** the Console SHALL show the `schematic clipboard set` line as its first line followed by its size in bytes, not one row per record

#### Scenario: Merge a file
- **GIVEN** `part.sch` holding a wire and an instance `R1`, and a schematic that already has `R1`
- **WHEN** `schematic merge part.sch 10 20` is executed
- **THEN** the wire and the instance SHALL be inserted moved by (10, 20), the instance named `R2`, and both selected

### Requirement: Hierarchy navigation
The user SHALL be able to:
- **descend** into the schematic of a selected `subcircuit` or `primitive` instance (capability `schematic-netlisting`, Requirement "Schematic resolution")
- **descend into the symbol** of any selected instance
- **ascend** back to the parent, restoring the parent's view

When descending into a vector instance (`x1[3:0]`), the user SHALL choose which element; elements are numbered 1 from the left, negative numbers count from the right, and an invalid number SHALL select element 1. Descending SHALL be limited to 40 levels. If the parent has unsaved changes, Hefesto SHALL keep them in memory (the parent stays open) instead of asking to save. A schematic reached by descending SHALL be editable; edits SHALL apply to the file and therefore to every instance of it.

The schematic descended into SHALL be the one capability `schematic-netlisting` resolves for the instance (Requirement "Schematic resolution"), in both modes of `xschem-compat`: the instance's `schematic` attribute with its `@` tokens substituted from the instance's attributes, then `@symname` replaced, a `tcleval(...)` value evaluated, and the result resolved as a symbol reference; otherwise the symbol's `schematic` attribute; otherwise the symbol's file with `.sch` in place of its extension. The netlist and the editor SHALL therefore always open the same schematic for an instance. A `schematic` value that resolves to no file SHALL be handled as rule 4 of that requirement (the symbol's own schematic), so descending and netlisting agree (specified in M8 round 7 for task 9.2).

Compatibility: behavioral

(xschem ref: actions.c:2639-2880, actions.c:2882-2990, xschem.h:200)

#### Scenario: Descend into vector element
- **GIVEN** an instance named `xinv[3:0]`
- **WHEN** the user descends and chooses element −1
- **THEN** the hierarchy path SHALL end in `.xinv[0].`

#### Scenario: Descend follows the substituted schematic attribute
- **GIVEN** the symbol `amp.sym` (whose own schematic is `amp.sch`) and its instance `x1` with `flavor=lvt schematic=amp_@flavor.sch`, the files `amp.sch` and `amp_lvt.sch` next to `amp.sym`, with `xschem-compat` ON or OFF
- **WHEN** the user selects `x1` and descends
- **THEN** the document descended into SHALL be `amp_lvt.sch`, the schematic the netlist writes as the definition of `x1`'s subcircuit

### Requirement: Hierarchy path
Each displayed schematic SHALL have a hierarchy path: `.` for the top, and on each descent the parent path followed by the chosen element of the instance name and a `.`. For example `.x1.xinv[2].`. While another document is active the path reads as `.`; reading the path SHALL NOT discard the descents, so activating the descended document again shows its path again (only descending or ascending from another document starts a new path). The path SHALL be used for `@path` (without the leading `.`), for highlight propagation and for simulation cross-probing (capability `schematic-simulation-integration`).

Compatibility: exact

(xschem ref: actions.c:2780-2822, token.c:4697-4707)

#### Scenario: Two-level path
- **GIVEN** the top schematic has instance `x1`, whose schematic has instance `x2`
- **WHEN** the user descends into `x1` and then `x2`
- **THEN** the hierarchy path SHALL be `.x1.x2.` and `@path` SHALL be `x1.x2.`

### Requirement: Net highlighting
The user SHALL be able to highlight the nets of selected wires, labels or pins, highlight a net by name, highlight instances, and clear one or all highlights. Each new highlight SHALL take the next colour in a cycle when the "increment highlight colour" setting is on (default on). Highlights SHALL propagate through the hierarchy: descending into an instance SHALL highlight the child nets connected to highlighted parent nets through the instance's pins, and highlights made in a child SHALL be shown on the connected parent nets when ascending. Global nets SHALL be highlighted at every level. Highlights SHALL NOT be saved in the file.

**Vector instances** (specified in M8 round 7 for task 9.2; both modes of `xschem-compat`). An element of a vector instance connects to each pin the names that capability `schematic-netlisting` gives it (Requirement "Element line post-processing", paragraph "Vector elements": with pin width w, k names on the pin's parent net and the element at position i of the instance's expanded name list, counting from 1 at the leftmost name, the names at positions ((l + w·(i−1)) mod k) + 1 for l = 0..w−1). Highlight propagation through an element SHALL use exactly these names: descending into element i highlights a child net when one of the names connected to its pin through element i is highlighted; ascending from element i highlights, on the parent, those names (each bit of a parent bus separately), and no name connected only through other elements.

Compatibility: behavioral

(xschem ref: hilight.c:1466-1700, hilight.c:1974-2100, xschem.tcl:11967)

#### Scenario: Highlight crosses into child
- **GIVEN** net `IN` highlighted at top level, connected to pin `A` of instance `x1`
- **WHEN** the user descends into `x1`
- **THEN** the child net `A` SHALL be highlighted in the same colour

#### Scenario: Highlight of a vector element shown on the parent bit
- **GIVEN** the vector instance `xinv[3:0]` of `inv.sym` (pins `A` and `Y`, width 1) with `A` on the parent bus `in[3:0]` and `Y` on `out[3:0]`, with `xschem-compat` ON or OFF
- **WHEN** the user descends into element 2 (path `.xinv[2].`), highlights the child net `Y`, and ascends
- **THEN** on the parent only the bit `out[2]` SHALL be highlighted, not `out[3]`, `out[1]` or `out[0]` and not the whole bus `out[3:0]`

### Requirement: Undo and redo
Every command that changes a document SHALL be one undo step; a compound gesture (for example a move with inserted wires) SHALL be one step. Undo and redo SHALL restore the full document state including properties, object order and the modified flag. Hefesto SHALL keep at least 80 undo steps per document (xschem's limit), up to 200, as long as the document states they hold take at most 512 MiB (`kUndoMaxBytes`); past that the oldest steps SHALL be dropped, and the newest step SHALL always be kept (Hefesto's bound, both modes; consecutive steps share the document state between them, so a history of n steps holds n + 1 states; Q-SCHEM-47, approved by the user 2026-11-25). Netlisting, highlighting, selection and view changes SHALL NOT create undo steps.

In xschem the undo history is per window and is cleared when another schematic is loaded into it, including on descend, unless the `preserve_undo` setting is on (informative). Hefesto SHALL keep a separate undo history per open document for the whole session, so descending and ascending never lose undo history.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12; xschem ref: xschem.h:278, in_memory_undo.c:246-460, xschem.tcl:12047)

#### Scenario: Undo history byte bound
- **GIVEN** a document whose undo history holds 6 steps and a byte bound of about three document states
- **WHEN** another edit is made
- **THEN** only the newest steps that fit the bound SHALL be kept (at least one), and undoing SHALL stop after them

#### Scenario: Undo after ascending
- **GIVEN** the user moved a wire in `top.sch`, descended into `x1`, and ascended again
- **WHEN** the user undoes
- **THEN** the wire move SHALL be undone

### Requirement: Desktop and touch interaction
On Linux and macOS with a mouse and keyboard, Hefesto SHALL offer xschem's default bindings for the operations of this capability (for example `w` wire, `m` move, `c` copy, `R` rotate, `F` flip, `e` descend, `Ctrl+e` ascend, `q` edit properties, `k` highlight, `u` undo, `U` redo, `Insert` place symbol, `n` netlist), selectable as a binding set, and SHALL let users rebind them (capability `hefesto-input-and-touch`). On iPadOS:
- one-finger drag in select mode SHALL select by rectangle; a tap SHALL select; a second finger tap SHALL add to the selection
- in wire mode, a drag SHALL place a wire with the current routing; lifting the finger SHALL end the segment and a double tap SHALL end placement
- long press SHALL open a context menu with move, copy, rotate, flip, delete, edit properties, descend and highlight
- Apple Pencil SHALL place points precisely with the pointer snap, and a hover preview SHALL be shown where supported
- a floating tool palette SHALL offer the select, wire, place-symbol, move, copy, rotate, flip, delete, edit-properties, descend, ascend, highlight, undo, redo and netlist operations, so that every common operation is one tap away without a keyboard
- every operation SHALL also be reachable from the command bar (capability `schematic-command`)

The gesture set and the palette are both offered. The gestures are provisional until a usability test of wire drawing with a finger and with Apple Pencil, which SHALL run before the iPad scope freezes for M14; it does not gate M8 desktop work. Desktop bindings SHALL stay xschem-style by default and rebindable (Q-SCHEM-24, `Resolution: platform`).

On desktop, the `R` and `F` keys SHALL name the snapped pointer position (the view's centre before the pointer came over the canvas) as the pivot of `schematic rotate` and `schematic flip` (Requirement "Rotate and flip").

**Copy and Paste keys** (Q-SCHEM-66 option (a), approved by the user on 2026-11-27; both modes of `xschem-compat`). Copy and Paste with the system clipboard (Requirement "Clipboard") SHALL be Command-C and Command-V on macOS, where Ctrl-C stays the interrupt of a running netlist; off macOS (Linux, Windows) they SHALL be Ctrl-C and Ctrl-V, where Ctrl-C interrupts the netlist run while one is going and copies otherwise. A binding of these chords in the key table comes first, so they are rebindable; Ctrl-C while it interrupts is not.

**Key table** (Q-SCHEM-67 option (a), approved by the user on 2026-11-27). The desktop bindings SHALL be the `schematic` table of the core `macro` command (capability `ui-command-dispatch`, Requirement "macro and imacro commands"), in both modes of `xschem-compat` and of `magic-compat`: `macro schematic <key> "<text>"` lines of the key-binding sheet, of startup files and of scripts SHALL bind a key of the schematic canvas to a command line or to an editor action (`wire`, `move`, `copy`, `properties`, `place`, `descend`); keys the table does not bind keep the defaults above, and a binding a startup file made SHALL be kept. Magic 8.3.694 ignores `macro schematic ...` (it has no such client), so with `magic-compat` ON this is a recorded Magic difference. The key-binding sheet's Reset of the `schematic` table SHALL restore these defaults.

**Schematic tab on desktop** (M8 round 7; Hefesto behaviour, no xschem counterpart; both modes). While the Schematic tab is shown, the layout docks SHALL be hidden, so the schematic view takes the window between the top bar and the status bar; they SHALL come back with the Layout tab. Texts on the schematic canvas SHALL be drawn at their size scaled by the view's zoom, so they grow and shrink with the drawing; a text too small to read at the current zoom MAY be left out. The window command `tab ?layout|schematic|3d?` SHALL show the Layout, Schematic or 3D view as a click on its tab in the top bar does (also when a narrow top bar hides the tabs), and with no argument SHALL return the name of the view shown; `tab 3d` SHALL raise an error when the 3D view is not open. After an `--input` script, the desktop application SHALL print, while the Schematic tab is shown, one line `hefesto: schematic tab <breadcrumb>, <n> instances, <k> pending, canvas <w> x <h>` on stderr: the breadcrumb, the number of instances drawn, the number of symbols still being resolved, and the canvas size in pixels (used by the CI capture step; informative otherwise).

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-5; xschem ref: keys.help)

#### Scenario: Rotate key names the pointer as pivot
- **GIVEN** a schematic on desktop with the pointer over the canvas at schematic position (123, -47)
- **WHEN** the user presses `R`
- **THEN** the command line `schematic rotate 120 -50` SHALL be run

#### Scenario: Copy and Paste keys
- **GIVEN** a selection in a schematic on desktop
- **WHEN** the user presses Command-C on macOS, or Ctrl-C on Linux with no netlist running
- **THEN** `schematic clipboard copy` SHALL run and its text SHALL be placed on the system clipboard; Ctrl-C on Linux while a netlist runs SHALL interrupt it instead (Q-SCHEM-66)

#### Scenario: Rebind a key through the schematic table
- **GIVEN** the Schematic tab on desktop
- **WHEN** `macro schematic z "schematic zoom_full"` is executed, then `z` is pressed on the canvas
- **THEN** `schematic zoom_full` SHALL be run, with `magic-compat` ON or OFF (Q-SCHEM-67)

#### Scenario: Ctrl-C on macOS during a netlist run
- **GIVEN** a netlist run going in the Schematic tab on macOS, with a selection in the schematic
- **WHEN** the user presses Ctrl-C, and after the run has stopped presses Command-C
- **THEN** Ctrl-C SHALL interrupt the run and SHALL NOT copy, and Command-C SHALL run `schematic clipboard copy` (Q-SCHEM-66)

#### Scenario: Reset of the schematic key table
- **GIVEN** `macro schematic w "schematic zoom_full"` executed in the Schematic tab on desktop
- **WHEN** the key-binding sheet's Reset is used for the `schematic` table, then `w` is pressed on the canvas
- **THEN** the wire tool SHALL start, as the default binding of `w` does (Q-SCHEM-67)

#### Scenario: Show the Schematic tab by command
- **GIVEN** the desktop application with a schematic open and the Layout tab shown
- **WHEN** the command `tab schematic` runs, and then `tab` with no argument
- **THEN** the Schematic tab SHALL be shown with the layout docks hidden, and `tab` SHALL return `schematic`

#### Scenario: Rotate on iPad
- **GIVEN** an instance selected on iPad with no keyboard
- **WHEN** the user long-presses it and chooses Rotate
- **THEN** the same rotate command SHALL be issued as the `R` key issues on desktop

#### Scenario: Wire from the palette on iPad
- **GIVEN** a schematic open on iPad with no keyboard
- **WHEN** the user taps the wire tool in the floating palette and drags from one pin to another
- **THEN** the same wire command SHALL be issued as the `w` key and two clicks issue on desktop

### Requirement: Read-only libraries
Files in the PDK store and the system library SHALL open read-only. Edits to them SHALL be refused with a message offering "Save as" to a user folder. Descending into a PDK cell SHALL be allowed for viewing.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-14)

#### Scenario: Edit PDK symbol
- **GIVEN** `sky130_fd_pr/nfet_01v8.sym` opened from the PDK store
- **WHEN** the user tries to move a line
- **THEN** the move SHALL be refused and "Save as" SHALL be offered

#### Scenario: Make symbol in the PDK store
- **GIVEN** a schematic `cell.sch` opened from the PDK store, next to `cell.sym`
- **WHEN** a script runs `schematic symbol make -force`
- **THEN** the option SHALL fail with `SCHEM-EDIT-001` and `cell.sym` SHALL be unchanged
