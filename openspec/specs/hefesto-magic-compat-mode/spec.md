# Hefesto Magic-Compat Mode

## Purpose
Implements decision X-2. Hefesto always fixes Magic's crashes, undefined behavior and data-loss bugs. Magic quirks that change output values are reproduced only when the `magic-compat` setting is on, which is the default. With the setting off, Hefesto uses the corrected behavior and documents each difference.

## Requirements

### Requirement: Always-fixed defect classes
Hefesto SHALL NOT reproduce any Magic behavior that falls into one of these classes:
- **Crashes:** behavior that crashes the program.
- **Undefined behavior:** for example, uninitialized reads, NULL dereferences, and buffer overruns.
- **Data loss:** behavior that silently discards user data. Examples include ports with use `reset`/`scan`/`tieoff` lost on save, label text truncated at 99 characters, `.mag.gz` saved as `.mag.gz.mag`, and backup content duplicated.

The open-question entry for each such case SHALL record `Resolution: fix`. The requirement that cites it SHALL state the corrected behavior.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2)

#### Scenario: Long label preserved
- **WHEN** a cell with a 150-character label is saved and reloaded
- **THEN** the full label text SHALL be preserved, whatever the `magic-compat` setting

### Requirement: magic-compat setting for value-changing quirks
Hefesto SHALL provide a setting named `magic-compat`, ON by default. When it is ON, every quirk whose open-question entry records `Resolution: compat` SHALL behave exactly as Magic does. Examples of such quirks:
- rounding asymmetries
- the `.res.ext` +0.5 mΩ offset
- triangle rounding by diagonal direction
- argument-parsing quirks that change results

When the setting is OFF, those quirks SHALL use the corrected behavior that the open-question entry defines. The setting SHALL be stored per project and SHALL be recorded in the header comment of every netlist and `.ext` file that Hefesto writes.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2)

#### Scenario: Compat on by default
- **GIVEN** a new project
- **WHEN** extresist output is written
- **THEN** the resistance values SHALL include Magic's +0.5 mΩ offset, matching Magic

#### Scenario: Compat off
- **GIVEN** `magic-compat` is OFF
- **WHEN** the same extresist output is written
- **THEN** the values SHALL omit the offset, and the file header SHALL state `magic-compat=off`

### Requirement: Resolution recorded for every open question
Every entry in `docs/decisions/open-questions/` SHALL carry a `Resolution:` line with one of the values `fix`, `compat`, `platform` or `n/a`. A spec requirement that cites an unresolved entry SHALL be treated as provisional, and implementation SHALL NOT begin until that entry is resolved.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2)

#### Scenario: Provisional requirement
- **GIVEN** a requirement cites Q-DRC-10, which has no Resolution line
- **WHEN** the release checklist runs
- **THEN** that requirement SHALL be reported as provisional
