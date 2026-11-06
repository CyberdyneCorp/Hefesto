# LVS Reports

## Purpose
Defines the outputs of LVS:
- which messages go to the comparison log (`comp.out`) and which go only to the console
- the two-column side-by-side layout
- the exact text of each report section: subcircuit summary, net and device mismatch fragments, pin table, property errors, final result
- the structured `-list` result
- the JSON file written by `lvs -json`
- the result classification exposed to Hefesto's API and GUI

**Compatibility adaptation.** This capability is about netgen, not Magic. The classes in `docs/spec-conventions.md` apply relative to **netgen** (`RTimothyEdwards/netgen` @ `93dcda14d36349d0b0ca63168d3cbbab20fe2ece`, version 1.5.327):
- `exact`: byte-identical lines and JSON formatting. Where netgen's order of lines comes from internal hash or list order, golden comparison is order-insensitive within the section, per decision X-1 and capability `hefesto-output-determinism`.
- `behavioral`: same content
- `informative`: netgen detail
- `native`: cites `(Hefesto decision: docs/decisions/product.md#P-5)`

Traces use `(netgen ref: path/file.c:line)`. They are for traceability only (capability `hefesto-licensing`).

## Requirements

### Requirement: Log and console routing
Report output SHALL go to two sinks:
- **Log** (when logging is active): a file, `comp.out` by default
- **Console**: the Hefesto console or stdout

There are two message kinds:
- **Log messages** SHALL be written to the log when logging is active. They SHALL also go to the console unless echo is off.
- **Console-only messages** SHALL never be written to the log.

The capability that defines each message marks it as console only where that applies. The following SHALL be console-only:
- `Contents of circuit N:` blocks
- `Circuit 1 contains … devices` summaries
- `Circuit was modified by parallel/series device merging.`
- `Flattened mismatched instances and attempting compare again.`
- `Circuits match with <k> symmetries.` from `verify`
- duplicate and shorted pin messages
- `No more changes can be made to series/parallel networks.`

**`lvs` logging sequence.** The `lvs` command SHALL do the following, in order:
1. Read the netlists and the setup file.
2. Unless the log name contains `nolog`, print `Comparison output logged to file <log>`, open the log for writing (truncating it), and turn echo off.
3. Turn echo on just before the final result and close the log at the end.

**`log` command.** `log [file <name>|start|end|reset|suspend|resume|echo [on|off]|put <text>…]` SHALL control logging. With no argument it means `resume` if a log is open, otherwise `start`. Its options are:

| Option | Effect |
|---|---|
| `start` | Open the log for writing |
| `resume` | Open the log for appending |
| `reset` | Truncate the log |
| `end`, `suspend` | Close the log |
| `file <name>` | Set the name, closing an open log and printing `Closed old log file "<name>".` |
| `put` | Write its arguments as a log message, plus a console-only newline when echo is on |

Errors:
- `Already logging output.`
- `Not logging data.`
- `No log file declared.  Use "log file <name>"`
- `Could not open log file.`
Compatibility: exact
(netgen ref: base/print.c:60-110, base/print.c:190-215, tcltk/tclnetgen.c:1890-2030, tcltk/netgen.tcl.in:541-550)

#### Scenario: Contents block not in log
- **WHEN** `lvs` compares a pair
- **THEN** `comp.out` SHALL NOT contain `Contents of circuit 1:`, but SHALL contain `Subcircuit summary:`

### Requirement: Two-column layout
Side-by-side lines SHALL use two column end positions, L = w1 + 2 and R = L + w2 + 3, where w1 and w2 are the `format` widths (default 41 and 41, giving L = 43 and R = 87).

**Row lines.** A row line SHALL be exactly R characters followed by a newline:
- positions 0 … L−1 hold the left text
- position L holds `|`
- positions L+1 … R−1 hold the right text

Each text SHALL be truncated to L−1 characters and padded with spaces. The same limit L−1 applies to the right text (see Q-LVS-16). Trailing spaces SHALL be kept.

**Dividers.** A divider SHALL be R `-` characters. A column divider SHALL also have `|` at position L.

**Header row.** The header row SHALL be `Circuit 1: <cell1>` on the left and `Circuit 2: <cell2>` on the right.

**Blank separator row.** The blank separator row between fragments SHALL be spaces with `|` at position L.

**Literal text.** Text placed in the columns SHALL be written literally. A `%` in a net or cell name SHALL appear as `%` (see Q-LVS-12).
Compatibility: exact
(netgen ref: base/netcmp.c:1505-1612, tcltk/tclnetgen.c:2056-2110)

#### Scenario: Default widths
- **WHEN** a header for cells `inv` and `inv` is written with default widths
- **THEN** the line SHALL be `Circuit 1: inv` padded to 43 characters, then `|`, then `Circuit 2: inv` padded to 43 characters, then a newline: 87 characters in total before the newline

### Requirement: Subcircuit summary section
For each compared pair, the log SHALL contain these lines, in order:
1. An empty line.
2. `Subcircuit summary:`
3. The header row.
4. A column divider.
5. One row per device class, in two groups.
   - **First group.** Classes in order of first appearance among circuit 1's devices. Left: `<class> (<C>)` or `<class> (<M>-><C>)` (capability `lvs-device-reduction`). Right: the equivalent circuit-2 class formatted the same way, with ` **Mismatch**` appended when the counts differ, or `(no matching element)` when there is none.
   - **Second group.** Circuit-2 classes with no circuit-1 equivalent. Left: `(no matching element)`. Right: the class entry.
6. A row `Number of devices: <n1>` | `Number of devices: <n2>`, with ` **Mismatch**` appended to both sides when the counts differ.
7. A row `Number of nets: <n1>` | `Number of nets: <n2>`, with the same mismatch rule.
8. A full divider (no `|`).

Any lines produced by flattening and pre-matching SHALL precede this section.
Compatibility: exact
(netgen ref: base/netcmp.c:3270-3552, base/netcmp.c:3890-3903)

#### Scenario: Device count mismatch
- **GIVEN** circuit 1 has 3 nfet and 2 pfet, and circuit 2 has 2 nfet and 2 pfet
- **WHEN** the summary is written
- **THEN** it SHALL contain the row `nfet (3)` | `nfet (2) **Mismatch**`, and the row `Number of devices: 5 **Mismatch**` | `Number of devices: 4 **Mismatch**`

### Requirement: Net mismatch report
When a mismatch is found and nets are reported, the log SHALL contain `NET mismatches: Class fragments follow (with fanout counts):` followed by the header row. Then, for each illegal net class:
1. An empty line.
2. For each fragment index n, up to the larger of the two circuits' fragment counts:
   a. A full divider before the first fragment, or a blank separator row before the others.
   b. A row `Net: <name>` | `Net: <name>`, with `(no matching net)` on a side that has no net at index n.
   c. One row per fanout entry, `  <class>/<pin> = <count>`. When the device's pin is permutable with others, the entry SHALL be `  <class>/(<pin1>|<pin2>…) = <count>`. Entries group, per net, all pins with the same class and pin label, and count them.
3. A full divider.

Fragments of the two circuits SHALL be paired to put equal net names side by side where possible.
Compatibility: exact
(netgen ref: base/netcmp.c:834-930, base/netcmp.c:1368-1500, base/netcmp.c:1965-2100)

#### Scenario: Swapped inputs
- **GIVEN** circuit 1 NAND input `A` drives one nfet gate and one pfet gate, but circuit 2's `A` drives two pfet gates
- **WHEN** the net report is written
- **THEN** it SHALL contain a fragment row `Net: A` | `Net: A`, followed by fanout rows such as `  nfet/gate = 1` | `  pfet/gate = 2`

### Requirement: Device mismatch report
When devices are reported, the log SHALL contain `DEVICE mismatches: Class fragments follow (with node fanout counts):` followed by the header row. Then, for each illegal device class:
- An empty line.
- Fragments, as in the net report but with `Instance: <name>` rows (leading `/` removed) and `(no matching instance)` for an absent side.
- A full divider.

Each pin row SHALL be `  <pin> = <fanout>`, where fanout is the number of device pins on that pin's net. For a group of permutable pins the row SHALL be `  (<p1>,<p2>…) = (<f1>,<f2>…)`, with fanouts in decreasing order. A pin whose name differs from the instance prefix (`port_match_error`) SHALL be shown in full.
Compatibility: exact
(netgen ref: base/netcmp.c:578-712, base/netcmp.c:1615-1795)

#### Scenario: Permutable pins shown grouped
- **GIVEN** an unmatched nfet with source fanout 3 and drain fanout 1, and source/drain permutable
- **WHEN** the device report is written
- **THEN** its rows SHALL include `  (drain,source) = (3,1)` or `  (source,drain) = (3,1)`, with names in pin order and counts in decreasing order (verify against netgen)

### Requirement: Pin table
Pin matching (capability `lvs-hierarchy`) SHALL write the following:
1. An empty line.
2. `Subcircuit pins:`
3. The header row.
4. A column divider.
5. One row per pin pairing: `<pin1>` | `<pin2>`, or one of the variants `<pin1>` | `<pin2> **Mismatch**`, `<pin1>` | `(no pin, node is <net2>)`, `(no pin, node is <net1>)` | `<pin2>`, `<pin1>` | `(no matching pin)`, `(no matching pin)` | `<pin2>`. Entries named `(no pins)` SHALL be omitted.
6. A full divider.

The result message SHALL follow: `Cell pin lists are equivalent.`, `Cell pin lists for <a> and <b> altered to match.` or `Cells have no pins;  pin matching not needed.` After a successful `equate classes`, `Device classes <a> and <b> are equivalent.` SHALL follow.
Compatibility: exact
(netgen ref: base/netcmp.c:7690-8150, tcltk/tclnetgen.c:3174-3192, tcltk/tclnetgen.c:3382-3390)

#### Scenario: Matching pin table
- **WHEN** `inv` with pins A, Y, VDD, VSS matches by name
- **THEN** the table SHALL have four rows whose left and right texts are identical, and SHALL be followed by `Cell pin lists are equivalent.`

### Requirement: Property error report
When property errors are printed after `run resolve`, each failing matched pair SHALL produce the lines specified in capability `lvs-property-matching`:
- `<i1> vs. <i2>:`, then one line per failing key
- `Property … has no matching property …` lines
- `Parallelized instances disagree …` lines
- network-mismatch lines with network dumps

The sequence SHALL be preceded by `Netlists match uniquely with property errors.` (or the symmetry or port-error variant).
Compatibility: exact
(netgen ref: base/netcmp.c:6453-6489, tcltk/tclnetgen.c:2534-2564)

#### Scenario: Property error block
- **GIVEN** matched nfet instances `M1` and `M5` that differ in L (0.15 versus 0.18)
- **WHEN** the result is reported
- **THEN** the log SHALL contain `Netlists match uniquely with property errors.`, `M1 vs. M5:` and the L line

### Requirement: Structured list result
`lvs … -list` SHALL return a list with one element per compared pair, in queue order. Each element SHALL be a key/value list whose keys appear in this order, each only when produced:

| Key | Value |
|---|---|
| `name` | `{cell1 cell2}` |
| `devices` | `{{{class count}…} {{class count}…}}`, as in the summary. `(no matching element)` has count 0. |
| `nets` | `{n1 n2}` |
| `badnets` | A list of fragments `{nets1 nets2}`. Each net is `{name {{class pin count}…}}`. |
| `badelements` | A list of fragments `{insts1 insts2}`. Each instance is `{name {{pin count}…}}`. |
| `properties` | Present only with property errors: a list of `{{inst1 inst2} {{key1 val1} {key2 val2}}…}`. A missing side is `(no matching parameter)`/`(no value)`. Value-type and expression values have a netgen defect (see Q-LVS-19). |
| `pins` | `{pins1 pins2}`. Absent when neither cell has pins. |

`badnets` and `badelements` SHALL always be present (possibly empty) once convergence was run. A pair whose circuits have no devices SHALL produce no `name`, `devices` or `nets` keys.
Compatibility: exact
(netgen ref: base/netcmp.c:3955-3970, base/netcmp.c:3425-3495, base/netcmp.c:3530-3550, tcltk/tclnetgen.c:2723-2770, base/netcmp.c:5427-5500, base/netcmp.c:6466-6480, base/netcmp.c:8510-8520, tcltk/netgen.tcl.in:696-699)

#### Scenario: Keys of a clean pair
- **WHEN** `lvs … -list` compares a matching `inv` pair with pins
- **THEN** its element SHALL have the keys `name devices nets badnets badelements pins`, in that order, with empty `badnets` and `badelements`

### Requirement: JSON file
`lvs … -json` SHALL build the list result and write it to a file. The file name is the log name with its text from the last `.` onward replaced by `.json`. A log name with no `.` gives the name `.json` (see Q-LVS-13).

The file SHALL be written line by line in this format. Indentation is exact, and `\` in names is written as `\\` (no other escaping):

```
[
  {
   "name": [
      "<cell1>",
      "<cell2>"
   ],
   "devices": [
       [
         ["<class>", <n>],
         ["<class>", <n> ]
       ], [
         ["<class>", <n> ],
         ["<class>", <n> ]
       ]
   ],
   "nets": [
    <n1>,
    <n2>
   ],
   "badnets": [
      [
        [
          [
            "<net>",
            [
              [ "<class>", "<pin>", <count> ],
              [ "<class>", "<pin>", <count> ]
            ]
          ]
        ], [
          …same shape for circuit 2…
        ]
      ]
   ],
   "badelements": [
      [
        [
          [
            "<inst>",
            [
              [ "<pin>", <count> ],
              [ "<pin>", <count> ]
            ]
          ]
        ], [
          …
        ]
      ]
   ],
   "properties": [
      [
         [
           "<inst1>",
           [
             ["<key>", "<value>"]
           ]
         ],
         [
           "<inst2>",
           [
             ["<key>", "<value>"]
           ]
         ]
      ]
   ],
   "pins": [
      [
        "<pin>",
        "<pin>"
      ], [
        "<pin>",
        "<pin>"
      ]
   ]
  }
]
```

**Exact rules.**
- In `devices`, circuit 1's non-last entries end in `],` with no space before `]`. Circuit 1's last entry, and all of circuit 2's entries, have a space before `]`.
- Every key's closing `]` SHALL be followed by `,` unless it is the pair's last key.
- Each pair's closing `}` SHALL be followed by `,` unless it is the last pair.
- An empty count SHALL be written as 0.
- An empty `badnets` or `badelements` SHALL be written as its opening line followed directly by its closing line.
Compatibility: exact
(netgen ref: tcltk/netgen.tcl.in:32-355, tcltk/netgen.tcl.in:726-730)

#### Scenario: JSON name
- **WHEN** `lvs a.spice b.spice setup.tcl comp.out -json` finishes
- **THEN** `comp.json` SHALL be written next to `comp.out`

#### Scenario: Nets block
- **GIVEN** a pair with 4 nets in each circuit
- **THEN** the JSON SHALL contain the lines `   "nets": [`, `    4,`, `    4` and `   ],`

### Requirement: Result classification
Hefesto SHALL expose each compared pair, and the overall run, as a structured result through the command API (capability `hefesto-architecture`). The status SHALL be exactly one of the values below; `uresult` is the netgen `verify unique` code:

| Status | netgen uresult | Meaning |
|---|---|---|
| `match` | 1 | Unique match |
| `match-property-errors` | −3 | Unique match with property errors |
| `match-port-errors` | −2 | Unique match with port errors |
| `match-port-property-errors` | −4 | Port and property errors |
| `symmetric` | 0 | Matches only locally (symmetries remain) |
| `black-box` | −1 | No devices; pins only |
| `mismatch` | not equivalent | Not equivalent |

The overall run SHALL also report:
- top pin-match failure
- the failed-subcell list
- the property-error list
- whether it was cancelled

The overall status SHALL be `clean` exactly when netgen's final result would be `Circuits match uniquely.` followed by a line `.`, with no further lists.
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: Clean status
- **WHEN** all pairs match uniquely
- **THEN** the API result SHALL report overall status `clean` and per-pair status `match`

### Requirement: Canonical golden comparison
Golden tests against netgen SHALL compare:
- the final result block and the classification byte for byte
- each section's set of lines order-insensitively, but only within: subcircuit summary class rows, mismatch fragments, pin table rows, property blocks and merge or flatten message runs
- JSON after parsing, as unordered sets wherever the list order comes from netgen's internal order (see Q-LVS-2)

Hefesto SHALL write these sections in the deterministic order of capability `lvs-comparison-algorithm`.
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: Pin rows reordered
- **GIVEN** Hefesto writes pin rows in port order and netgen in net-class order
- **WHEN** they are compared
- **THEN** the golden test SHALL pass if the sets of pin rows are equal
