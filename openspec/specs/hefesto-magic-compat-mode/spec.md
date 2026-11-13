# Hefesto Magic-Compat Mode

## Purpose
Implements decision X-2, generalized by decision D-2 to every reference tool that Hefesto must match. Hefesto always fixes the reference tools' crashes, undefined behavior and data-loss bugs. Quirks that change output values are reproduced only when the compatibility setting for that tool is on, which is the default. With a setting off, Hefesto uses the corrected behavior and documents each difference. This capability owns the set of reference-tool compatibility settings: `magic-compat` for Magic, `netgen-compat` for netgen and `xschem-compat` for xschem.

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

When the setting is OFF, those quirks SHALL use the corrected behavior that the open-question entry defines. The setting SHALL be stored per project and SHALL be recorded in the header comment of every netlist and `.ext` file that Hefesto writes, except the schematic SPICE netlists of capability `schematic-netlisting`, which record only `xschem-compat` (Requirement "xschem-compat setting for xschem quirks").

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
| `xschem-compat` | xschem | `docs/decisions/open-questions/SCHEM.md` (capabilities `schematic-*`) | ON |

A quirk whose open-question entry records `Resolution: compat` SHALL be governed by the setting of the tool that its open-questions file covers, unless the entry names another setting. Each setting SHALL be stored per project and SHALL be changed independently of the others. Changing one setting SHALL NOT change any output that another setting governs. `magic-compat` therefore does not govern `SCHEM.md`: switching it OFF to fix layout quirks SHALL NOT change a schematic netlist (D-21).

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2, docs/decisions/audit/DECISIONS.md#D-21)

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

### Requirement: xschem-compat setting for xschem quirks
Hefesto SHALL provide a setting named `xschem-compat`, ON by default. When it is ON, every xschem quirk whose entry in `docs/decisions/open-questions/SCHEM.md` records `Resolution: compat` SHALL behave exactly as xschem does; examples are Q-SCHEM-3, Q-SCHEM-12, Q-SCHEM-14, Q-SCHEM-19, Q-SCHEM-21 and Q-SCHEM-22. When it is OFF, those quirks SHALL use the corrected behavior that the entry defines. Warnings that an entry gives in both modes SHALL be reported whatever the setting.

The setting SHALL be stored per project. It SHALL NOT change any layout, extraction, LVS or netgen-compatible output, and `magic-compat` and `netgen-compat` SHALL NOT change any schematic file or schematic netlist.

Unlike `magic-compat`, the setting SHALL be recorded in a schematic netlist only when it is OFF: as the single line `** xschem-compat=off`, written directly after the first `** sch_path:` line (capability `schematic-netlisting`, Requirement "Netlist file structure"). With the setting ON nothing is added, so default netlists stay byte-identical to xschem's. The setting SHALL NOT be recorded in `.sch` or `.sym` files.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2, docs/decisions/audit/DECISIONS.md#D-21)

#### Scenario: xschem quirk replicated by default
- **GIVEN** a new project and a format containing `expr(2-1)` (Q-SCHEM-12)
- **WHEN** the schematic is netlisted
- **THEN** the value SHALL be `2`, as xschem gives, and the netlist SHALL contain no `xschem-compat` line

#### Scenario: xschem-compat off is recorded in the netlist
- **GIVEN** a project with `xschem-compat` OFF
- **WHEN** a schematic is netlisted
- **THEN** `expr(2-1)` SHALL give `1`, and the line after the first `** sch_path:` line SHALL be `** xschem-compat=off`

#### Scenario: xschem-compat off leaves layout and LVS output unchanged
- **GIVEN** a project whose `xschem-compat` is switched from ON to OFF
- **WHEN** the same cell is extracted again, and the same two netlists are compared by LVS again
- **THEN** the `.ext` file SHALL be byte-identical to the one written before the change, and the LVS result SHALL be the same

#### Scenario: magic-compat off leaves schematic netlists unchanged
- **GIVEN** a project whose `magic-compat` is switched from ON to OFF
- **WHEN** a schematic is netlisted again
- **THEN** the netlist SHALL be byte-identical to the one written before the change

### Requirement: Resolution recorded for every open question
Every entry in `docs/decisions/open-questions/` SHALL carry a `Resolution:` line with one of the values `fix`, `compat`, `platform` or `n/a`. A spec requirement that cites an unresolved entry SHALL be treated as provisional, and implementation SHALL NOT begin until that entry is resolved.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2)

#### Scenario: Provisional requirement
- **GIVEN** a requirement cites an open-question entry that has no Resolution line
- **WHEN** the release checklist runs
- **THEN** that requirement SHALL be reported as provisional
