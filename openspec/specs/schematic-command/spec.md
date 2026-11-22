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
| `close` | `close [-discard]` | Close the active document; fail with `schematic: unsaved changes` unless saved or `-discard`. |
| `reload` | `reload` \| `reload_symbols` | Re-read the file, or the symbol files, from disk |
| `path` | `path [list\|set <paths>\|prepend <dir>]` | Show or change the library search path (capability `schematic-symbols-and-libraries`). Return the list. |
| `resolve` | `resolve <reference>` | Return the absolute path of a symbol reference, or empty |
| `place` | `place <symbol> <x> <y> [-rot 0..3] [-flip 0\|1] [-props <string>]` | Place an instance. Return its name. |
| `wire` | `wire <x1> <y1> <x2> <y2> [-props <string>]` | Add one wire. Return its index. |
| `line` / `box` / `arc` / `polygon` / `text` | geometric arguments as in the file records, `-layer <n>`, `-props <string>` | Add graphics. Return the index. |
| `label` | `label <x> <y> <net> [-kind pin\|wire\|ipin\|opin\|iopin] [-rot r] [-flip f]` | Place a label or port instance |
| `select` | `select instance\|wire\|text\|all\|connected\|area <args>` \| `select clear` | Change the selection. Return the selected objects. |
| `selection` | `selection` | Return the selected objects as a list of `<kind> <index-or-name>` |
| `delete` / `move` / `copy` / `rotate` / `flip` / `flipv` | `move <dx> <dy> [-stretch] [-insertwires]`; `copy <dx> <dy>`; `rotate [-inplace] [<x> <y>]`; `flip [-inplace] [<x> <y>]` | Edit the selection (capability `schematic-editing`) |
| `align` | `align` | Snap the selection to the grid |
| `getprop` | `getprop instance <name> [<key>]` \| `getprop symbol <name> [<key>]` \| `getprop wire <n> [<key>]` \| `getprop schematic [K\|G\|V\|S\|F\|E]` | Return a property string or one decoded value |
| `setprop` | `setprop instance <name> <key> [<value>]` \| `setprop wire <n> <key> [<value>]` \| `setprop schematic <tag> <text>` | Set or remove (no value) an attribute (capability `schematic-file-format`, Requirement "Property strings") |
| `replace` | `replace <instance> <symbol>` | Change an instance's symbol, keeping its properties |
| `instances` | `instances [-pattern <glob>]` | Return `{name symbol x y rot flip}` per instance, in file order |
| `nets` | `nets` \| `net <instance> <pin>` | Return all net names, or the net on one pin, after connectivity (capability `schematic-netlisting`) |
| `descend` | `descend <instance> [<element>]` \| `descend -symbol <instance>` | Go down the hierarchy. Return the new hierarchy path. |
| `ascend` | `ascend` | Go up. Return the hierarchy path. |
| `hierarchy` | `hierarchy [path\|list]` | Return the current path, or the list of schematics used |
| `hilight` | `hilight net <name>\|selection\|instance <name>` \| `hilight clear [all]` \| `hilight list` | Net highlighting |
| `undo` / `redo` | `undo [<n>]`, `redo [<n>]` | Undo or redo n steps (default 1) |
| `netlist` | `netlist [<file>] [-hier\|-top\|-flat] [-lvs] [-topsubckt] [-lvsignore] [-noprefix] [-buschar <xy>] [-uppercase]` | Netlist the top of the active document's hierarchy (capability `schematic-netlisting`). Without `<file>`, return the text; with it, write the file. |
| `check` | `check` | Run connectivity checks only; return the list of errors and warnings |
| `symbol` | `symbol make [-force]` \| `symbol pins` \| `symbol fromschematic` \| `symbol toschematic` | Symbol generation (capability `schematic-editing`, Requirement "Labels and ports") |
| `simulate` | `simulate [-wait] [-corner <name>]` | Simulate the active testbench schematic (capability `schematic-simulation-integration`). Return the job id. |
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

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12; xschem ref: spice_netlist.c:636-641)

#### Scenario: CI netlist check
- **WHEN** `hefesto-cli --netlist inv.sch --lvs --pdk sky130A -o inv.spice` is run on a clean schematic
- **THEN** it SHALL write `inv.spice` and exit with status 0

### Requirement: xschem compatibility command
The sandboxed Tcl interpreter SHALL provide a command `xschem` with the subcommands that PDK symbols, `xschemrc` files and testbench launchers use, mapped onto the `schematic` and `sim` commands:

| xschem subcommand | Hefesto mapping |
|---|---|
| `xschem get current_name`, `get schname`, `get sch_path`, `get netlist_type`, `get raw_level`, `get topwindow`, `get current_win_path`, `get netlist_dir`, `get version` | Current document file name, absolute path, hierarchy path, `spice`, the loaded results level (0), an opaque window id, an opaque id, the netlist folder, the pinned xschem compatibility version |
| `xschem getprop instance <inst> <key>`, `getprop rect <layer> <n> <key>`, `getprop symbol <sym> <key>` | `schematic getprop` |
| `xschem setprop instance <inst> <key> <value>`, `setprop rect <layer> <n> <key> <value>` | `schematic setprop` |
| `xschem netlist` | `schematic netlist`, writing the file |
| `xschem raw_read <file> [<type>]`, `xschem annotate_op` | Load results / turn annotation on (capability `schematic-simulation-integration`, Requirement "Simulation launchers") |
| `xschem translate <inst> <string>`, `xschem expandlabel <s>`, `xschem get_tok <s> <key>`, `xschem subst_tok <s> <key> <value>` | Substitution and property helpers of capabilities `schematic-symbols-and-libraries`, `schematic-netlisting` and `schematic-file-format` |
| `xschem select`, `xschem unselect_all`, `xschem redraw`, `xschem zoom_full`, `xschem hilight_netname`, `xschem unhilight_all`, `xschem load <file>`, `xschem go_back`, `xschem instance_list` | The corresponding `schematic` options |
| `xschem set <var> <value>` | `netlist_type`: `spice` is accepted and any other value raises the error `xschem set netlist_type: only spice is supported`; `netlist_name`: the file name the next `xschem netlist` writes; `header_text`, `schprop`, `schsymbolprop`: the header text, the `S` text and the `K` text of the current document (`schematic setprop schematic`). Variables that only change xschem's drawing or GUI state (`cadgrid`, `cadsnap`, `no_draw`, `no_undo`, `draw_window`, `show_hidden_texts`, `semaphore`, `line_width`, `rectcolor`, ...) are accepted and do nothing. Any other variable raises `xschem command 'set <var>' is not supported` |
| `xschem descend [<n>] [<set_title>] [<instname>]`, `xschem change_sch_path <n> [<draw>]` | `schematic descend` into the selected instance or into `<instname>`; for a vector instance, `n` picks the element (1 the leftmost, -1 the rightmost; 0, which asks in xschem, picks the leftmost in a script). `change_sch_path` moves to element `n` of the vector instance last descended into, as `schematic ascend` followed by `schematic descend` would |
| `xschem load_new_window [<file>]` | `schematic open <file> -window new`; with no file or `{}`, `schematic new schematic` |
| `xschem place_symbol [<sym>] [<props>]` | Start placing `<sym>` interactively with `<props>` (default: the symbol's `template`), as the editor's place command (capability `schematic-editing`); in a run with no editor it raises `xschem command 'place_symbol' needs the editor` |
| `xschem hilight_instname [-fast] <inst>` | `schematic hilight instance <inst>` |
| `xschem logic_set <n> [<num>]` | Accepted and does nothing: Hefesto has no logic propagation (xschem sets logic levels on the selected nets for its logic simulation) |
| `xschem rebuild_connectivity`, `xschem reset_caches`, `xschem update_all_sym_bboxes` | `rebuild_connectivity` recomputes connectivity (as `schematic check` does, without returning its list); `reset_caches` re-reads changed symbol files (`schematic reload_symbols`); `update_all_sym_bboxes` does nothing, since Hefesto keeps symbol bounding boxes current. Each returns the empty string |
| `xschem save [fast]` | `schematic save` of the current document when it is modified, without asking; nothing when it is not |
| `xschem search regex\|exact <select> <key> <value> [no_match_case] [nodraw]` | Find the instances (and wires, texts) whose attribute `<key>` matches `<value>` (a regular expression, or an exact text), case-insensitively with `no_match_case`; `<select>` 1 selects them, -1 unselects them, 0 highlights them (`schematic select` / `schematic hilight`). Returns what xschem returns |
| `xschem selected_set [rect\|text]` | The names of the selected instances, or the selected boxes or texts with their coordinates (`schematic selection`) |
| `xschem simulate [<callback>]` | `schematic simulate` of the current testbench; `<callback>` is a Tcl procedure called when it ends (capability `schematic-simulation-integration`; until M9 the call answers CORE-NOTIMPL) |
| `xschem raw <what> ...`, `xschem raw_clear` | Query or unload the loaded simulation results (`raw value`, `values`, `index`, `points`, `datasets`, `loaded`, `rawfile`, `sim_type`, `vars`, `list`; capability `schematic-simulation-integration`, M9; until then `raw loaded` returns -1 and the others answer CORE-NOTIMPL) |

`xschem version` SHALL return the pinned compatibility version string. The table SHALL list every subcommand of the `xschem_subcommands` lists of `tests/data/schem/inventory.json` (checked 2026-11-21: `annotate_op`, `change_sch_path`, `descend`, `expandlabel`, `get`, `getprop`, `go_back`, `hilight_instname`, `instance_list`, `load_new_window`, `logic_set`, `netlist`, `place_symbol`, `raw`, `raw_clear`, `raw_read`, `rebuild_connectivity`, `redraw`, `reset_caches`, `save`, `search`, `select`, `selected_set`, `set`, `setprop`, `simulate`, `translate`, `unhilight_all`, `unselect_all`, `update_all_sym_bboxes`). Any other subcommand SHALL raise the error `xschem command '<cmd>' is not supported`, and so SHALL a `get` key not listed in the table (`xschem command 'get <key>' is not supported`). The result formats of the supported subcommands SHALL equal xschem's. The list SHALL be fixed by an inventory script over the `xschem` calls in the three pinned PDK libraries, kept as a test that fails when a PDK bump adds a subcommand that is not in the table (Q-SCHEM-10, `Resolution: other`).

Compatibility: exact

(xschem ref: scheduler.c:1-7433)

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

### Requirement: Command logging
Every `schematic` command issued by the GUI, a gesture or a key binding SHALL be recorded in the command log with its arguments, so that a session can be replayed by a script to produce the same files (capability `hefesto-architecture`).

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Replay a session
- **GIVEN** a command log recorded while a user drew a schematic by hand
- **WHEN** the log is run as a script on an empty document
- **THEN** the saved file SHALL be byte-identical to the one the user saved
