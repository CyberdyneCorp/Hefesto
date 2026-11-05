# Technology Files

## Purpose
A technology (`.tech`) file defines a fabrication process for Magic. It is a set of sections, each ending in `end`, that declare:
- planes, layer types, contacts and aliases
- display styles, paint composition and connectivity
- CIF/GDS input and output rules
- DRC, extraction, LEF, routing, wiring, plowing and plotting rules

Each subsystem registers a handler for its own section. Section order is enforced only through prerequisite masks.

## Requirements

### Requirement: Sections and prerequisites
The loader SHALL recognize these sections (src: utils/main.c:631-716, utils/tech.c:677-784):

- Required: `tech`, `planes`, `types`, `styles`, `contact` (alias `images`), `compose`, `connect`, `cifoutput`, `cifinput`, `drc`, `extract`
- Optional: `version`, `aliases`, `mzrouter`, `lef`, `wiring`, `router`, `plowing`, `plot`

A section that appears before its prerequisites SHALL produce "Section %s appears too early." and SHALL be skipped. A missing required section SHALL make the load fail with "Section \"%s\" was missing from %s.".

#### Scenario: Unknown section
- **WHEN** a section header `foo` is encountered
- **THEN** the loader SHALL report "Unrecognized section name: foo" and skip to its `end`

### Requirement: Lexical rules and directives
The loader SHALL apply these lexical rules (src: utils/tech.c:608-675,977-1101):
- A line whose first non-blank character is `#` is a comment.
- A trailing `\` continues the line.
- `"…"` quotes a token, and `\x` escapes the character x.
- A line is limited to 1024 characters and 30 tokens.

The directive `include <file>` SHALL be resolved against `.`, then the system path, then the including file's directory. The directive `repeat N … endrepeat` SHALL repeat the enclosed lines N times.

#### Scenario: Include missing file
- **WHEN** `include foo.inc` cannot be found
- **THEN** the loader SHALL warn "Couldn't find include file foo.inc"

### Requirement: Format and version metadata
The `tech` section SHALL name the technology and MAY set `format N` (or `version N`); the default is 27. The format number SHALL gate behavior: for example, format >= 34 enables `fringehalo` extraction by default.

The `version` section MAY contain `version`, `description` and `requires [magic-]M.m.r` lines. The load SHALL fail if the running magic is older than `requires`. (src: database/DBtech.c:70-258, utils/tech.h:40, extract/ExtTech.c:64,3582)

#### Scenario: Required magic version
- **GIVEN** the tech file has `requires 8.3.700` and the running magic is 8.3.694
- **WHEN** the tech file is loaded
- **THEN** the load SHALL fail with "Magic version 8.3.700 is required by this techfile, but this version of magic is 8.3.694."

### Requirement: Types, aliases and type-list syntax
Each `types` line SHALL be `<plane> <name>[,<short>...]`. A `-` prefix on the plane SHALL mark the type as locked. Layer names SHALL be matched by unique prefix.

Type lists SHALL support:
- commas
- `~` for complement
- parentheses
- `/plane` qualifiers
- `*x` for x plus every contact containing x
- `0` for the empty list

(src: database/DBtechtype.c:346-488, database/DBtechname.c:556-725)

#### Scenario: Ambiguous name
- **WHEN** a type name prefix matches two types
- **THEN** the system SHALL report "Ambiguous layer (type) name"

### Requirement: Contacts and stacking
`contact` lines SHALL declare a contact type and its residues. Each residue SHALL be on a different plane, and one residue SHALL be on the contact's home plane. `stackable` with no arguments SHALL make every pair of contacts that share a plane stackable. (src: database/DBtcontact.c:100-262)

#### Scenario: Residue on same plane
- **WHEN** two residues of a contact share a plane
- **THEN** the loader SHALL report "Contact residues (%s) must be on different planes"

### Requirement: Compose and connect rules
The `compose` section SHALL accept these rule forms, which modify the paint and erase tables (src: database/DBtpaint.c:352-692, database/DBtech.c:310-500):
- `compose <result> <a> <b>...`
- `decompose <result> <a> <b>...`
- `paint <t1> <t2> <result> [plane]`
- `erase <t1> <t2> <result> [plane]`

The `connect` section SHALL declare symmetric electrical connectivity between type lists. Extraction and net selection SHALL use these connections.

#### Scenario: Composite layer
- **GIVEN** `compose pfet poly pdiff`
- **WHEN** poly is painted over pdiff
- **THEN** the result SHALL be pfet

### Requirement: Loading and reloading
`tech load <name> [-noprompt] [-override]` SHALL find `<name>.tech` (or `.tech27`) in `.` and then in the system path. If loaded cells contain paint and `-noprompt` is not given, it SHALL first ask "Technology file (re)loading may invalidate the existing layout.  Continue?".

After a successful load the system SHALL:
- apply the CIF minimum grid
- rescale the tables to the current internal grid
- add or remove planes in every existing cell, warning "Number of planes has changed"

(src: utils/tech.c:425-860, commands/CmdTZ.c:546-596)

#### Scenario: Failed load
- **WHEN** the tech file is not found
- **THEN** the system SHALL report "Could not find file '%s.tech' in any of these directories:" followed by the path

### Requirement: Built-in technologies
The distribution SHALL install these technologies (src: scmos/Makefile.in:25-64):
- `minimum`: the fallback when no technology is chosen
- `gdsquery`
- `scmos`, `scmos-tm`, `scmos-sub`, `scmosWR`
- `nmos`

#### Scenario: GDS query without a PDK
- **WHEN** `magic -T gdsquery` is run
- **THEN** a technology SHALL be loaded that is suitable for inspecting GDS files without a PDK
