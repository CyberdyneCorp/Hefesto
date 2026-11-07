# Tapeout Sign-off Checklist

## Purpose
Defines Hefesto's sign-off checklist. It is an ordered, reproducible set of checks that runs on the exact GDS to be submitted. Its result tells the user whether the design is ready for a given PDK and shuttle program. It covers:
- top-cell rules
- cell-name legality and uniqueness
- GDS layer-map sanity
- boundary, origin and size
- pins
- zero-area and off-grid geometry
- DRC
- antenna
- density, fill and seal ring presence
- LVS
- optionally, electromigration and IR drop (capability `reliability-em-check`, Requirement "Sign-off checklist item")

The checklist also states Hefesto's position on KLayout DRC decks. Hefesto implements Magic's DRC exactly. The gf180mcu and IHP shuttles, and parts of the sky130 prechecks, gate on KLayout Ruby-DSL decks that Magic's DRC does not reproduce. Decision P-18 (resolving Q-TAPEOUT-1) runs those decks externally on desktop.

Related capabilities:
- `drc-engine`, `drc-commands`: Magic DRC
- `antenna-check`: antenna checking
- `lvs-command` and the other `lvs-*` capabilities: LVS
- `gds-write`: GDS output
- `tapeout-fill-and-density`: density and fill
- `tapeout-seal-ring`: seal rings
- `tapeout-shuttle-packages`: per-program profiles and package layout
- `reliability-em-check`, `reliability-ir-drop`: the optional electromigration and IR-drop item

Reference sources (read-only, never executed):
- mpw_precheck @ `0941bdc1b62b5c3f99c8683bd11199d330af2ef3` (`mpw_precheck ref:`)
- Tiny Tapeout tt-support-tools @ `9d13ff3e3e6202090740110438c6a10c1d62ca0f` (`tt ref:`)
- wafer.space gf180mcu-precheck @ `9b4e7dab6af073d02b81ecc368069540d2f67db9` (`ws-precheck ref:`)
- open_pdks @ `c6df28c1fccb05c286aa0a15e9c4c7dcd8502607` (`open_pdks ref:`)

## Requirements

### Requirement: Checklist model
A sign-off run SHALL evaluate an ordered list of check items against one immutable input. That input is the GDS (or OASIS) produced for submission, identified by its SHA-256, together with the PDK version pin and the program profile. Each item SHALL have:
- a stable id
- a title
- a category (`structure`, `geometry`, `drc`, `electrical`, `manufacturing`, `package`)
- a severity on failure (`blocking` or `warning`)
- the program check name it mirrors, if any
- a result: `pass`, `fail`, `warning`, `waived`, `not-applicable`, `not-run` or `external`

A run SHALL be `ready` only when every blocking item is `pass` or `waived`, and the profile allows waivers for that item. A result SHALL be stale as soon as the input hash, the PDK pin or the profile version changes. Stale results SHALL be shown as stale and SHALL never count toward `ready`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Edit invalidates sign-off
- **GIVEN** a run on GDS with SHA-256 `h1` is `ready`
- **WHEN** the layout is edited and GDS is exported again with hash `h2`
- **THEN** the checklist SHALL show every item as stale, until it is re-run on `h2`

### Requirement: Default check order
The default checklist SHALL run in this order. A program profile MAY drop items marked optional, add program items, or reorder them to match the program's own precheck:

1. GDS readable and single top cell
2. Top-cell name
3. Database unit and origin
4. Die size, boundary layer, and shapes inside the boundary
5. Cell-name legality and uniqueness
6. Layer-map sanity and forbidden layers
7. Zero-area shapes and zero-length paths
8. Off-grid vertices
9. Seal ring presence (where required)
10. Fill presence and freshness (where required)
11. Density
12. Pins and power pins
13. Magic DRC (full style)
14. KLayout DRC decks (external)
15. Antenna
16. LVS
17. Electromigration and IR drop (optional; capability `reliability-em-check`)

Cheap structural checks SHALL run before DRC. A blocking failure in items 1 to 3 SHALL stop the run, because later items depend on a valid top cell. Every other failure SHALL let the run continue, so that all problems are reported at once.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Continue after DRC failure
- **GIVEN** a design with Magic DRC errors and an over-density window
- **WHEN** sign-off runs
- **THEN** both the DRC item and the density item SHALL be reported as `fail` in one run

#### Scenario: Stop on missing top cell
- **GIVEN** a GDS with two top-level structures
- **WHEN** sign-off runs
- **THEN** item 1 SHALL fail, listing both names, and items 2 to 16 SHALL be `not-run`

### Requirement: Top-cell rules
The submitted GDS SHALL have exactly one top-level structure, meaning a structure that no other structure references. If there are zero or several, the item SHALL fail and list their names. When the profile specifies a top-cell name, the name SHALL be equal to it. The profile may give a literal name, or a pattern such as `tt_um_*`. When the profile requires it, the name SHALL also equal the file stem.

For profiles that require it, the top cell's bounding box SHALL have its lower-left corner at (0, 0), and the database unit SHALL be 0.001 µm. In GDS terms, the UNITS record's user unit SHALL be 0.001 and its database unit 1e-9 m.

Compatibility: behavioral

(mpw_precheck ref: checks/topcell_check/topcell.py:1-40; ws-precheck ref: scripts/klayout/check_size.py:30-45; tt ref: precheck/precheck.py:172-183, precheck/precheck.py:449-452)

#### Scenario: Wrong top name for Tiny Tapeout
- **GIVEN** the profile requires top `tt_um_alice_counter`, and the GDS top is `counter`
- **WHEN** sign-off runs
- **THEN** the top-name item SHALL fail with `expected tt_um_alice_counter, got counter`

### Requirement: Cell-name legality and uniqueness
Every structure name in the GDS SHALL be checked, including the top and every subcell:
- It SHALL NOT contain `#` or `/`.
- It SHALL be at most 32 characters when the profile sets the classic GDS limit.
- It SHALL appear in at most one STRNAME record.

A name violation SHALL fail the item and list every offending name. The checklist SHALL also warn when two cells that are different in content were exported under names that differ only in case. It SHALL report every name that Hefesto renamed on export to avoid a collision (capability `gds-write` structure naming). When the profile requires submission-unique names, every non-PDK cell name SHALL carry the profile's prefix. Examples are cells merged into a shuttle-level GDS, such as Tiny Tapeout tiles or IHP shared reticles. The prefix may be the top-cell name or a user prefix. PDK library cells, recognised by the installed `libs.ref` names, SHALL be exempt.

Compatibility: behavioral

(tt ref: precheck/precheck.py:306-316; mpw_precheck ref: checks/illegal_cellname_check/illegal_cellname.py:1-45)

#### Scenario: Slash in subcell name
- **GIVEN** a GDS with structure `top/inv`
- **WHEN** sign-off runs
- **THEN** the item SHALL fail with `Cell name top/inv contains invalid character '/'`

#### Scenario: Prefix rule
- **GIVEN** a Tiny Tapeout profile with prefix rule on, top `tt_um_bob_x`, and user subcell `adder`
- **WHEN** sign-off runs
- **THEN** the item SHALL warn that `adder` does not start with `tt_um_bob_x_`, and SHALL offer to rename it

### Requirement: Layer-map sanity
Every (layer, datatype) and (layer, texttype) pair used in the GDS SHALL appear in the PDK profile's valid-layer list. This list is derived from the PDK's layer properties file, for example `sky130A.lyp`, `sg13g2.lyp` or `gf180mcu.lyp`. Pairs not in the list SHALL fail the item and be listed. Pairs in the profile's forbidden list SHALL fail separately. Examples of forbidden layers:
- sky130 Tiny Tapeout: `met5.*`
- ihp-sg13g2 Tiny Tapeout: `TopMetal2.*`
- gf180mcuD Tiny Tapeout and wafer.space: anything above Metal5, such as `MetalTop`

The checklist SHALL also report:
- Magic layout types that had no GDS mapping in the active output style, and were therefore silently dropped on export
- GDS layers present on input that the active input style did not read

Compatibility: behavioral

(tt ref: precheck/precheck.py:184-190, precheck/precheck.py:283-303, precheck/tech_data.py:205-226; ws-precheck ref: scripts/klayout/check_size.py:46-62)

#### Scenario: Unknown datatype
- **GIVEN** a sky130 GDS containing shapes on 68/99 that are not listed as valid for the profile
- **WHEN** sign-off runs
- **THEN** the layer-map item SHALL fail and list `(68, 99)`

### Requirement: Boundary and size
When the profile defines a die area, the top cell's bounding box SHALL equal it exactly. It is compared in integer nanometres, truncating µm·1000. When the box differs:
- if any edge lies outside the die area, the result SHALL be "Shapes outside project area"
- otherwise it SHALL be "Boundary layer doesn't cover project area"

When the profile names a boundary layer, there SHALL be a rectangle on that layer in the top cell exactly equal to the die area. Examples are sky130 and IHP `prBoundary.boundary`, and gf180mcu `PR_bndry`.

For wafer.space, the slot size SHALL be computed from the user die `(3880 + 2·26) µm × (5070 + 2·26) µm`, divided by `div_x` and `div_y`, with saw-street allowance 60 µm: `slot = (die − (div − 1)·60)/div`. The divisors for slots `1x1`, `0p5x1`, `1x0p5` and `0p5x0p5` are (1,1), (2,1), (1,2) and (2,2).

Compatibility: behavioral

(tt ref: precheck/precheck.py:192-237; ws-precheck ref: scripts/klayout/check_size.py:8-16, scripts/klayout/check_size.py:75-110)

#### Scenario: wafer.space half slot
- **WHEN** the wafer.space slot is `0p5x1`
- **THEN** the required width SHALL be (3932 − 60)/2 = 1936 µm, and the required height SHALL be 5122 µm

#### Scenario: Shape outside tile
- **GIVEN** a Tiny Tapeout sky130 1x1 tile with die area 161.00 µm × 111.52 µm, and a metal shape reaching x = 161.20 µm
- **WHEN** sign-off runs
- **THEN** the boundary item SHALL fail with "Shapes outside project area"

### Requirement: Zero-area and off-grid geometry
The checklist SHALL fail on any BOUNDARY or BOX element of zero area, and on any PATH whose points are all identical. It SHALL also fail on any vertex not on the PDK manufacturing grid (5 nm for sky130, gf180mcu and IHP). Each offender SHALL be reported with its structure, layer and coordinates. Hefesto's own GDS writer never produces zero-area elements. These checks guard against imported or verbatim-copied cells.

Compatibility: behavioral

(mpw_precheck ref: checks/drc_checks/klayout/zeroarea.rb.drc:1-60; tt ref: precheck/precheck.py:120-121)

#### Scenario: Degenerate path in vendor GDS
- **GIVEN** a verbatim-copied vendor cell with a two-point path whose points are equal
- **WHEN** sign-off runs
- **THEN** the zero-area item SHALL fail, naming that cell and layer

### Requirement: Pin and power-pin checks
When the profile supplies a pin template, the pin check SHALL compare the submitted LEF and GDS pins against the template. A pin template is a DEF giving pin names, layers and rectangles. The comparison SHALL check:
- every template pin is present
- each pin has the same layer and rectangle as the template
- no extra pins are present
- pin shapes lie on the profile's allowed pin layers (for example sky130 met1–met4 `.pin` datatype 16)

The power-pin check SHALL verify that the required power and ground pins exist in both the Verilog and the LEF. Examples are `VGND` and `VDPWR`, plus `VAPWR` only when the project declares analog power. In the LEF, power pins SHALL be declared `USE POWER` and ground pins `USE GROUND`.

Compatibility: behavioral

(tt ref: precheck/pin_check.py:123-300, precheck/precheck.py:240-280)

#### Scenario: Missing USE GROUND
- **GIVEN** a LEF where `PIN VGND` has no `USE GROUND ;`
- **WHEN** sign-off runs
- **THEN** the power-pin item SHALL fail with `VGND does not have a corresponding 'USE GROUND ;'`

### Requirement: Magic DRC sign-off run
The Magic DRC item SHALL run Hefesto's DRC engine (capability `drc-engine`) on the submitted GDS. The GDS is read back with mask hints enabled. The run loads the top cell expanded, with Euclidean distance on and DRC style `drc(full)`, and performs a complete check of the whole cell. The result SHALL be the list of (rule text, error rectangles) of `drc listall why` (capability `drc-commands`). A text report SHALL be produced in this format:

```
<top>
----------------------------------------
<rule why text>
----------------------------------------
 x1 y1 x2 y2            # each scaled by the output scale, printf " %.3f %.3f %.3f %.3f"
...
----------------------------------------
[INFO]: COUNT: <n>
[INFO]: Should be divided by 3 or 4
```

The item SHALL pass when `n = 0`. For every PDK, Hefesto SHALL present Magic DRC as necessary for sign-off, but not as sufficient when the program profile also lists KLayout decks (see Requirement: Position on KLayout DRC decks). Some programs run Magic DRC as non-gating, for example wafer.space, which ignores Magic DRC errors. For those programs the item's severity SHALL be `warning`.

Compatibility: behavioral

(tt ref: precheck/magic_drc.tcl:17-75; mpw_precheck ref: check_manager/__init__.py:448-472; ws-precheck ref: precheck.py:462-464, precheck.py:524-527)

#### Scenario: Clean sky130 tile
- **GIVEN** a DRC-clean sky130A layout
- **WHEN** the Magic DRC item runs
- **THEN** the report SHALL end with `[INFO]: COUNT: 0`, and the item SHALL pass

#### Scenario: Non-gating for wafer.space
- **GIVEN** a gf180mcuD design for wafer.space with 3 Magic DRC errors and a clean KLayout run
- **WHEN** sign-off runs
- **THEN** the Magic DRC item SHALL be `warning` and SHALL not block `ready`

### Requirement: Position on KLayout DRC decks
Hefesto SHALL implement Magic's DRC exactly. It SHALL NOT claim that Magic DRC replaces a foundry or shuttle KLayout deck. Each program profile SHALL list the KLayout decks that its precheck gates on, each with its deck path within the pinned PDK and its `-rd` variables:

| PDK / program | KLayout decks gating sign-off | Magic DRC |
|---|---|---|
| sky130 (Caravel precheck, Tiny Tapeout) | `sky130A_mr.drc` FEOL, BEOL, offgrid; pin-label-overlap; zero-area; met min clear-area density | gating |
| gf180mcuD (Tiny Tapeout, Caravel precheck) | `gf180mcu.drc` (all except density and antenna); antenna; density | not run |
| gf180mcuD (wafer.space) | `gf180mcu.drc` DRC, antenna, density; zero-area | run, non-gating |
| ihp-sg13g2 / sg13cmos5l (Tiny Tapeout, IHP MPW) | `ihp-sg13g2.drc` or `ihp-sg13cmos5l.drc`, density deck | not run |

On Linux and macOS, Hefesto SHALL be able to run a listed deck as an external process. This uses a user-installed `klayout` executable with batch flags, the deck from the pinned PDK, and the profile's variables. The result database (`.lyrdb`) SHALL be imported as checklist results, with markers browsable on the canvas. The item SHALL be `external`, and SHALL become `pass` or `fail` from the imported report. The report's input hash SHALL be recorded. Hefesto SHALL NOT link or bundle KLayout code (decision P-18; see Q-TAPEOUT-1 for the license and iPadOS constraints).

On iPadOS, and on desktop without `klayout`, each deck item SHALL be `not-run`. Such a run SHALL NOT be `ready` for a program whose profile marks that deck as gating. A run can become `ready` when the user attaches a `.lyrdb` report produced elsewhere and that report names the same input hash or the same top cell and file size.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-18; mpw_precheck ref: check_manager/__init__.py:240-270, check_manager/__init__.py:587-610; tt ref: precheck/precheck.py:489-560; ws-precheck ref: precheck.py:437-470)

#### Scenario: gf180 on iPad
- **GIVEN** a gf180mcuD Tiny Tapeout project on iPadOS
- **WHEN** sign-off runs
- **THEN** the KLayout GF180MCU DRC and antenna items SHALL be `not-run`, the run SHALL not be `ready`, and Hefesto SHALL explain that these checks run on desktop or in the program's CI

#### Scenario: Imported deck report
- **GIVEN** desktop with `klayout` installed and an ihp-sg13g2 project
- **WHEN** the KLayout SG13G2 DRC item runs and reports 2 violations of `M2.b`
- **THEN** the item SHALL fail, and both markers SHALL be selectable on the canvas with rule `M2.b`

### Requirement: Antenna check item
For PDKs whose profile marks Magic antenna checking as supported, the antenna item SHALL run Hefesto's antenna check (capability `antenna-check`) on the extracted layout of the top cell. It SHALL pass when no violations are reported. sky130 is such a PDK, and its PDK tech file defines the antenna rules. For gf180mcuD, the gating antenna check is a KLayout deck. Hefesto's Magic antenna check MAY run there as an advisory `warning` item. The gating item SHALL follow the KLayout deck position.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Advisory antenna on gf180
- **GIVEN** a gf180mcuD design with one Magic antenna violation
- **WHEN** sign-off runs on desktop without `klayout`
- **THEN** the Magic antenna item SHALL be `warning`, and the KLayout antenna item SHALL be `not-run`

### Requirement: Density, fill and seal ring items
The density item SHALL run every density rule in the PDK profile (capability `tapeout-fill-and-density`), and SHALL fail if any blocking rule fails. The fill item SHALL fail if:
- the profile requires fill and no fill cell exists, or
- the fill is stale

The seal ring item SHALL fail if the profile requires a seal ring and none is found. A seal ring is found through the recipe's marker layer or the generated seal ring cell.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: wafer.space without seal ring
- **GIVEN** a gf180mcuD wafer.space design without layer 167/5
- **WHEN** sign-off runs
- **THEN** the seal ring item SHALL fail with a message that the program requires a seal ring

### Requirement: LVS item
The LVS item SHALL run Hefesto's built-in LVS (capability `lvs-command`). It compares the netlist extracted from the submitted layout against the user's reference netlist, using the PDK's setup file. The item SHALL pass only on a unique match. The profile decides whether LVS is blocking. It is blocking for Caravel and wafer.space. It is optional for Tiny Tapeout digital tiles, because the program verifies them in its own flow. Hefesto SHALL treat KLayout LVS decks like DRC decks: an external desktop item with report import.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: LVS mismatch
- **WHEN** the extracted netlist has one net fewer than the reference
- **THEN** the LVS item SHALL fail, and link to the LVS report (capability `lvs-reports`)

### Requirement: Waivers
A user MAY waive a failing item, or individual DRC markers, with a free-text reason. Waivers SHALL be stored in the project, keyed by item id, rule and marker geometry, and SHALL be shown in every report. A profile SHALL declare which items accept waivers. Items that the program's own precheck enforces SHALL NOT accept waivers toward `ready`. A waiver SHALL lapse when the waived marker no longer matches the input geometry.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Unwaivable precheck item
- **GIVEN** a Tiny Tapeout profile
- **WHEN** the user tries to waive the boundary item
- **THEN** Hefesto SHALL refuse, explaining that the program's precheck enforces it

### Requirement: Sign-off reports
Every run SHALL write, in the project's `signoff/<run-id>/` folder:
- `results.json` holding:
  - the input SHA-256, PDK family, variant and version hash, profile id and version, and Hefesto version
  - per item: id, result, duration, messages and evidence file paths
- `results.md`, a table `| Check | Result |` with one row per item, in run order
- `results.xml`, a JUnit `testsuites`/`testsuite`/`testcase` file, with an `error` element for each failed item
- each item's own report, for example the Magic DRC text report, the density JSON and imported `.lyrdb` files

Reports SHALL be deterministic for identical inputs, apart from timestamps and durations.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15; tt ref: precheck/precheck.py:600-640)

#### Scenario: CI-friendly output
- **WHEN** sign-off runs headless on desktop
- **THEN** `results.xml` SHALL contain one `testcase` per item, and the process SHALL exit non-zero if the run is not `ready`
