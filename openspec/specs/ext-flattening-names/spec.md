# ext-flattening-names

## Purpose

This capability defines hierarchical node names during netlisting:
- their structure and canonical string form
- instance and array-element naming
- the preference order that picks a node's canonical name
- global and ground name detection
- the per-component hash used to order name tables
- the two output conversions used by the netlisters: the SPICE form, used by the `netlist-spice-*` capabilities, and the `.sim` form, used by `netlist-sim`

## Requirements

### Requirement: Hierarchical name structure
A hierarchical name SHALL be a sequence of components ordered from the root side to the leaf. Its canonical string SHALL be the components joined with `/`. A name string read from a file SHALL be split at every `/` into components, and empty components are kept. Joining an instance path with a local name SHALL append the local name's components after the path's components. Two names SHALL be equal when they have the same number of components and the components are pairwise byte-equal.
Compatibility: exact
(Magic ref: extflat/EFname.c:174-360, extflat/EFname.c:820-838)

#### Scenario: Split and join
- **GIVEN** the instance path `x1/x2` and the local name `a/b`
- **WHEN** they are joined
- **THEN** the result has 4 components and the canonical string is `x1/x2/a/b`

### Requirement: Component hash
Each component SHALL carry a 32-bit unsigned hash. It is computed over its bytes with `sum = ((sum >> 28) | (sum << 4)) + c`, starting from 0, where `c` is the byte value. The hash of a whole name SHALL be the 32-bit signed sum of its component hashes. Name-keyed tables use this hash, as described in Requirement "Container iteration order" of capability `ext-flattening-hierarchy`. Global-name tables use the leaf component's hash only.
Compatibility: exact
(Magic ref: extflat/EFint.h:39-40, extflat/EFname.c:763-818, extflat/EFname.c:840-850, extflat/EFflat.c:908-912)

#### Scenario: Two-character component
- **GIVEN** the component `ab`
- **WHEN** its hash is computed
- **THEN** it is ((97 << 4) + 98) = 1650

### Requirement: Instance and array element names
The path component for a use SHALL be the use id. For an array use, a subscript is appended to the use id:
- `[y,x]`, y first, if the use spans both axes.
- `[x]` if it spans only x.
- `[y]` if it spans only y.

x and y are the element's decimal indices.
Compatibility: exact
(Magic ref: extflat/EFname.c:631-725)

#### Scenario: 2-D array element
- **GIVEN** a use `u1` with x range 0..3 and y range 0..1
- **WHEN** element x=2, y=1 is named
- **THEN** its path component is `u1[1,2]`

#### Scenario: 1-D array element
- **GIVEN** a use `bit` with x range 0..7 and a single y element
- **WHEN** element x=5 is named
- **THEN** its path component is `bit[5]`

### Requirement: Name preference order
Comparing name N1 against name N2, N1 SHALL be preferred when the first applicable rule below favors it. Let `last` be the final character of the leaf component.
1. Unless both leaves end in `!`: a leaf not ending in `#` is preferred over one ending in `#`.
2. Fewer components are preferred.
3. With one component each: the name `0` loses to any other name.
4. Fewer total characters, summed over all components, are preferred.
5. Otherwise, components are compared root-first by byte `strcmp`. N1 is preferred only if it compares greater (see Q-NETLIST-10).

Global names (ending in `!`) receive no extra preference.
Compatibility: exact
(Magic ref: extflat/EFname.c:510-625)

#### Scenario: Label beats generated name
- **GIVEN** the names `a` and `b#`
- **WHEN** they are compared
- **THEN** `a` is preferred

#### Scenario: Shallower name wins
- **GIVEN** the names `x/a` and `bb`
- **WHEN** they are compared
- **THEN** `bb` is preferred

#### Scenario: Lexically greater wins tie
- **GIVEN** the names `a` and `b`
- **WHEN** they are compared
- **THEN** `b` is preferred

#### Scenario: Zero substrate name loses
- **GIVEN** the names `0` and `VSS`
- **WHEN** they are compared
- **THEN** `VSS` is preferred

### Requirement: Global and ground detection
A name SHALL be global if its leaf component is an entry of `globalNets` in the project setting "global nets and power names" (capability `hefesto-settings`), which takes the place of Magic's Tcl array `globals`; this follows Magic's Tcl build. A trailing `!` does not by itself make a name global here, because Magic's Tcl build does not apply that rule in flattening. Informative: Magic's non-Tcl build treats a name as global if and only if its leaf ends in `!`; the same result is obtained in Hefesto by listing those names in `globalNets`. A name SHALL be the compatibility-mode ground if it has a single component equal to `GND!`, or equal to the `gnd` field of the same setting (Magic's `$GND`). See Q-NETLIST-11 and Q-NETLIST-33, decided by D-14.
Compatibility: behavioral
(Magic ref: extflat/EFname.c:101-170)

#### Scenario: Global leaf
- **GIVEN** the name `u1/vdd!` and `globalNets` = `vdd!`
- **WHEN** it is tested
- **THEN** it is global

#### Scenario: Trailing bang alone is not global
- **GIVEN** the name `u1/vss!` and `globalNets` empty
- **WHEN** it is tested
- **THEN** it is not global

#### Scenario: Ground from the setting
- **GIVEN** the `gnd` field is `VGND`
- **WHEN** the single-component names `VGND`, `GND!` and `u1/VGND` are tested
- **THEN** `VGND` and `GND!` are the compatibility-mode ground and `u1/VGND` is not

### Requirement: SPICE output name conversion
The SPICE netlisters SHALL convert a canonical name to text as follows, given the active conversion flags (trim-global, trim-local, convert-comma, convert-equal, convert-brackets) and the output format.

**When all five flags are off**, only the leaf component is output, verbatim. This happens only when an instance name is printed with every flag cleared. Netlisters set at least convert-comma whenever they print a name.

**Non-leaf components** are each output followed by `/`, character by character:
- `=` becomes `:` if convert-equal is on; otherwise it is kept.
- `[` or `]` becomes `_` if convert-brackets is on.
- `,` becomes `|` if convert-comma is on; otherwise it is deleted.
- Every other character is copied.

**The leaf component** is output character by character:
- `!` is deleted if trim-global is on.
- `.` becomes `@` in HSPICE format.
- `=` becomes `:` if convert-equal is on; otherwise it is deleted.
- `,` becomes `|` if convert-comma is on; otherwise it is deleted.
- `[` or `]` becomes `_` if convert-brackets is on.
- `#` is deleted if trim-local is on.
- Every other character is copied.

These deletions apply anywhere in the leaf, not only to a trailing character. See Q-NETLIST-30.
Compatibility: exact
(Magic ref: ext2spice/ext2spice.c:4308-4396)

#### Scenario: Default SPICE conversion
- **GIVEN** the flags trim-global, convert-comma and convert-equal, SPICE3 format, and the name `u1[0,1]/vdd!`
- **WHEN** it is converted
- **THEN** the text is `u1[0|1]/vdd`

#### Scenario: HSPICE dot in leaf
- **GIVEN** HSPICE format and the name `x/a.b`
- **WHEN** it is converted
- **THEN** the text is `x/a@b`

### Requirement: Sim output name conversion
The `.sim` netlister SHALL output a name as follows:
- Each non-leaf component is written verbatim followed by `/`.
- If no conversion flag is active, the leaf is written verbatim.
- Otherwise, every leaf character except the last is written verbatim, with two exceptions: `,` becomes `|` under convert-comma or is deleted otherwise, and `[`/`]` becomes `_` under convert-brackets.
- The last leaf character is deleted if it is `!` under trim-global or `#` under trim-local. Otherwise it is written verbatim.
Compatibility: exact
(Magic ref: extflat/EFvisit.c:878-939)

#### Scenario: Sim trailing bang
- **GIVEN** only the trim-global flag and the name `a!b!`
- **WHEN** it is written in `.sim` form
- **THEN** the text is `a!b`
