# Hefesto Output Determinism

## Purpose
Implements decision X-1. Hefesto reproduces Magic's ordering wherever that ordering is defined by Magic's canonical tile decomposition. Where Magic's ordering comes from hash tables, memory addresses or platform-dependent sorts, Hefesto writes a documented deterministic order instead, and compatibility tests compare canonicalized output.

## Requirements

### Requirement: Tile-derived order is reproduced
Wherever Magic's output order derives from its canonical maximal-horizontal-strip tile decomposition and traversal (capability `layout-geometry-model`), Hefesto SHALL produce the same order, so that the output is byte-identical. This covers:
- `.mag` rect order
- GDS/CIF boundary order
- LEF RECT order
- DEF route segments
- `drc find` stepping order
- DRC error-tile counts

Compatibility: exact

(Hefesto decision: docs/decisions/cross-cutting.md#X-1)

#### Scenario: Identical .mag paint section
- **GIVEN** the same tech file and the same paint operations
- **WHEN** Hefesto and Magic each save the cell
- **THEN** the `<< layer >>` sections SHALL be byte-identical

### Requirement: Deterministic order where Magic's order is arbitrary
Where Magic's output order comes from hash-table iteration, memory addresses or platform-dependent sorting, Hefesto SHALL write records in this order, on every platform:
1. by primary name, using byte-wise comparison
2. then by the record's lower-left coordinates (x, then y)
3. then by type

For two-terminal coupling records, the terminal with the smaller name SHALL be written first.

This covers:
- `.ext` `cap`/`merge` records
- SPICE device, instance and port order where Magic uses hash order
- LEF `-tech` layer blocks
- DEF `VIAS`/`NONDEFAULTRULES`
- `.mag` elements
- instance and property order where Magic's sort is platform-dependent

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-1)

#### Scenario: Stable coupling order
- **WHEN** the same layout is extracted twice, on Linux and on iPadOS
- **THEN** the `.ext` files SHALL be byte-identical to each other, and their `cap` records SHALL be sorted by terminal names

### Requirement: Canonical comparison against Magic
Golden compatibility tests SHALL compare each output file in the way its format requires:
- Formats listed in the tile-order requirement SHALL be compared byte for byte, after masking fields documented as time-dependent, such as GDS modification dates.
- For the formats listed in the deterministic-order requirement, both Magic's and Hefesto's output SHALL be put into the same canonical order before comparison. Each format's canonicalizer SHALL be specified alongside its test harness.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-1)

#### Scenario: Equivalent .ext despite order
- **GIVEN** Magic writes `cap "a" "b" 3` and `cap "c" "a" 2` in hash order
- **WHEN** Hefesto writes `cap "a" "b" 3` and `cap "a" "c" 2`
- **THEN** the canonical comparison SHALL report the files equivalent
