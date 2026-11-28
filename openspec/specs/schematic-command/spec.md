# Schematic Command

## Purpose
Defines the `schematic` command: the command-API surface (capability `hefesto-architecture`, Requirement "Single command API") through which the GUI, touch gestures, key bindings, scripts and the headless CLI do everything related to schematics and symbols (decision P-12). It also defines the `xschem` compatibility command that lets xschem Tcl snippets found in PDK libraries and testbenches run unchanged in the sandboxed Tcl interpreter (capability `hefesto-scripting`).

The behaviour behind each subcommand is specified in capabilities `schematic-file-format`, `schematic-symbols-and-libraries`, `schematic-editing`, `schematic-netlisting` and `schematic-simulation-integration`. This capability fixes only syntax, return values and errors.

**Compatibility adaptation.** This capability is about xschem, not Magic. The compatibility classes in `docs/spec-conventions.md` therefore apply relative to **xschem** (`StefanSchippers/xschem` @ `4142e6d6f1eb5cd0e271bb183f14202af374ce27`, version `3.4.8RC`):
- `exact`: same arguments and results as xschem's `xschem <subcommand>` Tcl command
- `behavioral`: same effect; syntax or result format MAY differ
- `informative`: xschem detail that Hefesto MAY differ from
- `native`: Hefesto-only, and cites `(Hefesto decision: docs/decisions/product.md#P-12)` or another decision

Traces use `(xschem ref: path/file.c:line)`, relative to the xschem `src/` directory. They exist for traceability only. Implementers SHALL work from this spec and SHALL NOT consult the GPL sources (capability `hefesto-licensing`).

Open questions are in `docs/decisions/open-questions/SCHEM.md` (Q-SCHEM-n).

## Requirements

### Requirement: Command and option matching
The command SHALL be `schematic <option> [args…]`. `<option>` and every keyword argument SHALL be matched case-insensitively with unique-prefix matching, as other Hefesto commands are. These cases SHALL fail without side effects:

| Case | Error |
|---|---|
| Ambiguous prefix | `schematic: ambiguous option "<w>"` |
| Unknown word | `schematic: unknown option "<w>". Use "schematic help".` |
| Missing or extra arguments | `Usage: schematic <option> <syntax>` with the syntax from Requirement "Subcommand table" |
| No schematic document active, for options that need one | `schematic: no schematic is open` |

`schematic` with no option, and `schematic help`, SHALL print one line per option in the table's order. File names, instance names, net names and attribute keys are not keywords and SHALL be matched exactly.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12)

#### Scenario: Prefix match
- **WHEN** `schematic netl` is executed
- **THEN** it SHALL act as `schematic netlist`

#### Scenario: Ambiguous prefix
- **WHEN** `schematic s` is executed
- **THEN** `schematic: ambiguous option "s"` SHALL be reported

### Requirement: Subcommand table
The `schematic` options SHALL be:

| Option | Syntax | Effect / result |
|---|---|---|
| `open` | `open <file> [-window new\|tab\|current]` | Open a `.sch` or `.sym`. Return the document id. |
| `new` | `new schematic\|symbol [<file>]` | New empty document. Return the document id. |
| `save` | `save [<file>] [-force]` | Save (capability `schematic-file-format`, Requirements "Canonical write order" and "Safe saving"). Return the absolute path. |
| `close` | `close [-discard]` | Close the active document; fail with `schematic: unsaved changes` unless saved or `-discard`. Closing ends the document's selection, undo history and launcher confirmation. |
| `reload` | `reload` \| `reload_symbols` | Re-read the file, or the symbol files, from disk. `reload` also drops the document's selection, the net highlights made in it, its undo and redo history and its launcher confirmation (capability `schematic-symbols-and-libraries`, Requirement "Script execution permission"), and re-reads the cached symbol files that changed on disk since they were read (modification time or size) |
| `path` | `path [list\|set <paths>\|prepend <dir>]` | Show or change the library search path (capability `schematic-symbols-and-libraries`). Return the list. |
| `resolve` | `resolve <reference>` | Return the absolute path of a symbol reference, or empty |
| `place` | `place <symbol> <x> <y> [-rot 0..3] [-flip 0\|1] [-props <string>]` | Place an instance. Return its name. |
| `wire` | `wire <x1> <y1> <x2> <y2> [-props <string>]` | Add one wire. Return its index. |
| `line` / `box` / `arc` / `polygon` / `text` | geometric arguments as in the file records, `-layer <n>`, `-props <string>` | Add graphics. Return the index. |
| `label` | `label <x> <y> <net> [-kind pin\|wire\|ipin\|opin\|iopin] [-rot r] [-flip f]` | Place a label or port instance |
| `select` | `select instance\|wire\|text\|all\|connected\|area <args> [-remove]` \| `select connected [-stopjunction]` \| `select clear` | Change the selection: an object or area is added to it, or removed from it with `-remove`; `connected` adds the wires, labels and ports on the nets of the selected objects, across junctions, or up to the first wire junction with `-stopjunction` (capability `schematic-editing`, Requirement "Selection"). With `-remove` the objects the same form would select are taken out of the selection (for `connected`, every object it reaches from the selection, the starting objects among them); `select clear -remove` and `-stopjunction` on another form are usage errors, and a form that fails leaves the selection unchanged. Return the selected objects. |
| `selection` | `selection` | Return the selected objects as a list of `<kind> <index-or-name>` |
| `delete` / `move` / `copy` / `rotate` / `flip` / `flipv` | `move <dx> <dy> [-stretch] [-insertwires]`; `copy <dx> <dy>`; `rotate [-inplace] [<x> <y>]`; `flip [-inplace] [<x> <y>]` | Edit the selection (capability `schematic-editing`) |
| `align` | `align` | Snap the selection to the grid |
| `snap` | `snap [<value>\|half\|double]` | Return the snap value of the active document's editor, or set it to a positive value, half or double of it (capability `schematic-editing`, Requirement "Snap and grid"; default 10). Not an undo step. |
| `layer` | `layer [<n>]` | Return the current layer for new graphics, or set it (0 to 21; default 4; capability `schematic-editing`, Requirement "Graphics and text"). With objects selected, setting it also moves the selected lines, boxes, arcs and polygons to layer `<n>` (one undo step). |
| `cut` | `cut <x> <y> [-nosnap]` | Split the wire under (x, y) at that point, snapped unless `-nosnap` (capability `schematic-editing`, Requirement "Wires", "Cut wire"). Return the indices of the two wires; fail with `schematic: no wire at <x> <y>` when no wire passes through the point. |
| `break` | `break` | Split the selected wires at every point where another wire's endpoint or an instance pin touches their interior ("Break wires"). Return the selected objects. |
| `trim` | `trim` | Merge collinear wires that overlap or touch end to end, and split wires at T junctions, over the whole document (the operation the "Auto-trim" setting runs after each wire edit). Return the number of wires. |
| `attach` | `attach [-prefix <text>]` | For the selected instances, place one `lab_pin` at each unconnected pin, named after the pin with the optional prefix (capability `schematic-editing`, Requirement "Labels and ports", "Attach labels to instance"). Return the names of the placed labels. |
| `order` | `order <index>` | Move the selected objects of each kind, as one block in their order, to position `<index>` (from 0) of their kind's list, or to its end when `<index>` is past it (capability `schematic-editing`, Requirement "Object order"). Return the selected objects. |
| `clipboard` | `clipboard [copy\|set <text>]` | Without an argument, return the clipboard text; `copy` puts the selected objects on the clipboard and returns the text (with nothing selected the clipboard is unchanged and the result empty); `set` replaces the clipboard text (capability `schematic-editing`, Requirement "Clipboard"). Needs no open document, except `copy`. |
| `paste` | `paste <x> <y>` | Insert the clipboard's objects with the anchor at (x, y), make instance names unique and select them. Return the selected objects. Fail with `schematic: the clipboard holds no schematic objects` when it holds none. |
| `merge` | `merge <file> [<x> <y>]` | Insert the objects of a schematic file as `paste` does, moved by (x, y) (default 0 0). Return the selected objects. |
| `getprop` | `getprop instance <name> [<key>]` \| `getprop symbol <name> [<key>]` \| `getprop wire <n> [<key>]` \| `getprop schematic [K\|G\|V\|S\|F\|E]` | Return a property string or one decoded value |
| `setprop` | `setprop instance <name> <key> [<value>]` \| `setprop wire <n> <key> [<value>]` \| `setprop selection <key> [<value>]` \| `setprop schematic <tag> <text>` | Set or remove (no value) an attribute (capability `schematic-file-format`, Requirement "Property strings"). `selection` applies the change to every selected instance and wire at once, as the property dialog's "apply to all selected" does (capability `schematic-editing`, Requirement "Property editing"), in one undo step; a `name` given this way is made unique per instance as `setprop instance` does. |
| `replace` | `replace <instance> <symbol>` | Change an instance's symbol, keeping its properties |
| `instances` | `instances [-pattern <glob>]` | Return `{name symbol x y rot flip}` per instance, in file order |
| `nets` | `nets` \| `net <instance> <pin>` | Return all net names, or the net on one pin, after connectivity (capability `schematic-netlisting`) |
| `descend` | `descend <instance> [<element>]` \| `descend -symbol <instance>` | Go down the hierarchy. Return the new hierarchy path. |
| `ascend` | `ascend` | Go up. Return the hierarchy path. |
| `hierarchy` | `hierarchy [path\|list]` | Return the current path, or with `list` the absolute paths of the schematics the active document's hierarchy uses: the top schematic first, then each subcircuit schematic once, in the order a `hier` netlist run writes their definitions (capability `schematic-netlisting`, Requirement "Subcircuit definitions") |
| `hilight` | `hilight net <name>\|selection\|instance <name>` \| `hilight clear [all]` \| `hilight list` | Net highlighting |
| `undo` / `redo` | `undo [<n>]`, `redo [<n>]` | Undo or redo n steps (default 1) |
| `netlist` | `netlist [<file>] [-hier\|-top\|-flat] [-lvs] [-topsubckt] [-lvsignore] [-noprefix] [-buschar <xy>] [-uppercase]` | Netlist the top of the active document's hierarchy (capability `schematic-netlisting`). Without `<file>`, return the text; with it, write the file. The front end's Interrupt (Ctrl-C, the Interrupt card) cancels the run: it fails with `schematic: the netlist run was interrupted; no file is written`, writes no file and stores no computed label; a `tcleval` value running when the Interrupt is made stops at once. `check` is cancelled the same way. |
| `check` | `check` | Run connectivity checks only; return the list of errors and warnings |
| `symbol` | `symbol make [-force]` \| `symbol pins` \| `symbol fromschematic <file.sch> [-force]` \| `symbol toschematic` | Symbol generation (capability `schematic-editing`, Requirement "Labels and ports"). `make` writes the symbol of the active schematic beside it; `fromschematic` does the same for the schematic file `<file.sch>` without opening it (the active document is not changed). Both refuse to overwrite an existing `.sym` without `-force`. Return the path of the written symbol. |
| `simulate` | `simulate [-wait] [-corner <name>]` | Simulate the active testbench schematic (capability `schematic-simulation-integration`). Return the job id. |
| `deck` | `deck [<file>]` | Build the simulation deck of the active document as a testbench (capability `schematic-simulation-integration`, Requirement "Schematic circuit source": its `hier` netlist with `.control` blocks translated and file references resolved). Without `<file>`, return the deck text; with it, write the file and return its absolute path. Fail with the `SCHEM-SIM-004` error when the testbench fails to netlist. Needs no simulator (available before M9). |
| `vector` | `vector <net> [-path <hierarchy path>]` | Return the simulator vector name of net `<net>` seen from the hierarchy path (default: the current path of the active document; capability `schematic-simulation-integration`, Requirement "Simulator vector names") |
| `annotate` | `annotate on [<job>]\|off` | Operating-point annotation on the schematic |
| `probe` | `probe [<net>\|-selection]` | Add traces for schematic nets. Return the vector names. |
| `export` | `export svg\|pdf\|png <file> [-selection]` | Export a drawing of the current view |
| `pdk` | `pdk [<name>]` | Select the PDK whose `xschemrc` is evaluated (capability `schematic-symbols-and-libraries`). Return the active PDK. |
| `help` | `help` | Usage list |

Coordinates SHALL be in schematic units, parsed as decimal floating-point numbers. Instance arguments SHALL name instances by their `name` attribute; wires and graphics are addressed by their index in the document lists. Every option that changes a document SHALL create exactly one undo step.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12)

#### Scenario: Script builds a divider
- **WHEN** the script `schematic new schematic div.sch; schematic place res.sym 0 0; schematic place res.sym 0 100; schematic wire 0 30 0 70; schematic label 0 -30 IN; schematic label 0 130 0; schematic netlist` is executed
- **THEN** it SHALL return a netlist containing the lines `R1 IN net1 1k` and `R2 net1 0 1k` (the resistor's pins being P at (0,−30) and M at (0,30) relative to its origin)

#### Scenario: Reload clears selection, highlights and undo history
- **GIVEN** an open schematic with a selected wire, a highlighted net and an edit made after its last save
- **WHEN** the file is changed on disk and `schematic reload` is executed
- **THEN** `schematic selection` and `schematic hilight list` SHALL return empty results, and `schematic undo` SHALL undo nothing (return `0`) and leave the reloaded content

#### Scenario: Reload forgets the launcher confirmation
- **GIVEN** the user confirmed the launchers of an open schematic
- **WHEN** `schematic reload` (or `schematic close`) is executed
- **THEN** the next launcher of that document SHALL ask for confirmation again

#### Scenario: Changed symbol file picked up on reload
- **GIVEN** an open schematic placing `r.sym`, whose file is changed on disk after it was read
- **WHEN** `schematic reload` is executed
- **THEN** `schematic getprop symbol r.sym` SHALL return the new properties, and a symbol file that did not change SHALL NOT be read again

#### Scenario: Interrupted netlist
- **GIVEN** the user presses Interrupt while `schematic netlist out.spice` runs
- **WHEN** the run ends
- **THEN** the command SHALL fail with `schematic: the netlist run was interrupted; no file is written` and `out.spice` SHALL NOT be written

#### Scenario: Deck of the active testbench
- **GIVEN** the IHP testbench `dc_lv_nmos.sch` open and active
- **WHEN** `schematic deck` is executed
- **THEN** it SHALL return the deck text: the element lines of the `hier` netlist and the `.control` block with its `write` commands turned into capture points (capability `schematic-simulation-integration`), with no simulator involved

#### Scenario: Vector name of a net below the top
- **GIVEN** a testbench whose instance `x1` places a schematic with an internal net `mid`
- **WHEN** `schematic vector mid -path .x1.` is executed
- **THEN** it SHALL return the vector name capability `schematic-simulation-integration` gives that net, `x1.mid`

#### Scenario: Remove one object from the selection
- **GIVEN** instances `R1` and `R2` selected
- **WHEN** `schematic select instance R1 -remove` is executed
- **THEN** the selection SHALL be `instance R2`

#### Scenario: Apply a property to all selected instances
- **GIVEN** resistors `R1` and `R2` selected
- **WHEN** `schematic setprop selection value 2k` is executed, then `schematic undo`
- **THEN** both SHALL have `value=2k` after the first command, and both SHALL have their old values after the undo (one step)

#### Scenario: Current layer of new graphics
- **GIVEN** a new schematic
- **WHEN** `schematic layer`, `schematic box 0 0 10 10`, `schematic layer 7` and `schematic box 20 0 30 10` are executed
- **THEN** `schematic layer` SHALL return `4` and the boxes SHALL be on layers 4 and 7

#### Scenario: Hierarchy list
- **GIVEN** a top schematic `top.sch` placing `a.sch` twice and `b.sch` once, where `a.sch` places `b.sch`
- **WHEN** `schematic hierarchy list` is executed
- **THEN** it SHALL return the absolute paths of `top.sch`, then `a.sch` and `b.sch` in the order the `hier` netlist writes their definitions, each once

### Requirement: Return values and errors in scripts
Each option SHALL return its result as a Tcl-compatible list in the scripting binding (capability `hefesto-scripting`) and as plain text in the command bar. Errors SHALL raise a script error with the message text, and SHALL NOT leave a partial edit: an option that fails SHALL undo its own changes. `schematic netlist` and `schematic check` SHALL raise an error when the netlist run reports failure (capability `schematic-netlisting`, Requirement "Errors and warnings"), with the error list as the message, after returning the text if `-force` was given.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Netlist error in a script
- **GIVEN** a schematic with two labels `A` and `B` on one wire
- **WHEN** a script runs `catch {schematic netlist} msg`
- **THEN** `catch` SHALL return 1 and `msg` SHALL contain `shorted`

### Requirement: Headless use
The headless CLI (capability `hefesto-architecture`, Requirement "Headless command-line tool") SHALL run `schematic` commands without a display, including `open`, `netlist`, `simulate` (with `-wait`), `getprop`, `setprop` and `save`. A command-line form of the headless executable (`hefesto-cli`) SHALL netlist a schematic in one call: `hefesto-cli --netlist <file.sch> [-o <out>] [--lvs] [--pdk <name>]`, where `--lvs` turns on options `lvs_netlist` and `lvs_ignore` (capability `schematic-netlisting`). It SHALL exit with status 0 on success, 10 when the netlist run reports failure (as xschem's netlist exit code), and 1 on other errors.

Hefesto SHALL NOT print xschem's own driver text (its start-up and `sourcing` lines, the `== <cell>` netlist banner); how the golden SCHEM cases compare their stdout and stderr with xschem's, which hold that text, is Q-SCHEM-46 (approved by the user 2026-11-25, option (b)): the capture harness filters xschem's driver text out of the captured stdout and stderr, and every other line the case prints stays compared.

**Golden comparison of diagnostics** (Q-SCHEM-60 option (a), approved by the user on 2026-11-27). Hefesto's diagnostics are its coded contract (`<severity>: <CODE>: <text>`, capability `hefesto-diagnostics`), not xschem's ERC and loader lines. The `stderr.txt` of every SCHEM golden case SHALL be an approved deviation (`tests/golden/deviations.toml`, SCHEM block): its text is not compared with xschem's. The files a case writes, its exit status (`exitcode.txt`) and the `puts` results of its script on stdout SHALL stay compared. The message texts SHALL be covered instead by the unit tests of capability `schematic-netlisting`, Requirement "Errors and warnings", and capability `schematic-file-format`, Requirement "Malformed input".

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12; xschem ref: spice_netlist.c:636-641)

#### Scenario: CI netlist check
- **WHEN** `hefesto-cli --netlist inv.sch --lvs --pdk sky130A -o inv.spice` is run on a clean schematic
- **THEN** it SHALL write `inv.spice` and exit with status 0

### Requirement: xschem compatibility command
The sandboxed Tcl interpreter SHALL provide a command `xschem` with the subcommands that PDK symbols, `xschemrc` files and testbench launchers use, mapped onto the `schematic` and `sim` commands:

| xschem subcommand | Hefesto mapping |
|---|---|
| `xschem get current_name`, `get schname`, `get sch_path`, `get netlist_type`, `get raw_level`, `get topwindow`, `get current_win_path`, `get netlist_dir`, `get version`, `get instances`, `get wires` | Current document file name, absolute path, hierarchy path, `spice`, the loaded results level (0), an opaque window id, an opaque id, the netlist folder, the pinned xschem compatibility version, the number of instances, the number of wires (decimal integers; golden cases SCHEM-PR-02, SCHEM-RT-06) |
| `xschem getprop instance <inst> [<key>]`, `getprop rect <layer> <n> <key>`, `getprop symbol <sym> <key>` | `schematic getprop`. `<inst>` is an instance index (from 0, in file order) when it is all decimal digits, otherwise an instance name, so an instance named `3` is reached only by its index; an index past the last instance or an unknown name raises `xschem getprop: instance not found:<inst>` (no space after the colon). Without `<key>`, the instance's whole property string is returned. A `<key>` of the form `cell::name` returns the symbol reference, and `cell::<attr>` the symbol's attribute `<attr>`. `getprop rect` addresses box `<n>` (from 0) of layer `<layer>`; a missing layer or index raises `xschem getprop rect: no such box` (Hefesto checks the bounds; xschem does not) |
| `xschem setprop [-fast\|-fastundo] instance <inst> <key> [<value>]`, `setprop [-fast\|-fastundo] rect <layer> <n> <key> [<value>]` | `schematic setprop`. `<inst>` as for `getprop`; without `<value>` the attribute is removed, and `setprop instance <inst>` with no key clears the whole property string; key `allprops` replaces the whole string; setting `name` renames the instance, made unique as `schematic setprop` does, and the result is the instance's resulting name. `setprop rect` sets or removes an attribute of box `<n>` of layer `<layer>` (one undo step; no change and no undo step when the value is already the same), result empty. `-fast` (no undo step) and `-fastundo` come before the object kind; Hefesto accepts both and always records the undo step, since it has no redraw to skip |
| `xschem netlist` | `schematic netlist`, writing the file. With `xschem-compat` ON the result SHALL be xschem's: `0` after a run that reports no error, and `1` after a run that reports an error (capability `schematic-netlisting`, Requirement "Errors and warnings"), without raising, so the script goes on; the netlist file is still written and the headless run still exits with status 10 (golden cases SCHEM-NL-22, SCHEM-SIM-03). With `xschem-compat` OFF a run that reports an error SHALL raise as `schematic netlist` does. `schematic netlist` itself SHALL raise in both modes (Q-SCHEM-61 option (a), approved by the user on 2026-11-27). An interrupted run raises in both modes |
| `xschem clear [force] [schematic\|symbol]` | Replace the active document by a new empty unsaved schematic (or symbol with `symbol`), named `untitled.sch` (`untitled.sym`) as `schematic new` names it, at the top of its hierarchy, with an empty undo history; result empty. Without `force`, a modified document is handled as xschem does in a run without its GUI: see the paragraph "clear without force" below |
| `xschem instance <sym> <x> <y> <rot> <flip> [<props>]` | `schematic place <sym> <x> <y> -rot <rot> -flip <flip> [-props <props>]`: without `<props>` the symbol's `template` gives the properties, and the instance name is made unique as `schematic place` makes it (`R1`, then `R2`; golden case SCHEM-PR-02); result empty |
| `xschem copy`, `xschem paste [<x> <y>]` | `schematic clipboard copy` (result empty), and `schematic paste <x> <y>` (the pasted instances renamed unique, so a copy of `R1` among `R1`, `R2` is `R3` and a copy of `x1[3:0]` is `x2[3:0]`; golden case SCHEM-PR-02); `paste` without `<x> <y>` starts an interactive placement in the editor and in a run with no editor raises `xschem command 'paste' needs the editor` |
| `xschem saveas <file> [schematic\|symbol]` | `schematic save <file>` without asking; the kind word is accepted and changes nothing: the document is written as the kind it is (a schematic document as a schematic, a symbol document as a symbol), whatever the word and the extension of `<file>` (Hefesto has no conversion of a schematic into a symbol; standing rule, reported 2026-11-27); the saved file becomes the document's file; result `1` on success, `0` when the file is not written (golden case SCHEM-RT-06). Without `<file>`, it asks for a name in the editor and raises `xschem command 'saveas' needs the editor` in a run with no editor |
| `xschem raw_read <file> [<type>]`, `xschem annotate_op` | Load results / turn annotation on (capability `schematic-simulation-integration`, Requirement "Simulation launchers") |
| `xschem translate <inst> <string>` | Text substitution (capability `schematic-symbols-and-libraries`, Requirement "Text substitution tokens") for instance `<inst>`, addressed as for `getprop`; `<inst>` = `-1` substitutes with no instance, which leaves `@` tokens and `%` tokens unchanged (`xschem translate -1 {@name %undef}` returns `@name %undef`; golden case SCHEM-PR-04); any other instance that does not exist raises `xschem translate: instance not found`, in both modes |
| `xschem expandlabel <s>`, `xschem get_tok <s> <key> [<with_quotes>]`, `xschem subst_tok <s> <key> <value>` | Substitution and property helpers of capabilities `schematic-symbols-and-libraries`, `schematic-netlisting` and `schematic-file-format`. `get_tok` returns the value of `<key>` in the property string `<s>` (capability `schematic-file-format`, Requirement "Property strings"), or the empty string when the key is absent. Its optional third argument SHALL be accepted in both modes (Q-SCHEM-62 option (a), approved by the user on 2026-11-27): `1` returns the value as it is written in `<s>`, a quoted value with its quotes and its backslash escapes kept (`device="tantalium capacitor"` gives `"tantalium capacitor"`; `drc="f {@name\}"` gives `"f {@name\}"`); `2`, `0` or no third argument return the decoded value, quotes removed and escapes resolved (`tantalium capacitor`, `f {@name}`); a value written without quotes and without backslashes is the same in every form, and one with backslash escapes keeps them only with `1` (`k=\"q\"` gives `"q"`, and `\"q\"` with `1`) (golden case SCHEM-PR-01). Any other third argument SHALL act as `0` (no capture covers other values; standing rule, reported 2026-11-27) |
| `xschem select`, `xschem unselect_all`, `xschem redraw`, `xschem zoom_full`, `xschem hilight_netname`, `xschem unhilight_all`, `xschem load <file>`, `xschem go_back`, `xschem instance_list` | The corresponding `schematic` options |
| `xschem set <var> <value>` | `netlist_type`: `spice` is accepted and any other value raises the error `xschem set netlist_type: only spice is supported`; `netlist_name`: the file name the next `xschem netlist` writes; `header_text`, `schprop`, `schsymbolprop`: the header text, the `S` text and the `K` text of the current document (`schematic setprop schematic`); `header_text` changes the document (modified flag and one undo step) only when the text differs. `rectcolor`: the current layer (`schematic layer`, clamped to 0..21). `cadsnap`: the snap value (`schematic snap`). Variables that only change xschem's drawing or GUI state (`cadgrid`, `no_draw`, `no_undo`, `draw_window`, `show_hidden_texts`, `semaphore`, `line_width`, `rectcolor`, ...) are accepted and do nothing. Any other variable raises `xschem command 'set <var>' is not supported` |
| `xschem descend [<n>] [<set_title>] [<instname>]`, `xschem change_sch_path <n> [<draw>]` | `schematic descend` into the selected instance or into `<instname>`; for a vector instance, `n` picks the element (1 the leftmost, -1 the rightmost; 0, which asks in xschem, picks the leftmost in a script). `change_sch_path` moves to element `n` of the vector instance last descended into, as `schematic ascend` followed by `schematic descend` would. The result of `xschem descend` SHALL be `1` when it descended and `0` when it did not (no instance selected, the selected object is not an instance, the instance's symbol has no schematic to descend into, or the 40-level limit is reached), without raising, in both modes (golden case SCHEM-SIM-03: `descend: 1`); `schematic descend` keeps returning the new hierarchy path |
| `xschem resolved_net [<net>]` | The topmost full hierarchy name of net `<net>` of the current hierarchy level (Requirement "Subcommand table", `hierarchy path`), as a simulator vector name (capability `schematic-simulation-integration`, Requirement "Simulator vector names"): a net carried by a port of the current level (an `ipin`, `opin` or `iopin` label, or a pin of the symbol it was descended through) SHALL be replaced by the net that the instance's pin connects to one level up, repeatedly up to the top or to the first level where it is not a port; the result is that net's name prefixed by the hierarchy path of that level without its leading `.` (`x1.MID` for the internal net `MID` of `.x1.`; the port nets `A` and `Y` of `.x1.` connected to `IN` and `OUT` at the top give `IN` and `OUT`; golden case SCHEM-SIM-03). At the top level the result is the net name. A name that is not a port of its level is taken as an internal net of it. Without `<net>`, the net of the one selected wire, label or pin is resolved; when the selection is not exactly one such object the result is the empty string. Both modes |
| `xschem load_new_window [<file>]` | `schematic open <file> -window new`; with no file or `{}`, `schematic new schematic` |
| `xschem place_symbol [<sym>] [<props>]` | Start placing `<sym>` interactively with `<props>` (default: the symbol's `template`), as the editor's place command (capability `schematic-editing`); in a run with no editor it raises `xschem command 'place_symbol' needs the editor` |
| `xschem hilight_instname [-fast] <inst>` | `schematic hilight instance <inst>` |
| `xschem logic_set <n> [<num>]` | Accepted and does nothing: Hefesto has no logic propagation (xschem sets logic levels on the selected nets for its logic simulation) |
| `xschem rebuild_connectivity`, `xschem reset_caches`, `xschem update_all_sym_bboxes` | `rebuild_connectivity` recomputes connectivity (as `schematic check` does, without returning its list); `reset_caches` re-reads changed symbol files (`schematic reload_symbols`); `update_all_sym_bboxes` does nothing, since Hefesto keeps symbol bounding boxes current. Each returns the empty string |
| `xschem save [fast]` | `schematic save` of the current document when it is modified, without asking; nothing when it is not |
| `xschem search regex\|exact <select> <key> <value> [no_match_case] [nodraw]` | Find the instances, wires, boxes and texts whose attribute `<key>` matches `<value>` (a regular expression, or an exact text), case-insensitively with `no_match_case`; `<select>` 1 selects them, -1 unselects them, 0 highlights them (`schematic select` / `schematic hilight`). Special keys: `propstring` (the whole property string), `txt_ptr` (the text of a text object), `cell::name` (the symbol reference of an instance), `cell::propstring` (the symbol's whole property string) and `cell::<attr>` (an attribute of the symbol). The result is `1` when anything matched and `0` otherwise |
| `xschem selected_set [rect\|text]` | From `schematic selection`: without an argument, the names of the selected instances as a Tcl list with each name in braces, separated by one space (`{R1} {M2}`); with `rect`, one line per selected box, `<layer> <n> <x1> <y1> <x2> <y2>`; with `text`, one line per selected text, `<n> <x> <y> <rot> <flip> {<text>}`; numbers as C `%g` prints them, lines separated by LF |
| `xschem select instance\|wire\|text <id> [clear] [fast]` | `schematic select`: the object is added to the selection, or removed from it with `clear`; `<id>` of an instance as for `getprop`, of a wire or text its index; `fast` is accepted. Result `1` when the object exists, `0` otherwise |
| `xschem simulate [<callback>]` | `schematic simulate` of the current testbench; `<callback>` is a Tcl procedure called when it ends (capability `schematic-simulation-integration`; until M9 the call answers CORE-NOTIMPL) |
| `xschem raw <what> ...`, `xschem raw_clear` | Query or unload the loaded simulation results (`raw value`, `values`, `index`, `points`, `datasets`, `loaded`, `rawfile`, `sim_type`, `vars`, `list`; capability `schematic-simulation-integration`, M9; until then `raw loaded` returns -1 and the others answer CORE-NOTIMPL) |

**clear without force.** xschem's `clear` without `force` asks whether to save a modified document; in a run without its GUI it saves the document to its file without asking and then clears it. With `xschem-compat` ON Hefesto SHALL do the same in a run with no editor (save the modified document as `schematic save` does, then clear; a document never saved is cleared without saving); with the editor it SHALL ask as `schematic close` does. With `xschem-compat` OFF, a run with no editor SHALL refuse with `schematic: unsaved changes` and change nothing (standing rule for xschem behaviour no Q-row covers; reported to the user 2026-11-26).

`xschem version` SHALL return the pinned compatibility version string. The table SHALL list every subcommand of the `xschem_subcommands` lists of `tests/data/schem/inventory.json` (checked 2026-11-21: `annotate_op`, `change_sch_path`, `descend`, `expandlabel`, `get`, `getprop`, `go_back`, `hilight_instname`, `instance_list`, `load_new_window`, `logic_set`, `netlist`, `place_symbol`, `raw`, `raw_clear`, `raw_read`, `rebuild_connectivity`, `redraw`, `reset_caches`, `save`, `search`, `select`, `selected_set`, `set`, `setprop`, `simulate`, `translate`, `unhilight_all`, `unselect_all`, `update_all_sym_bboxes`), and SHALL also list the subcommands the golden SCHEM cases call (`clear`, `copy`, `instance`, `paste`, `resolved_net`, `saveas`, and the `get` keys `instances` and `wires`). Any other subcommand SHALL raise the error `xschem command '<cmd>' is not supported`, and so SHALL a `get` key not listed in the table (`xschem command 'get <key>' is not supported`). The result formats of the supported subcommands SHALL equal xschem's. The list SHALL be fixed by an inventory script over the `xschem` calls in the three pinned PDK libraries, kept as a test that fails when a PDK bump adds a subcommand that is not in the table (Q-SCHEM-10, `Resolution: other`).

Compatibility: exact

(xschem ref: scheduler.c:1-7433, actions.c descend_schematic)

#### Scenario: Instance property from Tcl
- **GIVEN** an instance `M1` with `w=1.0u`
- **WHEN** a PDK script calls `xschem getprop instance M1 w`
- **THEN** it SHALL return `1.0u`

#### Scenario: Launcher saves and netlists
- **GIVEN** a modified testbench whose launcher runs `xschem set netlist_type spice; xschem save; xschem netlist`
- **WHEN** the launcher is confirmed and runs
- **THEN** the document SHALL be saved, the SPICE netlist SHALL be written, and no error SHALL be raised

#### Scenario: Non-SPICE netlist type
- **WHEN** a script calls `xschem set netlist_type verilog`
- **THEN** it SHALL get the error `xschem set netlist_type: only spice is supported`

#### Scenario: Unsupported subcommand
- **WHEN** a launcher calls `xschem fork_sch`
- **THEN** the script SHALL get the error `xschem command 'fork_sch' is not supported`

#### Scenario: Place, copy and paste from Tcl
- **GIVEN** an empty schematic made by `xschem clear force schematic`
- **WHEN** the script of golden case SCHEM-PR-02 places `res.sym` twice with `xschem instance res.sym <x> <y> 0 0`, copies `R1` with `xschem select instance R1; xschem copy; xschem paste 200 0`, places `res.sym` with `{name=x1[3:0] value=1k}` and copies it the same way, listing the names after each step through `xschem get instances` and `xschem getprop instance <index> name`
- **THEN** the lists SHALL be `R1`, `R1 R2`, `R1 R2 R3`, `R1 R2 R3 {x1[3:0]}` and `R1 R2 R3 {x1[3:0]} {x2[3:0]}`

#### Scenario: Instance addressed by index
- **GIVEN** a schematic whose instances in file order are `R7`, `3` and `M1`
- **WHEN** a script calls `xschem getprop instance 1 name`, `xschem getprop instance 3 name` and `xschem getprop instance R9 name`
- **THEN** they SHALL return `3`, raise `xschem getprop: instance not found:3` (index 3 is past the last instance), and raise `xschem getprop: instance not found:R9`

#### Scenario: Translate with and without an instance
- **GIVEN** the schematic of golden case SCHEM-PR-04 with instance `R7` (`res.sym`, `value=4.7k`)
- **WHEN** a script calls `xschem translate <index of R7> {@symname_ext}`, `xschem translate -1 {@name %undef}` and `xschem translate 999 {@name}`
- **THEN** they SHALL return `res.sym` and `@name %undef`, and raise `xschem translate: instance not found`

#### Scenario: Counts and saveas after a malformed file
- **GIVEN** the file of golden case SCHEM-RT-06, of which one wire and one instance load
- **WHEN** a script calls `xschem get wires`, `xschem get instances` and `xschem saveas <out>/bad.sch schematic`
- **THEN** they SHALL return `1`, `1` and `1`, and `<out>/bad.sch` SHALL be written as captured

#### Scenario: saveas with the symbol word
- **GIVEN** a schematic document open in a headless script, with `xschem-compat` ON or OFF
- **WHEN** the script calls `xschem saveas <out>/copy.sch symbol`
- **THEN** it SHALL return `1`, and `<out>/copy.sch` SHALL hold the document written as a schematic, byte for byte as `xschem saveas <out>/copy.sch schematic` writes it

#### Scenario: Search result and selected_set formats
- **GIVEN** instances `R1` and `R2` with `value=1k` and `R3` with `value=2k`
- **WHEN** a script calls `xschem search exact 1 value 1k`, then `xschem selected_set`, then `xschem search exact 1 value 5k`
- **THEN** they SHALL return `1`, `{R1} {R2}` and `0`

#### Scenario: clear without force in a headless run
- **GIVEN** a saved schematic `a.sch` modified by a script run with no editor
- **WHEN** the script calls `xschem clear`
- **THEN** with `xschem-compat` ON `a.sch` SHALL be saved with the change and the document SHALL become an empty `untitled.sch`; with `xschem-compat` OFF the call SHALL raise `schematic: unsaved changes`, `a.sch` SHALL NOT be written and the document SHALL be unchanged

#### Scenario: xschem netlist after a failing run, xschem-compat ON
- **GIVEN** the schematic of golden case SCHEM-NL-22, whose netlist run reports an error, with `xschem-compat` ON
- **WHEN** a headless script runs `puts "netlist [xschem netlist]"` and then `xschem saveas out/nl11-saved.sch schematic`
- **THEN** stdout SHALL hold `netlist 1`, the netlist file and `out/nl11-saved.sch` SHALL be written, and the run SHALL exit with status 10 (Q-SCHEM-61)

#### Scenario: xschem netlist after a failing run, xschem-compat OFF
- **GIVEN** the same schematic with `xschem-compat` OFF
- **WHEN** a script runs `catch {xschem netlist} msg`
- **THEN** `catch` SHALL return 1 and `msg` SHALL hold the run's error list, as for `schematic netlist` (Q-SCHEM-61)

#### Scenario: xschem netlist after a clean run
- **GIVEN** a schematic whose netlist run reports no error, with `xschem-compat` ON
- **WHEN** a script runs `xschem netlist`
- **THEN** it SHALL return `0` and write the netlist file (Q-SCHEM-61)

#### Scenario: get_tok with and without quotes
- **GIVEN** the property string `name=C4 m=1 device="tantalium capacitor"`, with `xschem-compat` ON or OFF
- **WHEN** a script runs `xschem get_tok $s device`, `xschem get_tok $s device 1`, `xschem get_tok $s device 2` and `xschem get_tok $s m 1`
- **THEN** the results SHALL be `tantalium capacitor`, `"tantalium capacitor"`, `tantalium capacitor` and `1` (golden case SCHEM-PR-01, Q-SCHEM-62)

#### Scenario: Descend result and resolved nets
- **GIVEN** the testbench of golden case SCHEM-SIM-03 (instance `x1` of `stage.sym`, whose schematic has the internal net `MID` and the ports `A` and `Y`, connected to `IN` and `OUT` at the top), with `x1` selected, with `xschem-compat` ON or OFF
- **WHEN** a script runs `xschem descend`, `xschem get sch_path` and `xschem resolved_net` for `MID`, `A` and `Y`
- **THEN** the results SHALL be `1`, `.x1.`, `x1.MID`, `IN` and `OUT`

#### Scenario: Descend with nothing selected
- **GIVEN** a schematic with no selection
- **WHEN** a script runs `xschem descend`
- **THEN** it SHALL return `0`, the hierarchy path SHALL stay `.`, and no error SHALL be raised

### Requirement: Command logging
Every `schematic` command issued by the GUI, a gesture or a key binding SHALL be recorded in the command log with its arguments, so that a session can be replayed by a script to produce the same files (capability `hefesto-architecture`).

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Replay a session
- **GIVEN** a command log recorded while a user drew a schematic by hand
- **WHEN** the log is run as a script on an empty document
- **THEN** the saved file SHALL be byte-identical to the one the user saved
