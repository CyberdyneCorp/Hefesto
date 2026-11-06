# Digital Flow Results

## Purpose
Defines how Hefesto presents the results of a LibreLane run: the run summary, the metrics, the logs, the sign-off reports (DRC, LVS, antenna) mapped onto layout locations, the final layout, and comparisons between runs. Results are read from the run directory described by capability `digital-flow-run`, Requirement "Run directory layout". Browsing results only reads files, so it is available read-only on every platform.

LibreLane reference: `librelane/librelane` @ `8494d4fea0ddd9bee12ea5e42a38b59438c2b128` (version 3.0.17). Report formats produced by OpenROAD, KLayout and Netgen are described here as LibreLane collects them; the Netgen LVS report itself is specified by capability `lvs-reports`.

## Requirements

### Requirement: Run summary
For each run, Hefesto SHALL show a summary with:
- tag, display label, status (capability `digital-flow-run`, Requirement "Run status"), start time and duration;
- design name, flow name, LibreLane version, PDK variant and standard-cell library, read from `hefesto-run.json` and `resolved.json`;
- a sign-off table of the gating metrics of Requirement "Metric evaluation", each marked pass, fail or not available;
- the number of lines in `warning.log` and `error.log`, each opening the log viewer filtered to that file.

A run directory without `hefesto-run.json` (for example a run started outside Hefesto) SHALL still be summarized from the files that exist, with missing fields shown as unknown.

Compatibility: native

(LibreLane ref: librelane/flows/flow.py:660-680, librelane/state/state.py:193-218)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Run started from the command line
- **GIVEN** a run directory created by running `librelane config.yaml` outside Hefesto, with `resolved.json`, step directories and `final/metrics.json`
- **WHEN** the user opens the design's run list
- **THEN** the run SHALL be listed and summarized, and its LibreLane version SHALL be shown as unknown

### Requirement: Metrics model
Hefesto SHALL read metrics from `final/metrics.json`, a JSON object whose keys are metric names and whose values are numbers, strings, booleans or null. For a run without `final/`, metrics SHALL be read from the `metrics` object of the newest step `state_out.json`.

Metric names SHALL be interpreted as `<category>__<name>[__<modifier>]*`, split at each double underscore. A modifier of the form `<key>:<value>` (for example `corner:nom_tt_025C_1v80`) SHALL be treated as a dimension of the metric. The metrics browser SHALL:
- group metrics by category and show dimension-qualified metrics under their base name, with one column per dimension value;
- show numbers with the precision found in the file, without rounding;
- show names that do not follow the pattern under the category `other`;
- show a per-step history of any metric, read from each step's `state_out.json` in ordinal order.

Compatibility: native

(LibreLane ref: librelane/common/metrics/metric.py:85-100, librelane/state/state.py:193-218, librelane/steps/openroad.py:850-860)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Corner-qualified metric
- **GIVEN** `metrics.json` containing `"timing__setup__ws": 2.1`, `"timing__setup__ws__corner:nom_tt_025C_1v80": 3.4` and `"timing__setup__ws__corner:max_ss_100C_1v60": 2.1`
- **WHEN** the metrics browser shows category `timing`
- **THEN** `setup__ws` SHALL be shown with value 2.1 and with columns `nom_tt_025C_1v80` = 3.4 and `max_ss_100C_1v60` = 2.1

### Requirement: Metric evaluation
Hefesto SHALL evaluate these gating metrics, using the aggregate value without dimensions:

| Metric | Fails when |
|---|---|
| `magic__drc_error__count` | > 0 |
| `klayout__drc_error__count` | > 0 |
| `route__drc_errors` | > 0 |
| `magic__illegal_overlap__count` | > 0 |
| `design__lvs_error__count` | > 0 |
| `route__antenna_violation__count` | > 0 |
| `design__xor_difference__count` | > 0 |
| `design__power_grid_violation__count` | > 0 |
| `design__critical_disconnected_pin__count` | > 0 |
| `timing__setup_vio__count` | > 0 |
| `timing__hold_vio__count` | > 0 |
| `design__max_slew_violation__count` | > 0 |
| `design__max_cap_violation__count` | > 0 |
| `timing__setup__ws` | < 0 |
| `timing__hold__ws` | < 0 |

A metric absent from the file SHALL be "not available", not pass. The user MAY add, remove or change gates per design; the gate set SHALL be stored with the design, not in the run directory.

For comparisons, Hefesto SHALL know the preferred direction of these metrics: lower is better for every count above and for `design__instance__area`, `design__instance__count`, `design__die__area`, `route__wirelength__max`, `ir__drop__worst` and `ir__drop__avg`; higher is better for `timing__setup__ws` and `timing__hold__ws`. Other metrics have no direction.

Compatibility: native

(LibreLane ref: librelane/steps/checker.py:142-680, librelane/common/metrics/library.py:14-60)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Hold violation fails the gate
- **GIVEN** `metrics.json` with `"timing__hold__ws": -0.05` and `"timing__hold_vio__count": 3`
- **WHEN** the summary is shown
- **THEN** both `timing__hold__ws` and `timing__hold_vio__count` SHALL be marked fail

#### Scenario: Missing metric
- **GIVEN** a run that skipped KLayout DRC, so `klayout__drc_error__count` is absent
- **WHEN** the summary is shown
- **THEN** that gate SHALL be marked "not available"

### Requirement: Log viewer
Hefesto SHALL show `flow.log`, `warning.log`, `error.log` and each step's `<slug>.log` in a log viewer that:
- follows the file while the run is active;
- filters by severity and by text;
- lists steps in ordinal order with their status (started, finished, skipped) and runtime from `runtime.txt`;
- reveals any step directory, report or log file in the system file manager.

Files larger than 64 MiB SHALL be opened in a paged mode that does not load the whole file into memory.

Compatibility: native

(LibreLane ref: librelane/flows/flow.py:660-670, librelane/steps/step.py:1195-1235)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Large log
- **GIVEN** a `flow.log` of 300 MiB
- **WHEN** the user opens it
- **THEN** the viewer SHALL open it in paged mode, and memory use SHALL NOT grow by the file size

### Requirement: Opening the final layout
Hefesto SHALL open a run's final layout from `final/gds/`, using the run's PDK technology from the PDK store. When `final/gds/` holds one file, that file SHALL be opened; when it holds several, the one whose name starts with the design name SHALL be opened. The layout SHALL be opened read-only, as by capability `gds-read` with read-only vendor views, and SHALL be closed without saving. The user MAY instead open `final/def/` through capability `def-read` with the PDK's LEF files and net annotation, or `final/mag/` as Magic cells.

The user SHALL be able to import the run's final views as a block (capability `digital-flow-block-import`) from the same place.

Compatibility: native

(LibreLane ref: librelane/flows/sequential.py:400-411, librelane/steps/magic.py:276-385)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Open final layout
- **GIVEN** a succeeded run of `spm` on sky130A whose `final/gds/` holds `spm.gds`
- **WHEN** the user chooses "open final layout"
- **THEN** `spm.gds` SHALL be read in read-only mode with sky130A's technology, and the top cell `spm` SHALL be shown

### Requirement: Magic DRC report
Hefesto SHALL parse the Magic DRC report `drc.magic.rpt` found in the `reports/` directory of the `Magic.DRC` step. Its format is:

```ebnf
report    = cell_line , split , { rule_block } , info_lines ;
cell_line = text , NL ;                         (* the checked cell name *)
split     = 40 * "-" , NL ;
rule_block= rule_line , split , { box_line } , split ;
rule_line = text , NL ;                         (* DRC why-text, optionally ending "(<layer>.<rule>)" *)
box_line  = " " , um , " " , um , " " , um , " " , um , NL ;   (* llx lly urx ury *)
um        = number , "um" ;                     (* printf "%.3f" followed by "um" *)
info_lines= "[INFO] COUNT: " , integer , NL , "[INFO] Should be divided by 3 or 4" , NL , NL ;
```

Each box line SHALL become one violation of its rule, located at the given rectangle in microns, in the coordinate system of the checked top cell. Lines containing `[INFO]` and empty lines SHALL be ignored wherever they occur. When a rule line ends with a parenthesized `<layer>.<rule>` group, the group SHALL be shown as the rule id. Identical boxes within one rule SHALL be shown once with a multiplicity count (see Q-DIGITAL-4).

Compatibility: native

(LibreLane ref: librelane/scripts/magic/drc.tcl:55-95, librelane/common/drc.py:136-218, librelane/steps/magic.py:530-560)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Parse one rule block
- **GIVEN** a report whose lines are `spm`, 40 dashes, `Metal1 spacing < 0.14um (met1.2)`, 40 dashes, ` 10.000um 20.000um 10.140um 21.000um`, 40 dashes, `[INFO] COUNT: 1`
- **WHEN** it is parsed
- **THEN** there SHALL be one violation in cell `spm`, rule id `met1.2`, description `Metal1 spacing < 0.14um (met1.2)`, at (10.000, 20.000)–(10.140, 21.000) µm

### Requirement: OpenROAD routing DRC report
Hefesto SHALL parse the detailed-routing DRC report `<design>.drc` written by the `OpenROAD.DetailedRouting` step. After trimming each line and skipping empty lines, the report is a sequence of three-line records:

```ebnf
record    = "violation type: " , text , NL ,
            "srcs: " , source , [ " " , source ] , NL ,
            "bbox = (" , num , ", " , num , ") - (" , num , ", " , num , ") on Layer " , layer , NL ;
```

Each record SHALL become one violation of category `<violation type>` on layer `<layer>`, located at the rectangle in microns, with the sources (nets, instances or obstructions) shown as its description. A record that does not match SHALL stop parsing with a diagnostic giving the line number, and the violations parsed before it SHALL be kept.

Compatibility: native

(LibreLane ref: librelane/common/drc.py:67-133, librelane/scripts/openroad/drt.tcl:17-18)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Short between two nets (verify against OpenROAD)
- **GIVEN** the record `violation type: Short`, `srcs: net12 net40`, `bbox = (5.1, 7.2) - (5.3, 7.4) on Layer met2`
- **WHEN** it is parsed
- **THEN** there SHALL be one `Short` violation on `met2` at (5.1, 7.2)–(5.3, 7.4) µm described as `net12 net40`

### Requirement: KLayout report databases
Hefesto SHALL read KLayout report databases (`.lyrdb` or `.xml`) found in step `reports/` directories, including `drc.klayout.lyrdb` from `KLayout.DRC` and `drc.magic.lyrdb` from `Magic.DRC`, and the XOR result of `KLayout.XOR`. It SHALL read the `categories` (name and description) and the `items`, taking for each item its `cell`, its category (with surrounding single quotes removed), and the geometry of each `value` whose text starts with `box:`, `polygon:`, `edge:` or `edge-pair:`. Values starting with `text:` SHALL be shown as item descriptions. Coordinates are in microns. Values of other kinds SHALL be listed without a location.

Compatibility: native

(LibreLane ref: librelane/common/drc.py:280-340, librelane/steps/klayout.py:495-560, librelane/scripts/klayout/xml_drc_report_to_json.py)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Polygon item
- **GIVEN** an item with category `'met1.2'` and value `polygon: (10,20;10.14,20;10.14,21;10,21)`
- **WHEN** the database is read
- **THEN** one violation of category `met1.2` SHALL be located at the polygon (10, 20), (10.14, 20), (10.14, 21), (10, 21) µm

### Requirement: Antenna report
Hefesto SHALL parse the antenna report `antenna.rpt` of each `OpenROAD.CheckAntennas` step. Reading line by line, it SHALL track the most recent values of the fields `Net:`, `Pin:`, `Layer:`, `Required ratio:` and `Partial area ratio:` (each followed by optional spaces and one whitespace-free token), and SHALL record one violation, with the current values, for every line that contains `VIOLATED`. Violations SHALL be listed in decreasing order of partial ratio ÷ required ratio, and the ratio, partial, required, net, pin and layer SHALL be shown with two decimals for numbers.

Each violation SHALL be located as described in Requirement "Mapping results to layout".

Compatibility: native

(LibreLane ref: librelane/steps/openroad.py:1700-1835)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Antenna violations sorted
- **GIVEN** two violations, net `n1` with partial 300 and required 100, and net `n2` with partial 500 and required 400
- **WHEN** the report is shown
- **THEN** `n1` (ratio 3.00) SHALL be listed before `n2` (ratio 1.25)

### Requirement: LVS report
Hefesto SHALL read the Netgen LVS results of the `Netgen.LVS` step: the JSON file `lvs.netgen.json` and the text report in the same `reports/` directory. The JSON file SHALL be interpreted as specified by capability `lvs-reports`, Requirement "JSON file", and the result SHALL be shown in the LVS result browser of capability `lvs-command`, Requirement "Result browser", with the run's final layout as the layout side. The metrics `design__lvs_error__count`, `design__lvs_unmatched_net__count`, `design__lvs_unmatched_device__count`, `design__lvs_unmatched_pin__count` and `design__lvs_property_fail__count` SHALL be shown alongside.

Compatibility: native

(LibreLane ref: librelane/steps/netgen.py:185-270)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: LVS mismatch in a run
- **GIVEN** a run whose `lvs.netgen.json` reports one unmatched net `_123_`
- **WHEN** the user opens the LVS result
- **THEN** the result browser SHALL list net `_123_` as unmatched, and `design__lvs_error__count` SHALL be shown next to it

### Requirement: Mapping results to layout
Every violation with a location SHALL be mappable onto the opened final layout (Requirement "Opening the final layout"):
- a rectangle or polygon in microns SHALL be converted to internal units with the technology's output scale and shown as a feedback area (capability `ui-feedback-and-highlights`) whose text is the rule or category and description;
- selecting a violation in the list SHALL center and zoom the view on it, and the user SHALL be able to step to the next and previous violation;
- violations SHALL be grouped by category, with counts, and the user SHALL be able to hide or show each category;
- a violation located by net name only (antenna, LVS) SHALL be highlighted by finding the named net in the run's final DEF read with net annotation (capability `def-read`); a pin `<instance>/<pin>` SHALL be highlighted as the instance's pin geometry. When the DEF is not available, such a violation SHALL be listed without a location.

Rounding a micron coordinate to internal units SHALL use round-half-away-from-zero, and a coordinate that is not on the internal grid SHALL be reported once per report as a warning.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Zoom to a DRC violation
- **GIVEN** a technology whose output style scale is 0.005 µm per internal unit, and a violation at (10.000, 20.000)–(10.140, 21.000) µm
- **WHEN** the user selects it
- **THEN** a feedback area SHALL cover (2000, 4000)–(2028, 4200) internal units, and the view SHALL center on it

#### Scenario: Antenna net without DEF
- **GIVEN** a run whose `final/def/` is missing
- **WHEN** the user selects antenna violation on net `n1`
- **THEN** the violation SHALL be shown in the list with no location, and a message SHALL state that the final DEF is needed to locate nets

### Requirement: Comparing runs
Hefesto SHALL compare two runs of the same design (or of different designs, with a warning). The comparison SHALL show:
- **metrics**: every metric present in either run, with both values, the absolute difference and the relative difference (difference ÷ |baseline| as a percentage, or "n/a" when the baseline is 0 or not a number), marked better or worse by the direction of Requirement "Metric evaluation";
- **configuration**: the top-level keys of the two `resolved.json` files that were added, removed or changed, with both values;
- **environment**: differences in LibreLane version, PDK commit and standard-cell library from the two `hefesto-run.json` files;
- **layout**: on request, a structural or visual diff of the two final layouts through capability `hefesto-collaboration-and-diff`.

The user SHALL be able to choose which run is the baseline. Comparing more than two runs SHALL show the metrics table with one column per run.

Compatibility: native

(LibreLane ref: librelane/common/metrics/__main__.py:79-186)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Metric change
- **GIVEN** baseline `design__instance__area` = 1200 and the other run = 1140
- **WHEN** the runs are compared
- **THEN** the row SHALL show −60 and −5.00 %, marked better

#### Scenario: Configuration change
- **GIVEN** baseline `resolved.json` with `"FP_CORE_UTIL": 45` and the other with `"FP_CORE_UTIL": 40`
- **WHEN** the runs are compared
- **THEN** the configuration section SHALL list `FP_CORE_UTIL` as changed from 45 to 40

### Requirement: Untrusted results and platform availability
All run files SHALL be read as untrusted input (capability `hefesto-security`). A malformed report SHALL produce a diagnostic and SHALL NOT prevent the rest of the run from being shown. Browsing results SHALL never start a process. On iPadOS, results of runs stored in a project SHALL be browsable read-only, including mapping onto the final layout; actions that run or delete runs SHALL be unavailable (capability `hefesto-ipad-feature-scope`). See Q-DIGITAL-1.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Results on iPad
- **GIVEN** a project with a run directory, opened on iPadOS
- **WHEN** the user opens the run's DRC report
- **THEN** the violations SHALL be listed and mapped onto the final layout, and the delete and rerun actions SHALL be unavailable
