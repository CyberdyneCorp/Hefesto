# LVS Property Matching

## Purpose
Defines how the properties of matched devices are compared:
- which properties are compared (properties of interest)
- value types and promotion between them
- tolerance rules for each type
- the implicit multiplier `M` and series count `S`
- pin-associated properties on permutable pins
- reconciling series/parallel networks of property records
- the property error report and how it affects the final result

**Compatibility adaptation.** This capability is about netgen, not Magic. The classes in `docs/spec-conventions.md` apply relative to **netgen** (`RTimothyEdwards/netgen` @ `93dcda14d36349d0b0ca63168d3cbbab20fe2ece`, version 1.5.327):
- `exact`: identical pass/fail decision for every compared property and identical report text
- `behavioral`: same pass/fail semantics
- `informative`: netgen detail
- `native`: cites `(Hefesto decision: docs/decisions/product.md#P-5)`

Traces use `(netgen ref: path/file.c:line)`. They are for traceability only (capability `hefesto-licensing`).

Other capabilities own related parts:
- how properties are declared: `lvs-setup-file`
- how property records are created by merging: `lvs-device-reduction`

## Requirements

### Requirement: Properties of interest
Only properties whose key is in the device class's property-definition table (its "properties of interest") SHALL be compared. Instance properties with other keys SHALL be ignored.

`M` and `S` SHALL always be compared, even when absent from the table, as integers with tolerance 0. A missing `M` or `S` SHALL count as 1.

Key equality SHALL follow the session's name-matching rule (capability `lvs-netlist-input`).
Compatibility: exact
(netgen ref: base/netcmp.c:5514-5650)

#### Scenario: Unlisted property ignored
- **GIVEN** nfet has properties of interest W and L, and the instances also carry `ad=1p` and `ad=2p`
- **WHEN** the devices are compared
- **THEN** no property error SHALL be reported for `ad`

### Requirement: Value types and promotion
Instance values SHALL start as strings. Each value SHALL be promoted toward the type of its definition, using these rules:

| From | To | Rule |
|---|---|---|
| string | integer | When the string is a number (capability `lvs-netlist-input`, Requirement "SI value parsing") whose value is integral |
| string | double or value | When the string is a number |
| double | integer | When the value is integral |
| integer | double | Always |
| integer or double | string | Printed with `%d` or `%g` |
| expression | any | First reduced to a number if possible |

A value that cannot be promoted SHALL keep its type.

A definition with string type and no default SHALL take the type and value of the first instance value compared against it.

**Target type.** When the two sides' definitions differ, both values SHALL be promoted to a single target type, chosen in this order of preference:
1. double from circuit 1
2. double from circuit 2
3. integer from circuit 1
4. integer from circuit 2
5. string from circuit 1
6. string from circuit 2
7. otherwise circuit 1's definition

If the promoted types still differ, the property SHALL be a mismatch reported as `(property type mismatch)`.
Compatibility: exact
(netgen ref: base/netgen.c:2193-2400, base/netcmp.c:5720-5810)

#### Scenario: SI string promoted
- **GIVEN** a W definition of type double, with W=`1u` in circuit 1 and W=`1e-6` in circuit 2
- **WHEN** compared
- **THEN** both SHALL become 1e-6 and SHALL match

### Requirement: Tolerance rules
For each compared key, with promoted values a (circuit 1) and b (circuit 2) and target definition tolerance t:

| Target type | Match condition | Mismatch report line |
|---|---|---|
| double or value | 2·\|a − b\| / (a + b) ≤ t, computed in double. If a + b = 0 and a = b, the ratio is not a number and SHALL count as a match (see Q-LVS-7). | ` <key> circuit1: <a %g>   circuit2: <b %g>   ` then `(delta=<100·ratio %.3g>%, cutoff=<100·t %.3g>%)` when a > 0 and b > 0 |
| integer | \|a − b\| ≤ t | ` <key> circuit1: <a>   circuit2: <b>   ` then `(delta=<\|a−b\|>, cutoff=<t>)` when a > 0 and b > 0 |
| string, n = round(t) | n = 0: names equal under the session matching rule.<br>n > 0: the first n characters equal case-insensitively. | n = 0: ` <key> circuit1: "<a>"   circuit2: "<b>"   (exact match req'd)`<br>n > 0: ` <key> circuit1: "<a>"   circuit2: "<b>"   (check to <n> chars.)` |
| expression | Always a mismatch | ` <key> (unresolved expressions.)` |

The first mismatch printed for a device pair SHALL be preceded by the line `<inst1> vs. <inst2>:`. Instance names SHALL be printed without a leading `/`.
Compatibility: exact
(netgen ref: base/netcmp.c:5810-5935)

#### Scenario: Width within tolerance
- **GIVEN** W tolerance 0.01, circuit 1 W=1.000e-6 and circuit 2 W=1.005e-6
- **WHEN** compared
- **THEN** the ratio 2·0.005/2.005 ≈ 0.00499 ≤ 0.01, so they SHALL match

#### Scenario: Length mismatch line
- **GIVEN** L tolerance 0.01, instances `M1` (L=0.15) and `M5` (L=0.18)
- **WHEN** compared with printing on
- **THEN** the report SHALL contain `M1 vs. M5:` and ` L circuit1: 0.15   circuit2: 0.18   (delta=18.2%, cutoff=1%)`

### Requirement: Missing properties
Within a single property record:
- A property of interest in circuit 1 with no same-key value in circuit 2 SHALL be a mismatch, reported as `Property <k> in circuit1 has no matching property in circuit2`. This does not apply to `M`/`S` equal to 1.
- The symmetric case SHALL be reported as `Property <k> in circuit2 has no matching property in circuit1`.

At the device level:
- If one device has no property records at all, it SHALL match only if every property-of-interest value on the other device is absent, or is `M`/`S` equal to 1.
- Otherwise the report SHALL be `Circuit 1 <cell> instance <inst> property "<k>" has no match in circuit 2.` (or the mirror for circuit 2), and the comparison SHALL count as a failure.
Compatibility: exact
(netgen ref: base/netcmp.c:5590-5715, base/netcmp.c:6208-6330)

#### Scenario: Missing multiplier is one
- **GIVEN** circuit 1 instance has `M=1` and circuit 2 instance has no `M`
- **WHEN** compared
- **THEN** they SHALL match

### Requirement: When properties are checked
Properties SHALL be checked for every device class that holds exactly one device from each circuit. The check SHALL run each time a match is verified, and a pair that has passed SHALL not be checked again (capability `lvs-comparison-algorithm`).

Before comparing two devices, the engine SHALL check that pins connected to no-connect nets agree. If they disagree even after trying permutable alternatives, the report SHALL contain these three lines, and the pair SHALL count one mismatch:
- `  Parallelized instances disagree on pin connections.`
- `    Circuit1 instance <i> pin <n> connections are tied together|no connects (<flag>)`
- `    Circuit2 instance <i> pin <n> connections are tied together|no connects (<flag>)`

Symmetric device classes SHALL use property matching only to break symmetries, never to report errors (capability `lvs-comparison-algorithm`, Requirement "Symmetry resolution").
Compatibility: exact
(netgen ref: base/netcmp.c:6037-6200, base/netcmp.c:6414-6450)

#### Scenario: Property error does not cause mismatch
- **GIVEN** an otherwise identical inverter pair where the nfet W differs by 50%
- **WHEN** compared
- **THEN** the result SHALL be "match uniquely with property errors", not "do not match"

### Requirement: Pin-associated properties
If a property of interest is associated with a pin (`property <c> associate {key pin}`) and that pin is permutable, the engine SHALL check whether the two devices connect the matched nets to swapped pins. If they do, the circuit-1 value of `key` SHALL be compared against the circuit-2 value of the other associated key whose pin is the swapped one. Example: compare `as` against `ad` when source and drain are swapped.
Compatibility: exact
(netgen ref: base/netcmp.c:5655-5700)

#### Scenario: Source/drain area swap
- **GIVEN** `as` is associated with source and `ad` with drain, with source/drain permutable, and the layout device is flipped relative to the schematic
- **WHEN** compared
- **THEN** layout `as` SHALL be compared with schematic `ad`

### Requirement: Series/parallel network reconciliation
When both devices of a pair have property records and either has more than one, the engine SHALL reconcile the two record lists before comparing them one-to-one.

**Topology string.** Each device's records form a string: for each record in order, its `_tag` value (if any) followed by `D`.
- A maximal run of `D` characters is a parallel group.
- A run of `D`s joined by `+D` is a series group.

**Reconciliation loop.** The loop SHALL repeat while anything changes. Each iteration SHALL do the following:
1. Remove group markers that no longer enclose series devices.
2. If both devices have fewer than 2 records, stop.
3. **Parallel step.** For the longest parallel group on each side (if longer than 1), merge records whose non-M properties match within tolerance by summing `M`.
   - If merging is impossible because of strict topology, and that side has the larger group, try reducing `S` instead (step 6 rule).
   - If anything changed, restart the loop.
4. Sort each such parallel group with the sort rule below.
5. **Series step.** For the longest series group on each side, merge by summing `S`, with the same fallback to `M`. If anything changed, restart the loop.
6. Sort each such series group with the sort rule below.

**Merging with comb.** When merging must also fold multiplicity ("comb"), the engine SHALL fold `M` (or `S`) into the additive properties and set the multiplier to 1:
- add-type values are multiplied by the multiplier
- par-type values are divided by it
- a property that is both critical and add-type or par-type in that direction SHALL be treated as critical only, and SHALL NOT be folded

It SHALL then collapse consecutive records that have equal critical values:
- add-type values are summed
- par-type values are combined as x·y/(x+y)
- the absorbed records are marked with multiplier 0 and removed

**Sort rule.** Sort by the critical value with tolerance. Among values equal within tolerance, sort by a secondary key:

| Case | Secondary key |
|---|---|
| A critical property exists | additive value × multiplier, or par value ÷ multiplier |
| No critical property, but an additive property exists | Primary key is the additive value; secondary key is the multiplier |
| Neither | Primary key is the multiplier; secondary key is any other property |

Equality within tolerance SHALL use |a − b| ≤ t for integers and |a − b| / |a + b| ≤ t for reals. The sort SHALL be stable (see Q-LVS-8).

**Missing values.** A record missing a property takes the definition's default. For double-typed defaults, netgen reads the wrong field (see Q-LVS-18).

**Retry.** If a one-to-one comparison of records then fails because of an `M` mismatch (or an `S` mismatch), both sides SHALL be folded to `M` = 1 (or `S` = 1) with comb, and compared again. A difference in record counts SHALL be reported as `Circuit 1 parallel/series network does not match Circuit 2` (or the mirror), followed by a dump of both networks.

**Completion.** When more than one iteration ran, the engine SHALL print `No more changes can be made to series/parallel networks.` (console only).
Compatibility: exact
(netgen ref: base/netcmp.c:4220-4640, base/netcmp.c:4640-4925, base/netcmp.c:4943-5420, base/netcmp.c:6200-6345)

#### Scenario: Fingers versus wide device
- **GIVEN** `property nfet parallel {w add} {l critical}`, layout nfet = 2 parallel records {W=1, L=0.15} and schematic nfet = 1 record {W=2, L=0.15, M=1}
- **WHEN** properties are compared
- **THEN** the layout records SHALL first merge to {W=1, L=0.15, M=2}, then (after the M mismatch) fold to {W=2, L=0.15, M=1}, and match

#### Scenario: Critical width is not folded (verify against netgen)
- **GIVEN** `property default`, which marks `w` as both parallel-add and parallel-critical, and the same two devices
- **WHEN** compared
- **THEN** W SHALL be treated as critical and not multiplied by M, so the pair SHALL report a property error

#### Scenario: Different finger counts with M
- **GIVEN** layout has 4 parallel records {W=0.5} and schematic has {W=0.5, M=4}
- **WHEN** compared
- **THEN** the layout SHALL reduce to {W=0.5, M=4} and match without a property error

### Requirement: Property error results
When any matched pair fails its property check, the comparison result SHALL become "unique with property errors" (verify `unique` = −3), or "port and property errors" (−4) when port errors also exist. In `lvs`, the cell pair SHALL be added to the list of cells with property errors. The final report SHALL end with:
- `The following cells had property errors:`
- the cell names, one per line, each prefixed by a space

(capability `lvs-reports`)

A failure to compare, meaning a class identity mismatch or a malformed record, SHALL set the property-error state to "failure". In netgen this state does not reach the `unique` result (see Q-LVS-10).
Compatibility: exact
(netgen ref: base/netcmp.c:6508-6526, tcltk/tclnetgen.c:2735-2745, tcltk/netgen.tcl.in:613-619, tcltk/netgen.tcl.in:716-718)

#### Scenario: Property error listed
- **GIVEN** cell `inv` matches with a W property error
- **WHEN** `lvs` finishes
- **THEN** the log SHALL end with `The following cells had property errors:` and ` inv`

### Requirement: Strict and relaxed topology
In relaxed topology mode (the default), records MAY have both `M` > 1 and `S` > 1. In strict mode, combination that would need both SHALL be refused. A refused combination that would otherwise have been possible SHALL cause a fallback to the other direction (see Requirement "Series/parallel network reconciliation").
Compatibility: exact
(netgen ref: base/netcmp.c:5028-5045, base/netcmp.c:5120-5130)

#### Scenario: Strict refuses
- **GIVEN** `property topology strict` and a record with S=2 among parallel records
- **WHEN** parallel merging is attempted
- **THEN** that merge SHALL NOT occur
