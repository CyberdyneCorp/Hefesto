# Simulation Sweeps

## Purpose
Defines how Hefesto runs one testbench many times with different values (decision P-19):
- run sets, the batch object shared by sweeps, corners (capability `simulation-corners`) and Monte Carlo (capability `simulation-monte-carlo`)
- sweep variables: `.param` values, temperature, supply scale and stimulus values
- point generation for linear, logarithmic and list sweeps, and nested sweeps
- design-of-experiments sampling: full factorial, one-at-a-time, Latin hypercube and random
- how each point's deck is derived from the testbench deck
- storage, progress, cancellation and resumption of run sets
- the `sim sweep`, `sim doe` and `sim runset` command options

Each point of a run set is an ordinary job of capability `simulation-analyses-and-results`, built by capability `simulation-netlist-preparation` and run by capability `simulation-engine-embedding`. Measurements and statistics over the points are in capability `simulation-measurements-and-statistics`. Waveforms are shown with the viewer of capability `simulation-ui-and-crossprobe`. Testbenches drawn as schematics (capability `schematic-simulation-integration`) are swept in the same way as any other testbench.

ngspice has no `.step` card. Hefesto therefore does not ask ngspice to sweep anything except the sweep variable of a `.dc` analysis: every point is a separate deck load. `.control` loops written by users (`foreach`, `dowhile`, `alterparam`, `reset`) stay supported inside a single job, as capability `schematic-simulation-integration` specifies, but they are not run sets.

ngspice reference: SourceForge `ngspice` master @ `7aa47164` (after ngspice-47). Open questions: `docs/decisions/open-questions/SIMADV.md` (Q-SIMADV-n).

## Requirements

### Requirement: Run sets
A run set SHALL be an ordered list of points, all derived from one testbench, that Hefesto submits, tracks, stores and analyses as a unit. A run set has an ordered list of dimensions, outermost first. Each dimension is one of:
- a sweep dimension (Requirement "Sweep variables")
- a design-of-experiments block (Requirement "Design of experiments"), which counts as one dimension whose levels are its sample rows
- the corner dimension (capability `simulation-corners`)
- the Monte Carlo dimension (capability `simulation-monte-carlo`)

A run set SHALL have at most one corner dimension and at most one Monte Carlo dimension. The Monte Carlo dimension, if present, SHALL be the innermost. The points are the cartesian product of the dimensions' levels. Point index `p` (0-based) is the mixed-radix number whose digits are the level indices, innermost digit fastest:

```
p = Σ_d  i_d · Π_{e > d} n_e        (d, e range over dimensions, outermost first; n_e = level count of e)
```

The total point count SHALL be at most 100 000. A larger product SHALL be rejected with `Run set has <N> points; the limit is 100000` before any job is created. A product of 0 points SHALL be rejected with `Run set has no points`.

The run set id SHALL be `<YYYYMMDD>-<HHMMSS>-<testbench>-rs`, in local time, with the `-2`, `-3`, ... collision rule of capability `simulation-analyses-and-results` (Requirement "Job lifecycle"). The job id of point `p` SHALL be `<run set id>-p<p>`, with `p` written as 6 decimal digits with leading zeros. Each point job SHALL also record its level values, one per dimension, in its `job.json` under the key `point`, as a JSON object whose keys are the dimension names.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Point index of a nested sweep
- **GIVEN** a run set with outer dimension `temp` = (−40, 125) and inner dimension `vdd` = (1.62, 1.8, 1.98)
- **WHEN** the points are enumerated
- **THEN** there SHALL be 6 points, and point 4 SHALL have `temp` = 125 and `vdd` = 1.8

#### Scenario: Too many points
- **GIVEN** three sweep dimensions of 100 levels each
- **WHEN** the run set is started
- **THEN** it SHALL be rejected with `Run set has 1000000 points; the limit is 100000`, and no job SHALL be created

### Requirement: Sweep variables
A sweep dimension SHALL name exactly one variable of one of these kinds:

| Kind | Name syntax | Effect on the point's deck |
|---|---|---|
| parameter | `param:<name>` | sets the top-level parameter `<name>` (Requirement "Point decks") |
| temperature | `temp` | sets the circuit temperature in °C |
| supply scale | `supply` | sets the supply scale factor `k` of capability `simulation-netlist-preparation`, Requirement "Corners" |
| stimulus value | `source:<name>` | sets the DC value of stimulus `<name>` |
| PDK corner | `corner` | not a sweep dimension; use the corner dimension of capability `simulation-corners` |

Parameter names SHALL match `[A-Za-z_][A-Za-z0-9_]*` and SHALL be compared case-insensitively, as ngspice does. A `source:` variable SHALL name a stimulus of the testbench; for a `pulse`, `sin` or `pwl` stimulus it SHALL be rejected with `Sweep source:<name>: only dc stimuli can be swept; use a parameter`. Two dimensions SHALL NOT name the same variable. A sweep of `supply` together with a corner dimension whose corners set a supply scale SHALL be rejected with `Sweep supply conflicts with the corner supply`. The same rule applies to `temp` and corner temperatures.

Device instance values (for example a transistor width) SHALL be swept through parameters: the user writes the instance value as `{<name>}` and sweeps `param:<name>`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Sweep a transistor width
- **GIVEN** a schematic testbench with `XM1 d g 0 0 sky130_fd_pr__nfet_01v8 W={wn} L=0.15` and a code block `.param wn=1`
- **WHEN** a sweep of `param:wn` over the list (1, 2, 4) is run
- **THEN** 3 jobs SHALL run, whose decks contain `.param wn=1`, `.param wn=2` and `.param wn=4` respectively

#### Scenario: Pulse source rejected
- **GIVEN** a stimulus `Vin` of kind `pulse`
- **WHEN** a sweep of `source:Vin` is defined
- **THEN** it SHALL be rejected with `Sweep source:Vin: only dc stimuli can be swept; use a parameter`

### Requirement: Sweep point generation
A sweep dimension SHALL generate its levels with one of these modes. All arithmetic is IEEE-754 binary64; level `x_k` is computed directly from `k`, never by repeated addition:

| Mode | Parameters | Count `n` | Levels `x_k`, k = 0 … n−1 |
|---|---|---|---|
| `lin` with points | start `a`, stop `b`, points `n ≥ 2` | `n` | `a + k·(b − a)/(n − 1)` |
| `lin` with step | start `a`, stop `b`, step `s ≠ 0` with `(b − a)/s ≥ 0` | `floor((b − a)/s + 1e-9) + 1` | `a + k·s` |
| `dec` | start `a > 0`, stop `b > 0`, points per decade `m ≥ 1` | `floor(m·log10(b/a) + 1e-9) + 1` | `a · 10^(k/m)` |
| `oct` | start `a > 0`, stop `b > 0`, points per octave `m ≥ 1` | `floor(m·log2(b/a) + 1e-9) + 1` | `a · 2^(k/m)` |
| `list` | values `v_0 … v_{n−1}`, `n ≥ 1` | `n` | `v_k` |

For `dec` and `oct`, `b < a` SHALL sweep downwards (the logarithm is negative and `m` is applied to its absolute value, `x_k = a · 10^(−k/m)` or `a · 2^(−k/m)`). A list MAY contain duplicates; they produce separate points. Violations SHALL be rejected with an error naming the dimension and the parameter, for example `Sweep param:wn: dec start must be > 0`. Numbers in commands follow capability `simulation-command`, Requirement "Numeric argument syntax". Level values SHALL be written into decks in C `%.12g` format.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Linear sweep with a step
- **GIVEN** a `lin` sweep of `supply` from 0.9 to 1.1 with step 0.05
- **WHEN** the levels are generated
- **THEN** there SHALL be 5 levels, written as `0.9`, `0.95`, `1`, `1.05` and `1.1`

#### Scenario: Decade sweep
- **GIVEN** a `dec` sweep of `param:rl` from 1k to 1meg with 2 points per decade
- **WHEN** the levels are generated
- **THEN** there SHALL be 7 levels, the second written as `3162.27766017` and the last as `1000000`

#### Scenario: Invalid logarithmic start
- **WHEN** a `dec` sweep of `param:wn` from 0 to 10 is defined
- **THEN** it SHALL be rejected with `Sweep param:wn: dec start must be > 0`

### Requirement: Point decks
The deck of each point SHALL be the testbench deck of capability `simulation-netlist-preparation` (Requirement "Testbench composition", or the schematic deck of capability `schematic-simulation-integration`) with only these changes, applied in this order:
1. **Corner.** The corner of the point, if the run set has a corner dimension (capability `simulation-corners`).
2. **Parameters.** For each `param:<name>` level: if the deck has exactly one top-level `.param` statement that defines `<name>`, the expression of that definition SHALL be replaced by the level value, and the other definitions on the same line SHALL be kept. If the deck defines `<name>` nowhere at top level, the line `.param <name>=<value>` SHALL be inserted directly after the title line. If `<name>` is defined more than once at top level, the run set SHALL be rejected with `Parameter <name> is defined more than once at top level`. Definitions inside `.subckt` bodies and inside `.control` blocks SHALL NOT be changed.
3. **Temperature.** A `temp` level SHALL replace every top-level `.temp` card, and every `temp=` option of a top-level `.option`/`.options` card, by the level value. If there is none, `.temp <value>` SHALL be inserted after the title line.
4. **Supply and stimuli.** `supply` and `source:` levels SHALL change the DC value of the stimuli they act on. For a schematic or raw deck, a `source:<name>` level SHALL replace the DC value of the top-level `V` or `I` line named `<name>`; the value is the first token after the two nodes, after an optional `DC` keyword.
5. **Monte Carlo.** The statistical settings of capability `simulation-monte-carlo`.
6. **Title.** The title line SHALL become `<testbench title> [<point job id>]`.

Each point deck SHALL then be resolved and validated as any other deck (capability `simulation-netlist-preparation`, Requirements "Model library resolution" and "Deck validation and sanitisation"). Two points with equal level values SHALL get identical decks apart from the title line. The base deck SHALL be prepared once per run set (netlisting, extraction, file resolution); points SHALL NOT re-extract or re-netlist the DUT.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (ngspice ref: src/frontend/inpcom.c:1140-1160) (See Q-SIMADV-9.)

#### Scenario: Parameter replaced in place
- **GIVEN** a deck line `.param wn=1 ln=0.15`
- **WHEN** a point with `param:wn` = 2 is prepared
- **THEN** that line SHALL read `.param wn=2 ln=0.15` and no other line SHALL change except the title

#### Scenario: Parameter defined twice
- **GIVEN** a deck with `.param vdd=1.8` on line 3 and `.param vdd=1.62` on line 9, both at top level
- **WHEN** a sweep of `param:vdd` is started
- **THEN** it SHALL be rejected with `Parameter vdd is defined more than once at top level`

### Requirement: Design of experiments
A design-of-experiments (DOE) block SHALL sample `d ≥ 1` factors jointly. Each factor is a sweep variable (Requirement "Sweep variables") with a low value `lo`, a high value `hi > lo` and a scale `lin` or `log` (`log` requires `lo > 0`). A unit value `u ∈ [0, 1]` maps to the factor value `lo + u·(hi − lo)` (`lin`) or `lo · (hi/lo)^u` (`log`). The methods SHALL be:

| Method | Rows | Unit values |
|---|---|---|
| `full` | `Π L_j` for per-factor level counts `L_j ≥ 2` | every combination of `u = k/(L_j − 1)`, factor 0 outermost |
| `oat` (one at a time) | `1 + 2d` | row 0: every factor at 0.5; then for j = 0 … d−1: factor j at 0, then at 1, others at 0.5 |
| `lhs` (Latin hypercube) | `n ≥ 2` | `u_ij = (π_j(i) + 0.5)/n` |
| `random` | `n ≥ 1` | `u_ij` = the next uniform draw, row by row, factor by factor |

For `lhs`, `π_0 … π_{d−1}` are permutations of `0 … n−1`, generated in factor order from one Hefesto random stream by the Fisher–Yates shuffle: start from the identity; for `i = n−1` down to 1, draw `u`, set `j = floor(u·(i+1))`, and swap entries `i` and `j`.

The Hefesto random stream with 64-bit seed `S` SHALL be the SplitMix64 sequence. All arithmetic is on unsigned 64-bit integers modulo 2^64:

```
mix(z):  z = (z XOR (z >> 30)) · 0xBF58476D1CE4E5B9
         z = (z XOR (z >> 27)) · 0x94D049BB133111EB
         return z XOR (z >> 31)
v_t = mix(S + (t + 1) · 0x9E3779B97F4A7C15)      for t = 0, 1, 2, ...
u_t = (v_t >> 11) · 2^-53                         uniform in [0, 1)
```

The DOE seed SHALL default to 1 and SHALL be stored with the run set, so the same seed and factors always give the same rows on every platform. This stream is Hefesto's own; it is independent of the random numbers that ngspice draws inside a deck (capability `simulation-monte-carlo`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: One-at-a-time rows
- **GIVEN** an `oat` block with factors `param:wn` (lo 1, hi 3, lin) and `temp` (lo −40, hi 120, lin)
- **WHEN** the rows are generated
- **THEN** there SHALL be 5 rows: (2, 40), (1, 40), (3, 40), (2, −40), (2, 120)

#### Scenario: Latin hypercube strata
- **GIVEN** an `lhs` block with 2 factors on `[0, 1]`, `n = 10` and seed 7
- **WHEN** the rows are generated twice, once on Linux and once on iPad
- **THEN** both runs SHALL give identical rows, and for each factor every interval `[k/10, (k+1)/10)` SHALL contain exactly one row's value

### Requirement: Run set execution
Hefesto SHALL submit the points of a run set to the job queue of capability `simulation-engine-embedding` in increasing point index, after the base deck has been prepared. On Linux and macOS, up to `N` points SHALL run at once, one per worker process, with `N` from Requirement "Multiple simulations and corner parallelism" of that capability. On iPadOS the points SHALL run one at a time in the in-process engine, and Requirement "In-process execution on iPadOS" applies to every point; the autosave before deck loading SHALL be done once per run set, before the first point, and again only if a cell is modified while the run set is active.

The result of every point SHALL NOT depend on the worker count, on which worker ran it, or on the order in which points finish. Each worker SHALL load each point's deck into a freshly reset circuit (`remcirc` and `destroy all` after the previous point, as the engine capability requires).

The run set state SHALL be `queued`, `running`, `paused`, `completed`, `failed` or `cancelled`. It is `completed` when every point reached a final state, even if some points `failed`; it is `failed` only when the base deck could not be prepared. Progress SHALL be `(points in a final state + Σ progress of running points) / total points`, shown at most 10 times per second.

A run set SHALL be cancellable. Cancelling SHALL cancel queued points with no result, cancel running points as capability `simulation-analyses-and-results` specifies (Requirement "Pause, resume and cancel"), keep all completed points, and mark the run set `cancelled`. Pausing a run set SHALL stop new points from starting and pause running points.

A cancelled or interrupted run set (for example, after the app was closed or the engine became unavailable) SHALL be resumable: resuming SHALL run exactly the points that have no completed result, with their stored decks or seeds, and SHALL give the same results as an uninterrupted run.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Same results with one or four workers
- **GIVEN** a 12-point sweep run on an 8-core Linux machine, once with `sim engine workers 1` and once with `sim engine workers 4`
- **WHEN** the stored vectors of each point are compared
- **THEN** they SHALL be bit-identical between the two runs

#### Scenario: Resume after cancellation on iPad
- **GIVEN** a 40-point run set on iPad that was cancelled after points 0–16 completed and point 17 was running
- **WHEN** the user resumes it
- **THEN** points 17–39 SHALL run, points 0–16 SHALL NOT run again, and the final results SHALL equal those of an uninterrupted run

### Requirement: Run set storage
A run set SHALL be stored in the project in folder `simulations/<testbench>/<run set id>/`:

| Path | Content |
|---|---|
| `runset.json` | definition and state, see below |
| `base.spice` | the prepared base deck, before point changes |
| `points.csv` | one row per point: index, job id, state, one column per dimension, then the measurements of capability `simulation-measurements-and-statistics` |
| `p<index>/` | the job folder of each point that has run, with the files of capability `simulation-analyses-and-results`, Requirement "Result storage"; `result.raw` is present only as the retention setting below allows |

`runset.json` SHALL hold: `id`, `testbench`, `state`, the dimension definitions (including DOE seed and Monte Carlo seed), `total`, `keep`, `deckSha256` of `base.spice`, and the PDK descriptor identity. Each point's deck SHALL be reproducible from `base.spice` and the point's level values, so that "re-run point" gives the same deck text.

Waveform retention SHALL follow the setting `keep`:
- `all`: every point keeps its `p<index>/` folder with `result.raw`
- `failing`: only points that `failed` or failed a specification keep `result.raw`
- `none`: no point keeps `result.raw`

The default SHALL be `all` when the run set has at most 100 points and `failing` otherwise. Every point SHALL keep its `job.json` and `log.txt` whatever the setting. The point with index 0 and any Monte Carlo nominal point SHALL always keep `result.raw`. Measurements SHALL be computed from a point's vectors before they are discarded.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-11.)

#### Scenario: Large run set keeps only failing waveforms
- **GIVEN** a 500-point run set with default settings, in which points 12 and 310 fail a specification
- **WHEN** it completes
- **THEN** `result.raw` SHALL exist for points 0, 12 and 310 only, and `points.csv` SHALL have 500 data rows

### Requirement: Sweep result views
For a completed or partial run set, Hefesto SHALL offer:
- **Family overlay.** Any vector of the same plot from many points shown in one waveform panel (capability `simulation-ui-and-crossprobe`), with the legend entry `<display name> [<dim>=<value>, ...]`. When more than 200 traces would be shown, the viewer SHALL draw them in one color with reduced opacity and show the legend as a single entry `<display name> (<n> points)`.
- **Measurement versus variable.** For a run set with one or more sweep dimensions, a plot of any measurement against one chosen dimension, with one curve per combination of the other dimensions' levels.
- **Table.** The contents of `points.csv`, sortable by any column, where selecting a row opens that point's waveforms if they are kept, or offers "re-run point" if they are not.

These views SHALL be available on every platform, with the input rules of capability `hefesto-input-and-touch`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19, docs/decisions/product.md#P-7)

#### Scenario: Measurement versus temperature
- **GIVEN** a completed run set with dimensions `temp` (5 levels) and `supply` (3 levels) and a measurement `tpd`
- **WHEN** the user plots `tpd` against `temp`
- **THEN** the plot SHALL show 3 curves of 5 points each, labeled `supply=0.9`, `supply=1` and `supply=1.1`

### Requirement: Sweep command options
The `sim` command (capability `simulation-command`) SHALL accept these additional options, with the matching, number, return-value and error rules of that capability:

| Option | Syntax | Effect / result |
|---|---|---|
| `sweep` | `sweep add <var> lin <start> <stop> -points <n>` \| `sweep add <var> lin <start> <stop> -step <s>` \| `sweep add <var> dec\|oct <start> <stop> <m>` \| `sweep add <var> list <v…>` \| `sweep delete <var>` \| `sweep clear` \| `sweep list` | Define sweep dimensions, outermost first in order of `add`. `list` returns `<var> <mode> <count>` per dimension. |
| `doe` | `doe <full\|oat\|lhs\|random> [-points <n>] [-seed <s>] <factor…>`, each factor `<var>:<lo>:<hi>[:log][:<levels>]` \| `doe clear` | Define the DOE block. |
| `runset` | `runset run [-keep all\|failing\|none] [-wait]` \| `runset status [<id>]` \| `runset pause\|resume\|cancel <id>` \| `runset list [<testbench>]` \| `runset points <id>` \| `runset rerun <id> <index>` | Run and manage run sets. `run` returns the run set id; `status` returns `<state> <done>/<total> <progress>`. |

With `-wait`, `runset run` SHALL return when the run set reaches a final state, and SHALL raise `sim: run set <id> <state>` if it is not `completed`. Sweep and DOE definitions SHALL be saved in `testbench.json` (capability `simulation-command`, Requirement "Testbench persistence") and SHALL be undoable as one undo event per command. In headless mode, every option above SHALL work.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-1.)

#### Scenario: Nested sweep from a script
- **WHEN** the script `sim sweep add temp list -40 27 125; sim sweep add supply lin 0.9 1.1 -points 3; sim runset run -wait` is executed
- **THEN** a run set of 9 points SHALL run, and `sim runset status <id>` SHALL return `completed 9/9 1.0`

#### Scenario: Unknown factor syntax
- **WHEN** `sim doe lhs -points 10 param:wn:1` is executed
- **THEN** it SHALL fail with `Usage: sim doe <full|oat|lhs|random> [-points <n>] [-seed <s>] <factor…>` and the testbench SHALL be unchanged
