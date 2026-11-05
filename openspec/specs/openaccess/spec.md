# OpenAccess Database Support (informative stub)

## Purpose

Records the existence of Magic's experimental OpenAccess (OA) integration, compiled only with both the Tcl wrapper and the `OPENACCESS` build option. It lets cell definitions flagged as OpenAccess-backed be searched through the OA database instead of Magic's own cell planes. It is not part of standard Magic builds and has no file-format implications for `.mag` data. Hefesto does not implement it.

## Requirements

### Requirement: OpenAccess scope
Hefesto SHALL treat OpenAccess support as out of scope: no OA database reading or writing is required, and `.mag`/GDS/LEF/DEF interoperability (other capabilities) is the supported data exchange path.

Compatibility: informative
(Magic ref: oa/oa.c:1-60, oa/magicOA.cpp, oa/magicInit.cpp, scripts/configure.in:1060-1075)

#### Scenario: OA library not supported
- **GIVEN** a design stored only in an OpenAccess library
- **WHEN** a user tries to open it in Hefesto
- **THEN** Hefesto reports that OpenAccess is not supported and suggests exporting to GDS, LEF/DEF or `.mag`

### Requirement: Magic OA behavior summary
When enabled in Magic, the OA module SHALL be documented as: initialising Tcl commands from the `magicoa` package at startup, and providing an OA-backed equivalent of the hierarchical cell search used for cells flagged as OpenAccess cells (the search callback contract is the same as for native cells: return 0 to continue, 1 to abort, 2 to skip remaining array elements).

Compatibility: informative
(Magic ref: oa/oa.c:20-120, oa/magicOA.h, oa/fake_oa_stub)

#### Scenario: Native cells unaffected
- **GIVEN** a Magic build with OA enabled and a design without OA-flagged cells
- **WHEN** cells are searched
- **THEN** behavior is identical to a build without OA
