# LVS Setup File

## Purpose
Defines the LVS setup file and the commands that PDK setup files use to configure a comparison. Examples of such files are `sky130A_setup.tcl`, `gf180mcuD_setup.tcl` and `sg13g2_setup.tcl`. The commands covered are:
- `permute`
- `property` and its sub-options: `default`, `add`, `remove`/`delete`, `tolerance`, `series`/`parallel`, `associate`, `derive`, `topology`
- `equate classes`
- `ignore`
- `model`
- `global`/`convert`
- `flatten`
- `format`
- `cells`, `ports` and other queries
- the cell-reference syntax shared by all of them

**Compatibility adaptation.** This capability is about netgen, not Magic. The classes in `docs/spec-conventions.md` apply relative to **netgen** (`RTimothyEdwards/netgen` @ `93dcda14d36349d0b0ca63168d3cbbab20fe2ece`, version 1.5.327):
- `exact`: same accepted syntax, effects and printed text
- `behavioral`: same effect
- `informative`: netgen detail
- `native`: cites `(Hefesto decision: docs/decisions/product.md#P-5)`

Traces use `(netgen ref: path/file.c:line)`. They are for traceability only (capability `hefesto-licensing`).

Other capabilities own related parts:
- netlist reading: `lvs-netlist-input`
- the effects of these settings on matching: `lvs-device-reduction`, `lvs-property-matching`, `lvs-hierarchy`
- the `lvs` command that runs the setup file: `lvs-command`

## Requirements

### Requirement: Setup file language and runtime
A setup file SHALL be a Tcl script. It SHALL be evaluated in the embedded, sandboxed Tcl interpreter of decision X-3, on every platform including iPadOS. The LVS commands of this capability SHALL be available unqualified and also as `netgen::<cmd>`. Standard Tcl control flow and list and string commands SHALL be available, because PDK setup files use them: `if`, `foreach`, `set`, `lappend`, `lsearch`, `regexp`, `catch`, `info`, `string`, `puts`.

`$env(NAME)` SHALL read from Hefesto settings instead of the process environment (decision X-4). Setup files reach it through `catch {format $env(NETGEN_COLUMNS)}`. See Q-LVS-15.
See Q-LVS-27 for App Store constraints. netgen quirks that this and the other `lvs-*` capabilities replicate are governed by the `netgen-compat` setting of capability `hefesto-magic-compat-mode` (Q-LVS-28).
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: Same setup file on iPad
- **GIVEN** `sky130A_setup.tcl` from the PDK store
- **WHEN** LVS runs on iPadOS and on macOS with the same inputs
- **THEN** the same setup commands SHALL take effect and the comparison results SHALL be identical

### Requirement: Setup file evaluation
The setup file named in `lvs` SHALL be evaluated after both netlists are read and the top cells are assigned as circuit 1 and circuit 2. It SHALL be evaluated before any comparison or logging starts.

**Line accumulation.** The file SHALL be read line by line, counting lines from 1. Lines SHALL be appended (each with a newline) to a pending command until the pending text is a complete Tcl command. The complete command SHALL then be evaluated in the `netgen` namespace.

**Errors.** An error SHALL NOT stop processing. The error SHALL be reported on stderr as `Error <setupfile>:<line> (ignoring), <message>`, where `<line>` is the number of the last line read and trailing newlines are removed from `<message>`. If text is still pending at end of file, it SHALL be evaluated so that a meaningful error is reported. If any error occurred, `Warning:  There were errors reading the setup file` SHALL be printed afterwards.

**Setup file name.**
- An empty name SHALL print `No setup file specified.  Using trivial default setup.` (with surrounding blank lines) and apply `permute default` and `property default`.
- A name that does not exist and does not contain `nosetup` SHALL print `Error:  Setup file <f> does not exist.` and end the `lvs` command.
- A missing name that contains `nosetup` SHALL print `No setup file specified.  Continuing without a setup.` and continue with no setup.
- A file that exists but cannot be opened SHALL print `Error:  Cannot read the setup file <f>`.
- Otherwise, before evaluation, `Reading setup file <f>` SHALL be printed (with surrounding blank lines).
Compatibility: exact
(netgen ref: tcltk/netgen.tcl.in:494-539)

#### Scenario: Error is skipped
- **GIVEN** line 12 of `setup.tcl` is `property nosuchcell tolerance {w 0.01}`
- **WHEN** the setup file is evaluated
- **THEN** stderr SHALL show `Error setup.tcl:12 (ignoring), No such cell!`, lines 13 onwards SHALL still be evaluated, and `Warning:  There were errors reading the setup file` SHALL be printed

#### Scenario: Default setup
- **WHEN** `lvs a.spice b.spice ""` is run
- **THEN** default permutations and default property handling SHALL be applied

### Requirement: Cell reference syntax
Every argument documented as `valid_cellname` SHALL accept the forms below. Circuit 1 and circuit 2 are assigned by `compare` or by the `lvs` command.

**One-word forms:**

| Form | Resolves to |
|---|---|
| `cell` | That cell. If it is a file's top cell, its file number is used. Otherwise the file is a wildcard meaning "all netlists". |
| `fnum` (an integer) | The top cell of netlist fnum |
| `-circuit1` | The cell assigned as circuit 1 |
| `-circuit2` | The cell assigned as circuit 2 |
| `-current` | The most recently read cell |

**Two-element list forms:**

| Form | Resolves to |
|---|---|
| `{cell fnum}` or `{fnum cell}` | `cell` in netlist fnum |
| `{cell file}` or `{file cell}` | `cell` in the netlist whose top cell is `file`. Whichever element names a top cell is taken as the file. |
| `{-circuit1 cell}`, `{-circuit2 cell}`, `{-current cell}`, or the same with the keyword second | `cell` in the netlist of that circuit |
| `{* cell}` | `cell` in all netlists |

**Errors.** These conditions SHALL raise an error with the given text:

| Condition | Message |
|---|---|
| Unknown cell | `No such cell!` (one-word form) or `No such cellname!` (list form) |
| Cell not present in the named file | `Cell is not in file!` |
| Unknown file number | `No such file number!` |
| A circuit keyword used before circuits are assigned | `No circuit has been declared for comparison` |
| `*` used alone | `Wildcards must be used with a valid cellname` |

Commands that take a wildcard file SHALL apply to both compared circuits when circuits are assigned.
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:285-658)

#### Scenario: PDK idiom
- **GIVEN** circuit 1 is `layout.spice` (file 0) and contains device cell `sky130_fd_pr__nfet_01v8`
- **WHEN** `property "-circuit1 sky130_fd_pr__nfet_01v8" tolerance {w 0.01}` is evaluated
- **THEN** the tolerance SHALL be set on (`sky130_fd_pr__nfet_01v8`, 0) only

### Requirement: permute command
`permute` SHALL declare pairs of interchangeable pins on a device class. Forms:

- **`permute`** (no arguments) or **`permute default`**: for every cell in the database, declare these pairs:
  - source/drain on every transistor class (nmos, pmos, fet, and their 4-pin forms)
  - end_a/end_b on every resistor class and inductor
- **`permute transistors`**, **`permute resistors`**, **`permute inductors`**, **`permute capacitors`**: declare only the corresponding pair. The capacitor pair is top/bottom; `permute default` does not include it.
- **`permute [pins] <valid_cellname> <pin1> <pin2>`**: declare that pair on that cell. On success it SHALL print `Model <c> pin <p1> == <p2>`. If the model is missing it SHALL print `No such model <c>`; if a pin is missing, `No such pin <p> in model <c>`. On either failure it SHALL then print `Unable to permute model <c> pins <p1>, <p2>.`
- **`permute forget [<valid_cellname> [<pin1> <pin2>]]`**:
  - with no cell, remove all declared pairs from every cell
  - with a cell only, remove all pairs from that cell and print `No permutations on circuit <c>`
  - with pins, remove that pair in either order and print `Model <c> pin <p1> != <p2>`

  After a cell-wide `forget`, the cell's pair list SHALL be empty. netgen frees the list without clearing it (see Q-LVS-5).

**Keyword matching.** Keywords SHALL use unique-prefix matching against `transistors resistors capacitors inductors default forget pins`.

**Pin names.** Pins SHALL be named by port name. For placeholder devices the names are `1`, `2`, …; PDK setups therefore write `permute "-circuit1 $dev" 1 2`.

**Duplicates and file wildcard.** A pair that is already declared SHALL not be added again. A wildcard file SHALL apply the declaration to both compared circuits.
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:4095-4230, base/netcmp.c:6879-7015)

#### Scenario: Resistor pins of a placeholder
- **GIVEN** circuit 1 placeholder `sky130_fd_pr__res_generic_po` has ports `1 2 3`
- **WHEN** `permute "-circuit1 sky130_fd_pr__res_generic_po" 1 2` is evaluated
- **THEN** ports 1 and 2 SHALL be permutable, and `Model sky130_fd_pr__res_generic_po pin 1 == 2` SHALL be printed

### Requirement: property default
`property default` SHALL apply these defaults to every cell currently in the database:

| Classes | Merge rules (see capability `lvs-device-reduction`) | Other effects |
|---|---|---|
| Transistor classes (nmos, pmos, fet, and their 4-pin forms) | `w`: parallel-add + parallel-critical | Delete properties of interest `as`, `ad`, `ps`, `pd` |
| Resistor classes | `w`: parallel-par + series-critical<br>`l`: series-add + parallel-critical<br>`value`: series-add + parallel-par | Enable series combination |
| Capacitor classes, including ecap and cap3 | `area`: parallel-add + series-par<br>`value`: parallel-add + series-par | Enable series combination |
| Inductor | `value`: series-add + parallel-par | Enable series combination |

A merge rule on a key that is not a property of interest SHALL print `No property <k> found for device <c>` and have no other effect.
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:3519-3566)

#### Scenario: Default transistor handling
- **GIVEN** a fet cell with properties of interest L, W, M, as, ad
- **WHEN** `property default` runs
- **THEN** `as` and `ad` SHALL no longer be compared, and parallel devices with equal L SHALL be summable by W

### Requirement: property add and type declarations
`property <valid_cellname> [add|create] <spec> …` SHALL add properties of interest. The option word is optional: if the third word is not an option keyword, `add` is assumed. Each spec SHALL be one of:

| Spec | Effect |
|---|---|
| `key` | A double property with tolerance 0.01, if `key` itself parses as a number (see Q-LVS-6). Otherwise a string property with tolerance 0. |
| `{key type}` | `integer`: tolerance 0, default 0.<br>`double`: tolerance 0.01.<br>`string`: tolerance 0, exact match. |
| `{key tol}` | A double property with tolerance `tol`. A non-numeric `tol` is undefined in netgen (see Q-LVS-6). |
| `{key type tol}` | `integer`: integer tolerance.<br>`double` or `value`: real tolerance.<br>`string`: integer prefix length n (n > 0 compares only the first n characters, case-insensitively).<br>`expression`: an expression-default property. |

Type keywords SHALL use unique-prefix matching against `integer double value string expression`.

Adding a key that already exists SHALL print `Device <c> already has property named "<k>"` and keep the old definition. An unknown cell SHALL print `No device <c> found for Property<Type>()`. With a wildcard file and assigned circuits, the property SHALL be added to the cell in both circuits.

`property <valid_cellname>` with no further arguments SHALL return a list of `{key type tolerance}` triples. The type is one of `double value integer expression string`.
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:3640-3680, tcltk/tclnetgen.c:3792-3911, base/netgen.c:1125-1260)

#### Scenario: Typed tolerance
- **WHEN** `property "-circuit2 nfet" add {nf integer 0} {w double 0.02}` is evaluated
- **THEN** `nf` SHALL be compared exactly as an integer, and `w` SHALL be compared with relative tolerance 0.02

### Requirement: property remove, tolerance and associate
`property <c> remove|delete [key …]` SHALL behave as follows:
- With keys, it SHALL remove those properties of interest. A missing key SHALL print a diagnostic and nothing else.
- With no keys, it SHALL remove every property of interest of the cell.

Removed properties SHALL no longer be compared, and a missing value for them SHALL no longer be an error.

`property <c> tolerance {key tol} …` SHALL set the tolerance of existing properties:
- an integer `tol` sets an integer slop
- a real `tol` sets a real slop
- `inf…` (case-insensitive prefix `inf`) sets an integer slop of 2^30 and a real slop of 1e300

A pair that is not a two-element list SHALL set the result `Not a {key tolerance} pair list.` and continue.

`property <c> associate {key pin} …` SHALL tie a property to a pin, for use with permutable pins (capability `lvs-property-matching`). A bad pair SHALL set `Not a {key pin} pair list.`
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:3913-3984, base/netgen.c:1060-1115)

#### Scenario: Remove multiplier
- **WHEN** `property "-circuit1 sky130_fd_pr__nfet_01v8" delete mult` is evaluated
- **THEN** property `mult` SHALL be ignored in comparisons of that device in circuit 1

### Requirement: property series and parallel controls
`property <c> series|serial|parallel` SHALL control series and parallel combination of a device class.

**Query.** With no further argument, it SHALL return whether series combination is enabled (for `series`/`serial`) or parallel combination is enabled (for `parallel`).

**Arguments.** Each later argument SHALL be either a yes/no word or a `{key combine}` pair.

Yes/no words SHALL use unique-prefix matching:
- `on yes true enable allow` enable combination
- `off no false disable prohibit` disable combination

`{key combine}` sets the merge rule of `key` for that direction and enables combination. `combine` SHALL use unique-prefix matching against `none par add critical`:

| Combine word | Series effect | Parallel effect |
|---|---|---|
| `none` | Clears the series rule and disables series combination | Clears the parallel rule and disables parallel combination |
| `par` | Sets series-par (replaces other series bits) | Adds parallel-par |
| `add` | Adds series-add | Adds parallel-add |
| `critical` | Adds series-critical | Adds parallel-critical |

Combination SHALL be enabled for every word except `none`. A malformed pair SHALL set `Not a {key merge_type} pair list.`

**Global forms.**
- `property parallel none` SHALL disable parallel combination for all existing cells and for every cell defined later.
- `property parallel all` SHALL undo that.
- `property parallel connected` SHALL stop treating single-connection (no-connect) pins as equivalent when parallel devices are detected.
- `property parallel open` SHALL restore the default, which is to treat them as equivalent.
- `property series none|all` SHALL disable or enable series combination for all existing cells.
- Any other word SHALL be an error: `Bad option, should be property parallel none|all|connected` or `Bad option, should be property series none|all`.

**Deprecated merge.** `property <c> merge {key type} …` SHALL accept:

| Type | Merge rule |
|---|---|
| `none` | no rule |
| `add` | parallel-add |
| `add_critical` | parallel-add + legacy parallel-critical |
| `par` or `parallel` | parallel-par |
| `par_critical` or `parallel_critical` | parallel-par + legacy parallel-critical |
| `ser`, `serial` or `series` | series-add |
| `ser_critical`, `serial_critical` or `series_critical` | series-add + legacy series-critical |

`merge` SHALL replace all merge bits of the key.
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:3567-3603, tcltk/tclnetgen.c:3684-3790, tcltk/tclnetgen.c:3986-4049, base/netgen.c:901-945)

#### Scenario: sky130 resistor setup
- **WHEN** `property "-circuit1 r1" series enable`, `property "-circuit1 r1" series {w critical}` and `property "-circuit1 r1" series {l add}` are evaluated
- **THEN** series resistors of class `r1` with equal `w` SHALL be combinable, with their `l` summed

### Requirement: property topology and derive
**Topology.**
- `property topology strict` SHALL make it an error for a device record to have both M > 1 and S > 1 when combination would need it.
- `property topology relaxed` (the default) SHALL allow it.
- `property topology` SHALL return `Strict topology property matching.` or `Relaxed topology property matching.`

**Derived properties.** `property <c> derive area|perimeter <wkey> <lkey>` SHALL declare a double property of interest `area` or `perimeter` (tolerance 0.01). For every instance of `<c>` that has both `<wkey>` and `<lkey>` and lacks the derived key, it SHALL add:
- area = w × l
- perimeter = 2 × (w + l)

Fewer than 6 words SHALL report a wrong-argument-count error.
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:3605-3638, tcltk/tclnetgen.c:4051-4078, base/netcmp.c:8651-8805)

#### Scenario: Derived area
- **GIVEN** an instance of `cap_mim` with `w=2` and `l=3`
- **WHEN** `property cap_mim derive area w l` is evaluated
- **THEN** that instance SHALL gain `area` = 6

### Requirement: equate classes
`equate [-unique] classes <cellA> <cellB>` SHALL declare two device classes in different netlists equivalent, so that their instances are matched as the same device type.

**Both cells resolve to specific netlists.** Their class identity SHALL be made equal: the second takes the first's identity, unless the first is a renamed duplicate, in which case the reverse.

With `-unique`, if either name also exists in the other netlist under a different name, the first cell SHALL be given a fresh identity first. The fresh identity is the name plus one random uppercase letter, retried until no such cell exists (see Q-LVS-29). This keeps it from also matching that same-named cell.

**A wildcard is involved.** The pair SHALL be recorded as a correspondence to apply during hierarchical queue building. See capability `lvs-hierarchy`.

**Messages.**
- Success: `Device classes <a> and <b> are equivalent.`
- Unresolvable cell: `Cell to equate does not exist.` with result 0.
- Both cells in one netlist: `Cells to equate are in the same netlist.` with an error.

**Pin lists.** The six-word form `equate classes <cellA> <pinsA> <cellB> <pinsB>` SHALL also rename or renumber pins of module or subcircuit cells from paired lists. Both lists SHALL have the same length and SHALL be either all `{name}` or all `{name index}` elements. The errors are:
- `Device class is not black box or subcircuit!`
- `Device classes are different, cannot match pins!`
- `Pin lists are different length, cannot match pins!`
- `List length does not match  number of pins in cell.`
- `All list items must have one or two entries.`
- `All list items must have the same number of entries.`
- `No pin order information  for the cell.`
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:3243-3392, base/netcmp.c:7258-7340)

#### Scenario: Prefixed layout cell
- **GIVEN** circuit 1 has cell `XY_inv` and circuit 2 has `inv`
- **WHEN** `equate classes "-circuit1 XY_inv" "-circuit2 inv"` is evaluated
- **THEN** instances of `XY_inv` and `inv` SHALL be counted as the same class in the subcircuit summary

### Requirement: ignore command
`ignore class|shorted <valid_cellname>` SHALL record the class in an ignore list. The keyword SHALL use unique-prefix matching against `class shorted`.

**`class`.** All existing instances of the class in that netlist SHALL be deleted. Instances read later SHALL be dropped (capability `lvs-netlist-input`).

**`shorted`.** Existing instances whose pins all connect to one net SHALL be removed, and later shorted instances SHALL be dropped.

A wildcard file with assigned circuits SHALL apply to both circuits. Fewer than 3 words SHALL report a wrong-argument-count error, even though the help text suggests the keyword is optional. In netgen, a first word that is neither keyword leaves the type undefined; Hefesto SHALL report an error instead (see Q-LVS-22).
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:2871-2916, base/netcmp.c:7216-7240, base/objlist.c:484-600)

#### Scenario: Ignore fill cells
- **WHEN** `ignore class "-circuit1 sky130_fd_sc_hd__fill_2"` is evaluated
- **THEN** no instance of `sky130_fd_sc_hd__fill_2` in circuit 1 SHALL take part in comparison

### Requirement: model command
`model <valid_cellname> <class>` SHALL set the class of a cell. `<class>` SHALL use unique-prefix matching against `undefined nmos pmos pnp npn resistor capacitor diode inductor module blackbox xline moscap mosfet bjt subcircuit copy`.

The cell's port count SHALL be validated as follows; a wrong count SHALL fail with `Wrong number of ports for device`:

| Keyword | Allowed ports | Resulting class |
|---|---|---|
| `nmos`, `pmos`, `mosfet` | 3 or 4 | the 4-pin variant when 4 |
| `pnp`, `npn`, `bjt` | 3 | as named |
| `resistor` | 2 or 3 | res3 when 3 |
| `capacitor` | 2 or 3 | cap3 when 3 |
| `diode`, `inductor` | 2 | as named |
| `xline` | 4 | transmission line |
| `moscap` | 3 | ecap |
| `module`, `blackbox` | any | module |
| `subcircuit` | any | subcircuit |

**`copy`.** `copy` SHALL take the class of the same-named cell in the other compared circuit. It requires assigned circuits (`Circuits have not been queued for comparison.`). The cell must belong to circuit 1 or circuit 2 (`The referenced netlist is not being compared.`).

**Query.** `model <c>` SHALL return the class as one of `nmos pmos mosfet bipolar npn pnp resistor capacitor subcircuit module blackbox undefined`. Module is reported as `blackbox` when automatic black-boxing is on.

**Black-box switch.** `model blackbox on|off` SHALL set automatic black-boxing of empty subcircuits. `model blackbox` SHALL return its state.
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:1554-1770)

#### Scenario: Treat a placeholder as a resistor
- **GIVEN** a 2-port placeholder `rpoly`
- **WHEN** `model "-circuit1 rpoly" resistor` is evaluated
- **THEN** `rpoly` SHALL have class resistor and SHALL be eligible for resistor permutation and series combination

### Requirement: global and convert commands
`global <valid_cellname> <pattern> …` SHALL change every net in the cell's hierarchy whose name matches a pattern (wildcards allowed) to global scope. It SHALL return the number of nets changed.

`convert <valid_cellname>` SHALL replace each global net of the cell with a local net plus a new port appended at the end of the port list. All parent cells SHALL be connected to the corresponding global net, which is created if missing. Hierarchical comparison SHALL apply `convert` to both compared cells automatically (capability `lvs-hierarchy`).
Compatibility: behavioral
(netgen ref: tcltk/tclnetgen.c:2804-2869, base/flatten.c:924-1047, base/query.c:428-520)

#### Scenario: Global supply becomes a port
- **GIVEN** subcircuit `inv` uses global `VDD` and has ports `A Y`
- **WHEN** `convert inv` runs
- **THEN** `inv` SHALL have ports `A Y VDD`, and every instance of `inv` SHALL connect its new pin to the parent's `VDD`

### Requirement: flatten command
`flatten` SHALL take these forms:
- **`flatten <valid_cellname>`**: flatten the whole contents of the cell. It SHALL print `Flattening contents of cell <c>`.
- **`flatten class <valid_cellname>`**: flatten every instance of that class throughout its netlist. It SHALL print `Flattening instances of <c> in file <top>`.
- **`flatten class <parent> <valid_cellname>`**: flatten only the instances inside `<parent>`. It SHALL print `Flattening instances of <c> in cell <p> within file <top>`. It SHALL fail with `No such cell.` or `No instances found to flatten.`
- **`flatten prohibit|deny <valid_cellname>`**: mark the cell so that hierarchical comparison never flattens it. It SHALL print `Will not flatten instances of <c> in file <top>`.

The naming of flattened objects is specified in capability `lvs-hierarchy`.
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:1022-1090)

#### Scenario: Prohibit flattening
- **WHEN** `flatten prohibit "-circuit1 sram_macro"` is evaluated before comparison
- **THEN** a mismatch in `sram_macro` SHALL be reported without flattening it into its parent

### Requirement: Query and output commands available to setup files
These commands SHALL be available:

**`format [w1 [w2]]`** SHALL set the two report column widths (default 41 and 41). One argument SHALL set both columns. With no arguments it SHALL return `{w1 w2}`. A width ≤ 0 SHALL set the result `Column width cannot be zero or less`, but the widths are still applied (see Q-LVS-16). Column end positions are left = w1 + 2 and right = left + w2 + 3.

**`cells [list] [-all] [-top] [<valid_cellname>]`** SHALL report cells:
- `list` returns a Tcl list instead of printing
- `-all` includes primitive cells
- `-top` lists only top cells
- a cell argument restricts the output to its netlist

PDK setup files use `cells list -all -circuit1`.

**`ports <c>`** SHALL print the ports of the cell.

**Other commands.** `nodes`, `elements`, `instances`, `contents`, `describe`, `leaves`, `log`, `readnet`, `readlib`, `writenet`, `canonical` and `help` SHALL be available as diagnostic commands. Their text output is informative.
Compatibility: behavioral
(netgen ref: tcltk/tclnetgen.c:2056-2110, tcltk/tclnetgen.c:1467-1552, tcltk/tclnetgen.c:110-262)

#### Scenario: List all cells of circuit 1
- **GIVEN** circuit 1 contains `inv` and the primitive `nfet`
- **WHEN** `cells list -all -circuit1` is evaluated
- **THEN** the result SHALL be a list containing both `inv` and `nfet`
