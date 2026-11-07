# Reliability Visualization

## Purpose
Defines how Hefesto presents IR-drop and electromigration results (decision P-21). It covers:
- 2D heat maps over the layout
- 3D overlays through capability `hefesto-3d-visualization`
- time scrubbing for dynamic results
- a violation browser
- cross-probing between reliability results, the layout, waveforms and schematics

Values shown SHALL come from the stored run files of capabilities `reliability-ir-drop` and `reliability-em-check`, without recomputation.

## Requirements

### Requirement: Overlay datasets
For each stored reliability run, Hefesto SHALL provide these overlay datasets:

| Dataset | Geometry | Value | Unit |
|---|---|---|---|
| `ir.drop` | each network node's tile region of the supply net | the node's drop: static value, or for dynamic runs the maximum over the window or the value at the time cursor | mV |
| `ir.current` | each resistor's tile rectangle | \|branch current\| | mA |
| `em.ratio` | each item's rectangle (wire tile, or contact rectangle) | the item's worst ratio, or the ratio of one chosen metric | dimensionless |
| `em.density` | each wire item's rectangle | value / width for a chosen metric | mA/µm |
| `em.cut` | each cut item's rectangle | per-cut current for a chosen metric | mA |

Each dataset SHALL carry its run id, its unit, its value range (min and max over the shown items), and the qualifier `estimated` or `approximate` when the run has one. These datasets SHALL be offered to plugins as read-only overlay data (capability `plugin-api`, Requirement "Overlays").

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/product.md#P-4)

#### Scenario: Values come from the run
- **GIVEN** an IR run whose `irdrop-nodes.csv` lists node `VDD.n12` with drop 2.300000e-02
- **WHEN** the `ir.drop` overlay is shown and the user hovers over that node's tile
- **THEN** the readout SHALL show 23.0 mV

### Requirement: 2D heat map
The layout view SHALL draw a selected dataset as a filled color overlay over the geometry it refers to, above the layout layers and below selection highlights. The overlay SHALL use these color maps:
- **`ir.drop`, `ir.current`, `em.density`, `em.cut`:** a perceptually uniform sequential map that color-blind viewers can distinguish (viridis-like), linear over [0, max] by default, with an optional user-set range and an optional log scale.
- **`em.ratio`:** a fixed threshold map. Ratio ≤ r_warn is drawn in a neutral color; r_warn < ratio ≤ 1 in the `warning` color token; ratio > 1 in the `danger` color token (capability `hefesto-design-system`). Within each band, intensity rises with the ratio. Unchecked items are drawn hatched.

A legend SHALL show the unit, the range, the thresholds (budget for IR, r_warn and 1.0 for EM), and the run id. The opacity SHALL be adjustable from 0 to 100 %. The overlay SHALL hide items smaller than one pixel at the current zoom, and SHALL instead draw a marker at the worst item in each 8×8-pixel screen bin, so that violations stay visible when zoomed out. The overlay SHALL NOT change the layout, and SHALL NOT be saved in cells.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/product.md#P-3)

#### Scenario: Violation visible when zoomed out
- **GIVEN** an EM run with one failing via of 0.2 µm × 0.2 µm in a 2 mm × 2 mm chip
- **WHEN** the whole chip is shown with `em.ratio` on
- **THEN** a marker in the `danger` color SHALL be drawn at the via's screen bin

#### Scenario: Overlay not saved
- **WHEN** a cell is saved while an IR overlay is shown
- **THEN** the saved file SHALL be byte-identical to one saved with the overlay off

### Requirement: 3D overlays
The 3D view SHALL show the same datasets as the "Parasitic and field overlays" of capability `hefesto-3d-visualization`:
- wire and node values color the extruded solids of their tiles
- cut values color the via solids

The legend, color map and thresholds SHALL match the 2D heat map. Picking a colored solid SHALL show the same readout as in 2D, and SHALL select the matching item in the violation browser.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/product.md#P-4)

#### Scenario: Via colored in 3D
- **GIVEN** the failing via1 of an EM run
- **WHEN** the 3D view shows `em.ratio`
- **THEN** the via1 solid SHALL be drawn in the `danger` color, and picking it SHALL show ratio 1.25 and rule `JMAXVIA1`

### Requirement: Time scrubbing
For a dynamic IR run, and for an EM run from a transient job, the overlays SHALL support a time cursor:
- **IR datasets.** Moving the cursor SHALL show the node drops and branch currents at that time, interpolated linearly between stored samples.
- **EM datasets.** These are window statistics and stay fixed, except that `ir.current` follows the cursor.

The cursor SHALL be shared with the waveform viewer of capability `simulation-ui-and-crossprobe` when that viewer shows the same job. A "worst time" control SHALL move the cursor to the time of the maximum drop.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/product.md#P-7)

#### Scenario: Jump to worst time
- **GIVEN** a dynamic IR run whose maximum drop occurs at 4.2 ns
- **WHEN** the user chooses "worst time"
- **THEN** the cursor SHALL move to 4.2 ns in both the overlay and the waveform viewer

### Requirement: Violation browser
Hefesto SHALL provide a violation browser panel for the selected run. It SHALL list:
- for EM runs, the grouped violations and warnings (capability `reliability-em-check`, Requirement "Violation grouping and diagnostics")
- for IR runs, the devices over budget

The columns SHALL include:
- status
- net
- layer or device
- metric
- value
- limit or budget
- ratio
- location
- rule id

The browser SHALL:
- sort by any column, defaulting to the report order
- filter by net, layer, status and metric
- step to the next and previous violation, with keys on desktop and buttons on iPadOS
- show summary counts per status and per net

Selecting a row SHALL zoom the active layout view to the row's box, with a 10 % margin. It SHALL also highlight the row's items in 2D and 3D. The browser SHALL read only the stored run files, and SHALL show `stale` when the run is stale.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Select violation
- **WHEN** the user taps the top violation in the browser on iPad
- **THEN** the layout view SHALL zoom to its box, and its items SHALL be highlighted

### Requirement: Review notes
In the violation browser, the user SHALL be able to mark a violation `reviewed`, with a free-text comment. Notes SHALL be stored in `reliability/<run-id>/review.json` as `[{key, status, comment, author, time}]`. The key is (net, layer, rule id, box rounded to 0.01 µm), so that notes carry over to a later run of the same cell when the key matches. Review notes SHALL NOT change the run's results. A note is not a sign-off waiver: waivers belong to the checklist of capability `tapeout-signoff-checklist`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/product.md#P-15)

#### Scenario: Note carried to a new run
- **GIVEN** a reviewed violation on VDD metal1, rule JMAXM1, with box (10.00, 5.00)–(12.00, 5.40)
- **WHEN** a new EM run reports a violation with the same key
- **THEN** the browser SHALL show the earlier note, marked as from the previous run

### Requirement: Cross-probing
Hefesto SHALL provide these cross-probes:
- **Violation to waveform.** From an EM item or violation, show the item's branch-current trace in the waveform viewer for the run's job: the I = (V(a) − V(b)) / R series for `cosim`, or the stored redistributed or IR series otherwise. Mark the window, the value and the limit as horizontal lines.
- **Violation to schematic.** When the net is mapped to a schematic net through the net-name map (capability `simulation-netlist-preparation`), highlight that schematic net. For a device-level IR violation, select the schematic instance.
- **Layout to results.** Selecting a net or a device in the layout SHALL list its EM items and its IR drop in the browser, filtered to that selection.
- **Diagnostics.** Every reliability diagnostic SHALL navigate like other diagnostics (capability `hefesto-diagnostics`).

A cross-probe target that cannot be resolved SHALL show `No <target> for <item>` and SHALL change nothing.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/product.md#P-7, docs/decisions/product.md#P-12)

#### Scenario: Show current waveform
- **GIVEN** an EM violation on resistor `R17` of net VDD from a `cosim` run
- **WHEN** the user chooses "show current"
- **THEN** the waveform viewer SHALL add the trace (V(a) − V(b)) / R for R17, with lines at the avg value and the limit

#### Scenario: No schematic
- **GIVEN** a cell with no associated schematic
- **WHEN** the user chooses "show in schematic" for a violation
- **THEN** the message `No schematic net for VDD` SHALL appear, and nothing else SHALL change

### Requirement: Touch and platform parity
All reliability views SHALL be available on Linux, macOS and iPadOS:
- **iPadOS:** a tap on a colored item shows its readout, and a long press opens its cross-probe menu.
- **Desktop:** the same actions are on hover and the context menu.

Image export of the current heat map (PNG, at the view's resolution or a chosen size) SHALL go through the plot output of capability `plot-output`, and SHALL include the legend.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/product.md#P-13)

#### Scenario: Long press on iPad
- **WHEN** the user long-presses a red via in the 2D heat map on iPad
- **THEN** a menu SHALL offer "Show current", "Show in schematic", "Open in browser" and "Show in 3D"
