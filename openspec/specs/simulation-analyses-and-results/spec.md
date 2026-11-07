# Simulation Analyses and Results

## Purpose
Defines which analyses Hefesto runs through the embedded ngspice engine (capability `simulation-engine-embedding`), and the job lifecycle around them: states, progress reporting, cancellation, and how vectors are retrieved. It also defines how results are stored in a project, the export formats (ngspice raw binary and ASCII, CSV), how external raw files are imported, the memory budget, and the built-in waveform measurements. Deck composition is in capability `simulation-netlist-preparation`.

ngspice reference: SourceForge `ngspice` master @ `7aa47164` (after ngspice-47).

## Requirements

### Requirement: Supported analyses
A testbench SHALL support the analyses below. Each SHALL be emitted as exactly the card shown, with numbers in C `%.12g` format and optional fields omitted from the end when not given:

| Analysis | Parameters | Card |
|---|---|---|
| op | — | `.op` |
| dc | source, start, stop, step; optional second source, start2, stop2, step2 | `.dc <src> <start> <stop> <step> [<src2> <start2> <stop2> <step2>]` |
| ac | sweep `dec`\|`oct`\|`lin`, points, fstart, fstop | `.ac <sweep> <n> <fstart> <fstop>` |
| tran | tstep, tstop, optional tstart, tmax, uic | `.tran <tstep> <tstop> [<tstart> [<tmax>]] [uic]` |
| noise | output `v(<n>[,<ref>])`, input source, sweep, points, fstart, fstop | `.noise v(<n>[,<ref>]) <src> <sweep> <n> <fstart> <fstop>` |
| tf | output, input source | `.tf <out> <src>` |

The following analyses SHALL be accepted only inside raw decks or as testbench extra lines, and passed through unchanged: `.pz`, `.sp`, `.pss`, `.disto`, `.sens`.

A testbench MAY hold several analyses. They SHALL run in insertion order within one job, and each SHALL produce its own plot.

Validation SHALL reject:
- `tstep <= 0`, `tstop <= tstart` or `tmax <= 0`
- `n < 1`
- `fstart <= 0` for `dec`/`oct`, or `fstop < fstart`
- `step = 0` in `dc`
- a `dc`, `noise` or `tf` source name that does not exist in the testbench or DUT

Each error SHALL name the analysis and the parameter.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/frontend/commands.c:420-520)

#### Scenario: Transient card
- **GIVEN** a tran analysis with tstep 10 ps, tstop 20 ns and nothing else
- **WHEN** the deck is composed
- **THEN** the card SHALL be `.tran 1e-11 2e-08`

#### Scenario: Invalid AC sweep
- **GIVEN** an ac analysis `dec 10 0 1e9`
- **WHEN** the testbench is validated
- **THEN** the error `ac: fstart must be > 0 for dec sweep` SHALL be reported

### Requirement: Job lifecycle
Every simulation run SHALL be a job with a unique id `<YYYYMMDD>-<HHMMSS>-<testbench>-<corner>`, using local time. If two jobs would get the same id, the second SHALL get the suffix `-2`, the third `-3`, and so on. A job that is a point of a run set SHALL instead take the id `<run set id>-p<index>` defined in capability `simulation-sweeps`, Requirement "Run sets".

A job SHALL pass through these states in this order: `queued` → `preparing` (netlisting, resolution, validation) → `loading` (deck sent to ngspice) → `running` → one final state. The final state SHALL be one of:
- `completed`
- `failed` (with a diagnostic)
- `cancelled` (with a partial result)

A job in `running` MAY move to `paused` and back. A job that fails in `preparing` SHALL never reach the engine. A job SHALL be `completed` only if every analysis in it produced a plot and ngspice reported no error-stream line that starts with `Error`. An error-stream line starting with `Warning` SHALL be kept in the job log and SHALL NOT fail the job.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Convergence failure
- **GIVEN** a deck whose transient analysis ends with `Error: Timestep too small`
- **WHEN** the job ends
- **THEN** its state SHALL be `failed`, its diagnostic SHALL include that line, and any data computed before the error SHALL be kept as a partial result

### Requirement: Progress reporting
Hefesto SHALL derive the progress of a job from the status strings delivered by ngspice:

| Status string | Progress |
|---|---|
| `<analysis>: <p>%` with `<p>` a decimal number | `p / 100` for that analysis |
| `<analysis>` alone | 0 for that analysis |
| `shooting: <n>` | indeterminate; show iteration `n` |
| `--ready--` | 1.0 for the current analysis |
| anything else | unchanged |

For `tran`, when a streamed data point carries scale value `t`, progress SHALL also be `max(current, (t − tstart) / (tstop − tstart))`. Shown progress SHALL never go down within one analysis. Overall job progress SHALL be `(completed analyses + current analysis progress) / total analyses`. The UI SHALL update progress at most 10 times per second.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/sharedspice.c:1955-2110)

#### Scenario: Parse transient status
- **GIVEN** a job with one tran analysis
- **WHEN** the status string `tran: 34.5%` arrives
- **THEN** the job progress SHALL be 0.345

#### Scenario: Two analyses
- **GIVEN** a job with `.op` and then `.tran`
- **WHEN** op has completed and the status string `tran: 50.0%` arrives
- **THEN** the job progress SHALL be 0.75

### Requirement: Pause, resume and cancel
A `running` job SHALL be pausable and resumable, and any non-final job SHALL be cancellable:
- **Pause.** A running job SHALL pause through `bg_halt`, with state `paused`.
- **Resume.** A paused job SHALL continue through `bg_resume`, and the analysis SHALL continue from where it stopped.
- **Cancel while queued or preparing.** The job SHALL become `cancelled` with no result.
- **Cancel while loading, running or paused.** Cancellation SHALL follow capability `simulation-engine-embedding`, Requirement "Cancellation and fault states". The vectors computed so far SHALL be kept as a partial result, marked `partial`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/sharedspice.c:640-730)

#### Scenario: Pause and resume
- **GIVEN** a tran analysis from 0 to 100 ns that is paused at 40 ns
- **WHEN** it is resumed and completes
- **THEN** the final `time` vector SHALL start at 0, end at 100 ns and be strictly increasing

### Requirement: Final vector retrieval
When an analysis completes, or a job ends with a partial result, the engine adapter SHALL copy out of ngspice:
- every plot that the job created, found by comparing `ngSpice_AllPlots` before and after the job
- every vector of each such plot (`ngSpice_AllVecs(plot)` and then `ngGet_Vec_Info`)
- for each vector: its name, type (time, frequency, voltage, current, or other), real or complex flag, and data as IEEE-754 float64

The plot's scale vector SHALL be identified and stored first. Stored results SHALL come only from these final vectors. Points streamed through `SendData` are for live display only. Vector names SHALL be stored as ngspice reports them. The display names `v(<node>)` for node voltages and `i(<src>)` for `<src>#branch` currents SHALL be derived as ngspice derives them when writing raw files.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/include/ngspice/sharedspice.h:150-160, src/sharedspice.c:1340-1355, src/frontend/rawfile.c:160-200)

#### Scenario: Branch current naming
- **GIVEN** a tran plot that contains ngspice vector `vdd#branch` of type current
- **WHEN** results are stored
- **THEN** the stored vector SHALL have raw name `vdd#branch` and display name `i(vdd)`

### Requirement: Saved vectors and memory budget
A testbench SHALL select saved vectors with one of these modes:

| Mode | Deck effect | Vectors |
|---|---|---|
| `default` | `.save` listing every testbench-level node voltage, every DUT internal node that has a probe (capability `simulation-ui-and-crossprobe`), and `i(<src>)` for every testbench voltage source | as listed |
| `all` | no `.save` line | all node voltages, including subcircuit-internal ones, and all source branch currents |
| `all+currents` | no `.save` line, plus `.options savecurrents` | as `all`, plus device terminal currents |
| `list` | `.save` with the user's list | as listed |

The default mode SHALL be `default`. Each job SHALL have a result memory budget `B`: by default 512 MiB on iPadOS and 4 GiB on Linux and macOS, configurable. The size estimate is `points × Σ(8 × (2 if complex else 1))` over all saved vectors. When the estimate passes `B` while data streams in, the job SHALL be cancelled with the diagnostic `result memory budget exceeded (<estimate> > <B>)`, and its partial result SHALL be kept.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-16.)

#### Scenario: Budget exceeded on iPad
- **GIVEN** an iPad job saving 1000 real vectors, with the default budget of 512 MiB
- **WHEN** the streamed point count passes 67 109 (1000 × 8 B × 67 109 > 536 870 912 B)
- **THEN** the job SHALL be cancelled with `result memory budget exceeded`

### Requirement: Result storage
Each job that reaches a final state SHALL be stored in the project, in folder `simulations/<testbench>/<job-id>/`, with these files. A run-set point job is stored instead in `simulations/<testbench>/<run set id>/p<index>/`, and its `result.raw` is kept only as the retention setting of capability `simulation-sweeps`, Requirement "Run set storage", allows:

| File | Content |
|---|---|
| `deck.spice` | the exact deck text sent to ngspice, followed by a newline |
| `result.raw` | all plots of the job, in order, in ngspice binary raw format (Requirement "Raw file format") |
| `log.txt` | every output-stream and error-stream line, in arrival order; error lines prefixed `E ` and others `O ` |
| `job.json` | metadata, see below |

`job.json` SHALL be a UTF-8 JSON object with these keys:
- `id`, `testbench`, `corner`, `state`, `partial` (bool)
- `deckSha256`
- `pdk` (name and descriptor version)
- `engineVersion`, `hefestoVersion`
- `platform` (`linux-x86_64`, `linux-aarch64`, `macos-arm64` or `ipados-arm64`; there is no `macos-x86_64`, decision D-12)
- `started` and `finished` (ISO 8601 with UTC offset)
- `plots`: an array of `{name, type, title, scale, vectors: [{name, displayName, type, complex}]}`
- `netMap`: the net-name map of capability `simulation-netlist-preparation`, or `null`

If the project folder is not writable (for example, a read-only iPad location), results SHALL be stored in the app's own storage, and the user SHALL be told where.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Stored job files
- **GIVEN** job `20261105-143000-inv_tran-tt_27_1v8` completed
- **WHEN** the project folder is listed
- **THEN** `simulations/inv_tran/20261105-143000-inv_tran-tt_27_1v8/` SHALL contain `deck.spice`, `result.raw`, `log.txt` and `job.json`

### Requirement: Raw file format
Hefesto SHALL write and read raw files in the ngspice raw format, so that ngspice's `load` command and tools such as xschem and gaw can read them. Each plot is written as:

```
rawfile   = plot { plot } ;
plot      = "Title: " text NL
            "Date: " text NL
            [ "Command: " text NL ]
            "Plotname: " text NL
            "Flags: " ( "real" | "complex" ) [ " unpadded" ] NL
            "No. Variables: " int NL
            "No. Points: " int NL
            [ "Dimensions: " dims NL ]
            { ( "Command: " | "Option: " ) text NL }
            "Variables:" NL
            var_line { var_line }
            ( "Binary:" NL binary_data | "Values:" NL ascii_data ) ;
var_line  = TAB int TAB name TAB type { " " attr } NL ;   (* index from 0; scale first *)
type      = "time" | "frequency" | "voltage" | "current" | other_type_name ;
binary_data = { point } ;   (* per point: for each variable in order, one float64 (real plot)
                               or two float64 re, im (complex plot); little-endian *)
ascii_data  = { " " int { TAB number [ "," number ] NL } NL } ;
```

Rules for writing:
- Hefesto SHALL write `Title:` with the testbench title line text without its leading `* `, and `Date:` in the C `asctime` layout without the trailing newline (for example `Thu Nov  5 14:30:00 2026`).
- It SHALL write one `Command: hefesto-<hefestoVersion> ngspice-<engineVersion>` line.
- Flags SHALL be `real` if every vector of the plot is real, otherwise `complex`. In a complex plot, real vectors SHALL be written with imaginary part 0.0.
- Variable names SHALL be the display names: `v(<node>)`, `i(<src>)`, others as reported.
- ASCII values SHALL use C `%.15e`.

Rules for reading:
- Hefesto SHALL accept the header lines in any order up to `Variables:`, and ignore header lines it does not know.
- It SHALL accept both `Binary:` and `Values:`, padded and `unpadded` files, and files holding several plots.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (ngspice ref: src/frontend/rawfile.c:31, src/frontend/rawfile.c:60-300, src/frontend/typesdef.c:40-43) (See Q-SIM-17.)

#### Scenario: ASCII raw of a two-point op-like plot
- **GIVEN** a real plot `dc1` with scale `v(in)` = [0, 1] and `v(out)` = [0, 0.5]
- **WHEN** it is exported as ASCII raw
- **THEN** the data section SHALL be `Values:`, then ` 0\t0.000000000000000e+00`, `\t0.000000000000000e+00`, an empty line, ` 1\t1.000000000000000e+00`, `\t5.000000000000000e-01`, and an empty line

#### Scenario: Round trip through ngspice
- **GIVEN** a `result.raw` written by Hefesto
- **WHEN** a reference ngspice executable runs `load result.raw` followed by `print v(out)`
- **THEN** the printed values SHALL equal Hefesto's stored values (verify against ngspice)

### Requirement: CSV export
Hefesto SHALL export a chosen plot, or a subset of its vectors, as CSV in this format:
- separator `,` and line ending LF
- first row: the header, which is the scale display name followed by each exported vector's display name, quoted per RFC 4180 only when the name contains `,` or `"`
- then one row per point
- numbers in C `%.17g` format

A complex vector SHALL become two columns:
- in mode `reim` (the default): `re(<name>)` and `im(<name>)`
- in mode `dbphase`: `db(<name>)` = 20·log10|x| and `ph(<name>)` = atan2(im, re) in degrees, in the range (−180, 180]

A complex scale (frequency) SHALL be written as its real part. Vectors shorter than the scale SHALL leave empty cells.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Transient CSV
- **GIVEN** a tran plot with `time` = [0, 1e-9] and `v(out)` = [0, 0.632120558828558]
- **WHEN** it is exported to CSV
- **THEN** the file SHALL be `time,v(out)`, `0,0`, `1.0000000000000001e-09,0.632120558828558` (one line each)

#### Scenario: AC dB/phase
- **GIVEN** an ac value `v(out)` = −1 + 0j at 1 kHz
- **WHEN** it is exported in mode `dbphase`
- **THEN** that row SHALL hold `db(v(out))` = 0 and `ph(v(out))` = 180

### Requirement: Raw file import
Hefesto SHALL import raw files produced by any ngspice version since 30, including files from Magic/xschem flows, as an "imported" job. The job has state `completed`, no deck, and `netMap` null. Imported plots SHALL be viewable and exportable exactly as simulated ones. If the import is linked to a layout cell, probing SHALL fall back to matching vector names case-insensitively (capability `simulation-ui-and-crossprobe`). A malformed file SHALL be rejected with the line number and expected token, for example `result.raw:7: expected "No. Points:"`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Truncated binary
- **GIVEN** a binary raw file declaring 100 points but holding data for 60
- **WHEN** it is imported
- **THEN** the import SHALL fail with `result.raw: binary data ends after point 60 of 100`

### Requirement: Waveform measurements
Hefesto SHALL compute these measurements on stored real vectors `y(x)`, where `x` is the plot scale. Between samples, `y` is taken as piecewise linear:

| Measurement | Definition |
|---|---|
| `value_at x0` | linear interpolation of `y` at `x0`; an error if `x0` is outside `[x_first, x_last]` |
| `min`, `max` | extreme sample value and the first `x` where it occurs |
| `avg from a to b` | `(1/(b−a)) ∫ y dx` by the trapezoidal rule, interpolating at `a` and `b`; default `a`, `b` = the scale range |
| `rms from a to b` | `sqrt((1/(b−a)) ∫ y² dx)`, with `y²` integrated exactly over each linear segment |
| `cross level k dir` | `x` of the k-th crossing (k ≥ 1) of `level` in direction `rise`, `fall` or `either`, by linear interpolation. A sample exactly equal to `level` counts once, in the direction of the next differing sample. |
| `delay trig targ` | `cross(targ) − cross(trig)`, each given as `(vector, level, k, dir)` |
| `risetime lo hi` | `cross(lo + 0.1(hi−lo), 1, rise)` to `cross(lo + 0.9(hi−lo), 1, rise)` measured as the difference; the 10 % and 90 % fractions are configurable |

A measurement with no result (no crossing) SHALL return the error `no crossing of <level> (<dir>, k=<k>)`. On complex vectors, measurements SHALL apply to the magnitude unless `re`, `im`, `db` or `ph` is given.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Triangle wave
- **GIVEN** `x` = [0, 1, 2] and `y` = [0, 2, 0]
- **WHEN** `cross 1 1 rise`, `cross 1 1 fall`, `avg` and `max` are measured
- **THEN** the results SHALL be 0.5, 1.5, 1.0 and 2 (at x = 1)

#### Scenario: RC rise time
- **GIVEN** `y(t) = 1 − e^(−t/τ)` with τ = 1 ns, sampled every 1 ps from 0 to 10 ns
- **WHEN** `risetime 0 1` is measured
- **THEN** the result SHALL be τ·ln 9 ≈ 2.1972 ns within 1 ps
