# SPICE Port-Order Annotation (readspice)

## Purpose
Specifies the `readspice <netlist>` command. It is defined by `readspice.tcl` and loaded at startup when present. The command annotates already-loaded layout cells with the port order and port direction found in a SPICE, CDL or Spectre netlist. It is not a schematic-to-layout generator; for that see `magic::netlist_to_layout` in capability `pcell-toolkit`. The port numbers it assigns end up in extracted netlists (see the extraction and netlist-generation capabilities), so the numbering algorithm is `exact`.

## Requirements

### Requirement: Netlist file resolution
`readspice f` SHALL open f for reading. If that fails, it SHALL try `<root>.spi`, `<root>.spc`, `<root>.spice`, `<root>.sp`, `<root>.ckt` and `<root>.cdl` in that order, where `<root>` is f with its last extension removed. If none opens, it SHALL print `Error:  Can't read netlist file <f>` on stderr and return `1`. The file SHALL be treated as CDL when its extension is `.cdl` or when the `.cdl` fallback was used. On success it SHALL print `Annotating port orders from <file>`.
Compatibility: exact

(Magic ref: tcltk/readspice.tcl:16-56)

#### Scenario: Extension fallback
- **GIVEN** only `cells.spice` exists
- **WHEN** `readspice cells` runs
- **THEN** `cells.spice` SHALL be read

#### Scenario: Missing file
- **WHEN** `readspice nofile.sp` runs and no candidate exists
- **THEN** the result SHALL be `1` and the error message SHALL be printed

### Requirement: Line preprocessing
The reader SHALL split the file into lines and process them as follows:
1. In CDL mode, a line starting with `*.` whose characters 2-8 equal `pininfo` (case-insensitive) SHALL have its leading `*` removed.
2. Any other line starting with `*` SHALL be discarded.
3. A line starting with `+` SHALL be appended to the current logical line, without its `+`. Whether one space is first appended to the logical line depends on the `magic-compat` setting (Q-SCRIPT-32, resolution: compat for the tokenization):
   - ON (the default), as in Magic: one space is appended unless the continuation line itself ends with a space. This tests the continuation line, not the logical line, so when the logical line does not end with a space and a continuation such as `+c d ` has no space after `+` and ends with one, the last token of the logical line and the first token of the continuation are joined (`…b` and `+c d ` give `…bc d `).
   - OFF: one space is appended unless the logical line already ends with a space, so a `+` line always starts a new token.
4. Every other line starts a new logical line.

Comment lines do not break continuations. Tokens SHALL be produced by splitting on single spaces, so consecutive spaces yield empty tokens, which are later skipped.
Compatibility: exact

(Magic ref: tcltk/readspice.tcl:58-80,89-90)

#### Scenario: Continuation across a comment
- **GIVEN** lines `.subckt inv A Y`, `* comment` and `+ VDD VSS`
- **THEN** the logical line SHALL be `.subckt inv A Y  VDD VSS`, which has two spaces between Y and VDD and so yields one empty token, in both `magic-compat` settings

#### Scenario: Continuation joins tokens with magic-compat ON
- **GIVEN** `magic-compat` ON and lines `.subckt inv A Y` and `+VDD VSS ` (ending with a space)
- **THEN** the logical line SHALL be `.subckt inv A YVDD VSS `, whose pin candidates are `A`, `YVDD`, `VSS` and an empty token

#### Scenario: Continuation starts a token with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the lines of the previous scenario
- **THEN** the logical line SHALL be `.subckt inv A Y VDD VSS `, whose pin candidates are `A`, `Y`, `VDD`, `VSS` and an empty token

### Requirement: Subcircuit recognition
The keyword of a logical line is its first token in lower case. A keyword `.subckt` SHALL begin a subcircuit with cell name = token 1 and pin candidates = tokens 2..end. A Spectre line whose keyword is `inline` and whose token 1 is `subckt` (case-insensitive) SHALL be re-tokenized by splitting on space, tab, `(` and `)`. Its first token is dropped and it is then treated as `.subckt`. A keyword `.ends` SHALL end the subcircuit. If the cell is not loaded (`cellname list exists` returns 0), the reader SHALL print `Cell <name> in netlist has not been loaded.` and skip the subcircuit. The whole run SHALL be wrapped in nested redraw suspension.
Compatibility: exact

(Magic ref: tcltk/readspice.tcl:84-112,224-253)

#### Scenario: Unloaded cell
- **GIVEN** the netlist contains `.subckt nand2 A B Y VDD VSS` and `nand2` is not loaded
- **THEN** the message `Cell nand2 in netlist has not been loaded.` SHALL be printed and no cell SHALL change

### Requirement: Port renumbering algorithm
For a loaded cell C with pin candidates T (tokens 2..end of the `.subckt` line), the reader SHALL do the following:
1. Load C and set the box to 0 0 0 0. Print `Annotating cell <C>`.
2. Let npins = |tokens| − 1 and highport = `port last`. Move every existing port out of the way: walking ports from `port first` via `port <p> next` while p ≠ −1 and p ≤ highport, assign index outport, outport+1, … to each port that has a name. outport starts at max(highport, npins).
3. Collect all labels of C. Select the top cell, unexpand, select the area of labels, expand, take element 1 of `what -list`, and clear the selection.
4. Set n = 1. For each token t in T, in order:
   - Stop at the first token containing `=` (parameters follow).
   - Skip empty tokens.
   - Find the port index by name t. If not found, try t with `[`/`]` mapped to `<`/`>`: with `magic-compat` ON as Magic's `string map {[ < ] >]}`, which maps `]` to `>]` (so `D[0]` becomes `D<0>]` and never matches), with it OFF `]` to `>` (Q-SCRIPT-47, resolution: compat). If still not found, try `<`/`>` mapped to `[`/`]`. If still not found, try a case-insensitive comparison against each port name for indices 0..`port last`. If still not found, try a case-insensitive match against the collected label texts: on a match, `goto` that label and `port <label> make n`.
   - If a port was found or created, set its index to n, increment n, and record the mapping t → matched name.
   - If nothing matched, run `goto t`. If that returns a layer, `port t make n` and increment n. Record t → t.
5. If some port's final index differs from its index before step 2, or a port was created in step 4, print `Cell <C> port order was modified.` This holds in both `magic-compat` settings (Q-SCRIPT-32, resolution: fix for the message). Magic compares each pin's index after step 2 with n, so it reports every cell that already had named ports.

Ports of C that are absent from the netlist SHALL keep the high indices assigned in step 2.
Compatibility: exact

(Magic ref: tcltk/readspice.tcl:100-223)

#### Scenario: Reordering ports
- **GIVEN** cell `inv` has ports Y=1, A=2, VDD=3, VSS=4
- **WHEN** a netlist line `.subckt inv A Y VDD VSS` is read
- **THEN** after annotation, A=1, Y=2, VDD=3, VSS=4, and `Cell inv port order was modified.` SHALL be printed

#### Scenario: Already-ordered ports not reported
- **GIVEN** cell `inv` has ports A=1, Y=2, VDD=3, VSS=4
- **WHEN** a netlist line `.subckt inv A Y VDD VSS` is read
- **THEN** the indices SHALL be unchanged and `Cell inv port order was modified.` SHALL NOT be printed (Magic prints it; golden deviation)

#### Scenario: Bus-bracket mapping
- **GIVEN** cell `reg` has port `D<0>` and the netlist pin is `D[0]`
- **THEN** with `magic-compat` OFF port `D<0>` SHALL receive the netlist position of `D[0]`
- **AND** with `magic-compat` ON `D[0]` SHALL NOT match `D<0>`, which keeps the index step 2 gave it, as in Magic (Q-SCRIPT-47)

#### Scenario: Extra layout port
- **GIVEN** cell `inv` has ports A=1, Y=2, VDD=3, VSS=4, BODY=5 and the netlist pins are `A Y VDD VSS`
- **THEN** BODY SHALL end with an index above 4 (verify against Magic)

### Requirement: Port direction from PININFO
A `.PININFO` line inside a loaded subcircuit SHALL be processed token by token. Each token has the form `name:dir`. If `name` was recorded for this subcircuit and the port's current class is `default`, the class SHALL be set according to dir: `B` → `inout`, `I` → `input`, `O` → `output`. Other values are ignored, and so are ports whose class is not `default`. An unrecorded non-empty name SHALL print `.PININFO error:  Pin <name> not found.` on stderr. The direction letter is matched case-sensitively against the three literals with `switch`, which gives the result of Magic's Tcl 8 `case` on any embedded Tcl version (Q-SCRIPT-19, resolution: fix; Tcl 9 removed `case`).
Compatibility: exact

(Magic ref: tcltk/readspice.tcl:227-247)

#### Scenario: CDL pin directions
- **GIVEN** a CDL file with `.SUBCKT inv A Y VDD VSS` and `*.PININFO A:I Y:O VDD:B VSS:B`
- **WHEN** `readspice inv.cdl` runs
- **THEN** A SHALL be class `input`, Y `output`, and VDD and VSS `inout`
