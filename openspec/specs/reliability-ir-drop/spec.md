# Reliability IR Drop

## Purpose
Defines static and dynamic IR-drop analysis of power and ground nets (decision P-21). The resistive grid is the resistance network that capability `extresist-network` builds, with reduction turned off. The load currents come from embedded-simulation results (`simulation-*` capabilities), from a co-simulation of the extracted grid, or from user estimates. Hefesto solves the grid with its own sparse solver.

This capability also defines:
- the solver requirements and the accuracy and performance targets
- the `irdrop` command
- the reliability run folder, which capability `reliability-em-check` shares

Magic has no IR-drop feature. Every requirement is `native`.

Units: resistance in Ω, conductance in S, voltage in V, current in A in the solver (reports use mA and mV where stated), capacitance in F, time in s. All solver arithmetic is IEEE-754 double precision.

## Requirements

### Requirement: Supply definitions
An IR-drop run SHALL analyse one or more supply nets of one cell (the *analysed cell*). Each supply SHALL have:
- a net name in the analysed cell
- a polarity, `power` or `ground`
- a nominal voltage V_nom in V (default 0 for ground)
- one or more source points

By default the source points are the ports of the analysed cell on that net (capability `layout-labels-ports`). The user MAY instead place source points at layout coordinates on a layer of the net. Each source point MAY have a series resistance R_s ≥ 0 Ω, default 0, which models a bond wire or package pin.

Polarity defaults:
- `power` for nets whose port `use` is `power`
- `ground` for `use ground`
- otherwise, the user SHALL choose

A supply with no source point SHALL fail the run with `REL-IR-001`, naming the net.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Ports as sources
- **GIVEN** cell `core` with port labels `VPWR` (use power) at two pads and `VGND` (use ground) at one pad
- **WHEN** an IR-drop run is created with V_nom(VPWR) = 1.8
- **THEN** the run SHALL have supply VPWR (power, 1.8 V, two sources) and VGND (ground, 0 V, one source)

### Requirement: Power-grid network extraction
For each supply net, Hefesto SHALL build the *analysis network* with the network construction of capability `extresist-network`, configured as follows:
- the net is processed regardless of the lumped-resistance and delay cutoffs
- simplification, Elmore pruning, and lumped and `.res.ext` output are all off
- every tile resistor and contact resistor is kept, with its layer type, its width field (the tile width across the current direction, in internal units) and, for contacts, its cut counts nx·ny

The network SHALL be held in memory and SHALL NOT change the user's `extresist` settings or files. Resistances SHALL be converted from the single-precision mΩ values of the network to double-precision Ω by `R = (double)r_mΩ / 1000`. Coordinates SHALL be converted to µm with the cell's scale.

Load attachment points are the network nodes of the device terminals on the net, found by device annotation (capability `extresist-network`, Requirement "Device annotation"). For a subcell treated as a black box, they are the subcell's port locations instead (Q-RELIABILITY-7).

A network SHALL be cached by (cell, net, the cell's modification stamp, the extract style). An edit to the cell SHALL make its cached networks and every run built on them stale.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21) (See Q-RELIABILITY-6.)

#### Scenario: Reduction disabled
- **GIVEN** a VPWR strap of three abutting metal1 tiles, which `extresist` with simplification on would merge into one resistor
- **WHEN** the analysis network is built
- **THEN** it SHALL contain one resistor per tile, each with its own width

### Requirement: Load current sources
A run SHALL take its load currents from exactly one of these sources:

| Source | Description |
|---|---|
| `sim` | A stored simulation job of the analysed cell (capability `simulation-analyses-and-results`). For each device terminal attached to a supply net, the terminal-current vector of that device. The job must have saved device currents: mode `all+currents` or explicit saves. |
| `cosim` | A stored job whose DUT netlist contained the analysis networks of the supply nets, as the resistive part of parasitic mode `rc`. Node voltages are read directly from the job, and no grid solve is done. |
| `estimate` | A total current per supply, in A, distributed over the load attachment points: MOSFETs by the ratio of their W to the total W on that net, and other devices equally |
| `table` | A CSV file `device,current_A` with one row per device instance name, mapped through the net-name map |

For `sim`, missing current vectors SHALL fail the run with `REL-IR-004`, listing up to 20 device names and the total count. Sign convention: a load current is positive when it flows from the grid into the device. That is the conventional supply current on power nets, and the return current on ground nets is negative. Results from `estimate` and `table` SHALL be marked `estimated` in every report and view.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/product.md#P-7) (See Q-RELIABILITY-8.)

#### Scenario: Missing currents
- **GIVEN** a job run with mode `all` (no device currents)
- **WHEN** a `sim`-sourced IR-drop run uses it
- **THEN** the run SHALL fail with `REL-IR-004` and the hint `enable "save currents" and run again`

### Requirement: Static analysis
A static run SHALL use one constant current per load point:
- for an `op` job, the operating-point value
- for a transient job, the time average over the window [t₁, t₂] (default: the whole plot), computed by the trapezoidal rule over the stored samples, with values at t₁ and t₂ linearly interpolated: Ī = (1/(t₂−t₁)) ∫ i(t) dt

It SHALL then solve the grid (Requirement "Grid solver"). The drop at node n SHALL be:
- `ΔV = V_nom − V(n)` for power nets
- `ΔV = V(n) − V_nom` for ground nets

so that a positive drop is always the bad direction.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Two-node ladder
- **GIVEN** a power net with a source at node 0 (V_nom 1.8 V, R_s 0), resistors 0–1 of 1 Ω and 1–2 of 1 Ω, and loads of 10 mA at nodes 1 and 2
- **WHEN** a static run solves it
- **THEN** V(1) = 1.780 V and V(2) = 1.770 V, so ΔV(1) = 20 mV and ΔV(2) = 30 mV

#### Scenario: Window average
- **GIVEN** a load whose current samples are 0, 20, 20 and 0 mA at t = 0, 1, 2 and 3 ns
- **WHEN** a static run averages over [0, 3 ns]
- **THEN** the load current SHALL be 13.333 mA (to 3 decimals)

### Requirement: Dynamic analysis
A dynamic run SHALL compute node voltages over time for the window [t₁, t₂] of a transient job. The model is:
- **Conductances:** the grid conductance matrix G.
- **Capacitance:** a diagonal C, holding each node's capacitance from the network's capacitance distribution (capability `extresist-network`, Requirement "Path resistance and capacitance distribution"), plus any user-specified decoupling capacitance at chosen nodes.
- **Integration:** backward Euler by default, or the trapezoidal rule when chosen. Time points are the job's samples inside the window plus t₁ and t₂. The user MAY instead choose a fixed step h.
- **Initial state:** the static solution with the currents at t₁.

Backward Euler solves `(G + C/h) v₍ₖ₊₁₎ = (C/h) v₍ₖ₎ + b₍ₖ₊₁₎` at each step, where b holds the source and load terms. The factorization SHALL be reused for every step with the same h, with h compared bit-for-bit.

For each node, the result SHALL keep the maximum drop over the window and the first time it occurs. The full voltage time series SHALL be kept only for nodes the user selects, and for the overlay dataset (capability `reliability-visualization`), decimated to at most 2000 samples per node by keeping each interval's min and max.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: RC step response with backward Euler
- **GIVEN** a node joined to an ideal 1.8 V source by 1 Ω, with C = 1 nF, a load current of 0 before t₁ and 10 mA from t₁ on, and a fixed step h = 0.1 ns
- **WHEN** the first step is computed
- **THEN** the drop SHALL be 0.01 · 1e-10 / (1e-9 + 1e-10) = 0.909091 mV (to 6 decimals)

### Requirement: Grid solver
The grid SHALL be solved by nodal analysis as follows:
1. **Merging.** Merge the end nodes of every resistor with R = 0 (union-find). A resistor with R < 0 or a non-finite value SHALL fail the run with `REL-IR-002`.
2. **Sources.** A source with R_s = 0 fixes its node at V_nom; such nodes are eliminated. A source with R_s > 0 adds a conductance 1/R_s from the node to an ideal V_nom.
3. **Floating parts.** A connected component with no source is floating. Its nodes SHALL be excluded, and reported with `warning` `REL-IR-003`, giving the node count, the total load current in the component, and a bounding box.
4. **Assembly.** Build the matrix in a canonical order: node indices sorted by node name in code-point order, and entries summed in the order of resistors sorted by (smaller index, larger index, resistance).
5. **Solve.** The reduced matrix is symmetric positive definite. The default method is sparse Cholesky with an approximate-minimum-degree ordering. When the estimated factor memory exceeds the analysis memory budget, the method is conjugate gradients with an incomplete-Cholesky preconditioner, stopped when ‖r‖₂ ≤ 1e-10 · ‖b‖₂. Not converging within 10·n iterations SHALL fail with `REL-IR-005`.

Results SHALL be bit-identical between runs and between thread counts on the same platform and build. Between platforms, they SHALL agree within a relative 1e-9 of the largest drop. Hefesto SHALL implement the solver itself, or use a permissively licensed library (Q-RELIABILITY-9). It SHALL NOT shell out to an external program.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/product.md#P-9)

#### Scenario: Floating island
- **GIVEN** a VPWR metal2 strap that is not connected to any source, and carries two loads of 1 mA
- **WHEN** a static run solves VPWR
- **THEN** `REL-IR-003` SHALL report the island with its two loads and 2 mA, and its nodes SHALL have no drop value

#### Scenario: Source resistance
- **GIVEN** the two-node ladder above, with R_s = 0.5 Ω at node 0
- **WHEN** it is solved
- **THEN** V(0) = 1.8 − 0.5·0.02 = 1.790 V and ΔV(2) = 40 mV

### Requirement: Branch currents
For every resistor of the analysis network, a run SHALL provide the branch current `I = (V(a) − V(b)) / R`, in A, positive from a to b:
- for a static run, one value
- for a dynamic or `cosim` run, a time series

Contact resistors SHALL also carry their cut count nx·ny. These currents feed capability `reliability-em-check` for supply nets. For a `cosim` run, branch currents SHALL come from the job's node voltages and the deck's resistor values, as capability `simulation-ui-and-crossprobe`, Requirement "Data feed to 3D overlays", specifies.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Ladder branch currents
- **WHEN** the two-node ladder is solved
- **THEN** resistor 0–1 SHALL carry 20 mA and resistor 1–2 SHALL carry 10 mA

### Requirement: Drop budget and violations
Each supply SHALL have a drop budget in V. The defaults are:
- **Static runs:** 5 % of the reference voltage.
- **Dynamic runs:** 10 % of the reference voltage.

For a power net the reference voltage is |V_nom|. For a ground net it is the largest |V_nom| among the power supplies of the run. A device whose attachment-point drop exceeds the budget SHALL be a violation. Violations SHALL be reported as `warning` diagnostics `REL-IR-010`, one per device, with the device, net, drop, budget and location. The diagnostics SHALL be sorted by drop (descending), then by device name. A run SHALL also report, per supply:
- the maximum drop and the node where it occurs
- the mean drop over load attachment points
- the total current
- the number of violating devices

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Budget violation
- **GIVEN** the two-node ladder, with a 1.8 V supply and the default static budget of 90 mV
- **WHEN** load currents are raised to 40 mA each, giving ΔV(2) = 120 mV
- **THEN** the device at node 2 SHALL get `REL-IR-010` with drop 120 mV and budget 90 mV

### Requirement: Reliability run storage
Every IR-drop or EM run SHALL be stored in the project, in `reliability/<run-id>/`. `<run-id>` is `<yyyymmdd-hhmmss>-<cell>-<kind>`, where kind is `irdrop` or `em`. The folder SHALL contain:

| File | Content |
|---|---|
| `run.json` | id, kind, cell, cell modification stamp, extract style, PDK and version, EM rule source (for EM), supplies or nets, current source and job id, window, temperature, options, solver method, Hefesto version, platform, start and finish times (ISO 8601), `estimated` flag, and `stale` flag |
| `irdrop-nodes.csv` | IR runs: `net,node,x_um,y_um,layer,drop_V,time_s`, one row per non-floating node |
| `irdrop-devices.csv` | IR runs: `net,device,node,drop_V,budget_V,violation` |
| `em-items.csv` | EM runs: defined in capability `reliability-em-check` |

CSV files SHALL use `,` separators, `\n` line ends, a header row, numbers in C `%.6e` format, coordinates in `%.4f`, and rows sorted as the requirement that defines each file says. `irdrop-nodes.csv` is sorted by drop descending, then net, then node, by code point. If the project folder is not writable, runs SHALL be stored in app storage, and the user SHALL be told where. A run SHALL be marked stale when the cell, the job, or the rule file it used changes.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Stored IR run
- **WHEN** a static IR run of cell `core` finishes at 2026-11-06 14:30:00
- **THEN** `reliability/20261106-143000-core-irdrop/` SHALL contain `run.json`, `irdrop-nodes.csv` and `irdrop-devices.csv`

### Requirement: The irdrop command
The command API SHALL provide:

```
irdrop [-net <name>[=<V>][:power|:ground]]... [-source sim|cosim|estimate|table]
       [-job <job-id>] [-current <net>=<A>]... [-table <file>]
       [-mode static|dynamic] [-window <t1> <t2>] [-step <h>] [-method be|trap]
       [-budget <net>=<V>]... [-cell <name>]
```

The defaults are:
- `-net`: every port net with `use power` or `use ground`
- `-source`: `sim`
- `-job`: the most recent completed job of the cell
- `-mode`: `static`
- `-method`: `be`
- `-cell`: the edit cell

The command SHALL run in the background, report progress, and be cancellable. It SHALL return the run id. It SHALL be available in `hefesto-cli`, where the process exits 0 if the run completed, whether or not there were violations. With `--fail-on-violation`, it exits 1 when there are violations.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/cross-cutting.md#X-6)

#### Scenario: Headless static run
- **WHEN** `hefesto-cli -T sg13g2 -c "load core; irdrop -net VDD=1.2:power -net VSS:ground -job j7; quit"` is run
- **THEN** a run folder SHALL be written, its id printed, and the tool SHALL exit 0

### Requirement: Accuracy targets
Hefesto SHALL meet these accuracy targets, checked by golden cases (`tests/golden/RELIABILITY/CASES.md`):
1. **Solver.** On the same analysis network and currents, the static drop at every node SHALL match a reference ngspice `.op` of the network written as resistors and DC current sources within max(1 µV, 1e-6 · max drop). A dynamic run with a fixed step SHALL match a reference ngspice `.tran` of the same RC network, using the same integration method and step (`method=gear maxord=1` for backward Euler, `method=trap` otherwise), within max(1 µV, 1e-4 · max drop).
2. **Extraction.** Resistance values are those of `extresist-network`. Hefesto does not claim field-solver accuracy.
3. **Method.** For the reference designs, where the maximum co-simulated drop is ≤ 10 % of V_nom, the maximum drop of a `sim` run SHALL be within 10 % (relative) of the maximum drop of the matching `cosim` run. This is a tracked target, not a guarantee for every design (Q-RELIABILITY-8).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Solver matches ngspice
- **GIVEN** a 10 × 10 mesh of 0.1 Ω resistors with a 1.2 V source at one corner and 1 mA loads at all 100 nodes
- **WHEN** Hefesto and the reference ngspice solve it
- **THEN** every node's drop SHALL agree within max(1 µV, 1e-6 · max drop)

### Requirement: Performance and memory
On the reference desktop of capability `hefesto-performance`, a static solve of a grid with 1 000 000 nodes and 2 000 000 resistors SHALL complete within 10 s and within 4 GiB of additional memory. This excludes network extraction.

Before factorizing, the solver SHALL estimate the memory it needs, and compare it with the analysis memory budget. The budget defaults to 50 % of physical memory on desktop, and on iPadOS to 1.5 GiB, so that the 3 GB resident bound of `hefesto-performance` holds. If even the iterative method's estimate exceeds the budget, the run SHALL fail with `REL-IR-020`, giving the estimate and the budget, before allocating. Runs SHALL be available on iPadOS, subject to this budget.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/product.md#P-13)

#### Scenario: Too large for iPad
- **GIVEN** an iPad with the default budget, and a grid whose iterative-solver estimate is 2.4 GiB
- **WHEN** an IR run starts
- **THEN** it SHALL fail with `REL-IR-020` before any large allocation, and Hefesto SHALL keep running
