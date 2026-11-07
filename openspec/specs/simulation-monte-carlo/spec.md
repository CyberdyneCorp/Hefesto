# Simulation Monte Carlo

## Purpose
Defines Monte Carlo analysis in Hefesto (decision P-19):
- the Monte Carlo modes (process, mismatch, both) and how each maps onto a PDK's own statistical model sections and switches
- how ngspice draws statistical values, which Hefesto relies on
- per-sample seeds and the guarantee that every sample is reproducible on every platform
- the Monte Carlo dimension of a run set: run count, the nominal sample, early stopping
- parallel execution over worker processes on Linux and macOS, and sequential in-process execution on iPadOS
- cancellation, failed samples and resumption
- user-defined statistical parameters
- the `sim mc` command option

Hefesto does not model variation itself: process and mismatch variation come only from the PDK's statistical models (or from parameters the user writes), evaluated by ngspice. Run sets, point decks and storage are defined in capability `simulation-sweeps`. Corners are in capability `simulation-corners`. Statistics over the samples (histograms, mean, sigma, yield, Cpk, correlation) are in capability `simulation-measurements-and-statistics`. The engine, its workers and its iPadOS constraints are in capability `simulation-engine-embedding`.

ngspice reference: SourceForge `ngspice` master @ `7aa47164` (after ngspice-47). `(ngspice ref: ...)` paths are relative to that tree. PDK sources are those listed in capability `simulation-corners`. Open questions: `docs/decisions/open-questions/SIMADV.md` (Q-SIMADV-n).

## Requirements

### Requirement: Statistical functions in ngspice (informative)
The specification SHALL record how ngspice evaluates statistical functions, for reference. Hefesto relies on these mechanics only through the other requirements of this capability:
- **Functions.** With a standard normal draw `g` and a uniform draw `r ∈ [−1, 1)`:
  - `agauss(nom, abs, sigma)` = `nom + (abs/sigma)·g`; it returns `nom` if `abs ≤ 0` or `sigma ≤ 0`
  - `gauss(nom, rel, sigma)` = `nom + nom·(rel/sigma)·g`; it returns `nom` if `rel ≤ 0` or `sigma ≤ 0`
  - `aunif(nom, abs)` = `nom + abs·r`; `unif(nom, rel)` = `nom + nom·rel·r`
  - `limit(nom, abs)` = `nom + abs` if `r > 0`, else `nom − abs`
- **Generator.** Draws come from a combined Tausworthe/LCG generator whose eight state words are seeded from the C library `rand()` after `srand(seed)`. The normal draw uses the polar Box–Muller method and keeps one of the two values per pass.
- **Seeding.** `setseed <n>` and `.option seed=<n>` reseed immediately. `set rndseed=<n>` only sets a variable, which numparam's statistical functions do not read. The shared-library initialisation seeds the generator from the process id.
- **When values are drawn.** A top-level `.param` with a statistical function is drawn once per circuit load, so every reference shares one value (global process variation). A `.param` inside a `.subckt` is drawn once per subcircuit instance (device mismatch). Statistical functions in `B` source lines are replaced by one value each after subcircuit expansion. The variable `stat_param_as_func` restores the behaviour of ngspice 29 to 47, in which such parameters become functions re-evaluated at every reference.
- **Re-drawing.** `reset` and `mc_source` reload the circuit from the stored deck, so every statistical function is drawn again from the current generator state.

Compatibility: informative

(Hefesto decision: docs/decisions/product.md#P-19) (ngspice ref: src/frontend/numparam/xpressn.c:45-85, src/maths/misc/randnumb.c:101-116, src/maths/misc/randnumb.c:220-235, src/maths/misc/randnumb.c:287-330, src/frontend/com_option.c:142-152, src/sharedspice.c:930-936, src/frontend/inpcom.c:8380-8405, src/frontend/inp.c:1030-1042, src/frontend/runcoms2.c:173-185, src/frontend/inp.c:1729-1735) (See Q-SIMADV-6, Q-SIMADV-7.)

#### Scenario: agauss with zero sigma
- **GIVEN** the deck line `.param p = 'agauss(1.0, 0.1, 0)'`
- **WHEN** the circuit is loaded
- **THEN** `p` SHALL be exactly 1.0, whatever the seed

### Requirement: Monte Carlo modes
A Monte Carlo run SHALL use one of three modes:
- `process`: global (die-to-die) process variation only
- `mismatch`: local (device-to-device) variation only
- `both`: process and mismatch together

The PDK descriptor (capability `simulation-netlist-preparation`, Requirement "Simulation model descriptor in the PDK store") SHALL define, for each mode it supports, which section words replace the corner's words and which `.param` assignments are added. The added assignments SHALL be placed directly after the last model line, so that they follow the definitions they override. Hefesto SHALL bundle these mode definitions:

| PDK | `process` | `mismatch` | `both` |
|---|---|---|---|
| sky130A/B | section `mc` | section `<c>_mm` of the selected process corner `<c>` | section `mc`, plus `.param MC_MM_SWITCH=1` |
| gf180mcuC/D | `<mos>` = `statistical`, other variables their `_statistical` word where one exists; `.param sw_stat_global=1` | corner unchanged; `.param sw_stat_mismatch=1` | as `process`, plus `.param sw_stat_mismatch=1` |
| sg13g2 | each variable's statistical word | each variable's `_mismatch` word for the selected corner; every MOSFET, HBT, resistor and capacitor instance SHALL have `mm_ok=1` | not supported |

For sg13g2 in `mismatch` mode, Hefesto SHALL check the DUT netlist and report `Monte Carlo mismatch: instance <name> has mm_ok=0; it will not vary` as a warning for each PDK device instance whose `mm_ok` is absent or 0. A mode that the descriptor does not define SHALL be rejected with `<PDK> does not define Monte Carlo mode <mode>`.

`process` and `both` modes describe variation around the typical process, so they SHALL be combined with a corner dimension only for corners whose descriptor row names a statistical counterpart; otherwise the run set SHALL be rejected with `Monte Carlo mode <mode> is not defined at corner <c>`. `mismatch` mode MAY be combined with any process corner that has a mismatch word.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-3, Q-SIMADV-4, Q-SIMADV-5, Q-SIMADV-8.)

#### Scenario: sky130 mismatch at the slow corner
- **GIVEN** a sky130A testbench with corner `ss` and Monte Carlo mode `mismatch`
- **WHEN** a sample deck is composed
- **THEN** it SHALL contain `.lib "<store>/sky130A/libs.tech/combined/sky130.lib.spice" ss_mm` and no `MC_PR_SWITCH` override

#### Scenario: gf180 process and mismatch
- **GIVEN** a gf180mcuD testbench with Monte Carlo mode `both`
- **WHEN** a sample deck is composed
- **THEN** it SHALL contain `.lib "<store>/gf180mcuD/libs.tech/ngspice/sm141064.ngspice" statistical`, followed after the last model line by `.param sw_stat_global=1` and `.param sw_stat_mismatch=1`

#### Scenario: sg13g2 both modes
- **WHEN** a Monte Carlo run in mode `both` is started for an sg13g2 testbench with the bundled descriptor
- **THEN** it SHALL be rejected with `sg13g2 does not define Monte Carlo mode both`

### Requirement: Monte Carlo dimension
A run set (capability `simulation-sweeps`, Requirement "Run sets") SHALL accept one Monte Carlo dimension, which is always the innermost. Its parameters are:
- mode (Requirement "Monte Carlo modes")
- run count `N`, an integer from 1 to 100 000, default 100
- base seed `B`, an integer from 1 to 2 147 483 647, default 1
- `nominal`, default on

When `nominal` is on, the dimension has `N + 1` levels: level 0 is the nominal sample `nom`, simulated with the corner's ordinary (non-statistical) words and none of the mode's `.param` assignments; levels 1 … N are the samples with sample index `i = level − 1`. When `nominal` is off, levels 0 … N−1 are the samples with `i = level`. The dimension's name is `mc` and each level's value is `nom` or the sample index `i`.

Sample index `i` and base seed `B` alone SHALL determine the sample's random values (Requirement "Sample seeds"). Increasing `N` SHALL add samples without changing the existing ones: samples `0 … N−1` of a run with count `N' > N` SHALL equal those of a run with count `N`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Extending a run
- **GIVEN** a completed Monte Carlo run of 100 samples with base seed 5
- **WHEN** the user runs 200 samples with base seed 5
- **THEN** the measurements of samples 0–99 SHALL equal those of the first run, bit for bit on the same machine

### Requirement: Sample seeds
Each Monte Carlo sample `i` SHALL be simulated with its own ngspice seed:

```
K   = B · 2^32 + i                                  (unsigned 64-bit)
s_i = 1 + (v_0(K) mod 2147483646)                   (1 ≤ s_i ≤ 2147483646)
```

where `v_0(K)` is the first value of the Hefesto random stream with seed `K`, as defined in capability `simulation-sweeps`, Requirement "Design of experiments". The nominal sample SHALL use seed 1. Seeds therefore do not depend on the platform, the worker or the execution order.

Immediately before passing a sample's deck to ngspice (capability `simulation-netlist-preparation`, Requirement "Deck delivery"), the engine adapter SHALL send `setseed <s_i>`, after the PDK initialisation commands. Each sample SHALL be one deck load into a freshly reset circuit; Hefesto SHALL NOT draw several samples from one load by `reset`, `mc_source` or loops.

In a Monte Carlo deck, Hefesto SHALL remove every top-level `.option`/`.options` `seed=` assignment, and every `setseed` and `set rndseed` line in `.control` blocks, each with the warning `Removed seed setting on line <n>; Monte Carlo seeds are set by Hefesto`. Hefesto SHALL NOT set `stat_param_as_func` or `statlocal`, and SHALL reject a deck that sets either of them in a Monte Carlo run with `<variable> is not supported in Monte Carlo runs`.

`job.json` of each sample SHALL record `mc: {mode, baseSeed, index, seed}`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (ngspice ref: src/maths/misc/randnumb.c:287-330, src/frontend/com_option.c:142-152, src/sharedspice.c:930-936) (See Q-SIMADV-6, Q-SIMADV-7.)

#### Scenario: Seeds for base seed 1
- **GIVEN** base seed 1
- **WHEN** the seeds of samples 0, 1 and 2 are derived
- **THEN** they SHALL be 875784623, 2120735756 and 1645304719

#### Scenario: Seeds for base seed 42
- **GIVEN** base seed 42
- **WHEN** the seeds of samples 0 and 1 are derived
- **THEN** they SHALL be 1749773200 and 112509509

#### Scenario: Seed option removed from a schematic deck
- **GIVEN** a schematic testbench whose code block contains `.option seed=12`
- **WHEN** a Monte Carlo run is started
- **THEN** that line SHALL be removed with the warning `Removed seed setting on line <n>; Monte Carlo seeds are set by Hefesto`, and each sample SHALL use its derived seed

### Requirement: Portable random numbers
Hefesto's ngspice build SHALL produce the same sequence of random draws for the same seed on every supported platform. Because the stock generator takes its initial state from the C library `rand()`, whose sequence differs between C libraries, Hefesto SHALL patch the seeding so that, for a seed `s`, the eight generator state words are

```
state_k = 129 + (v_{k−1}(s) mod 32639)        for k = 1 … 8
```

with `v_t` the Hefesto random stream of capability `simulation-sweeps` (Requirement "Design of experiments"), seeded with `s` as an unsigned 64-bit integer. No other part of the generator SHALL change. The reference ngspice used for golden comparisons (`tests/golden/SIM/CASES.md`) SHALL carry the same patch. The patch SHALL be listed in the engine's build manifest (capability `simulation-engine-embedding`, Requirement "ngspice build configuration and license hygiene").

For the same seed and deck, each statistical draw SHALL be equal on all platforms to within 4 units in the last place (the normal draw uses `log` and `sqrt`, whose last bit MAY differ between math libraries). Simulation results of a sample SHALL then agree across platforms within the tolerances of capability `simulation-engine-embedding`, Requirement "Cross-platform result consistency".

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (ngspice ref: src/maths/misc/randnumb.c:101-116, src/maths/misc/randnumb.c:140-150) (See Q-SIMADV-6.)

#### Scenario: Generator state for seed 1
- **GIVEN** the patched generator
- **WHEN** `setseed 1` is executed
- **THEN** the state words SHALL be 29963, 3443, 21757, 5511, 10925, 11052, 26345 and 26017

#### Scenario: Same sample on Linux and iPad
- **GIVEN** a sky130A inverter testbench, mode `mismatch`, base seed 3, sample 17
- **WHEN** it is simulated on Linux x86_64 and on iPad
- **THEN** both SHALL use seed `s_17`, and `v(out)` SHALL agree within the voltage tolerance of capability `simulation-engine-embedding`

### Requirement: Reproducibility
For the same base deck, PDK descriptor, mode, base seed, sample index and Hefesto version:
- a sample's vectors and measurements SHALL be bit-identical across runs on one machine
- they SHALL NOT depend on the worker count, the worker that ran the sample, the order in which samples finish, or whether the run was interrupted and resumed
- across platforms they SHALL agree within the tolerances of Requirement "Portable random numbers"

ngspice draws values in deck order. Any change to the deck text that adds, removes or reorders statistical draws (for example adding a device to the DUT) SHALL be expected to change every later draw; Hefesto SHALL therefore record `deckSha256` of the base deck in `runset.json` (capability `simulation-sweeps`) and SHALL show `The deck changed since run set <id>; samples are not comparable` when the user compares two run sets whose base decks differ.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Comparing runs of different decks
- **GIVEN** two Monte Carlo run sets of one testbench, the second made after a transistor was added to the schematic
- **WHEN** the user overlays their histograms
- **THEN** the overlay SHALL be shown with the notice `The deck changed since run set <first id>; samples are not comparable`

### Requirement: Parallel and sequential execution
On Linux and macOS, Monte Carlo samples SHALL be spread over the worker processes of capability `simulation-engine-embedding`, Requirement "Multiple simulations and corner parallelism", and dispatched in level order (capability `simulation-sweeps`, Requirement "Run set execution"). A worker crash SHALL fail only the sample it was running (`simulation engine terminated`); the remaining samples SHALL continue on a new worker.

On iPadOS, samples SHALL run one at a time in the in-process engine, with the same seeds and results. A run set SHALL pause when the app moves to the background and continue when it returns, as for single jobs. If the engine becomes `unavailable`, the run set SHALL become interrupted and resumable after the app restarts (capability `simulation-sweeps`, Requirement "Run set execution"). Before a run set larger than 1 000 samples starts on iPadOS, Hefesto SHALL show the estimated duration, computed as `N ×` the duration of the nominal sample (or of the first sample when `nominal` is off), and ask for confirmation.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19, docs/decisions/product.md#P-13)

#### Scenario: Worker crash during a run
- **GIVEN** a 500-sample run on macOS with 4 workers, in which sample 233 crashes its worker
- **WHEN** the run set completes
- **THEN** sample 233 SHALL be `failed` with `simulation engine terminated`, and the other 499 samples SHALL be `completed`

#### Scenario: Large run on iPad
- **GIVEN** an iPad, a testbench whose nominal sample took 0.8 s, and a Monte Carlo run of 2 000 samples
- **WHEN** the user starts it
- **THEN** Hefesto SHALL show an estimated duration of about 27 minutes and SHALL start only after confirmation

### Requirement: Failed samples and early stopping
A sample that ends `failed` SHALL be kept in the run set with its diagnostic. By default (`failedSamples = fail`) it SHALL count as a failing sample in yield, and SHALL be excluded from mean, sigma, Cpk, histograms and correlation, which SHALL report the number of excluded samples. With `failedSamples = exclude`, it SHALL also be excluded from yield, and the yield report SHALL state how many samples were excluded.

A Monte Carlo dimension MAY set an early-stop target `h` for the half-width of the 95 % yield confidence interval (capability `simulation-measurements-and-statistics`, Requirement "Yield"). Hefesto SHALL check the target only at checkpoints `m = 50, 100, 150, …`, using samples `0 … m−1`, and only when all of them are final. If the half-width is `≤ h`, the remaining samples SHALL be cancelled and the run set SHALL be `completed` with `stoppedEarly: m` in `runset.json`. A checkpoint SHALL NOT use samples with index `≥ m`, so the stopping point does not depend on execution order. Early stopping SHALL be disabled when the Monte Carlo dimension is inside other dimensions; it then applies only if the user enables it per outer level.

A cancelled Monte Carlo run SHALL keep its completed samples, and its statistics SHALL be marked `partial`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Early stop at a checkpoint
- **GIVEN** a run of 1 000 samples with early-stop target `h` = 0.01, in which exactly samples 3, 40, 120, 200 and 260 fail
- **WHEN** the checkpoints are evaluated
- **THEN** checkpoints 50 to 450 SHALL NOT stop the run (half-width at 450 = 0.01049), checkpoint 500 SHALL stop it (495 passes of 500, half-width 0.00946), samples 500–999 SHALL be cancelled, and `runset.json` SHALL record `stoppedEarly: 500`

### Requirement: User statistical parameters
A testbench SHALL be able to declare statistical parameters of its own, each with a name, a distribution and its arguments:

| Distribution | Arguments | Emitted line |
|---|---|---|
| `agauss` | `nom abs sigma` | `.param <name>='agauss(<nom>,<abs>,<sigma>)'` |
| `gauss` | `nom rel sigma` | `.param <name>='gauss(<nom>,<rel>,<sigma>)'` |
| `aunif` | `nom abs` | `.param <name>='aunif(<nom>,<abs>)'` |
| `unif` | `nom rel` | `.param <name>='unif(<nom>,<rel>)'` |
| `limit` | `nom abs` | `.param <name>='limit(<nom>,<abs>)'` |

Each parameter is marked `process` (one value per sample, emitted at top level) or `mismatch`. A `mismatch` parameter SHALL be emitted inside the subcircuit that uses it, so that each instance draws its own value; it SHALL be rejected for raw and schematic decks with `Mismatch parameter <name> needs a layout or netlist DUT`. The lines SHALL be placed after the mode's `.param` assignments, in declaration order, and SHALL be emitted only in samples, not in the nominal sample, where `.param <name>=<nom>` is emitted instead. Numbers SHALL be written in C `%.12g` format. Parameters of the active mode only SHALL vary: in mode `process`, `mismatch` parameters are emitted at their nominal value, and vice versa.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (ngspice ref: src/frontend/numparam/xpressn.c:45-85)

#### Scenario: Resistor tolerance
- **GIVEN** a testbench parameter `rload`, distribution `gauss`, arguments 10k, 0.05, 3, marked `process`
- **WHEN** sample 4 and the nominal sample are composed
- **THEN** sample 4 SHALL contain `.param rload='gauss(10000,0.05,3)'` and the nominal deck SHALL contain `.param rload=10000`

### Requirement: Monte Carlo command options
The `sim` command (capability `simulation-command`) SHALL accept the option `mc` with these forms:

| Syntax | Effect / result |
|---|---|
| `mc [off]` | Return the Monte Carlo settings as key-value pairs, or remove the Monte Carlo dimension |
| `mc set -mode process\|mismatch\|both [-runs <n>] [-seed <b>] [-nominal on\|off] [-stop <h>] [-failed fail\|exclude]` | Define the Monte Carlo dimension |
| `mc param <name> <dist> <args…> [-mismatch]` \| `mc param delete <name>` \| `mc param list` | User statistical parameters |
| `mc seed <index>` | Return the ngspice seed `s_i` of a sample, for the current base seed |
| `mc run [-wait]` | Start the run set; return its id |

Starting the run set SHALL use `sim runset` of capability `simulation-sweeps`; `mc run` is a shortcut. Errors SHALL follow capability `simulation-command`; for example, `-runs 0` SHALL fail with `sim mc: runs must be between 1 and 100000`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-1.)

#### Scenario: Script query of a seed
- **GIVEN** `sim mc set -mode mismatch -seed 1`
- **WHEN** a script evaluates `sim mc seed 2`
- **THEN** it SHALL receive `1645304719`

#### Scenario: Invalid run count
- **WHEN** `sim mc set -mode process -runs 0` is executed
- **THEN** it SHALL fail with `sim mc: runs must be between 1 and 100000`, and the testbench SHALL be unchanged
