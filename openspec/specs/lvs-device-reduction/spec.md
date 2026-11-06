# LVS Device Reduction

## Purpose
Defines how devices are reduced before and during LVS comparison:
- combining parallel devices into one device with several property records
- combining series devices into one device
- permutable pins and the effect of `permute` on matching
- the per-class enable flags for combination
- removing zero-valued shorting devices
- how merged-device counts are reported

**Compatibility adaptation.** This capability is about netgen, not Magic. The classes in `docs/spec-conventions.md` apply relative to **netgen** (`RTimothyEdwards/netgen` @ `93dcda14d36349d0b0ca63168d3cbbab20fe2ece`, version 1.5.327):
- `exact`: identical reduced netlist and messages
- `behavioral`: same effect on match results
- `informative`: netgen detail
- `native`: cites `(Hefesto decision: docs/decisions/product.md#P-5)`

Traces use `(netgen ref: path/file.c:line)`. They are for traceability only (capability `hefesto-licensing`).

Other capabilities own related parts:
- reconciling the combined property records: `lvs-property-matching`
- the commands that set the flags: `lvs-setup-file`

## Requirements

### Requirement: Combination flags
Every device class SHALL carry two flags:
- **parallel-allowed.** It defaults to true for every class, including subcircuit and module classes. A class defined while `property parallel none` is in force SHALL start with it false.
- **series-allowed.** It defaults to false. `property default` sets it to true for resistor, capacitor and inductor classes, and `property <c> series …` can set it on any class.

Every property of interest SHALL carry a merge rule: a set of bits drawn from parallel-add, parallel-par, parallel-critical, series-add, series-par and series-critical. Two legacy "critical" bits are also accepted. "add" and "par" are mutually exclusive within one direction.
Compatibility: exact
(netgen ref: base/objlist.h:95-120, base/objlist.h:236-240, base/netgen.c:54-55, base/netgen.c:1315)

#### Scenario: Subcircuits merge in parallel by default
- **GIVEN** two instances of subcircuit `buf` with identical connections
- **WHEN** reduction runs with default settings
- **THEN** they SHALL be combined into one `buf` device with two property records

### Requirement: Reduction schedule
Before the comparison graph of a cell pair is built (capability `lvs-comparison-algorithm`), each circuit's cell SHALL be reduced on its own, in this exact loop:
1. Run one parallel pass.
2. Run series passes repeatedly until a series pass merges nothing.
3. Stop if the series loop did nothing on its first pass.
4. Otherwise run another parallel pass. Stop if it merged nothing (on any parallel pass after the first). Otherwise repeat from step 2.

If either circuit was modified, the engine SHALL print `Circuit was modified by parallel/series device merging.`, then `New circuit summary:`, then each circuit's contents (console only).
Compatibility: exact
(netgen ref: base/netcmp.c:2565-2578, base/netcmp.c:3992-4002)

#### Scenario: Series then parallel
- **GIVEN** two chains of two series resistors each, connected in parallel between the same nets
- **WHEN** reduction runs with `property default`
- **THEN** the series passes SHALL reduce each chain to one device, and the following parallel pass SHALL combine the two chains into one device

### Requirement: Parallel combination
A parallel pass SHALL combine devices of the same class that have identical pin connections. Only classes with parallel-allowed SHALL take part.

**Signature.** Each device's signature SHALL be its class name followed by, for each pin in order, `_<net number>`. A pin's net is written as `_nc` instead when all of these hold:
- "open" mode is on (the default; `property parallel open`)
- the cell is not a top cell
- the net is connected to exactly one pin in the whole cell (a no-connect)

Such a no-connect net SHALL be flagged no-connect. Treating every no-connect as one net can combine devices whose open pins are unrelated (see Q-LVS-26).

**Permutations.** If the class has permutable pins, a second signature SHALL also be registered, with the nets of the first declared pair exchanged. Only the first declared pair is used (see Q-LVS-9).

**Merging.** When a device's signature matches an earlier device:
1. The later device SHALL be removed.
2. Its property records SHALL be appended to the earlier device's list of property records.
3. If either device had no property record, a record `{M = 1}` (integer) SHALL be created for it first.
4. The engine SHALL maintain series-group markers (see Requirement "Series combination").

Property values SHALL NOT be part of the signature. Devices with different properties are combined, and their records are reconciled at property-matching time.

**Message.** Each pass that merges n > 0 devices of cell c in netlist f SHALL print `Class <c> (<f>):  Merged <n> parallel devices.`
Compatibility: exact
(netgen ref: base/netgen.c:3200-3530)

#### Scenario: Two fingers
- **GIVEN** non-top cell `cell` in file 0 has `M1 y a vss vss nfet W=1` and `M2 y a vss vss nfet W=1`
- **WHEN** a parallel pass runs
- **THEN** one nfet device with two property records `{W=1}`, `{W=1}` SHALL remain, and `Class cell (0):  Merged 1 parallel devices.` SHALL be printed

#### Scenario: Permuted parallel
- **GIVEN** source/drain are permutable and `M1 a g b x nfet`, `M2 b g a x nfet`
- **WHEN** a parallel pass runs
- **THEN** M1 and M2 SHALL be combined

### Requirement: Series combination
A series pass SHALL find each net n that meets all of these conditions:
- n is connected to exactly two device pins.
- Both pins belong to devices of the same class, and that class has series-allowed.
- Each pin is the first or second pin of its device.
- n is not a port of the cell.
- All other pins (third and later) of the two devices connect to the same nets, pairwise in order.
- The two devices are distinct.

The two devices SHALL be merged:
1. The pin of the first device on n SHALL be reconnected to the far net of the second device.
2. The second device and net n SHALL be removed.
3. The second device's property records SHALL be appended to the first device's records.

**Topology marker.** The appended records SHALL carry a string marker `_tag`. Its value is `+` for a record that had no marker; a record that already had one gets `S` prepended to its marker. The records are inserted at the front if the device had no records.

**Chains.** References in other candidate nets to the removed device SHALL be redirected to the surviving device. A candidate net whose two devices become the same device SHALL be dropped.

**Message.** Each pass that merges n > 0 devices SHALL print `Class <c> (<f>):  Merged <n> series devices.`
Compatibility: exact
(netgen ref: base/netgen.c:3531-3790)

#### Scenario: Resistor chain
- **GIVEN** `R1 a m 1k` and `R2 m b 2k`, with series allowed on `r` and m not a port
- **WHEN** a series pass runs
- **THEN** one resistor from a to b SHALL remain, with records `{value=1k}` and `{_tag=+, value=2k}`, and net m SHALL be removed

#### Scenario: Port blocks series merge
- **GIVEN** the same resistors, but m is a port of the cell
- **WHEN** a series pass runs
- **THEN** nothing SHALL be merged

### Requirement: Permutable pins in matching
The pins of a permutable pair SHALL be interchangeable during graph matching (capability `lvs-comparison-algorithm`, Requirement "Class identity and pin labels").

**Missing pin.** If a declared pin name does not exist on an instance, the engine SHALL print `Class <c> does not have pin <p>.` and stop applying permutations.

**Shorted ports.** A subcircuit with two different-named ports on the same net SHALL get those ports declared permutable automatically, with the message `Pins <a> and <b> are shorted in cell <c> (<f>)` (console only).
Compatibility: exact
(netgen ref: base/netcmp.c:7017-7085, base/flatten.c:1215-1240)

#### Scenario: Swapped source and drain
- **GIVEN** `permute default`, layout `M1 out g gnd b nfet` and schematic `M1 gnd g out b nfet`
- **WHEN** they are compared
- **THEN** the netlists SHALL match uniquely

### Requirement: Zero-valued device removal
During pre-match of a cell pair, the engine SHALL look at each device class of circuit 1 whose instance count differs between the circuits and that is a resistor, voltage source or current source. The same check SHALL be applied to circuit 2.

For each such device, if a property `value` or `length` is zero, the device SHALL be removed and its two terminal nets merged. "Zero" means the string `0`, the integer 0, or the double 0.0.

When the cell is not the top of the compare queue and the class is not a current source, and the device connects two ports, the engine SHALL first print `Warning: zero-valued device connects port <p> to another port;  pin matching may be affected.`

Each removal SHALL print `Removing zero-valued device <class> from cell <c> (<f>) makes a better match`, where `<class>` is the device class name, not the instance name.
Compatibility: exact
(netgen ref: base/flatten.c:1968-2230)

#### Scenario: Zero-ohm jumper
- **GIVEN** the schematic has `R1 a b 0` and the layout has nets a and b as one net
- **WHEN** the cells are pre-matched
- **THEN** R1 SHALL be removed, a and b SHALL be merged, and the device counts SHALL then agree

### Requirement: Merged-device counts in the summary
In the subcircuit summary, each class entry SHALL show the number of devices after reduction (C). It SHALL also show the total multiplicity before reduction (M), which is the sum over property records of their integer `M` value, counting a record with no `M` as 1. When M = C the entry SHALL read `<class> (<C>)`; otherwise it SHALL read `<class> (<M>-><C>)`.
Compatibility: exact
(netgen ref: base/netcmp.c:3220-3265, base/netcmp.c:3396-3420)

#### Scenario: Four fingers merged
- **GIVEN** four parallel nfet instances reduced to one device
- **WHEN** the summary is printed
- **THEN** the entry SHALL read `nfet (4->1)`

### Requirement: Reduction preserves pin order and names
Reduction SHALL NOT change any cell's port list or the port order of surviving instances. Net names of surviving nets SHALL be unchanged. In layout-to-layout navigation, the surviving device of a parallel or series group SHALL be the first one in instance order. Hefesto SHALL keep the list of combined instance names so that cross-probing can highlight every original device (capability `lvs-command`).
Compatibility: native
(Hefesto decision: docs/decisions/product.md#P-5)

#### Scenario: Cross-probe merged fingers
- **GIVEN** four fingers merged into one nfet device in the layout circuit
- **WHEN** the user selects that device in the LVS result browser
- **THEN** all four layout transistors SHALL be highlighted
