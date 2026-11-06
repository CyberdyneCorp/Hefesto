# Simulation Corners

## Purpose
Defines corner analysis in Hefesto (decision P-19):
- the corner definitions and corner sets held by each PDK's simulation model descriptor in the PDK store
- the bundled corner tables for sky130A/B, gf180mcuC/D and IHP sg13g2
- user-defined corners and corner sets in a testbench
- the corner dimension of a run set and its combination with sweeps and Monte Carlo
- the corner matrix view, "run all", re-running failed corners, and pass/fail per corner
- the `sim corners` command option

This capability builds on capability `simulation-netlist-preparation`: Requirement "Simulation model descriptor in the PDK store" (the descriptor and its section variables) and Requirement "Corners" (the corner tuple, the supply scale and the corner naming rule). It does not change them. Run sets are defined in capability `simulation-sweeps`. Specifications, limits and pass/fail come from capability `simulation-measurements-and-statistics`. Waveforms open in the viewer of capability `simulation-ui-and-crossprobe`.

PDK sources examined (read-only):
- sky130: `fossi-foundation/skywater-pdk-libs-sky130_fd_pr` @ `29b55153`, `combined_models/sky130.lib.spice` (installed by open_pdks @ `c6df28c1`, version 1.0.617, as `libs.tech/combined/sky130.lib.spice`)
- gf180mcu: `google/gf180mcu-pdk-libs-gf180mcu_fd_pr` @ `aacc04cc`, `models/ngspice/sm141064.ngspice` and `design.ngspice`
- IHP: `IHP-GmbH/IHP-Open-PDK` @ `5e6d592e`, `ihp-sg13g2/libs.tech/ngspice/models/corner*.lib`

Open questions: `docs/decisions/open-questions/SIMADV.md` (Q-SIMADV-n).

## Requirements

### Requirement: Corner definitions in the PDK descriptor
The simulation model descriptor of every PDK in the PDK store SHALL define its corners as a table with one row per named corner and these columns:
- `name`: unique within the PDK, matching `[a-z0-9_]+`
- one section word for each section variable of the descriptor (capability `simulation-netlist-preparation`), or `-` for "use the default corner's word"
- `params`: zero or more `.param` assignments that the corner needs (for example model switches)
- `kind`: `process` (an ordinary corner), `mismatch` (a section that enables mismatch, used by capability `simulation-monte-carlo`) or `statistical` (a section that enables process variation, used by that capability)

The descriptor SHALL also define named corner sets: ordered lists of process corners, each with a list of temperatures (°C) and a list of supply scale factors. The descriptor SHALL name a default corner set. Only corners of kind `process` SHALL appear in corner sets and in the corner matrix; the other kinds are reserved for Monte Carlo. A corner whose section word is not defined in the PDK's library file SHALL be rejected when the descriptor is imported, with `Corner <name>: section <word> not found in <file>`. On Linux and macOS, a descriptor generated from a PDK directory (capability `simulation-netlist-preparation`) SHALL list every `.lib` section of the referenced library file as a candidate process corner, for the user to confirm.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-2.)

#### Scenario: Missing section rejected
- **GIVEN** a user descriptor for sky130A that defines a corner `sf_lh` with section word `sf_lh`
- **WHEN** the descriptor is imported against the pinned `sky130.lib.spice`
- **THEN** the import SHALL fail with `Corner sf_lh: section sf_lh not found in sky130.lib.spice`

### Requirement: Bundled corner tables
Hefesto SHALL bundle these corner definitions with the descriptors of capability `simulation-netlist-preparation`. Section names were read from the PDK sources listed in the Purpose; exact model file paths and the remaining items are tracked as open questions.

**sky130A and sky130B.** One section variable `<proc>` in `.lib <model root>/sky130.lib.spice <proc>`, where the model root is `libs.tech/combined`:

| Kind | Section words |
|---|---|
| process (device) | `tt`, `ss`, `ff`, `sf`, `fs` |
| process (R/C only, typical devices) | `ll`, `hh`, `hl`, `lh` |
| process (device and R/C) | `ss_ll`, `ss_hl`, `ss_lh`, `ss_hh`, `sf_ll`, `sf_hl`, `sf_hh`, `fs_ll`, `fs_hl`, `fs_lh`, `fs_hh`, `ff_ll`, `ff_hl`, `ff_lh`, `ff_hh` |
| mismatch | each process word above with suffix `_mm` (for example `tt_mm`, `ss_hh_mm`) |
| statistical | `mc` |

There is no `sf_lh` and no `sf_lh_mm` section. Default corner set `pvt`: corners `tt ss ff sf fs`, temperatures `-40 27 125`, supply factors `0.9 1 1.1`.

**gf180mcuC and gf180mcuD.** Model lines `.include design.ngspice` and `.lib sm141064.ngspice <mos>`, plus the optional variables `<res>`, `<cap>`, `<bjt>`, `<moscap>`, `<dio>` when the testbench uses those devices:

| Variable | Process words | Statistical word |
|---|---|---|
| `<mos>` | `typical`, `ff`, `ss`, `fs`, `sf` | `statistical` |
| `<res>` | `res_typical`, `res_ff`, `res_ss` | `res_statistical` |
| `<cap>` | `mimcap_typical`, `mimcap_ff`, `mimcap_ss` | `mimcap_statistical` |
| `<bjt>` | `bjt_typical`, `bjt_ff`, `bjt_ss` | `bjt_statistical` |
| `<moscap>` | `moscap_typical`, `moscap_ff`, `moscap_ss` | — |
| `<dio>` | `diode_typical`, `diode_ff`, `diode_ss` | — |

Corner `typical` maps every optional variable to its `_typical` word; `ff` and `ss` map them to `_ff` and `_ss`; `fs` and `sf` map them to `_typical`. Default corner set `pvt`: corners `typical ff ss fs sf`, temperatures `-40 25 125`, supply factors `0.9 1 1.1`.

**IHP sg13g2.** Independent section variables, each in its own library file:

| Variable | File | Process words | Mismatch words | Statistical words |
|---|---|---|---|---|
| `<mos>` | `cornerMOSlv.lib` | `mos_tt mos_ss mos_ff mos_sf mos_fs` | `mos_<c>_mismatch` | `mos_tt_stat` |
| `<moshv>` | `cornerMOShv.lib` | `mos_tt mos_ss mos_ff mos_sf mos_fs` | `mos_<c>_mismatch` | `mos_tt_stat` |
| `<res>` | `cornerRES.lib` | `res_typ res_bcs res_wcs` | `res_<c>_mismatch` | `res_stat` |
| `<cap>` | `cornerCAP.lib` | `cap_typ cap_bcs cap_wcs` | `cap_<c>_mismatch` | `cap_typ_stat` |
| `<hbt>` | `cornerHBT.lib` | `hbt_typ hbt_bcs hbt_wcs` | `hbt_<c>_mismatch` | `hbt_typ_stat` |
| `<dio>` | `cornerDIO.lib` | `dio_tt dio_ss dio_ff` | — | `dio_tt_stat` |
| `<moscap>` | `cornerMOSCAP.lib` | `moscap_tt` | `moscap_tt_mismatch` | `moscap_tt_stat` |

Process corners `tt ss ff sf fs` set `<mos>` and `<moshv>` to `mos_<c>`, and the other variables to their typical word (`res_typ`, `cap_typ`, `hbt_typ`, `dio_tt`, `moscap_tt`). Default corner set `pvt`: corners `tt ss ff sf fs`, temperatures `-40 27 125`, supply factors `0.9 1 1.1`.

A model line for a variable SHALL be emitted only if the variable's file is used by the deck: the descriptor marks `<mos>` as always used, and the others as used when the DUT netlist instantiates a subcircuit or model defined in that file.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-2, Q-SIMADV-3, Q-SIMADV-4, Q-SIMADV-5.)

#### Scenario: sky130 R/C corner
- **GIVEN** sky130A with the bundled descriptor and the corner `ss_hh` at 125 °C, supply factor 0.9, supply stimulus 1.8 V
- **WHEN** the deck is composed
- **THEN** it SHALL contain `.lib "<store>/sky130A/libs.tech/combined/sky130.lib.spice" ss_hh` and `.temp 125`, and the corner SHALL be named `ss_hh_125_1v62`

#### Scenario: sg13g2 slow corner
- **GIVEN** an sg13g2 testbench whose DUT uses `sg13_lv_nmos`, `sg13_lv_pmos` and `rppd`
- **WHEN** the process corner `ss` is selected
- **THEN** the deck SHALL contain `.lib cornerMOSlv.lib mos_ss` and `.lib cornerRES.lib res_typ` (with resolved paths), and SHALL NOT contain `cornerHBT.lib`

### Requirement: User-defined corners and corner sets
A testbench SHALL be able to define its own corners and corner sets in addition to the PDK's. A user corner SHALL have the columns of Requirement "Corner definitions in the PDK descriptor", and MAY also fix a temperature, a supply factor and extra `.param` assignments. A user corner set MAY list explicit corner tuples (corner, temperature, supply factor) instead of a cartesian product. User corners SHALL shadow PDK corners of the same name, with the warning `Testbench corner <name> overrides the PDK corner`. User definitions SHALL be stored in `testbench.json` (capability `simulation-command`, Requirement "Testbench persistence").

The corner name of each tuple SHALL follow capability `simulation-netlist-preparation`, Requirement "Corners" (`<section>_<temp>_<supply>`), where `<section>` is the corner name. Two tuples of one corner set that produce the same name SHALL be rejected with `Duplicate corner <name> in corner set <set>`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Explicit tuple list
- **GIVEN** a user corner set `signoff` with tuples (ss, 125, 0.9), (ff, −40, 1.1) and (tt, 27, 1.0), for a supply stimulus of 1.8 V
- **WHEN** it is expanded
- **THEN** exactly 3 corners SHALL be produced, named `ss_125_1v62`, `ff_m40_1v98` and `tt_27_1v8`

### Requirement: Corner dimension
A run set (capability `simulation-sweeps`, Requirement "Run sets") SHALL accept one corner dimension whose levels are the tuples of one corner set, in the set's order. For a cartesian corner set, the order SHALL be corner outermost, then temperature, then supply factor, each in the order listed. The dimension's name is `corner` and each level's value is the corner name. The corner dimension MAY be combined with sweep dimensions and with the Monte Carlo dimension (capability `simulation-monte-carlo`): "Monte Carlo at every corner" is a run set with the corner dimension outside the Monte Carlo dimension.

`sim run -all` (capability `simulation-command`) SHALL be equivalent to starting a run set with only the testbench's corner dimension.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Corner sweep order
- **GIVEN** the sky130A corner set `pvt`
- **WHEN** it becomes a run-set dimension
- **THEN** it SHALL have 45 levels, level 0 SHALL be `tt_m40_1v62` and level 1 SHALL be `tt_m40_1v8`

### Requirement: Pass and fail per corner
For each corner level, Hefesto SHALL compute a corner verdict from the specifications of capability `simulation-measurements-and-statistics`:
- `pass`: the point completed and every applicable specification passed
- `fail`: the point completed and at least one applicable specification failed or had no value
- `error`: the point `failed` or was `cancelled`, so no specification could be checked

A specification SHALL apply to a corner unless its `corners` filter excludes it. The filter is a list of glob patterns (`*` and `?`) matched against the corner name; an empty filter means every corner. A specification MAY carry per-corner limit overrides, keyed by glob pattern; the first matching override in list order replaces the limits for that corner.

When a run set has more than one point per corner (sweep or Monte Carlo dimensions inside the corner), the corner verdict SHALL be `pass` only if every point of that corner passes; the matrix then shows the worst value of each measurement over the corner's points (Requirement "Corner matrix").

The run set verdict SHALL be `pass` when every corner passes, `fail` otherwise.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Corner-specific limit
- **GIVEN** a specification `tpd ≤ 120p` with an override `ss_* → tpd ≤ 180p`
- **WHEN** corner `ss_125_1v62` has `tpd` = 150 ps and corner `tt_27_1v8` has `tpd` = 130 ps
- **THEN** `ss_125_1v62` SHALL be `pass` and `tt_27_1v8` SHALL be `fail`

### Requirement: Corner matrix
Hefesto SHALL show the results of a run set with a corner dimension as a corner matrix:
- one row per corner level, in level order
- one column per measurement, with specification columns first in definition order
- each cell holding the value with up to 4 significant digits and an SI prefix, as capability `simulation-ui-and-crossprobe` formats axis labels

Each cell SHALL be marked `pass`, `fail`, `marginal` or `no value`. `marginal` is a passing value whose normalised margin (capability `simulation-measurements-and-statistics`, Requirement "Specifications") is below the testbench setting `marginalThreshold`, default 0.1. Marks SHALL NOT rely on colour alone: each mark also has a distinct symbol. A summary row SHALL show, per column, the worst value and the corner where it occurs. A verdict column SHALL show the corner verdict. Rows SHALL be sortable by any column and filterable by verdict.

Selecting a cell SHALL open that corner's waveforms in the viewer, or offer "re-run point" if the waveforms were not kept (capability `simulation-sweeps`, Requirement "Run set storage"). The matrix SHALL update while the run set is running, with not-yet-run rows marked `pending`. The matrix SHALL be exportable as CSV in the format of capability `simulation-measurements-and-statistics`, Requirement "Result export".

The matrix SHALL work on every platform, including iPadOS with touch input (capability `hefesto-input-and-touch`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Worst corner in the summary
- **GIVEN** a completed 5-corner run set with `tpd` values 80p (tt), 131p (ss), 55p (ff), 97p (sf) and 102p (fs)
- **WHEN** the matrix is shown
- **THEN** the summary row SHALL show `131p` and the corner `ss` (with its temperature and supply suffix) for the `tpd` column

### Requirement: Run all and re-run failed corners
Hefesto SHALL provide "run all", which starts a run set over the testbench's selected corner set (combined with its sweep and Monte Carlo dimensions, if any), and "re-run failed", which starts a new run set containing only the corner levels whose verdict was `fail` or `error` in a chosen earlier run set, with the same base deck unless the user chooses "regenerate". A re-run SHALL be stored as its own run set and SHALL record the id of the run set it re-runs as `rerunOf` in `runset.json`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Re-run failed after a fix
- **GIVEN** a run set in which corners `ss_125_1v62` and `sf_125_1v62` failed
- **WHEN** the user edits the schematic and chooses "re-run failed" with "regenerate"
- **THEN** a new run set of 2 points SHALL run with a freshly netlisted base deck, and its `rerunOf` SHALL name the earlier run set

### Requirement: Corner command options
The `sim` command (capability `simulation-command`) SHALL accept the option `corners` with these forms:

| Syntax | Effect / result |
|---|---|
| `corners list [-pdk]` | Return the corner names of the testbench, or of the PDK with `-pdk`, one per line: `<name> <kind> <section words…>` |
| `corners sets` | Return the corner set names; the selected one is marked with `*` |
| `corners select <set>` | Select the corner set used by "run all" |
| `corners define <name> <var>=<word>… [-temp <t>] [-supply <k>] [-param <p>=<v>]…` | Define or replace a user corner |
| `corners set <set> <corner…> -temp <list> -supply <list>` \| `corners set <set> -tuples <c>:<t>:<k>…` | Define a user corner set |
| `corners run [-wait]` | Run all; return the run set id |
| `corners rerun <run set id> [-regenerate] [-wait]` | Re-run failed corners; return the new run set id |
| `corners matrix <run set id>` | Return the matrix rows as lists: corner, verdict, then each column value |

Every form SHALL follow the rules of capability `simulation-command` for matching, numbers, return values and errors. An unknown section word SHALL fail with `sim corners: <var> has no section <word>`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-1.)

#### Scenario: Define a corner from a script
- **WHEN** `sim corners define slowcold mos=mos_ss -temp -40` is executed for an sg13g2 testbench
- **THEN** `sim corners list` SHALL include `slowcold process mos_ss`

#### Scenario: Unknown section
- **WHEN** `sim corners define x proc=zz` is executed for a sky130A testbench
- **THEN** it SHALL fail with `sim corners: proc has no section zz`
