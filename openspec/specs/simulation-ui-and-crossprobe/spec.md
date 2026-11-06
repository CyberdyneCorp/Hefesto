# Simulation UI and Cross-Probing

## Purpose
Defines the waveform viewer and the links between simulation results and layout:
- probing layout nets and devices to show their vectors
- highlighting the layout net of a waveform
- annotating the layout with operating-point values
- providing voltage and current data to the 3D parasitic and field overlays of capability `hefesto-3d-visualization`

Results come from capability `simulation-analyses-and-results`. Name mapping uses the net-name map of capability `simulation-netlist-preparation`. Drawing goes through the rendering stack of capability `hefesto-rendering`. Input follows capability `hefesto-input-and-touch`.

## Requirements

### Requirement: Waveform viewer layout
Hefesto SHALL provide a waveform viewer with one or more vertically stacked panels that share one x axis. Each panel holds any number of traces. A trace SHALL be one vector of one plot of one job, shown in one display mode. Users SHALL be able to:
- add, remove and move traces between panels
- add and remove panels
- show several jobs in one panel, for example the same vector across corners

A trace's legend entry SHALL show `<display name>` when only one job is shown, and `<display name> [<corner>]` otherwise. Every trace whose plot scale differs in type from the panel's x axis (for example, a frequency plot in a time panel) SHALL be refused with `Cannot mix <type1> and <type2> scales in one viewer`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Corner overlay
- **GIVEN** completed jobs for corners `tt_27_1v8` and `ss_m40_1v62` of one testbench
- **WHEN** the user adds `v(out)` from both jobs to one panel
- **THEN** the panel SHALL show two traces, labeled `v(out) [tt_27_1v8]` and `v(out) [ss_m40_1v62]`

### Requirement: Axes, units and complex display
Axes SHALL be labeled with the vector type's unit: s, Hz, V or A, or no unit for other types. Tick labels SHALL use SI prefixes from f to T (f p n µ m – k M G T), with up to 4 significant digits. The x axis SHALL default to logarithmic for `ac` and `noise` plots swept `dec` or `oct`, and to linear otherwise. Either axis MAY be switched between linear and log; values ≤ 0 are not drawn on a log axis. A complex vector SHALL be shown in one of these modes (default `db`):
- `mag` = |x|
- `db` = 20·log10|x|
- `phase` = atan2(im, re) in degrees in (−180, 180], with an optional unwrap mode
- `re`
- `im`

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Tick labels
- **GIVEN** a transient panel spanning 0 to 20 ns
- **WHEN** it is drawn
- **THEN** the x ticks SHALL be labeled with the `n` prefix, for example `5n`, `10n`, `15n`, and the axis unit SHALL be `s`

### Requirement: Cursors and readouts
The viewer SHALL provide two vertical cursors, A and B. For each trace, the readout SHALL show:
- the trace value at A and at B, by linear interpolation in the displayed mode
- `ΔX = B − A`
- `ΔY` for each trace
- `1/ΔX` with unit Hz, when the x unit is s

Snapping a cursor to the nearest sample or to the next crossing of a level SHALL use the measurement definitions of capability `simulation-analyses-and-results`, Requirement "Waveform measurements".

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Period readout
- **GIVEN** cursors at 2 ns and 12 ns on a transient panel
- **WHEN** the readout is shown
- **THEN** it SHALL show ΔX = 10n s and 1/ΔX = 100M Hz

### Requirement: Waveform rendering performance
The viewer SHALL draw through the shared rendering interface (Metal on Apple platforms, Vulkan on Linux). When a trace has more samples than the panel has device pixels in its visible x range, it SHALL be drawn decimated: for each pixel column, the minimum and maximum of all samples in that column are drawn as a vertical span, and these spans are joined to the neighbouring columns. Peaks SHALL therefore never disappear at any zoom level. With 10 traces of 1 000 000 points each, pan and zoom SHALL keep the interactive frame rate of capability `hefesto-performance`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-3, docs/decisions/product.md#P-7)

#### Scenario: Glitch remains visible
- **GIVEN** a 1 000 000-point trace that is 0 everywhere except one sample of 1.8 V
- **WHEN** the whole trace is shown in an 800-pixel-wide panel
- **THEN** a vertical span reaching 1.8 V SHALL be drawn in the column holding that sample

### Requirement: Live waveforms during a run
While a job is `running`, traces of its current plot SHALL update from the streamed data points (capability `simulation-engine-embedding`, Requirement "Callback contract") at most 10 times per second. When the job reaches a final state, the traces SHALL be replaced by the stored final vectors. A live trace SHALL be marked as live, for example with a dashed legend swatch, until it is replaced.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Live then final
- **GIVEN** a running transient job with `v(out)` shown
- **WHEN** the job completes
- **THEN** the trace SHALL be redrawn from the stored vector and the live marking SHALL be removed

### Requirement: Viewer input on every platform
The viewer SHALL support:
- **Desktop:** mouse-wheel zoom around the pointer, drag to pan, drag a rectangle to zoom, and keyboard shortcuts for fit-all (`f`) and for placing cursors (`a`, `b`).
- **iPadOS:** pinch to zoom on each axis separately (horizontal pinch for x, vertical for y), two-finger drag to pan, double-tap to fit, and long-press to place the nearest cursor. With Apple Pencil hover, a readout tooltip SHALL follow the hover position.

Every viewer action that changes saved state SHALL go through the command API (capability `simulation-command`): adding or removing traces, panels and cursors, and changing axis modes.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Pinch on iPad
- **GIVEN** a panel spanning 0 to 20 ns on iPad
- **WHEN** the user pinches horizontally to 2× magnification centered at 10 ns
- **THEN** the panel SHALL span 5 ns to 15 ns, and the y range SHALL be unchanged

### Requirement: Probing a layout net
Selecting a net in the layout and choosing "probe" SHALL resolve the net to a simulation vector and add it as a trace. Selection works as in `getnode` (capability `irsim-interface`); this and the naming rule below apply on every platform, including iPadOS, where the IRSIM interface itself is unavailable (capability `hefesto-ipad-feature-scope`). Resolution SHALL work as follows:
1. Compute the net's hierarchical name relative to the root cell of the layout window, as specified by capability `irsim-interface`, Requirement "Hierarchical node names", including the handling of global names.
2. If the root cell is not the DUT cell of the selected job, fail with `Layout window shows <root>, but job simulated <dut>`.
3. Look up the name in the job's net-name map, getting `simName`.
4. Find the vector whose name equals `simName` case-insensitively in the job's plot. If none, the probe fails with `Net <name> was not saved in this run`. Hefesto SHALL then offer to add `simName` to the testbench's saved vectors and run again.
5. If the job has no net-name map (raw deck or imported), compare the layout name, with `/` replaced by `.`, to vector names case-insensitively.

Selecting paint that covers several nets SHALL probe each net.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Probe an internal net
- **GIVEN** a completed job of DUT `buf` in mode `all`, and a layout window rooted at `buf`
- **WHEN** the user selects paint of net `mid` inside instance `inv_0` and probes it
- **THEN** a trace of vector `xdut.xinv_0.mid` SHALL be added

#### Scenario: Net not saved
- **GIVEN** a job in mode `default` that did not save `xdut.xinv_0.mid`
- **WHEN** the same net is probed
- **THEN** `Net inv_0/mid was not saved in this run` SHALL be shown, with the option to save it and run again

### Requirement: Probing device currents
Selecting a device in the layout and choosing "probe current" SHALL list the device's current vectors available in the job. ngspice names these `@<device path>[<param>]`, for example `@m.xdut.xm1[id]`, and they exist only when the job used mode `all+currents` or listed them explicitly. The device path SHALL be derived from the device's netlist instance name, through the net-name map's instance entries. When the PDK wraps devices in subcircuits, the path ends at the primitive inside the wrapper subcircuit. If no current vector exists, the probe SHALL fail with `No current saved for device <name>; enable "save currents" and run again`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-6.)

#### Scenario: Drain current of a top-level MOSFET (verify against ngspice)
- **GIVEN** a job of DUT `inv` with mode `all+currents`, in which device `M1` is a primitive MOSFET
- **WHEN** the user probes M1's current
- **THEN** vector `@m.xdut.m1[id]` SHALL be offered

### Requirement: Highlighting the layout net of a trace
Selecting a trace whose vector maps back to a net through the net-name map SHALL highlight that net's connected geometry in every layout window rooted at the DUT cell. The highlight uses the net-highlight mechanism of capability `hefesto-3d-visualization` in 3D views, and the selection highlight in 2D views. A trace with no net (a current, or a vector from a raw deck) SHALL highlight nothing and SHALL show `No layout net for <vector>`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Trace to layout
- **GIVEN** a trace of `out` from a job of DUT `inv`
- **WHEN** the user taps the trace on iPad
- **THEN** net `Y` of `inv`, which is bound to `out`, SHALL be highlighted in the layout window

### Requirement: Operating-point annotation
After an `op` analysis, or at cursor A of a transient or DC plot, Hefesto SHALL be able to annotate the layout with each mapped net's voltage. Each value is drawn as a temporary label on the net in the form `<value><prefix>V` with 3 significant digits, for example `1.20V` or `650mV`. Annotations SHALL NOT be stored in the cell, SHALL NOT be written to any file, and SHALL NOT affect extraction or DRC. They SHALL be removed by clearing them or when the job is closed. This replaces the `RSIM=` temporary labels of capability `irsim-interface` for analog results.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Annotate divider
- **GIVEN** an op result with `v(out)` = 1.2 V, and `out` mapped to layout net `Y`
- **WHEN** operating-point annotation is turned on
- **THEN** a temporary label `1.20V` SHALL appear on net `Y`, and saving the cell SHALL write no such label

### Requirement: Data feed to 3D overlays
For a job of a layout DUT, Hefesto SHALL provide capability `hefesto-3d-visualization` with overlay datasets. Each dataset is a time series indexed by the plot scale, or a single point for `op`. Two kinds SHALL be provided:
- **Net voltage.** For every mapped net with a saved vector: the net's voltage, in V.
- **Resistor-edge current.** Available in parasitic mode `rc`. For every `extresist` resistor between network nodes `n1` and `n2` with resistance `R` Ω, both of whose node vectors were saved: `I = (V(n1) − V(n2)) / R` in A, positive from `n1` to `n2`. Using the segment's width `w` in µm, from the extresist geometry of capability `extresist-output`, and its layer thickness `t` in µm, from the technology, the dataset SHALL also give:
  - the linear current density `Jw = |I| / w` in mA/µm
  - the volumetric current density `J = |I| / (w · t)` in mA/µm²

A segment with unknown `w` or `t` SHALL report `I` only. Values SHALL come directly from the stored vectors and the deck's resistor values, and SHALL NOT be re-simulated. Scrubbing a time cursor in the viewer SHALL update the 3D overlay to that time. The value is interpolated linearly between samples.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-4, docs/decisions/product.md#P-7)

#### Scenario: Current density on a metal segment
- **GIVEN** an extresist resistor of 10 Ω between nodes at 1.000 V and 0.990 V, with segment width 0.5 µm and metal thickness 0.36 µm
- **WHEN** the overlay dataset is computed
- **THEN** it SHALL report I = 1 mA, Jw = 2 mA/µm and J ≈ 5.556 mA/µm²

#### Scenario: Time scrubbing
- **GIVEN** a transient job feeding the voltage overlay
- **WHEN** cursor A moves to 7.5 ns, between samples at 7 ns (0.2 V) and 8 ns (0.6 V) of net `out`
- **THEN** the 3D overlay SHALL color `out` for 0.4 V

### Requirement: Viewer state persistence
The viewer configuration of a testbench SHALL be stored in the project, in `simulations/<testbench>/views.json`, and restored when the testbench is reopened. The configuration covers panels, traces (by job id, plot name and vector name), display modes, axis modes and cursor positions. A trace whose job or vector no longer exists SHALL be restored as a placeholder labeled `missing: <vector> [<job>]`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Deleted job
- **GIVEN** a saved view with a trace from job `j1`, which has since been deleted
- **WHEN** the testbench is reopened
- **THEN** the panel SHALL show the placeholder `missing: v(out) [j1]`
