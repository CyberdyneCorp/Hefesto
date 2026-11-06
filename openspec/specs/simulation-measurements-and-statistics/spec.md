# Simulation Measurements and Statistics

## Purpose
Defines how Hefesto turns simulation results into numbers and verdicts, and how it summarises many runs (decision P-19):
- named measurements on stored vectors, extending the waveform measurements of capability `simulation-analyses-and-results`
- import of ngspice `.meas` statements from decks and schematic code blocks
- measurement expressions
- specifications with limits, margins and pass/fail
- statistics over a run set: descriptive statistics, yield with confidence interval, process capability (Cp, Cpk), histograms and correlation
- result export
- the `sim meas`, `sim spec` and `sim stats` command options

Measurements are computed by Hefesto from the final stored vectors of each job (capability `simulation-analyses-and-results`, Requirement "Final vector retrieval"), not by ngspice, so the same definition gives the same value whether the job ran on Linux, macOS or iPadOS, and whether its waveforms were kept. Run sets come from capabilities `simulation-sweeps`, `simulation-corners` and `simulation-monte-carlo`. Charts are drawn with the rendering stack of capability `hefesto-rendering` and the viewer conventions of capability `simulation-ui-and-crossprobe`.

ngspice reference: SourceForge `ngspice` master @ `7aa47164` (after ngspice-47). Open questions: `docs/decisions/open-questions/SIMADV.md` (Q-SIMADV-n).

## Requirements

### Requirement: Measurement definitions
A testbench SHALL hold an ordered list of named measurements. A name SHALL match `[A-Za-z_][A-Za-z0-9_]*`, be unique case-insensitively within the testbench, and not be a function name of Requirement "Measurement expressions". Each measurement selects a plot by analysis kind (`op`, `dc`, `ac`, `tran`, `noise`) and, when a job has several plots of that kind, by 1-based occurrence (`tran#2`); the default is the first. Windowed kinds take an optional window `from a to b` on the plot scale, default the whole scale.

The kinds SHALL be those of capability `simulation-analyses-and-results`, Requirement "Waveform measurements" (`value_at`, `min`, `max`, `avg`, `rms`, `cross`, `delay`, `risetime`), with the same definitions, plus:

| Kind | Definition (piecewise-linear `y(x)` unless stated) |
|---|---|
| `op <vector>` | the single value of `<vector>` in an `op` plot |
| `min_at`, `max_at` | the first `x` where the minimum or maximum sample in the window occurs |
| `pp` | `max − min` over the window, including the interpolated values at `a` and `b` |
| `integ` | `∫ y dx` from `a` to `b` by the trapezoidal rule |
| `deriv_at x0` | slope of the linear segment containing `x0`; at a sample, the segment to its right, except at the last sample, where the segment to its left is used |
| `falltime lo hi` | `cross(lo + 0.1(hi−lo), 1, fall)` minus `cross(lo + 0.9(hi−lo), 1, fall)`; fractions configurable |
| `when level dir k\|last [td t]` | the `x` of the k-th (or last) crossing of `level` in direction `dir`, ignoring crossings at `x < t` |
| `delay ... [td t]` | as in `simulation-analyses-and-results`, with `td` applied to the trigger crossing |

On complex AC vectors `H(f)`, with `dB(f) = 20·log10|H(f)|` and phase `φ(f)` in degrees unwrapped from the first frequency (successive differences brought into (−180, 180]), and with interpolation linear in `log10 f` for `dec`/`oct` sweeps and in `f` for `lin` sweeps:

| Kind | Definition |
|---|---|
| `dcgain` | `dB(f_first)` |
| `ugbw` | the first `f` where `dB` crosses 0 falling |
| `bw3db` | the first `f` where `dB` crosses `dB(f_first) − 3` falling |
| `phase_margin` | `180 + φ(ugbw)` |
| `gain_margin` | `−dB(f180)`, where `f180` is the first `f` where `φ` crosses −180 falling |

A measurement with no result SHALL have "no value" with a reason, for example `no crossing of 0 (fall, k=1)` or `vector v(x) not in plot tran`, and SHALL NOT stop the other measurements.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-10.)

#### Scenario: Unity-gain bandwidth and phase margin
- **GIVEN** an `ac` plot swept `dec` with `H(f) = 1000 / (1 + j f/1k)` sampled at 100 points per decade from 1 Hz to 100 MHz
- **WHEN** `dcgain`, `ugbw` and `phase_margin` are measured
- **THEN** the results SHALL be 60 dB within 0.001, 1 MHz within 0.5 %, and 90° within 0.1°

#### Scenario: Peak to peak in a window
- **GIVEN** `x` = [0, 1, 2, 3] and `y` = [0, 4, −2, 0]
- **WHEN** `pp from 0.5 to 2.5` is measured
- **THEN** the result SHALL be 6 (maximum 4 at x = 1, minimum −2 at x = 2)

### Requirement: Import of ngspice measure statements
Hefesto SHALL turn `.meas`/`.measure` statements found at top level of a testbench deck, and `meas`/`measure` commands found in `.control` blocks of schematic testbenches (capability `schematic-simulation-integration`), into Hefesto measurements, with this mapping:

| ngspice form | Hefesto kind |
|---|---|
| `TRIG ... TARG ...` (with `VAL=`, `RISE=`/`FALL=`/`CROSS=`, `TD=`) | `delay` |
| `FIND <v> AT=<x>` | `value_at` |
| `FIND <v> WHEN <w>=<level> ...` | `value_at` at the `when` crossing |
| `WHEN <v>=<level> ...` | `when` |
| `MIN`, `MAX`, `MIN_AT`, `MAX_AT`, `AVG`, `RMS`, `PP`, `INTEG` with `FROM=`/`TO=` | the kind of the same name |
| `DERIV <v> AT=<x>` | `deriv_at` |
| `PARAM='<expr>'` | an expression measurement |

The analysis word (`tran`, `ac`, `dc`, `op`, `noise`) selects the plot. `CROSS=LAST`, `RISE=LAST` and `FALL=LAST` map to `last`. Other forms (`ERR`, `ERR1`–`ERR3`, `PHASE_MARGIN`, `GAIN_MARGIN` written in ngspice syntax, `DERIV ... WHEN`) SHALL be reported as `Measure <name>: form not supported; computed by ngspice only` and left in the deck. Imported measurements SHALL be marked `imported` and SHALL be re-imported whenever the deck is regenerated. The statements SHALL stay in the deck, so ngspice also prints its own values in the job log; Hefesto's value is authoritative.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (ngspice ref: src/frontend/com_measure2.c:55-62, src/frontend/com_measure2.c:215-262) (See Q-SIMADV-10.)

#### Scenario: IHP testbench measure
- **GIVEN** a schematic `.control` block containing `meas ac gain_passband_dB max gain_dB` and `let gain_dB = vdb(inv_out)`
- **WHEN** the deck is prepared
- **THEN** a measurement `gain_passband_dB` of kind `max` on the `ac` plot SHALL exist, applied to the vector `gain_db` that the `let` command adds to that plot (capability `simulation-analyses-and-results`, Requirement "Final vector retrieval")

#### Scenario: Unsupported form
- **GIVEN** a deck line `.meas tran e1 ERR v(a) v(b)`
- **WHEN** the deck is prepared
- **THEN** the warning `Measure e1: form not supported; computed by ngspice only` SHALL be logged and no Hefesto measurement `e1` SHALL exist

### Requirement: Measurement expressions
A measurement MAY be an expression over other measurements, the point's dimension values (`temp`, `supply`, parameter names, `mc` sample index) and numbers, with this grammar:

```ebnf
expr    = term , { ( "+" | "-" ) , term } ;
term    = unary , { ( "*" | "/" ) , unary } ;
unary   = ( "+" | "-" ) , unary | power ;
power   = primary , [ "^" , unary ] ;                  (* right-associative; -2^2 = -4 *)
primary = number | name | func , "(" , expr , { "," , expr } , ")" | "(" , expr , ")" ;
func    = "abs" | "sqrt" | "exp" | "ln" | "log10" | "db" | "min" | "max" | "pow" ;
number  = (* as capability simulation-command, Requirement "Numeric argument syntax" *) ;
name    = letter_or_underscore , { letter_or_underscore | digit } ;
```

`db(x)` = `20·log10|x|`. Names SHALL be resolved case-insensitively: measurements first, then dimension values. An unknown name SHALL be reported when the expression is defined: `Measure <m>: unknown name <n>`. A cycle between expression measurements SHALL be rejected with `Measure <m>: circular reference`. Evaluation SHALL use IEEE-754 binary64; division by zero, `sqrt` of a negative number, `ln` or `log10` of a number `≤ 0`, an operand with no value, or a non-finite result SHALL give "no value" with the reason.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Gain-bandwidth product
- **GIVEN** measurements `a0` = 1000 and `fc` = 1meg
- **WHEN** the expression measurement `gbw = a0 * fc` is evaluated
- **THEN** `gbw` SHALL be 1e9

#### Scenario: Power precedence
- **WHEN** the expression `-2^2 + 10/4` is evaluated
- **THEN** the result SHALL be −1.5

### Requirement: Specifications
A testbench SHALL hold specifications, each naming one measurement and one limit:

| Limit | Pass when | Normalised margin `μ` |
|---|---|---|
| `min lo` | `v ≥ lo` | `(v − lo)/|lo|` |
| `max hi` | `v ≤ hi` | `(hi − v)/|hi|` |
| `range lo hi` (`lo < hi`) | `lo ≤ v ≤ hi` | `min(v − lo, hi − v) / ((hi − lo)/2)` |
| `target t tol` (`tol > 0`) | `t − tol ≤ v ≤ t + tol` | as `range t−tol t+tol` |

For a one-sided limit equal to 0, the margin SHALL be divided by the specification's `scale` if given, and SHALL otherwise be "undefined" (shown as `—`; the cell cannot be `marginal`). The specification result SHALL be `pass`, `fail` or `no value` (the measurement has no value); `no value` counts as a failure in every verdict. A specification MAY carry the corner filter and per-corner overrides of capability `simulation-corners`, Requirement "Pass and fail per corner". The point verdict SHALL be `pass` when every applicable specification passes.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Two-sided margin
- **GIVEN** a specification `range 1.1 1.3` on measurement `vout`
- **WHEN** `vout` = 1.25
- **THEN** the result SHALL be `pass` with margin 0.5

#### Scenario: Missing value fails
- **GIVEN** a specification `max 100p` on measurement `tpd`, and a point where the output never switches
- **WHEN** the point is evaluated
- **THEN** the specification SHALL be `no value` with reason `no crossing of 0.9 (rise, k=1)`, and the point verdict SHALL be `fail`

### Requirement: Descriptive statistics
For each measurement over a set of points (a whole run set, or one group, see below), Hefesto SHALL compute over the `n` points that have a value:
- `n`, and `excluded` = the number of points without a value (including failed jobs)
- mean `x̄ = (1/n) Σ x_i`
- sample standard deviation `s = sqrt( Σ (x_i − x̄)² / (n − 1) )` for `n ≥ 2`, undefined for `n < 2`
- `min`, `max`
- percentiles `P_q` for `q ∈ [0, 1]` by linear interpolation between order statistics: with sorted values `x_(1) ≤ … ≤ x_(n)`, `h = (n − 1)·q + 1`, `P_q = x_(⌊h⌋) + (h − ⌊h⌋)·(x_(⌊h⌋+1) − x_(⌊h⌋))` (for `h = n`, `P_q = x_(n)`); the median is `P_0.5`

Sums SHALL be computed in point-index order (Kahan compensated summation), so results do not depend on the order in which points finished. When a run set has a corner dimension and other dimensions, statistics SHALL be computed per corner (group = corner name) and for the whole run set (group `all`). When a Monte Carlo run has a nominal sample, the nominal sample SHALL be excluded from statistics and shown separately.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Five values
- **GIVEN** a measurement with values 1, 2, 3, 4 and 5
- **WHEN** statistics are computed
- **THEN** mean SHALL be 3, `s` SHALL be 1.5811388300841898 within 1e-15 relative, and `P_0.25`, median and `P_0.75` SHALL be 2, 3 and 4

### Requirement: Yield
The yield of a run set, or of a group, SHALL be `Y = k / m`, where `m` is the number of points counted (Requirement "Failed samples and early stopping" of capability `simulation-monte-carlo` decides whether failed samples count) and `k` is the number of counted points whose verdict is `pass`. Hefesto SHALL report the 95 % Wilson score interval:

```
z = 1.959963984540054
c = (Y + z²/(2m)) / (1 + z²/m)
h = z · sqrt( Y(1 − Y)/m + z²/(4m²) ) / (1 + z²/m)
interval = [max(0, c − h), min(1, c + h)]
```

Yield SHALL also be reported per specification (the fraction of counted points passing that specification). A run set with `m = 0` SHALL report yield as undefined.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: 95 of 100 samples pass
- **GIVEN** a Monte Carlo run of 100 counted samples, of which 95 pass
- **WHEN** yield is computed
- **THEN** yield SHALL be 0.95 with interval [0.8882, 0.9785] to 4 decimal places

### Requirement: Process capability
For a measurement with a specification and `n ≥ 2` values, Hefesto SHALL compute:
- two-sided limits `LSL`, `USL` (from `range` or `target`): `Cp = (USL − LSL)/(6s)` and `Cpk = min(USL − x̄, x̄ − LSL)/(3s)`
- one-sided `min LSL`: `Cpk = (x̄ − LSL)/(3s)`, Cp undefined
- one-sided `max USL`: `Cpk = (USL − x̄)/(3s)`, Cp undefined

When `s = 0`, Cp and Cpk SHALL be reported as `+∞` if every value passes and `−∞` otherwise. The capability values SHALL be shown with the note `assumes a normal distribution` whenever the measurement's values fail a normality screen: `|skewness| > 1` or `|excess kurtosis| > 2`, using the sample skewness `g1 = m3/m2^(3/2)` and excess kurtosis `g2 = m4/m2² − 3` with central moments `m_k = (1/n) Σ (x_i − x̄)^k`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Centred process
- **GIVEN** values 1, 2, 3, 4, 5 and a specification `range 0 6`
- **WHEN** capability is computed
- **THEN** Cp and Cpk SHALL both be 0.6324555320336759 within 1e-15 relative

#### Scenario: One-sided upper limit
- **GIVEN** values 1, 2, 3, 4, 5 and a specification `max 4.5`
- **WHEN** capability is computed
- **THEN** Cpk SHALL be 0.31622776601683794 within 1e-15 relative and Cp SHALL be undefined

### Requirement: Histograms
For any measurement over a run set or group, Hefesto SHALL draw a histogram:
- default bin count `k = ⌈log2(n) + 1⌉` (Sturges), user-selectable from 1 to 1 000
- bin width `w = (max − min)/k`; bin `j` covers `[min + j·w, min + (j+1)·w)`, the last bin also includes `max`; value `x` goes to bin `min(k − 1, ⌊(x − min)/w⌋)`
- when every value is equal, one bin holding every value
- vertical lines at the specification limits, a marker at the nominal value if there is one, and optionally the normal density with mean `x̄` and deviation `s`, scaled to the counts (`n·w·φ`)
- the summary: `n`, excluded, `x̄`, `s`, min, max, yield and Cpk

Histograms of several groups (for example, per corner) MAY be overlaid with shared bin edges computed over the union of their values. Bars SHALL be distinguishable without colour alone (pattern or outline). Histograms SHALL be available on every platform.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Bins for 1000 samples
- **GIVEN** 1 000 values between 0.40 and 0.62
- **WHEN** a histogram is drawn with default settings
- **THEN** it SHALL have 11 bins of width 0.02, and a value of exactly 0.62 SHALL fall in the last bin

### Requirement: Correlation
For any two numeric columns of a run set (measurements, dimension values, or the Monte Carlo seed index), Hefesto SHALL compute over the points where both have a value:
- Pearson's `r = Σ (x_i − x̄)(y_i − ȳ) / sqrt( Σ (x_i − x̄)² · Σ (y_i − ȳ)² )`
- Spearman's `ρ`: Pearson's `r` of the ranks, ties receiving the average of their ranks

Either is undefined when fewer than 3 pairs exist or when one column is constant. Hefesto SHALL show a scatter plot for one pair and a correlation matrix for a chosen set of columns, with each cell selectable to open its scatter plot.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Pearson and Spearman
- **GIVEN** pairs (1, 2), (2, 4), (3, 5), (4, 9)
- **WHEN** correlation is computed
- **THEN** `r` SHALL be 0.9647638212377322 within 1e-15 relative and `ρ` SHALL be 1

### Requirement: Result export
Hefesto SHALL export run-set results in these formats, with separator `,`, line ending LF, UTF-8, numbers in C `%.17g` format, empty cells for "no value", and RFC 4180 quoting only for text that contains `,`, `"` or a line break:
- **Points table** (`points.csv`, capability `simulation-sweeps`): columns `index`, `job`, `state`, one column per dimension in dimension order, one column per measurement in definition order, one column `<spec>:result` per specification, then `verdict`.
- **Statistics table** (`statistics.csv`): one row per (group, measurement), groups in level order with `all` last, and columns `group`, `measurement`, `n`, `excluded`, `mean`, `std`, `min`, `p25`, `median`, `p75`, `max`, `lsl`, `usl`, `cp`, `cpk`, `yield`, `yield_lo`, `yield_hi`. Infinite values SHALL be written `inf` and `-inf`.
- **Corner matrix** (capability `simulation-corners`): the matrix rows in display order.
- **JSON**: one object with keys `runSet` (the `runset.json` content), `points` and `statistics`, holding the same values as the tables.

Exports SHALL be possible from the GUI, from scripts and headless, and on iPadOS to any location the user grants. Exporting SHALL NOT recompute any simulation.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Statistics row
- **GIVEN** a run set without corners whose measurement `vout` has values 1, 2, 3, 4, 5, one excluded point, and specification `range 0 6`
- **WHEN** `statistics.csv` is exported
- **THEN** its header SHALL be `group,measurement,n,excluded,mean,std,min,p25,median,p75,max,lsl,usl,cp,cpk,yield,yield_lo,yield_hi`, and the row for `vout` SHALL start with `all,vout,5,1,3,1.5811388300841898,1,2,3,4,5,0,6,`

### Requirement: Measurement and statistics command options
The `sim` command (capability `simulation-command`) SHALL accept these options, which replace nothing in that capability (`sim measure` remains the one-off measurement on a stored job):

| Option | Syntax | Effect / result |
|---|---|---|
| `meas` | `meas add <name> <plot> <kind> <vector> <args…> [-from <a>] [-to <b>]` \| `meas expr <name> <expression>` \| `meas delete <name>` \| `meas list` \| `meas eval <job>` | Define measurements; `eval` returns `<name> <value or "novalue: reason">` per measurement |
| `spec` | `spec add <measurement> min <lo>\|max <hi>\|range <lo> <hi>\|target <t> <tol> [-scale <x>] [-corners <glob…>]` \| `spec override <measurement> <glob> <limit…>` \| `spec delete <measurement>` \| `spec list` | Define specifications |
| `stats` | `stats <run set id> [<measurement>] [-group <g>]` \| `stats yield <run set id>` \| `stats corr <run set id> <col1> <col2> [-spearman]` \| `stats export <run set id> <file> [-format points\|statistics\|matrix\|json]` | Return statistics as key-value pairs, or write a file |

Definitions SHALL be stored in `testbench.json` and SHALL be undoable as one undo step per command (capability `layout-undo`, requirement "Native undo events for Hefesto compound operations"). Adding or changing measurements or specifications SHALL NOT re-run simulations: Hefesto SHALL recompute the affected values from stored vectors where waveforms were kept, and SHALL mark points whose waveforms were not kept as `stale` for the changed measurement until they are re-run.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-1, Q-SIMADV-11.)

#### Scenario: Add a spec after a run
- **GIVEN** a completed 20-point sweep with all waveforms kept
- **WHEN** `sim meas add tpd tran delay ...` and `sim spec add tpd max 100p` are executed
- **THEN** `tpd` and its specification results SHALL be computed for all 20 points without running ngspice

#### Scenario: Yield from a script
- **GIVEN** a completed Monte Carlo run set `rs` of 100 counted samples with 95 passes
- **WHEN** a script evaluates `sim stats yield rs`
- **THEN** it SHALL receive the list `yield 0.94999999999999996 lo 0.88824953076808078 hi 0.97845632084563194`
