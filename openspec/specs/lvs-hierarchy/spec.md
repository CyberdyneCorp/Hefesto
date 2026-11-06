# LVS Hierarchical Comparison

## Purpose
Defines hierarchical LVS:
- building the bottom-up compare queue of matching cell pairs
- the per-pair comparison flow of the `lvs` command
- flattening unmatched or mismatched subcircuits
- pre-match flattening heuristics
- global-net conversion
- duplicate and shorted pins
- black boxes and placeholders
- pin matching between matched cells, with proxy pins and instance pin reordering
- deciding the final result

**Compatibility adaptation.** This capability is about netgen, not Magic. The classes in `docs/spec-conventions.md` apply relative to **netgen** (`RTimothyEdwards/netgen` @ `93dcda14d36349d0b0ca63168d3cbbab20fe2ece`, version 1.5.327):
- `exact`: identical queue, flattening decisions, pin correspondence and final result
- `behavioral`: same outcome
- `informative`: netgen detail
- `native`: cites `(Hefesto decision: docs/decisions/product.md#P-5)`

Traces use `(netgen ref: path/file.c:line)`. They are for traceability only (capability `hefesto-licensing`).

Other capabilities own related parts:
- per-pair graph matching: `lvs-comparison-algorithm`
- report text: `lvs-reports`

## Requirements

### Requirement: Compare queue construction
`compare hierarchical <cellA> <cellB>` SHALL discard any existing queue and build a new one as follows:
1. **Depth.** Let D be the greatest depth of subcircuit or module instances below either top cell. Recursive self-instances SHALL be ignored.
2. **Levels.** For each level L from D down to 1, visit every cell at depth L below circuit 1's top, then every cell at depth L below circuit 2's top, in instance order.
3. **Partner lookup.** For each visited cell c that is not yet matched, find its partner in the other netlist:
   a. A correspondence recorded by `equate classes` takes precedence.
   b. Otherwise the cell in the other netlist with the same class identity is used. This is skipped if that cell is already the `equate classes` partner of a different cell.

   A duplicate cell (`name[[k]]`) SHALL be looked up by its name without the suffix.
4. **Queueing.** If a partner exists, append the pair (circuit 1 cell first) to the queue and mark both cells matched. Otherwise leave c unmatched; it will be flattened.
5. **Top pair.** Append the top pair last and mark it matched.

`compare hierarchical` with no cell arguments SHALL pop the next pair, or return an empty result when the queue is empty. Both forms SHALL return `{cellA cellB}` for the pair to compare now.
Compatibility: exact
(netgen ref: base/netcmp.c:3614-3733, base/netcmp.c:3762-3872, tcltk/tclnetgen.c:2117-2200)

#### Scenario: Bottom-up order
- **GIVEN** `top` instances `mid`, and `mid` instances `inv`, in both netlists
- **WHEN** the queue is built
- **THEN** it SHALL be `(inv, inv)`, `(mid, mid)`, `(top, top)`

#### Scenario: Unpaired cell
- **GIVEN** layout `top` instances `filler_x`, which does not exist in the schematic
- **WHEN** the queue is built
- **THEN** no pair for `filler_x` SHALL be queued, and it SHALL be flattened when `top` is compared

### Requirement: Preparing a cell pair
Before comparing a popped pair, the engine SHALL do the following, in order:
1. Remove duplicate ports (Requirement "Duplicate and shorted pins").
2. In hierarchical mode, convert globals to ports in both cells (capability `lvs-setup-file`, Requirement "global and convert commands").
3. Check for black boxes.
   - If one cell has contents and the other is an empty placeholder, print `Circuit 2 cell <c> is a black box; will not flatten Circuit 1` (or the mirror) and skip steps 4–5.
   - If both are empty placeholders, print `Circuit 1 cell <a> and Circuit 2 cell <b> are black boxes.` and skip steps 4–5.
4. Flatten into each cell every direct subcircuit instance whose class is not matched. Each flattened class SHALL print `Flattening unmatched subcell <c> in circuit <parent> (<f>)(<n> instance[s])`.
5. Repeat pre-matching (Requirement "Pre-match flattening") while it changes anything. Before each repeat, print `Making another compare attempt.` (log) and `Flattened mismatched instances and attempting compare again.` (console), then flatten unmatched subcells again.
6. Run device reduction (capability `lvs-device-reduction`) and build the graph (capability `lvs-comparison-algorithm`).
7. Apply declared permutations.
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:2212-2287, base/netcmp.c:3573-3612)

#### Scenario: Flatten unmatched child
- **GIVEN** layout `top` has 3 instances of `filler_x` with no partner class
- **WHEN** `top` is prepared
- **THEN** the log SHALL contain `Flattening unmatched subcell filler_x in circuit top (0)(3 instances)`

### Requirement: Pre-match flattening
For a pair (c1, c2), the engine SHALL count the instances of each class in c1 and in c2. Classes are paired by class identity; duplicates count under their base name. It SHALL then apply these heuristics:
1. **Both subcircuits.** For a class pair, both subcircuits, whose counts differ: flatten both if the counts of the class's own children then move toward agreement in every child class that already exists in both circuits.
2. **Subcircuit only in circuit 2**, or circuit 2's count larger: flatten it in c2 if that improves child-count agreement.
3. The mirror of case 2 for circuit 1.
4. Remove zero-valued devices (capability `lvs-device-reduction`).

Each flattening SHALL print `Flattening instances of <c> in cell <p> (<f>) makes a better match`. Placeholder (black-box) cells SHALL never be flattened.

The exact acceptance test is a heuristic. Hefesto SHALL make the same flatten decisions for every golden case (see Q-LVS-11).
Compatibility: behavioral
(netgen ref: base/flatten.c:1560-1960, base/flatten.c:2230-2404)

#### Scenario: Extra hierarchy level in layout
- **GIVEN** layout `top` has 2 instances of `pair`, each holding 2 `inv`, and schematic `top` has 4 `inv` directly
- **WHEN** `top` is pre-matched
- **THEN** `pair` SHALL be flattened in the layout and the `inv` counts SHALL then agree (4 = 4)

### Requirement: Flattening semantics
Flattening an instance i of subcircuit s into parent p SHALL:
1. Replace i with copies of s's contents.
2. Name each copied instance `<i>/<name>` and each internal net `<i>/<net>`. `<i>` is the instance name; a leading `/` added by the SPICE reader is part of it.
3. Connect s's port nets to the parent nets on i's pins.
4. Evaluate s's property defaults and expressions with i's parameters.
5. Copy s's global nets into p.

Flattening SHALL never apply to:
- the cell itself
- cells marked by `flatten prohibit`
- cells listed with `-noflatten`
- placeholder cells

(See Requirement "Black boxes and flattening prohibitions".)
Compatibility: exact
(netgen ref: base/flatten.c:253-722, base/flatten.c:723-920)

#### Scenario: Flattened net names
- **GIVEN** instance `/x1` of `inv` with internal net `n1`
- **WHEN** it is flattened into `top`
- **THEN** `top` SHALL contain a net named `/x1/n1`

### Requirement: Duplicate and shorted pins
Before comparison, and before any pin matching, the engine SHALL examine every pair of ports of a cell that share a net:
- **Equal names (duplicate pin).** All but the first SHALL be removed from the cell and from every instance, and the corresponding parent nets merged. The engine SHALL print `Duplicate pin <p> in cell <c> (<f>)` (console).
- **Different names (shorted pins).** Both ports SHALL stay, SHALL be declared permutable, and `Pins <a> and <b> are shorted in cell <c> (<f>)` SHALL be printed (console).

Ports that lost all connections through flattening SHALL be removed from the cell and all instances when pin matching marks them (cleanup).
Compatibility: exact
(netgen ref: base/flatten.c:1180-1320, base/flatten.c:1320-1560)

#### Scenario: Shorted outputs
- **GIVEN** cell `buf2` has ports `Y1` and `Y2` on the same net
- **WHEN** it is prepared
- **THEN** `Y1` and `Y2` SHALL be permutable, and neither SHALL be removed

### Requirement: lvs per-pair flow
For each pair popped from the queue, the `lvs` command SHALL run this procedure:
1. `run converge`.
2. Set `equatePins` to true if this is the top pair (the queue is now empty), otherwise false. Set `forceMatch` to false.
3. **If `verify equivalent` is true:**
   - Run `run resolve`, then `u ← verify unique`.
   - **u = 0** (symmetries remain): log `   Networks match locally but not globally.`, `   Probably connections are swapped.` and `   Check the end of logfile <log> for implicated nodes.`, then run `verify nodes` and set `checkFlatten`.
   - **Any other u:** set `equatePins`. Then:
     - u = −1 (black box): set `forceMatch`.
     - u = −3: record the pair in the property-error list.
     - u = −2: set `checkFlatten`.
     - u = −4: record the property error and set `checkFlatten`.
4. **Otherwise:** set `checkFlatten`.
5. **If `checkFlatten` and this is not the top pair:**
   - If neither cell is in the no-flatten list, set `doFlatten`.
   - Otherwise log `  Continuing with black-boxed subcircuits <a> <b>`, add `<a>(<f1>)` and `<b>(<f2>)` to the failed-subcell list, prohibit flattening of both, and set `equatePins`, `childMismatch` and `forceMatch`.
6. **If `equatePins`:** run pin matching, forced if `forceMatch`. If the result is ≥ 0, run `equate classes` on the pair. Then:
   - A result of 0 on the top pair SHALL set `pinMismatch`.
   - A result ≥ 0 that is not that case SHALL clear `doFlatten`.
7. **If `doFlatten`:** log `  Flattening non-matched subcircuits <a> <b>` and flatten all instances of `<a>` in netlist 1 and of `<b>` in netlist 2 (`flatten class`).
8. Pop the next pair. Stop when the queue is empty.

After the last pair, the final result SHALL be determined by Requirement "Final result".
Compatibility: exact
(netgen ref: tcltk/netgen.tcl.in:552-703)

#### Scenario: Mismatched subcell flattened
- **GIVEN** cell `nand2` differs between the netlists, and `top` instances it
- **WHEN** `lvs` processes `(nand2, nand2)`
- **THEN** the log SHALL contain `  Flattening non-matched subcircuits nand2 nand2`, and `top` SHALL then be compared with `nand2` flattened in both

#### Scenario: No-flatten list
- **GIVEN** `-noflatten={nand2}` and the same mismatch
- **WHEN** `lvs` runs
- **THEN** the log SHALL contain `  Continuing with black-boxed subcircuits nand2 nand2`, and the final result SHALL report `Subcell(s) failed matching.`

### Requirement: Pin matching
`equate [-list] [-force] pins [<cellA> <cellB>]` SHALL match the ports of two compared cells. It then reorders circuit 2's ports, and the pins of every circuit-2 instance, to circuit 1's port order.

**No ports.** If neither cell has ports, the result SHALL be 2 and the engine SHALL print `Cells have no pins;  pin matching not needed.`

**Matching rules.** Otherwise the engine SHALL print the pin table (capability `lvs-reports`) and apply these rules in order:
1. **By net class.** For each port net of circuit 1 that sits in a matched net class, its circuit-2 partner net SHALL be found. A circuit-2 port on that net with the same name, or else the first port on that net, SHALL be paired. If no port on it has the same name, the row SHALL show `<name2> **Mismatch**` and the result becomes 0. A circuit-1 port whose partner net has no port SHALL show `(no pin, node is <net2>)` and the result becomes 0.
2. **By name, for still-unpaired circuit-1 ports.** The comparison SHALL:
   - ignore a trailing `!`
   - ignore a leading `\`
   - treat a `proxy` prefix as matching a disconnected pin
   - for placeholders, treat a size-1 array `a[0]` as equal to `a`

   Ports with equal names SHALL be paired when any of these holds:
   - both are disconnected
   - both cells are placeholders
   - no partition exists (forced mode)
   - both nets connect only to ports (shorted ports), with the correspondence kept consistent
   - they are members of permutable pairs whose partners sit on the same nets
3. **Unpaired circuit-2 ports.**
   - A port with real connections SHALL be appended to the order. A proxy port `proxy<name>` SHALL be added to circuit 1 and to every instance of it. The result becomes 0.
   - A port whose net became empty after flattening SHALL be marked for removal. On the top pair this also makes the result 0.
4. **Unpaired circuit-1 ports.**
   - A still-connected port SHALL get a proxy port `proxy<name>` in circuit 2.
   - A disconnected port SHALL be removed, its row shown as `<name>` versus `(no matching pin)`.

**Port connection errors.** When every port matched (result 1), (see Q-LVS-23 and Q-LVS-24) any net class whose port counts differ SHALL print `Port connection errors found:` and then, for each net in that class, `    <net> (<f>)` or `    <net> (<f>) (no port)`.

**Results and messages.**

| Result | Message |
|---|---|
| 1 | `Cell pin lists are equivalent.` |
| 0 | `Cell pin lists for <a> and <b> altered to match.` |

netgen also defines the results −1 and −2 (`Cell pin lists for <a> and <b> do not match.` and `Attempt to match empty cell to non-empty cell.`), but never returns them (see Q-LVS-17).

**Forced mode.** `-force` SHALL match pins by name alone, ignoring the partition. `lvs` uses it for black boxes and for cells it continues past.

**Placeholders.** When no partition exists and black-boxing is off, if either cell is a placeholder, both SHALL be marked placeholders and `Warning: Equate pins:  cell <c> is a placeholder, treated as a black box.` SHALL be printed.
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:3115-3205, base/netcmp.c:7344-7480, base/netcmp.c:7483-7620, base/netcmp.c:7636-8525)

#### Scenario: Reordered ports
- **GIVEN** layout `inv` ports `Y A VDD VSS` and schematic `inv` ports `A Y VSS VDD`, matching uniquely
- **WHEN** pins are matched
- **THEN** the result SHALL be 1, and every schematic instance of `inv` SHALL have its pins reordered to `Y A VDD VSS`

#### Scenario: Missing pin at top
- **GIVEN** the schematic top has port `EN` that the layout top lacks, and EN is connected in the schematic
- **WHEN** pins are matched on the top pair
- **THEN** the table SHALL show `(no matching pin)` against `EN`, the result SHALL be 0, and the final result SHALL be `Top level cell failed pin matching.`

### Requirement: Black boxes and flattening prohibitions
A cell SHALL be treated as a black box when any of these holds:
- its class is module
- it was created as a placeholder for an undefined subcircuit
- `model blackbox on` (or `lvs … -blackbox`) is in effect and the subcircuit is empty

Black boxes SHALL be compared only by their pins, matched by name with `-force`.

`flatten prohibit` and the `lvs` option `-noflatten=<list|file>` SHALL keep a cell from ever being flattened. A file argument is read line by line, and lines whose first element is `#` are ignored. Names are matched as anchored regular expressions against each netlist's cell list. When a prohibited pair fails to match, it SHALL be recorded as a failed subcell instead of being flattened.
Compatibility: exact
(netgen ref: tcltk/netgen.tcl.in:376-418, tcltk/netgen.tcl.in:480-490, tcltk/tclnetgen.c:1060-1070)

#### Scenario: Analog macro black box
- **GIVEN** both netlists contain an empty subcircuit `adc` and `-blackbox` is given
- **WHEN** `lvs` runs
- **THEN** `adc` SHALL be compared only by pin names, and its contents SHALL never be compared

### Requirement: Final result
After the queue is exhausted, the `lvs` command SHALL log `\nFinal result: ` and then:
- If a top-level pin mismatch or a failed subcell occurred: `Subcell(s) failed matching.` (if any subcell failed), then `Top level cell failed pin matching.` (if the top pair's pin result was 0).
- Otherwise the output of `verify only` on the top pair (capability `lvs-comparison-algorithm`).

Then:
- If any pairs had property errors: `\nThe following cells had property errors:\n ` followed by their circuit-1 names joined by `\n `, then `\n`.
- If any subcells failed: `\nThe following subcells failed to match:\n ` followed by the `<name>(<f>)` entries joined the same way.

The console SHALL receive `LVS Done.`
Compatibility: exact
(netgen ref: tcltk/netgen.tcl.in:704-725)

#### Scenario: Clean result
- **GIVEN** every pair matches uniquely and has no property errors
- **WHEN** `lvs` finishes
- **THEN** the log SHALL end with `Final result: Circuits match uniquely.`, a line `.`, and nothing else
