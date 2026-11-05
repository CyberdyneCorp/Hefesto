# Circuit Extraction

## Purpose
Magic extracts a hierarchical circuit description from layout. Each cell is written to its own `.ext` file, which records:
- nodes with lumped resistance, capacitance, area and perimeter
- devices
- subcell uses
- merges and coupling capacitances that arise from interactions between the parent and its subcells

Extraction runs bottom-up. Interaction regions are found in step-size squares. Pass 1 builds connectivity; pass 2 adjusts for capacitance and area that would otherwise be counted twice. Parasitic coefficients and device recognition come from the tech file's `extract` section.

## Requirements

### Requirement: Extract tech section
The `extract` section SHALL support these keywords, matched by unique prefix:
- Style and units: `style [variant]`, `variants`, `units lambda|microns`, `lambda`, `cscale`, `rscale`, `step`, `sidehalo`, `planeorder`/`noplaneordering`
- Substrate and resistance: `substrate`, `resist`, `contact`
- Capacitance: `areacap`, `perimc`, `overlap`, `sidewall [offset]`, `sideoverlap`, `defaultareacap`, `defaultperimeter`, `defaultsidewall`, `defaultoverlap`, `defaultsideoverlap`
- Connectivity: `connect`, `disconnect`
- Device and antenna parameters: `height`, `antenna`, `model`, `tiedown`, `devresist`, `fet`
- Devices: `device <class> ...` with the classes `mosfet`, `bjt`, `capacitor`, `capreverse`, `resistor`, `diode`, `pdiode`, `ndiode`, `subcircuit`, `rsubcircuit`, `msubcircuit`, `csubcircuit`, `dsubcircuit`, `veriloga`

Sidewall capacitance values SHALL be halved when parsed, and coupling SHALL be computed as `cap·overlap/(sep+offset)`, with `offset` a float. Layer types that the section never mentions SHALL be reported and treated as non-electrical. (src: extract/ExtTech.c:107-266,2002-3621)

#### Scenario: Illegal keyword
- **WHEN** an unknown keyword appears
- **THEN** the loader SHALL report "Illegal keyword.  Legal keywords are:" followed by the list

#### Scenario: Style defaults
- **WHEN** a style sets none of these values
- **THEN** step size SHALL be 100, units per lambda 100, cscale 1000, rscale 1000, and side halo 0 (src: extract/ExtTech.c:837-978)

### Requirement: extract command
`extract` with no arguments SHALL extract incrementally: a cell SHALL be re-extracted only when its `.ext` timestamp line differs from its cell timestamp.

`extract` SHALL also accept these options:
- `all`, `cell <name>`, `parents`, `showparents`
- `halo [d]`, `stepsize [d]`, `path [dir|none]`, `style [name]`
- `unique [all|#|noports|notopports]`
- `warn [[no] all|dup|fets|labels]`
- `length clear|driver|receiver`
- `do|no <option>`

(src: commands/CmdE.c:1064-1530, extract/ExtMain.c:360-991)

#### Scenario: Unnamed cell
- **WHEN** `extract` is run on an unnamed cell
- **THEN** the system SHALL report "Please name the cell before extracting."

#### Scenario: Output path
- **GIVEN** `extract path ext/` and the directory `ext/` does not exist
- **WHEN** the cell is extracted
- **THEN** magic SHALL try to create `ext/` and write the `.ext` files there

### Requirement: Extraction options and defaults
`extract do|no` SHALL toggle these options:
- `adjust`, `capacitance`, `coupling`
- `lumped` (lumped resistance; the keyword was `resistance` before 8.3.597)
- `length`, `fringehalo`, `labelcheck`, `aliases`
- `unique [notopports]`
- `extresist` / `resistance` (full R-C extraction)
- `corners [k]`
- `local`
- `all`

`all` SHALL cover adjust, capacitance, coupling, lumped, length and unique. It SHALL exclude `fringehalo`, `labelcheck`, `aliases`, `extresist`, `corners` and `notopports`.

At startup, every option in `all` plus `fringehalo`, `labelcheck` and `aliases` SHALL be on. When a technology loads, `fringehalo` SHALL be set from the tech format: on when the format is >= 34, off otherwise. (src: extract/extract.h:70-87, extract/ExtMain.c:58, extract/ExtTech.c:64,3580-3587, commands/CmdE.c)

#### Scenario: Individually settable fringehalo
- **GIVEN** a format-35 technology is loaded
- **WHEN** the user runs `extract no all` and then `extract do capacitance`
- **THEN** `fringehalo` SHALL remain on, because `all` does not include it

#### Scenario: Old technology
- **WHEN** a tech file with format 33 or earlier (or no format, which means 27) is loaded
- **THEN** `fringehalo` SHALL be off by default

### Requirement: .ext file format
Each `.ext` file SHALL start with `timestamp`, followed by `version`, `tech`, `style`, `scale R C L` and `resistclasses`. After the header it SHALL contain these record types:
- `parameters`, `port`, `node`, `substrate`, `attr`, `equiv`
- `device` (or legacy `fet`), `use`, `merge`, `cap`, `distance`
- `abstract`, `primitive`
- `connect`, written only with `extresist`

Capacitances SHALL be in aF divided by `cscale`. Resistances SHALL be in milliohms divided by `rscale`. (src: extract/ExtCell.c:562-675, extract/ExtBasic.c:216-864, extract/ExtHier.c:1110-1180, extflat/EFread.c:228-656)

#### Scenario: Version mismatch
- **WHEN** a `.ext` file from another magic version is read
- **THEN** the reader SHALL warn "Cell was extracted using version %s of the extractor."

### Requirement: Device recognition
For a tile whose type is a device type, the system SHALL choose the device record whose terminal types, substrate types and identifier layers (`+types`) match the surroundings. It SHALL write L/W or a value, any parameters declared as `x=name[*scale]`, and the terminals.

The reserved model names SHALL behave as follows:
- `Ignore`: the device is suppressed.
- `Short`: the terminals are merged.
- `None`: a model-less R or C is written.

A cell with a `device` property SHALL be written as that device, with its node parasitics zeroed. (src: extract/ExtBasic.c:242-559,2591-3243)

#### Scenario: No matching device
- **WHEN** no device record matches the surroundings
- **THEN** the system SHALL warn "No matching extraction type for device at (x y)"

### Requirement: Hierarchical interactions
Interactions between a parent and its subcells, and between subcells, SHALL be found within the side halo + 1 in step-size squares. Each interaction area SHALL be flattened and resolved into `merge` records, with capacitance adjustments (pass 2, only with `adjust`) and `cap` coupling records. Overlapping types that do not connect SHALL be reported as "Illegal overlap between %s and %s (types do not connect)". (src: extract/ExtSubtree.c:164-640, extract/ExtHier.c:837,963)

#### Scenario: Wire over subcell
- **WHEN** parent metal crosses subcell metal on a different plane with coupling enabled
- **THEN** the parent's `.ext` SHALL contain a `cap` record between the two nodes

### Requirement: Substrate handling
The substrate type declared in the tech file SHALL be painted into isolated substrate regions before extraction, and the plane SHALL be restored afterwards. Substrate nodes that are not global SHALL become implicit ports of subcircuits. (src: extract/ExtCell.c:290-451, extflat/EFbuild.c:1449-1458)

#### Scenario: Isolated well
- **WHEN** a device sits in an isolated well
- **THEN** that well SHALL be extracted as a separate node from the global substrate

### Requirement: Unique net names
`extract unique` SHALL permanently rename labels that name more than one unconnected node to `name_uqN`, keeping port labels in preference to internal labels. `do unique` SHALL apply the same renaming temporarily during extraction and then revert it. Names ending in `!` SHALL be global, and SHALL be uniquified only by `unique all`. (src: extract/ExtUnique.c:83-408)

#### Scenario: Duplicate label
- **WHEN** two unconnected shapes are both labeled `net1`
- **THEN** extraction SHALL write them as distinct nodes named `net1` and `net1_uq1` (or similar)

### Requirement: Label attributes
Label text ending in `@` SHALL become a node attribute, `$` a terminal attribute, and `^` a gate attribute. The terminal attributes `D`/`S` SHALL force drain/source order in the netlist. (src: extract/ExtBasic.c:4689-4709)

#### Scenario: Node attribute
- **WHEN** a label `res:force@` is placed on a node
- **THEN** an `attr` record SHALL be written for that node
