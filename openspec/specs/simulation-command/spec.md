# Simulation Command

## Purpose
Defines the `sim` command: the command-API surface (capability `hefesto-architecture`, Requirement "Single command API") through which the GUI, touch gestures, scripts and the headless CLI do everything related to simulation. It covers testbench editing, model and corner selection, running jobs, querying and exporting results, probing, and the waveform viewer. The behaviour behind each subcommand is specified in capabilities `simulation-engine-embedding`, `simulation-netlist-preparation`, `simulation-analyses-and-results` and `simulation-ui-and-crossprobe`. This capability fixes only the syntax, return values and errors. `sim` has no Magic counterpart. Magic's `rsim`, `startrsim` and `simcmd` stay as specified in capability `irsim-interface`.

## Requirements

### Requirement: Command and option matching
The command SHALL be `sim <option> [args…]`. `<option>` and every keyword argument SHALL be matched case-insensitively with unique-prefix matching, as other Hefesto commands are. These cases SHALL fail without side effects:

| Case | Error |
|---|---|
| Ambiguous prefix | `sim: ambiguous option "<w>"` |
| Unknown word | `sim: unknown option "<w>". Use "sim help".` |
| Missing arguments | `Usage: sim <option> <syntax>`, with the syntax from the table of Requirement "Subcommand table" |

`sim` with no option, and `sim help`, SHALL print one line per option, in the table's order. Testbench names, job ids, node names and vector names are not keywords, and SHALL be matched exactly, except that vector names SHALL be matched case-insensitively.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Prefix match
- **WHEN** `sim stat` is executed
- **THEN** it SHALL act as `sim status`

#### Scenario: Ambiguous prefix
- **WHEN** `sim s` is executed
- **THEN** `sim: ambiguous option "s"` SHALL be reported, because `save`, `source` and `status` all start with `s`

### Requirement: Subcommand table
The `sim` options SHALL be:

| Option | Syntax | Effect / result |
|---|---|---|
| `testbench` | `testbench [new\|select\|delete\|list] [<name>]` | Manage testbenches. Without arguments, return the current testbench name. |
| `dut` | `dut layout <cell> [-parasitics none\|c\|rc] [-cthresh <fF>] [-reuse]` \| `dut netlist <file> <subckt>` \| `dut none <deckfile>` | Set the DUT source. Without arguments, return the current source. |
| `bind` | `bind [<port> [<node>]]` | Bind a DUT port to a testbench node. Return all bindings, or one. |
| `source` | `source <name> <n+> <n-> <kind> <params…> [ac <mag> [<phase>]] [-supply]` \| `source delete <name>` \| `source list` | Define stimuli. |
| `extra` | `extra add <line>` \| `extra clear` \| `extra list` | Extra deck lines. |
| `pdk` | `pdk [<name>]` | Select the PDK descriptor. Return the current one. |
| `corner` | `corner [<name>]` \| `corner sweep <sections> -temp <list> [-supply <list>]` \| `corner list` | Select a corner, or define a sweep. |
| `analysis` | `analysis add <op\|dc\|ac\|tran\|noise\|tf> <params…>` \| `analysis clear` \| `analysis list` | Analyses. |
| `save` | `save default\|all\|all+currents` \| `save <vector…>` | Saved-vector mode. |
| `option` | `option <text>` \| `option clear` | `.option` lines. |
| `deck` | `deck [write <file>]` | Return the composed deck for the current corner, or write it to `<file>`. |
| `run` | `run [-corner <name>\|-all] [-wait]` | Submit jobs. Return their job ids. |
| `status` | `status [<job>]` | Return `<state> <progress>` for the job, or for every non-final job. |
| `pause` / `resume` / `cancel` | `<op> <job>\|all` | Job control. |
| `jobs` | `jobs [<testbench>]` | Return the stored job ids, newest first. |
| `plots` | `plots <job>` | Return plot names. |
| `vectors` | `vectors <job> <plot>` | Return the display names of the vectors. |
| `vector` | `vector <job> <plot> <name> [-mode re\|im\|mag\|db\|phase]` | Return the values. |
| `measure` | `measure <job> <plot> <kind> <vector> <args…>` | Return one number. |
| `export` | `export <job> <file> [-format raw\|rawascii\|csv] [-plot <p>] [-mode reim\|dbphase] [<vector…>]` | Write a file. |
| `import` | `import <file> [-cell <cell>]` | Import a raw file. Return the new job id. |
| `probe` | `probe [<net>\|-selection\|-device]` | Add traces. Return the vector names. |
| `view` | `view panel add\|remove <k>` \| `view trace add\|remove <job> <plot> <vector> [-panel <k>] [-mode <m>]` \| `view axis x\|y lin\|log` \| `view fit` | Viewer state. |
| `cursor` | `cursor a\|b [<x>]` | Set or return a cursor position. |
| `annotate` | `annotate on [<job> [<x>]]\|off` | Operating-point labels. |
| `engine` | `engine` \| `engine workers [<n>]` \| `engine reset` | Engine information and control. |
| `help` | `help` | Usage list. |

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Build and run a testbench
- **WHEN** the script `sim testbench new inv_tran; sim dut layout inv -parasitics c; sim pdk sky130A; sim bind A in; sim bind Y out; sim bind VPWR vdd; sim bind VGND 0; sim source Vdd vdd 0 dc 1.8 -supply; sim source Vin in 0 pulse 0 1.8 1n 100p 100p 5n 10n; sim analysis add tran 10p 20n; sim run -wait` is executed
- **THEN** one job SHALL be created, and `sim status <job>` SHALL return `completed 1.0`

### Requirement: Numeric argument syntax
Numeric arguments SHALL accept:
- a decimal or exponent float, for example `1e-9`
- an optional SPICE scale suffix, matched case-insensitively: `t`=1e12, `g`=1e9, `meg`=1e6, `k`=1e3, `m`=1e-3, `u` or `µ`=1e-6, `n`=1e-9, `p`=1e-12, `f`=1e-15. `meg` is checked before `m`.
- trailing unit letters after the suffix, which are ignored (for example `10ns`, `1.8V`)

Anything else SHALL fail with `sim: bad number "<w>"`. Temperatures in `-temp` lists are plain numbers in °C. Lists are comma-separated without spaces, or a script list.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: SPICE suffixes
- **WHEN** `sim analysis add tran 10ps 20NS` is executed
- **THEN** the analysis SHALL have tstep 1e-11 and tstop 2e-08

#### Scenario: Meg versus milli
- **WHEN** a source frequency `1meg` and a voltage `1m` are given
- **THEN** they SHALL be 1e6 and 1e-3

### Requirement: Return values and errors in scripts
When `sim` is called from a script runtime (capability `hefesto-scripting`), list-valued results SHALL be returned as a script list in the documented order, and single values as scalars. Numbers SHALL be formatted with C `%.17g`. When called interactively, the same values SHALL be printed one per line. Every failure SHALL raise a script error carrying the message given by the specifying capability, and SHALL leave the testbench unchanged. `sim run` without `-wait` SHALL return as soon as the jobs are queued. With `-wait`, it SHALL return when every submitted job reaches a final state. It SHALL raise an error `sim: job <id> <state>: <diagnostic>` for the first job, in submission order, that did not complete.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3, docs/decisions/product.md#P-7)

#### Scenario: Vector values in a script
- **GIVEN** a completed op job `j` with `v(out)` = 1.2
- **WHEN** a script evaluates `sim vector j op1 v(out)`
- **THEN** it SHALL receive the one-element list `1.2`

#### Scenario: Failed run with wait
- **GIVEN** a deck that fails with `Error: Timestep too small`
- **WHEN** `sim run -wait` is executed in a script
- **THEN** a script error `sim: job <id> failed: Error: Timestep too small` SHALL be raised

### Requirement: Headless and batch use
The headless executable (capability `hefesto-architecture`, Requirement "Headless command-line tool") SHALL support every `sim` option except the viewer-only ones (`probe -selection`, `view`, `cursor`, `annotate`). In headless mode these SHALL fail with `sim <option>: requires a layout or waveform window`. When a headless script ends, the tool SHALL wait for every queued simulation job. Its exit status SHALL be non-zero if any job ended `failed`.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-6, docs/decisions/product.md#P-7)

#### Scenario: CI corner sweep
- **WHEN** `hefesto-cli -T sky130A -c "source tb.hef; sim corner sweep tt,ss,ff -temp -40,27,125; sim run -all -wait; sim export [lindex [sim jobs] 0] out.csv -format csv; quit"` is run
- **THEN** 9 jobs SHALL run, `out.csv` SHALL be written, and the exit status SHALL be 0 if all jobs completed

### Requirement: Engine subcommand
`sim engine` SHALL return a list of key-value pairs, in this order:
- `version <engineVersion>`
- `mode in-process|worker`
- `workers <n>`
- `state <state of each instance, comma-separated>`
- `features <comma-separated list of xspice, osdi, klu, openmp, as built>`
- `osdi <comma-separated built-in OSDI module names>`

`sim engine workers <n>` SHALL set the desktop worker count, `1 ≤ n ≤ physical cores`. On iPadOS, any `n` other than 1 SHALL fail with `sim engine workers: only 1 engine is available on this platform`. `sim engine reset` SHALL be refused while a job is running, and SHALL otherwise reinitialise every engine instance as in Requirement "Cancellation and fault states" of capability `simulation-engine-embedding`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Engine info on iPad
- **WHEN** `sim engine` is executed on iPad
- **THEN** the result SHALL include `mode in-process` and `workers 1`, and `osdi` SHALL list at least `psp103_nqs`, `r3_cmc` and `mosvar`

#### Scenario: Worker count on iPad
- **WHEN** `sim engine workers 4` is executed on iPad
- **THEN** it SHALL fail with `sim engine workers: only 1 engine is available on this platform`

### Requirement: Testbench persistence
A testbench SHALL be saved in the project as `simulations/<testbench>/testbench.json`. It SHALL record:
- the DUT source, port bindings and stimuli
- extra lines and options
- the PDK, the corner or sweep, analyses and save mode

Each change made through `sim` SHALL be saved immediately and SHALL be undoable with the editor's undo (capability `layout-undo`) as one undo event per command. Testbench edits SHALL NOT change any cell. `sim testbench delete <name>` SHALL delete the testbench and its stored jobs, after confirmation in interactive use.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Undo a binding
- **GIVEN** `sim bind A in` followed by `sim bind A clk`
- **WHEN** the user undoes once
- **THEN** `sim bind A` SHALL return `in`
