# Reliability EM Check

## Purpose
Defines the electromigration (EM) check (decision P-21). For every wire segment, via and contact of the selected nets, Hefesto computes current metrics (average, absolute average, RMS, peak and peak-direction average) from simulation-derived branch currents. It compares each metric with the PDK limit from capability `reliability-em-rules`, and reports passes, warnings, violations and unchecked items.

Branch currents come from:
- an IR-drop run, for supply nets (capability `reliability-ir-drop`)
- a co-simulated RC netlist
- a redistribution of device terminal currents over the net's resistance network

Magic has no EM check. Every requirement is `native`.

Units: currents in A internally and mA in reports, widths in µm, time in s.

## Requirements

### Requirement: Check scope and items
An EM run SHALL analyse a set of nets of one cell: given net names, all supply nets, or all nets. For each net it SHALL use the analysis network of capability `reliability-ir-drop`, Requirement "Power-grid network extraction", built the same way for signal nets. The items are:
- **Wire item:** one non-contact tile resistor. Its attributes are the net, the layer type, the width w (the resistor's width field converted to µm), the length along the current direction in µm, the rectangle in cell coordinates, and the end nodes.
- **Cut item:** one contact resistor. Its attributes are the net, the contact type, the cut count n = nx·ny, the contact rectangle, and the end nodes. Its rule lookup tries kind `via` and kind `contact` for the type. A type may appear under only one of the two kinds (capability `reliability-em-rules`, Requirement "Rule file validation").

A contact whose via resistance is 0 has no resistor and therefore no current. It SHALL be counted as `unchecked` with reason `no-resistance`. Diagonal (split) tiles SHALL be items like the others, with the width field as recorded by the network.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Via item with two cuts
- **GIVEN** a via1 contact that the network models as one resistor with nx = 2 and ny = 1
- **WHEN** EM items are built
- **THEN** one cut item SHALL exist, with n = 2

### Requirement: Branch current sources
Each net SHALL take its branch currents (time series or constants, A, positive from end node a to end node b) from one of these sources:

| Source | Applies to | Definition |
|---|---|---|
| `irdrop` | supply nets | The branch currents of a completed IR-drop run (capability `reliability-ir-drop`, Requirement "Branch currents") |
| `cosim` | any net | A stored job whose DUT netlist contained the net's analysis network; I = (V(a) − V(b)) / R from the job's node vectors, as capability `simulation-ui-and-crossprobe`, Requirement "Data feed to 3D overlays", specifies |
| `redistribute` | signal nets | See below |

**Redistribution.** At each time sample t of a job that saved device currents and the net's voltage vector:
1. Each device-terminal node gets the terminal current flowing into the net, which is the negative of the current into the device.
2. Each node n gets −C_n · dV/dt, where C_n is the node capacitance of the network, and dV/dt is the central difference of the net's voltage vector (one-sided at the window ends).
3. The resulting Laplacian system is solved with the network's origin node grounded. Only branch currents are used.

If the injections at time t do not sum to zero, the imbalance is assigned to the origin node. When that imbalance exceeds 1 % of the largest injection magnitude at t, it SHALL be reported once per net as `warning` `REL-EM-004`. Results from `redistribute` SHALL be marked `approximate` (Q-RELIABILITY-10).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Charging current through a wire
- **GIVEN** a signal net with nodes A (driver terminal, C_A = 0) and B (C_B = 10 fF), joined by a 10 Ω resistor A→B, with the net voltage rising at 1 V/ns at time t
- **AND** a driver terminal current into the net of 10 µA at t
- **WHEN** the branch current is redistributed at t
- **THEN** the A→B resistor SHALL carry 10 µA, and no imbalance warning SHALL be raised

### Requirement: Current metrics
Over the window [t₀, t_N], with the samples inside it plus linearly interpolated end samples, let i_k = i(t_k), T = t_N − t₀, and for each interval h = t_{k+1} − t_k, a = i_k, b = i_{k+1}. The metrics SHALL be computed exactly for the piecewise-linear waveform:
- **Signed integral:** S = Σ h(a+b)/2
- **Absolute integral:** A = Σ (if a·b ≥ 0 then h(|a|+|b|)/2, else h(a²+b²)/(2(|a|+|b|)))
- **Square integral:** Q = Σ h(a²+ab+b²)/3
- **`avg`** = |S| / T
- **`absavg`** = A / T
- **`rms`** = √(Q / T)
- **`peak`** = max over k of |i_k|
- **`peakdir`** = P / T. Here s is the sign of the first sample whose |i_k| equals `peak`, and P is the integral of max(0, s·i) over the window. For an interval with a′ = s·a and b′ = s·b, P adds h(a′+b′)/2 if both are ≥ 0, 0 if both are ≤ 0, and otherwise h·p²/(2(|a′|+|b′|)), where p is whichever of a′ and b′ is positive.

For a single value (an `op` job or a static IR run), every metric SHALL equal |I|. If T = 0 and there is more than one sample, the run SHALL fail with `REL-EM-002`. For a cut item, each metric SHALL be divided by n to give the per-cut value.

**Direction.** An item is `uni` if no two samples with |i_k| > 1e-3 · `peak` have opposite signs, and `bi` otherwise.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21) (gf ref: drm_14_2.rst §14.2.1) (See Q-RELIABILITY-5.)

#### Scenario: Metrics of a bidirectional waveform
- **GIVEN** samples 0, 2, 2, −2, 0 mA at t = 0, 1, 2, 3, 4 ns
- **WHEN** the metrics are computed over [0, 4 ns]
- **THEN** avg = 0.5 mA, absavg = 1.25 mA, rms = 1.41421 mA (to 5 decimals), peak = 2 mA and peakdir = 0.875 mA
- **AND** the direction SHALL be `bi`

#### Scenario: DC current
- **GIVEN** a static IR run in which a wire carries −3 mA
- **WHEN** its metrics are computed
- **THEN** avg, absavg, rms, peak and peakdir SHALL each be 3 mA, and the direction SHALL be `uni`

### Requirement: Limit comparison
For each item and each metric m ∈ {avg, absavg, rms, peak, peakdir}, Hefesto SHALL look up the limit with capability `reliability-em-rules`, Requirement "Limit lookup". The query is the item's type, kind, width (wires), metric m, the item's direction, and the analysis temperature (default from the rule file). Metrics with no rule are skipped.

For each metric with a limit, ratio = value / I_lim, and its status is:
- `fail` if ratio > 1
- `warn` if ratio > r_warn (default 0.8; allowed range 0 < r_warn ≤ 1)
- `pass` otherwise

An item's status is the worst of its metric statuses, in the order fail > warn > pass. It is `unchecked` if no metric had a limit. An item whose current is zero for every sample SHALL be `pass` when any limit applies.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: IHP metal2 pass and warn
- **GIVEN** sg13g2 rules, T = 105 °C, and a metal2 wire of width 1.0 µm
- **WHEN** its avg current is 1.5 mA, and in a second case 1.7 mA
- **THEN** the first case SHALL be `pass` with ratio 0.75, and the second `warn` with ratio 0.85

#### Scenario: IHP via1 violation
- **GIVEN** sg13g2 rules and a via1 item with n = 2 carrying an avg current of 1.0 mA
- **WHEN** it is checked
- **THEN** the per-cut avg SHALL be 0.5 mA against 0.4 mA, so ratio = 1.25 and the status is `fail`

#### Scenario: gf180 bidirectional current uses peakdir
- **GIVEN** gf180mcuD rules at 110 °C, and a metal2 wire 0.5 µm wide whose waveform is `bi`, with peakdir = 0.70 mA and avg = 0.10 mA
- **WHEN** it is checked
- **THEN** the peakdir/bi rule (1.5 mA/µm, so I_lim = 0.75 mA) SHALL give ratio 0.9333 (`warn`)
- **AND** the avg/uni rule, reached by direction fallback (1.0 mA/µm, so I_lim = 0.5 mA), SHALL give ratio 0.2 (`pass`), so the item status is `warn`

### Requirement: Violation grouping and diagnostics
Failing items of one net and one layer type that share an end node SHALL be grouped, transitively, into one violation. A violation SHALL be reported as one `error` diagnostic `REL-EM-010` (wires) or `REL-EM-011` (cuts). It carries the net, layer, the worst metric and its ratio, the bounding box of the group, and the related items. Warnings SHALL be grouped the same way and reported as `warning` `REL-EM-020` and `REL-EM-021`.

Diagnostics SHALL be ordered by worst ratio (descending), then by net name, then by bounding-box lower-left y, then by x. Each diagnostic SHALL link to its location (capability `hefesto-diagnostics`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Series segments grouped
- **GIVEN** three abutting metal1 wire items on net VDD, all failing, with ratios 1.1, 1.3 and 1.2
- **WHEN** diagnostics are produced
- **THEN** exactly one `REL-EM-010` SHALL be reported, with ratio 1.3 and a box covering all three items

### Requirement: EM report file
An EM run SHALL be stored as capability `reliability-ir-drop`, Requirement "Reliability run storage", specifies. Its `em-items.csv` has one row per (item, metric that has a limit), plus one row per unchecked item with metric `-`. The columns are:

```
net,item,kind,layer,x1_um,y1_um,x2_um,y2_um,width_um,cuts,direction,metric,value_mA,limit_mA,ratio,status,rule_id,temperature_C,source
```

The fields are:
- `item`: `R<k>`, where k is the resistor's index in the analysis network's canonical order
- `kind`: `wire` or `cut`
- `width_um`: empty for cuts
- `cuts`: empty for wires
- `value_mA` and `limit_mA`: per cut for cut items
- `status`: `pass`, `warn`, `fail` or `unchecked`
- `source`: `irdrop`, `cosim` or `redistribute`, plus `;approximate` or `;estimated` when those apply

Numbers SHALL use C `%.6e`, except coordinates and widths (`%.4f`) and the ratio (`%.4f`). Rows SHALL be sorted by ratio descending (unchecked rows last), then net, then item index, then metric in the order avg, absavg, rms, peak, peakdir.

`run.json` SHALL also hold, per net, the counts of pass, warn, fail and unchecked items, and the list of model limitations from Requirement "Modelling limitations".

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Row format
- **GIVEN** the failing via1 item of the IHP scenario, as resistor index 17 on net `VDD` at (1.0, 2.0)–(1.5, 2.2)
- **WHEN** the report is written
- **THEN** it SHALL contain `VDD,R17,cut,via1,1.0000,2.0000,1.5000,2.2000,,2,uni,avg,5.000000e-01,4.000000e-01,1.2500,fail,JMAXVIA1,1.050000e+02,cosim`

### Requirement: Modelling limitations
Every EM report and the EM panel SHALL state that the check does not model:
- short-length (Blech) immunity: no credit is given
- current crowding at wire-to-via junctions and corners: the tile resistor current is used
- self-heating: no temperature rise is computed from the RMS current, and RMS limits apply only where the rule file has them
- slotting rules for wide metal: these are DRC matters
- lifetime extrapolation beyond the rule file's basis

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21) (See Q-RELIABILITY-11.)

#### Scenario: Limitations in the report
- **WHEN** any EM run is stored
- **THEN** `run.json` SHALL contain a `limitations` array with the five entries above

### Requirement: The emcheck command
The command API SHALL provide:

```
emcheck [-net <name>]... [-supplies] [-all] [-source irdrop|cosim|redistribute]
        [-run <irdrop-run-id>] [-job <job-id>] [-window <t1> <t2>]
        [-temp <C>] [-warn <ratio>] [-cell <name>]
```

The defaults are:
- the nets: `-supplies`
- `-source`: `irdrop` for supply nets and `redistribute` for others
- `-run`: the newest non-stale IR run of the cell
- `-job`: the newest completed job of the cell
- `-temp`: from the rule file
- `-warn`: 0.8

It SHALL run in the background, report progress, and be cancellable. It SHALL return the run id, and SHALL be available in `hefesto-cli`. The CLI exits 0 when the run completes; with `--fail-on-violation`, it exits 1 if any item is `fail`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/cross-cutting.md#X-6)

#### Scenario: No rules
- **GIVEN** a sky130A design with no EM rule file
- **WHEN** `emcheck -supplies` runs
- **THEN** the run SHALL complete with every item `unchecked`, and `REL-EMR-020` SHALL be reported

### Requirement: Sign-off checklist item
Hefesto SHALL offer an optional sign-off checklist item "Electromigration and IR drop" (capability `tapeout-signoff-checklist`), with category `electrical` and severity `warning` unless the program profile says otherwise. Its result is:
- `fail` if the newest non-stale EM run of the top cell has any `fail` item, or the newest IR run has a budget violation
- `pass` if both runs exist, are non-stale, and have no failures
- `not-run` when a run is missing or stale
- `not-applicable` when the PDK has no EM rules and no IR run was requested

Results based on `estimated` or `approximate` currents SHALL show that qualifier. The item SHALL follow the checklist's staleness and waiver rules.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/product.md#P-15)

#### Scenario: Stale after edit
- **GIVEN** a passing EM item in the checklist
- **WHEN** the top cell is edited
- **THEN** the item SHALL show as stale until the EM run is repeated

### Requirement: Determinism
For the same cell, rule file, job and options, an EM run SHALL produce byte-identical `em-items.csv` files and identical diagnostics on repeated runs and with any thread count. Across platforms, numeric fields SHALL agree within 1e-9 relative, and statuses SHALL be identical except for items whose ratio lies within 1e-9 of a threshold.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Thread count independence
- **WHEN** the same EM run is executed with 1 worker thread and with 8
- **THEN** the two `em-items.csv` files SHALL be byte-identical
