# LVS Comparison Algorithm

## Purpose
Defines how two flat views of a cell pair (circuit 1 and circuit 2) are compared for graph isomorphism. It covers:
- the bipartite device/net graph
- the initial partition
- iterative hash refinement of device and net classes
- legality checks and convergence
- classification of the result: unique match, match with symmetries, port errors, or mismatch
- symmetry (automorphism) resolution by property, pin name, net name and arbitrary choice
- forced equivalences (`equate nodes`/`equate elements`)
- the `run` and `verify` commands

**Compatibility adaptation.** This capability is about netgen, not Magic. The classes in `docs/spec-conventions.md` apply relative to **netgen** (`RTimothyEdwards/netgen` @ `93dcda14d36349d0b0ca63168d3cbbab20fe2ece`, version 1.5.327):
- `exact`: identical classification and identical final partition
- `behavioral`: same result semantics
- `informative`: netgen internals that Hefesto MAY differ from
- `native`: cites `(Hefesto decision: docs/decisions/product.md#P-5)`

Traces use `(netgen ref: path/file.c:line)`. They are for traceability only (capability `hefesto-licensing`).

Other capabilities own related parts:
- device merging before graph construction: `lvs-device-reduction`
- property comparison: `lvs-property-matching`
- hierarchy, flattening and pin matching: `lvs-hierarchy`
- report text: `lvs-reports`

## Requirements

### Requirement: Comparison graph
For each compared cell, after device reduction, the engine SHALL build a bipartite graph:
- **Devices.** There SHALL be one device vertex per instance (primitive device, module or unflattened subcircuit) in the cell.
- **Nets.** There SHALL be one net vertex per net that at least one instance pin connects to. Nets connected to nothing, including unconnected ports, SHALL NOT be vertices.
- **Edges.** Each instance pin SHALL be an edge to its net.

The vertices of both circuits SHALL be placed in one shared partition structure. Each vertex remembers which circuit it came from. Device vertices SHALL be ordered circuit 1 first, then circuit 2, each in instance order. Net vertices SHALL be ordered circuit 1 first, then circuit 2, each in increasing net number.

If circuit 1 or circuit 2 has no devices, the comparison SHALL stop with `Circuit <c> contains no devices.` If it has no nets, it SHALL stop with `Circuit <c> contains no nets.`
Compatibility: behavioral
(netgen ref: base/netcmp.c:2406-2612, base/netcmp.c:3905-3976)

#### Scenario: Unconnected port is not a net
- **GIVEN** cell `inv` with ports A, Y, VDD, VSS and a port `NC` connected to nothing
- **WHEN** its graph is built
- **THEN** it SHALL have 4 net vertices

### Requirement: Class identity and pin labels
Every cell SHALL have a class identity. By default it is a 64-bit hash of its name, case-folded when matching is case-insensitive. `equate classes` can make two identities equal (capability `lvs-setup-file`).

Every device pin SHALL carry a pin label. Pin labels SHALL satisfy two rules:
1. Two pins of devices whose classes have the same identity, at the same pin position, SHALL have equal labels in both circuits.
2. Different pin positions of a class SHALL have different labels, except that pins declared permutable SHALL share one label.

When several permutation pairs chain together, all pins in the chain SHALL share one label. The shared label SHALL not depend on pin order.
Compatibility: behavioral
(netgen ref: base/netcmp.c:3448-3456, base/netcmp.c:7017-7080, base/objlist.c:386)

#### Scenario: Source and drain interchangeable
- **GIVEN** `permute default` and a 4-pin fet class
- **WHEN** pin labels are assigned
- **THEN** source and drain SHALL have equal labels, and gate and bulk SHALL have labels distinct from them and from each other

### Requirement: Initial partition
The initial partition SHALL be formed as follows:
1. Every device SHALL get the initial key of its class identity.
2. Every net SHALL get the initial key of its fanout, which is the number of device pins connected to it.
3. Devices SHALL be grouped by equal key, and nets SHALL be grouped by equal key, giving the initial device classes and net classes.

The device and net counts of the subcircuit summary SHALL be reported before the partition (see capability `lvs-reports`). The console-only summary also reports orphan nets; netgen prints that line with missing arguments (see Q-LVS-25). The partition SHALL then be checked for legality.
Compatibility: exact
(netgen ref: base/netcmp.c:3270-3552, base/netcmp.c:4036-4040)

#### Scenario: Fanout split
- **GIVEN** both circuits are an inverter with nets A (fanout 2), Y (2), VDD (2), VSS (2)
- **WHEN** the initial partition is formed
- **THEN** all 8 net vertices SHALL be in one net class, and the 2 + 2 devices SHALL be in two device classes (pmos, nmos)

### Requirement: Partition legality
A class SHALL be legal when it holds equally many vertices from circuit 1 and from circuit 2. Each time classes are re-formed, all illegal classes SHALL be merged into a single illegal class placed first in the class list. This merging SHALL raise the sticky "bad match" condition. The condition stays raised until the comparison is reset.

A class of exactly two vertices from the same circuit SHALL be marked illegal during refinement.
Compatibility: exact
(netgen ref: base/netcmp.c:2775-2803, base/netcmp.c:2805-2897, base/netcmp.c:2899-3021, base/netcmp.c:4185-4205)

#### Scenario: Extra device
- **GIVEN** circuit 1 has 3 nmos and circuit 2 has 2 nmos
- **WHEN** the initial partition is checked
- **THEN** the nmos class SHALL be illegal and the bad-match condition SHALL be raised

### Requirement: Iterative refinement
Refinement SHALL repeat rounds until convergence. One round SHALL work as follows:
1. Give every device class and net class a fresh pseudo-random label.
2. Recompute the key of every device d:
   key(d) ← old_key(d) ⊕ Σ over pins p of d with a net (label(p) ⊕ label(class(net(p))))
3. Re-partition the device classes by the new keys.
4. Recompute the key of every net n:
   key(n) ← Σ over pins p on n (label(p) ⊕ key(device(p)) ⊕ label(class(device(p))))
5. Re-partition the net classes by the new keys.

Arithmetic SHALL be unsigned 64-bit, with wrap-around addition and bitwise XOR (⊕).

**Which classes are split.** A class that already holds exactly one vertex from each circuit (a matched pair) SHALL NOT be re-split. The exception is exhaustive mode, where every class is re-split.

**Convergence.** A round SHALL report convergence when it creates no new device class and no new net class.

**`run converge`.** It SHALL do two things in order:
1. refine until convergence with exhaustive mode off
2. turn exhaustive mode on and refine until convergence again

The final partition SHALL be the coarsest equitable refinement consistent with the initial keys, up to hash collisions (see Q-LVS-4).
Compatibility: exact
(netgen ref: base/netcmp.c:3023-3130, base/netcmp.c:4125-4213, tcltk/tclnetgen.c:2484-2500)

#### Scenario: Swapped connection detected
- **GIVEN** circuit 2 equals circuit 1 except that one NAND's two inputs are wired to the outputs of two different drivers
- **WHEN** refinement converges
- **THEN** the classes holding the affected nets and devices SHALL be illegal

### Requirement: Match verification
After refinement, verification SHALL return one of these results, applying the checks in order:
1. **−1 (mismatch).** If the bad-match condition is raised, or any device class or net class has unequal counts from the two circuits.
2. **Property check.** Each device class with exactly one vertex from each circuit SHALL be property-checked once (capability `lvs-property-matching`). A failed check SHALL set the property-error state. A pair that passed SHALL not be checked again.
3. **k > 0 (symmetries).** k is the number of classes (device plus net) that hold more than one vertex per circuit.
4. **−2 (port errors).** If k = 0 and any of these holds:
   - Some net class has different numbers of port nets from circuit 1 and circuit 2.
   - The cells have different numbers of ports (counting disconnected ports).
   - The counts are equal, but a disconnected port of circuit 1 has no same-named disconnected port in circuit 2.
5. **0 (unique match).** Otherwise.

The property-error state SHALL be one of three values: none, errors found, or failure to compare.
Compatibility: exact
(netgen ref: base/netcmp.c:6491-6565)

#### Scenario: Symmetric parallel devices
- **GIVEN** two identical nmos devices in parallel in both circuits, with parallel combination disabled
- **WHEN** verification runs after convergence
- **THEN** the result SHALL be k > 0 (symmetries), not a mismatch

#### Scenario: Extra disconnected pin
- **GIVEN** circuit 1 has an extra port `TEST` connected to nothing
- **WHEN** verification runs and all classes are matched
- **THEN** the result SHALL be −2

### Requirement: Symmetry resolution
`run resolve` (the default `run` option) SHALL do the following:
1. Converge as `run converge` does.
2. Verify. If the result is k > 0, attempt these resolvers in order. Each stage runs only while symmetries remain.
   a. **By property** (prints `Resolving symmetries by property value.`). Within each symmetric device class, devices whose properties match are grouped under fresh keys. Each group is trimmed so that it holds equally many devices from each circuit; the extra devices return to the original class.
   b. **By pin name** (prints `Resolving symmetries by pin name.`). Within each symmetric net class, a circuit-1 net and a circuit-2 net with equal names, where at least one of them is a port, get a shared fresh key.
   c. **By net name** (prints `Resolving symmetries by net name.`). The same as (b) but without the port condition.
   d. **Arbitrary.** Repeatedly take the first symmetric class (device classes before net classes). Pair the last circuit-1 vertex with the last circuit-2 vertex under a fresh key. Converge, and continue while symmetries remain and no mismatch appears.

After each of a–c, the engine SHALL re-partition, enable exhaustive mode, refine until convergence or a mismatch appears, and verify again.

**Messages.** After resolution:
- On −1: `Netlists do not match.`
- On −2: `Netlists match uniquely with port errors.`, or `Netlists match uniquely with port and property errors.` followed by the property report
- Otherwise: `Netlists match uniquely` or `Netlists match with <k> symmetry|symmetries`, followed by `.` or ` with property errors.` (the latter followed by the property report)
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:2502-2568, base/netcmp.c:6623-6877)

#### Scenario: Symmetry broken by pin name
- **GIVEN** a two-input cell whose input pins A and B are symmetric in topology, and both netlists name them A and B
- **WHEN** `run resolve` runs
- **THEN** A SHALL be paired with A and B with B, and the result SHALL be `Netlists match uniquely.`

### Requirement: verify command
`verify [-list] [nodes|elements|properties|only|all|equivalent|unique]` SHALL report on the current partition. Options SHALL use unique-prefix matching.

**No partition exists.**
- `equivalent` and `unique` SHALL return −1.
- Other options SHALL print `Verify:  cell <c> has no elements and/or nodes.  Not checked.` or `Verify:  no current cell to verify.`

**Mismatch (−1).**
- With no option, `nodes`, or `all`: print the net mismatch report.
- With no option, `elements`, or `all`: print the device mismatch report.
- `equivalent` and `unique` SHALL return 0.
- Every other option SHALL print `Netlists do not match.` and `Port matching may fail to disambiguate symmetries.`

**Port errors (−2).**
- `equivalent` SHALL return 1.
- `unique` SHALL return −2, or −4 when property errors exist.
- `only`, `all` and `properties` SHALL print `Circuits match uniquely with port errors.`

**Symmetries (k > 0).**
- `equivalent` SHALL return k.
- `unique` SHALL return 0.
- `only`, `all` and `properties` SHALL print `Circuits match with <k> symmetry|symmetries.` (console only; see Q-LVS-20).

**Unique (0).**
- `equivalent` and `unique` SHALL return 1, or −3 when property errors exist.
- `only`, `all` and `properties` SHALL print `Circuits match uniquely.` followed by `.` on its own line, or by `Property errors were found.` (see Q-LVS-21)
- `properties` SHALL also print the property report when there are property errors.

**`-list`.** With `-list`, keys `badnets` and `badelements` SHALL be appended to the structured result (capability `lvs-reports`).
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:2594-2775)

#### Scenario: Final unique result text
- **GIVEN** a unique match with no property errors
- **WHEN** `verify only` runs
- **THEN** the log SHALL receive `Circuits match uniquely.` and then a line containing only `.`

### Requirement: Forced equivalences
`equate nodes <n1> <n2>` and `equate elements <e1> <e2>` SHALL force a net pair or a device pair to correspond. The two-name form refers to the current circuits; the four-name form takes `<valid_cellname> <name>` for each side.

**Effect.** The two vertices SHALL be split out of their class into a class of their own, and the rest of the class SHALL stay together. This SHALL succeed only when both vertices are in the same class.

**Messages and results.**
- Success: `Nodes <a> and <b> are equivalent.` or `Elements <a> and <b> are equivalent.`, with result 1.
- Failure: `Unable to equate nodes <a> and <b>.` or `Unable to equate elements <a> and <b>.`, with result 0.
- No current comparison: `Circuits not being compared, must specify netlists.`
- Same netlist: `Objects in the same netlist cannot be equated.`
Compatibility: exact
(netgen ref: tcltk/tclnetgen.c:3061-3110, base/netcmp.c:7087-7210)

#### Scenario: Equate across classes fails
- **GIVEN** net `a` of circuit 1 and net `b` of circuit 2 are in different classes
- **WHEN** `equate nodes a b` is issued
- **THEN** the result SHALL be 0

### Requirement: Pseudo-random labels
Fresh labels SHALL be drawn from a deterministic pseudo-random generator, so that a given comparison always produces the same partition. Pin labels for a class SHALL be derived deterministically from the class identity, so that equal identities in both circuits give equal labels. netgen uses a Numerical-Recipes `ran2`-style generator scaled to [0, 2^31−1). The exact label values are not observable in results. Hefesto MAY use any generator that meets these determinism and equality properties (see Q-LVS-4).
Compatibility: informative
(netgen ref: base/netcmp.c:3018-3021, base/pdutils.c:39-114)

#### Scenario: Repeatable result
- **WHEN** the same comparison is run twice in one session after `restart`
- **THEN** the classification and every reported pairing SHALL be identical

### Requirement: Deterministic arbitrary pairing
Wherever netgen's choice between symmetric candidates depends on internal list order, Hefesto SHALL use a documented deterministic order. This covers arbitrary symmetry breaking and the order of report sections. The order SHALL be: within each class, circuit 1 vertices in instance order or increasing net number, and the same for circuit 2. The pairing SHALL be stable across platforms. Golden comparison of affected report sections SHALL be order-insensitive (decision X-1).
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: Cross-platform pairing
- **WHEN** a circuit with four identical parallel inverters and parallel combination disabled is compared on Linux and on iPadOS
- **THEN** the arbitrary pairings reported SHALL be identical on both

### Requirement: Long-running comparisons
The comparison SHALL run on a background worker (capability `hefesto-architecture`). It SHALL be cancellable between refinement rounds. A cancelled refinement SHALL leave the partition consistent: reports can still be produced, and the result SHALL be marked cancelled. netgen implements this with an interrupt flag checked between rounds.
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: Cancel
- **WHEN** the user cancels LVS during refinement of a large design
- **THEN** the run SHALL stop within one refinement round and report `cancelled` without crashing

### Requirement: Reset and stepping commands
These commands SHALL step through a comparison by hand:
- `iterate` SHALL perform one refinement round.
- `restart` SHALL merge all classes back into one device class and one net class, and redo the initial partition. It SHALL require an existing partition (`Need to initialize data structures first!`).
- `exhaustive [on|off]` SHALL set or report exhaustive mode, printing `Exhaustive subdivision ENABLED.` or `Exhaustive subdivision DISABLED.`
- `symmetries` SHALL list symmetric classes.
- `symmetry` SHALL print `Symmetry breaking method has been deprecated.`
- `summary [elements|nodes]` and `print [nodes|elements|queue] [legal|illegal]` SHALL print diagnostics.
- `print queue` SHALL return the pending compare queue, which is empty when the current pair is the last.
Compatibility: behavioral
(netgen ref: tcltk/tclnetgen.c:2294-2440, tcltk/tclnetgen.c:4236-4310, base/netcmp.c:4043-4123)

#### Scenario: Queue empty at top
- **WHEN** the top-level pair is being compared
- **THEN** `print queue` SHALL return an empty result
