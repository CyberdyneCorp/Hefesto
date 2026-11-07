# Hefesto Magic-Compat Mode

## Purpose
Implements decision X-2, generalized by decision D-2 to every reference tool that Hefesto must match. Hefesto always fixes the reference tools' crashes, undefined behavior and data-loss bugs. Quirks that change output values are reproduced only when the compatibility setting for that tool is on, which is the default. With a setting off, Hefesto uses the corrected behavior and documents each difference. This capability owns the set of reference-tool compatibility settings: `magic-compat` for Magic and `netgen-compat` for netgen.

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

### Requirement: Reference-tool compatibility settings
Hefesto SHALL provide one compatibility setting per reference tool whose quirks it reproduces, and this capability SHALL be the only place where such settings are defined. The settings are:

| Setting | Reference tool | Governs the quirks recorded in | Default |
|---|---|---|---|
| `magic-compat` | Magic | every open-questions file except those listed below | ON |
| `netgen-compat` | netgen | `docs/decisions/open-questions/LVS.md` (capabilities `lvs-*`) | ON |

A quirk whose open-question entry records `Resolution: compat` SHALL be governed by the setting of the tool that its open-questions file covers, unless the entry names another setting. Each setting SHALL be stored per project and SHALL be changed independently of the others. Changing one setting SHALL NOT change any output that another setting governs. Whether xschem quirks get an `xschem-compat` setting of the same kind is open (D-21).

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2)

#### Scenario: Settings are independent
- **GIVEN** a project with `magic-compat` ON and `netgen-compat` OFF
- **WHEN** the layout is extracted and LVS is run
- **THEN** the `.ext` and netlist output SHALL include Magic's quirks, and the LVS comparison SHALL use the corrected netgen behavior

### Requirement: netgen-compat setting for netgen quirks
Hefesto SHALL provide a setting named `netgen-compat`, ON by default. When it is ON, every netgen quirk whose entry in `docs/decisions/open-questions/LVS.md` records `Resolution: compat` SHALL behave exactly as netgen does; examples are Q-LVS-7, Q-LVS-9, Q-LVS-20 and Q-LVS-24. When it is OFF, those quirks SHALL use the corrected behavior that the entry defines. The setting SHALL NOT change any layout, extraction or netlist output, so that LVS fidelity can be toggled without changing what Magic-compatible tools produce.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2)

#### Scenario: netgen quirk replicated by default
- **GIVEN** a new project, and two devices whose property values are both 0 under a relative tolerance (Q-LVS-7)
- **WHEN** LVS compares them
- **THEN** the result SHALL be the one netgen gives

#### Scenario: netgen-compat off leaves layout output unchanged
- **GIVEN** a project whose `netgen-compat` is switched from ON to OFF
- **WHEN** the same cell is extracted again
- **THEN** the `.ext` file SHALL be byte-identical to the one written before the change

### Requirement: Resolution recorded for every open question
Every entry in `docs/decisions/open-questions/` SHALL carry a `Resolution:` line with one of the values `fix`, `compat`, `platform` or `n/a`. A spec requirement that cites an unresolved entry SHALL be treated as provisional, and implementation SHALL NOT begin until that entry is resolved.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2)

#### Scenario: Provisional requirement
- **GIVEN** a requirement cites an open-question entry that has no Resolution line
- **WHEN** the release checklist runs
- **THEN** that requirement SHALL be reported as provisional
