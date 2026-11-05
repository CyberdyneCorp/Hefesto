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
3. A line starting with `+` SHALL be appended to the current logical line. Before appending, one space SHALL be appended to the logical line unless the continuation line itself ends with a space. This test looks at the continuation line, not at the logical line (Q-SCRIPT-32). The text appended is the continuation line without its `+`.
4. Every other line starts a new logical line.

Comment lines do not break continuations. Tokens SHALL be produced by splitting on single spaces, so consecutive spaces yield empty tokens, which are later skipped.
Compatibility: exact

(Magic ref: tcltk/readspice.tcl:58-80,89-90)

#### Scenario: Continuation across a comment
- **GIVEN** lines `.subckt inv A Y`, `* comment` and `+ VDD VSS`
- **THEN** the logical line SHALL be `.subckt inv A Y  VDD VSS`, which has two spaces between Y and VDD and so yields one empty token

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
   - Find the port index by name t. If not found, try t with `[`/`]` mapped to `<`/`>`. If still not found, try `<`/`>` mapped to `[`/`]`. If still not found, try a case-insensitive comparison against each port name for indices 0..`port last`. If still not found, try a case-insensitive match against the collected label texts: on a match, `goto` that label and `port <label> make n`.
   - If a port was found or created, set its index to n, mark the cell changed if the old index ≠ n, increment n, and record the mapping t → matched name.
   - If nothing matched, run `goto t`. If that returns a layer, `port t make n`, increment n and mark changed. Record t → t.
5. If changed, print `Cell <C> port order was modified.` Because step 2 moves every pre-existing port, any cell that already had named ports is always reported as changed (Q-SCRIPT-32).

Ports of C that are absent from the netlist SHALL keep the high indices assigned in step 2.
Compatibility: exact

(Magic ref: tcltk/readspice.tcl:100-223)

#### Scenario: Reordering ports
- **GIVEN** cell `inv` has ports Y=1, A=2, VDD=3, VSS=4
- **WHEN** a netlist line `.subckt inv A Y VDD VSS` is read
- **THEN** after annotation, A=1, Y=2, VDD=3, VSS=4, and `Cell inv port order was modified.` SHALL be printed

#### Scenario: Already-ordered ports still reported
- **GIVEN** cell `inv` has ports A=1, Y=2, VDD=3, VSS=4
- **WHEN** a netlist line `.subckt inv A Y VDD VSS` is read
- **THEN** the indices SHALL be unchanged and `Cell inv port order was modified.` SHALL still be printed

#### Scenario: Bus-bracket mapping
- **GIVEN** cell `reg` has port `D<0>` and the netlist pin is `D[0]`
- **THEN** port `D<0>` SHALL receive the netlist position of `D[0]`

#### Scenario: Extra layout port
- **GIVEN** cell `inv` has ports A=1, Y=2, VDD=3, VSS=4, BODY=5 and the netlist pins are `A Y VDD VSS`
- **THEN** BODY SHALL end with an index above 4 (verify against Magic)

### Requirement: Port direction from PININFO
A `.PININFO` line inside a loaded subcircuit SHALL be processed token by token. Each token has the form `name:dir`. If `name` was recorded for this subcircuit and the port's current class is `default`, the class SHALL be set according to dir: `B` → `inout`, `I` → `input`, `O` → `output`. Other values are ignored, and so are ports whose class is not `default`. An unrecorded non-empty name SHALL print `.PININFO error:  Pin <name> not found.` on stderr. See Q-SCRIPT-19 for the Tcl 9 incompatibility.
Compatibility: exact

(Magic ref: tcltk/readspice.tcl:227-247)

#### Scenario: CDL pin directions
- **GIVEN** a CDL file with `.SUBCKT inv A Y VDD VSS` and `*.PININFO A:I Y:O VDD:B VSS:B`
- **WHEN** `readspice inv.cdl` runs
- **THEN** A SHALL be class `input`, Y `output`, and VDD and VSS `inout`
