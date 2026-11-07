# Hefesto Output Determinism

## Purpose
Implements decision X-1 (as amended on 2026-11-06). Hefesto reproduces Magic's ordering wherever that ordering is a deterministic function of the input: Magic's canonical tile decomposition, and Magic's string-keyed hash tables, whose order depends only on key names and insertion history. This capability owns the definition of the "Magic string hash table" that other capabilities cite. Where Magic's ordering comes from memory addresses or platform-dependent sorting, and for the few name-keyed records whose order never affects any result, Hefesto writes a documented deterministic order instead, and compatibility tests compare canonicalized output.

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

### Requirement: Magic string hash table
Wherever a capability says that an order is the iteration order of a "Magic string hash table" (also written "Magic hash order" or "table enumeration order"), Hefesto SHALL produce the order of a chained hash table that behaves exactly as follows:
- **Size.** The initial bucket count `size` is the smallest power of two ≥ max(2, requested). The citing capability states the requested count; most Magic tables request 32.
- **Shift.** `shift = 30 − m`, where `size = 2^m`. So `shift` is 29 for size 2 and 25 for size 32, and decreases by 1 for each doubling.
- **String hash.** `h` is an unsigned 64-bit accumulator starting at 0. For each key byte `c`, in order, `h = c + (h<<6) + (h<<16) − h` (mod 2^64). Each key byte is taken as a signed 8-bit value sign-extended to 64 bits. Magic takes key bytes as C `char`, which is signed on x86-64 Linux and on macOS but unsigned on Linux/arm64. Hefesto SHALL use the signed interpretation on every platform (see Q-LEFDEF-116, Q-NETLIST-2, Q-LEGACY-70). Magic's variant with a 32-bit accumulator, on hosts where `unsigned long` is 32 bits, is not reproduced.
- **Client hash.** A table keyed by a client value with its own hash function, such as hierarchical names (capability `ext-flattening-names`), uses `h` = that signed 32-bit client hash sign-extended to 64 bits. Such a table counts as name-keyed when the client hash depends only on the key's names.
- **Bucket.** `bucket = (((h × 1103515245 + 12345) mod 2^64) >> shift) & (size − 1)`.
- **Insertion.** A new key is inserted at the head of its bucket chain. Before inserting, if the entry count is ≥ 3 × size, the table grows to 4 × size: old buckets are visited in ascending index, each chain from head to tail, and each entry is pushed onto the head of its bucket in the new table. The entry count is then recounted.
- **Removal.** Removing a key unlinks its entry and leaves the relative order of the other entries unchanged. The entry count used by the growth test is not decreased until the next growth recounts it. A citing capability may instead specify that removal only clears an entry's value and keeps its position.
- **Iteration.** Buckets are visited in ascending index 0 … size − 1, each chain from head to tail.

The iteration order of such a table is therefore fully determined by its requested size, its key bytes and its insertion and removal history. A capability that iterates one of these tables SHALL name the table and its requested size, and SHALL cite this requirement instead of restating the algorithm.

Compatibility: exact

(Magic ref: utils/hash.c:39, utils/hash.c:146-262, utils/hash.c:353-470, utils/hash.c:492-530, utils/hash.c:592-630, utils/hash.c:641-690)

#### Scenario: Hash of a short key
- **GIVEN** the key `"ab"` (bytes 97, 98) in a table of size 32 (m = 5, shift = 25)
- **THEN** h = 98 + 97·65599 = 6363201, and the bucket SHALL be `((6363201·1103515245 + 12345) mod 2^64 >> 25) & 31` = 1

#### Scenario: Insertion history decides the order
- **GIVEN** a 32-bucket table into which `u3`, `u1` and `u2` are inserted in that order
- **WHEN** the table is iterated
- **THEN** the hashes are 7675132, 7675133 and 7675134, the buckets are 3, 4 and 5, and the order SHALL be `u1`, `u2`, `u3`

#### Scenario: Same bucket, most recent first
- **GIVEN** keys `a` then `b`, both of which hash to bucket 22 of 32
- **WHEN** the table is iterated
- **THEN** `b` SHALL be visited before `a`

### Requirement: Name-keyed hash order is reproduced
Wherever Magic's output order, or a choice that changes a result, comes from iterating a Magic string hash table whose keys are names (requirement "Magic string hash table"), Hefesto SHALL reproduce Magic's order exactly, so that the output is byte-identical. Such an order depends only on names and insertion history. In several places it decides results that canonicalized comparison cannot hide, such as the LEF layer-map winner and implicit SPICE port numbering. This covers:
- netlist device, subcircuit, instance and flatten order, and implicit port numbering (capabilities `ext-flattening-hierarchy`, `netlist-spice-options`)
- LEF `-tech` LAYER blocks, PROPERTYDEFINITIONS keys, SITE blocks and the magic-type-to-LEF-layer winner (capability `lef-write`)
- DEF `VIAS`, `NONDEFAULTRULES` and BLOCKAGES order (capability `def-write`)
- `.ext` subcell and array `merge` groups (capability `extract-file-format`)
- `.net` netlist files and router net order (capability `netlist-window`)

The only exceptions are the name-keyed records listed in requirement "Deterministic order where Magic's order is arbitrary", whose order never affects any result.

Compatibility: exact

(Hefesto decision: docs/decisions/cross-cutting.md#X-1)

#### Scenario: Layer-map winner follows hash order
- **GIVEN** a 32-bucket layer table holding route records `metal1` (bucket 11) and `met1` (bucket 6), both with magic type `m1`
- **WHEN** Hefesto writes LEF for paint of type `m1`
- **THEN** the paint SHALL be written under `LAYER met1 ;`, as Magic writes it, because bucket 6 is visited first

### Requirement: Deterministic order where Magic's order is arbitrary
Hefesto SHALL write records in the following order, on every platform, wherever Magic's output order comes from memory addresses, from platform-dependent sorting, or from a name-keyed hash table whose order never affects any result:
1. by primary name, using byte-wise comparison or the comparator named by the owning capability
2. then by the record's lower-left coordinates (x, then y)
3. then by type

For two-terminal coupling records, the terminal with the smaller name SHALL be written first.

This covers:
- `.ext` `cap` records and the terminal order within them, which Magic keys by node addresses (Q-EXTRACT-7)
- flat coupling-capacitor order in netlists (Q-NETLIST-1)
- resistor-network orders that depend on unstable sorts or address-keyed tables (Q-NETLIST-26, Q-NETLIST-45)
- instance and property order where Magic's sort is platform-dependent
- `drc count` cell order, which Magic keys by memory addresses (capability `drc-commands`)
- `.mag` elements, sorted by name with the natural comparator (Q-CELL-11). The reader does not depend on their order.
- cell order in crash-backup and archive files, sorted by name with the natural comparator (Q-CELL-28). The reader does not depend on their order.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-1)

#### Scenario: Stable coupling order
- **WHEN** the same layout is extracted twice, on Linux and on iPadOS
- **THEN** the `.ext` files SHALL be byte-identical to each other, and their `cap` records SHALL be sorted by terminal names

### Requirement: Canonical comparison against Magic
Golden compatibility tests SHALL compare each output file in the way its format requires:
- Records covered by the tile-order and name-keyed hash-order requirements SHALL be compared byte for byte, after masking fields documented as time-dependent, such as GDS modification dates.
- For the records covered by the deterministic-order requirement, both Magic's and Hefesto's output SHALL be put into the same canonical order before comparison. Each format's canonicalizer SHALL be specified alongside its test harness.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-1)

#### Scenario: Equivalent .ext despite order
- **GIVEN** Magic writes `cap "a" "b" 3` and `cap "c" "a" 2` in address order
- **WHEN** Hefesto writes `cap "a" "b" 3` and `cap "a" "c" 2`
- **THEN** the canonical comparison SHALL report the files equivalent

#### Scenario: Merge groups compared byte for byte
- **GIVEN** a parent cell whose subcell `merge` groups Magic writes in name-keyed hash order
- **WHEN** Hefesto extracts the same cell
- **THEN** the `merge` lines SHALL appear in Magic's order, and no canonicalization SHALL be applied to them
