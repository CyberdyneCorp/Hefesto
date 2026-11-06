# Digital Flow Run

## Purpose
Defines how the Hefesto desktop app configures and launches the open digital flow (LibreLane, built on Yosys, OpenROAD, Magic, KLayout and Netgen) as an external process. It covers locating the flow installation, editing the flow configuration, choosing the PDK through Hefesto's PDK manager, launching and cancelling runs, tracking progress from the run directory, managing run directories, recording what is needed to reproduce a run, and the consent and isolation rules that apply because the flow executes third-party code.

LibreLane reference: `librelane/librelane` @ `8494d4fea0ddd9bee12ea5e42a38b59438c2b128` (version 3.0.17). LibreLane references cite paths in that repository. Hefesto SHALL NOT bundle LibreLane or its tools; it drives an installation the user provides (see Q-DIGITAL-9).

The LibreLane run directory is not a stable public interface. Hefesto relies only on the parts listed in Requirement "Run directory layout" and tolerates their absence (see Q-DIGITAL-3).

## Requirements

### Requirement: Platform availability
Running the digital flow SHALL be available only in the desktop app on Linux and macOS, and only when the platform-services layer provides external-process execution (capability `hefesto-architecture`, Requirement "Platform services abstraction"). On iPadOS, or when process execution is unavailable, every run command SHALL fail with a diagnostic stating that the digital flow runs only in the desktop app, and the user interface SHALL follow capability `hefesto-ipad-feature-scope`. Existing run directories SHALL remain browsable read-only on every platform (capability `digital-flow-results`). See Q-DIGITAL-1.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Run requested on iPad
- **GIVEN** a project with a LibreLane design directory opened on iPadOS
- **WHEN** a script runs `flow run`
- **THEN** the command SHALL fail with a diagnostic stating that the digital flow is available only in the desktop app, and no file SHALL be written

### Requirement: Flow installation
Hefesto SHALL let the user register one or more LibreLane installations. Each installation SHALL have one of these launch modes:
- **native**: an executable path to the `librelane` command;
- **nix**: a Nix flake reference whose default app provides `librelane`, run with `nix run <ref> --`;
- **container**: a native `librelane` executable invoked with `--containerized` (LibreLane then runs itself in a Docker-compatible engine).

When an installation is registered, and again before each run, Hefesto SHALL query its version by running it with `--bare-version` and SHALL record the trimmed standard output as the installation's version. A query that fails, times out after 60 s, or prints an empty string SHALL mark the installation unusable with a diagnostic that includes the command's standard error.

Hefesto SHALL declare a set of tested LibreLane versions. Running with a version outside that set SHALL require the user to confirm a warning once per installation and version.

Compatibility: native

(LibreLane ref: librelane/__main__.py:368-430, pyproject.toml:64-68)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Version recorded
- **GIVEN** a native installation at `/opt/librelane/bin/librelane` whose `--bare-version` prints `3.0.17\n`
- **WHEN** it is registered
- **THEN** its recorded version SHALL be `3.0.17`

#### Scenario: Untested version
- **GIVEN** the tested set is {3.0.17} and an installation reports `3.1.0`
- **WHEN** the user starts the first run with it
- **THEN** Hefesto SHALL ask for confirmation, naming both versions, and SHALL NOT ask again for that installation and version once confirmed

### Requirement: Design directory and configuration file
A flow design SHALL be a directory that contains one LibreLane configuration file, `config.json`, `config.yaml` or `config.yml`. A directory with more than one of these SHALL require the user to pick one. Legacy `config.tcl` files SHALL be launchable but SHALL be read-only in the editor. Run directories SHALL be created by LibreLane under `<design dir>/runs/`.

Hefesto SHALL offer a "new flow design" action that creates the directory with a minimal configuration holding `DESIGN_NAME`, `VERILOG_FILES`, `CLOCK_PORT` and `CLOCK_PERIOD`, with `VERILOG_FILES` written using the `dir::` prefix.

Compatibility: native

(LibreLane ref: librelane/config/config.py:113-125, librelane/flows/flow.py:568-590, librelane/examples/spm/config.yaml)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: New flow design
- **WHEN** the user creates a flow design `counter` with Verilog file `src/counter.v`, clock port `clk` and period 10 ns, in JSON
- **THEN** `config.json` SHALL contain `"DESIGN_NAME": "counter"`, `"VERILOG_FILES": ["dir::src/counter.v"]`, `"CLOCK_PORT": "clk"` and `"CLOCK_PERIOD": 10`

### Requirement: Configuration editor
Hefesto SHALL provide a structured editor for JSON and YAML configuration files that:
- shows each top-level key with its value, and, where the selected installation can describe the flow's variables, the variable's type, default and documentation (obtained by running the installation's help for the selected flow, for example `python3 -m librelane.help Classic`);
- validates edited values against the known type (number, boolean, string, list, dictionary, path) before saving, and reports unknown keys as warnings, not errors;
- preserves, byte for byte, the text of every value it does not edit, including LibreLane preprocessor expressions (`dir::`, `pdk_dir::`, `ref::`, `refg::`, `expr::`) and conditional sections keyed `pdk::<glob>` and `scl::<glob>`;
- preserves the order of keys and inserts new keys at the end of their dictionary;
- for YAML, preserves comments attached to keys it does not edit;
- writes JSON with four-space indentation, LF line endings and one trailing LF.

Hefesto SHALL NOT evaluate preprocessor expressions itself; their effective values are taken from the run's `resolved.json` after a run (capability `digital-flow-results`). See Q-DIGITAL-8.

Compatibility: native

(LibreLane ref: librelane/config/preprocessor.py:223-300, librelane/help/__main__.py:22-25)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Editing one key keeps an expression
- **GIVEN** a `config.yaml` with `FP_CORE_UTIL: 45`, `PNR_SDC_FILE: dir::src/impl.sdc` and a `pdk::sky130*:` section
- **WHEN** the user changes `FP_CORE_UTIL` to 40 and saves
- **THEN** only that value SHALL change, and `PNR_SDC_FILE` and the `pdk::sky130*:` section SHALL be unchanged byte for byte

#### Scenario: Wrong type
- **WHEN** the user enters `fast` for `CLOCK_PERIOD`, whose type is a number
- **THEN** the editor SHALL refuse to save and SHALL report the expected type

### Requirement: Macros in the configuration
Hefesto SHALL let the user add an imported block (capability `digital-flow-block-import`) to the configuration's `MACROS` dictionary. The entry SHALL be keyed by the block name and SHALL list the block's views in the fields `gds`, `lef`, `vh`, `nl`, `pnl`, `spice`, `lib`, `spef`, `sdf` and `json_h`, omitting fields with no files. Each path SHALL be written relative to the design directory with the `dir::` prefix when the file lies inside the design directory, and as an absolute path otherwise. Instance placements MAY be added under `instances` as `{"<instance>": {"location": [x, y], "orientation": "<N|S|E|W|FN|FS|FE|FW>"}}`, with `x` and `y` in microns.

Compatibility: native

(LibreLane ref: librelane/config/variable.py:69-189, docs/source/usage/using_macros.md)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Add a block to MACROS
- **GIVEN** design directory `/w/top` and block `spm` stored at `/w/top/blocks/spm/` with `gds/spm.gds` and `lef/spm.lef`
- **WHEN** the user adds `spm` to `MACROS` with instance `u_spm` at (10, 20) µm, orientation N
- **THEN** the entry SHALL be `"spm": {"instances": {"u_spm": {"location": [10, 20], "orientation": "N"}}, "gds": ["dir::blocks/spm/gds/spm.gds"], "lef": ["dir::blocks/spm/lef/spm.lef"]}`

### Requirement: PDK selection through the PDK manager
The PDK for a run SHALL be chosen from the PDKs installed in Hefesto's PDK store (capabilities `hefesto-files-and-pdks` and `hefesto-pdk-manager`, decision P-14). The user SHALL choose the PDK variant (for example `sky130A`, `gf180mcuD`, `ihp-sg13g2`) and MAY choose a standard-cell library; with none chosen, LibreLane's PDK default is used.

Hefesto SHALL launch LibreLane with `--manual-pdk`, `--pdk-root <store root>` and `--pdk <variant>`, plus `--scl <library>` when chosen, so that LibreLane never downloads a PDK itself (see Q-DIGITAL-2). Before launching, Hefesto SHALL compare the store's open_pdks commit for the PDK family with the commit LibreLane expects, read from the installation's `pdk_hashes.yaml` when that file can be found; a difference SHALL be shown as a warning that the user must confirm, and both commits SHALL be recorded with the run.

Compatibility: native

(LibreLane ref: librelane/flows/cli.py:426-470, librelane/pdk_hashes.yaml:1-3)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: PDK version mismatch
- **GIVEN** the PDK store holds sky130A at open_pdks commit `0fe599b2`, and the installation's `pdk_hashes.yaml` lists `sky130: 8afc8346a57fe1ab7934ba5a6056ea8b43078e71`
- **WHEN** the user starts a run with sky130A
- **THEN** Hefesto SHALL warn that the PDK differs from the one LibreLane expects, show both commits, and launch only after confirmation

#### Scenario: PDK not installed
- **WHEN** the user selects `gf180mcuD` and it is not in the PDK store
- **THEN** Hefesto SHALL offer to install it through the PDK manager and SHALL NOT launch the run until it is installed

### Requirement: Launch
A run SHALL be started by creating a process directly from an argument vector, never through a shell. The vector SHALL be, in order:
1. the launch prefix of the installation (`<exe>` for native; `nix run <ref> --` for nix; `<exe> --containerized` for container, preceded by `--container-mount <dir>` for each extra directory that must be visible, before `--containerized`);
2. the PDK options of Requirement "PDK selection through the PDK manager";
3. `--run-tag <tag>`;
4. `--flow <name>` when the user chose a flow other than the configuration's default;
5. `--from <step id>`, `--to <step id>`, `--skip <step id>` (repeated) and `--only <step id>` when the user restricted the steps;
6. `-j <n>` when the user limited parallel jobs;
7. the absolute path of the configuration file.

The working directory SHALL be the design directory. The environment SHALL be the user's login environment with `PDK_ROOT` set to the store root and `PDK` to the variant. Hefesto SHALL generate the run tag as `RUN_%Y-%m-%d_%H-%M-%S` in local time, the same format LibreLane uses when no tag is given, so that the run directory `<design dir>/runs/<tag>` is known before launch. If that directory already exists, Hefesto SHALL append `_2`, `_3`, … to the tag until it does not, because LibreLane would otherwise resume the existing run.

The complete argument vector SHALL be shown to the user before the first launch and SHALL be recorded with the run.

Compatibility: native

(LibreLane ref: librelane/flows/flow.py:568-590, librelane/flows/flow.py:596-655, librelane/flows/cli.py:278-400)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Argument vector for a native run
- **GIVEN** a native installation `/opt/ll/bin/librelane`, PDK store root `/home/u/.hefesto/pdks`, PDK `sky130A`, library `sky130_fd_sc_hd`, design `/w/spm/config.yaml`, and the local time 2026-11-06 14:03:09
- **WHEN** the user starts a full run
- **THEN** the argument vector SHALL be `/opt/ll/bin/librelane --manual-pdk --pdk-root /home/u/.hefesto/pdks --pdk sky130A --scl sky130_fd_sc_hd --run-tag RUN_2026-11-06_14-03-09 /w/spm/config.yaml`, with working directory `/w/spm`

#### Scenario: Tag collision
- **GIVEN** `/w/spm/runs/RUN_2026-11-06_14-03-09` already exists
- **WHEN** a run is started in the same second
- **THEN** the tag SHALL be `RUN_2026-11-06_14-03-09_2`

### Requirement: Resuming a run
Hefesto SHALL let the user resume an incomplete run. Resuming SHALL launch with `--run-tag <existing tag>` and, when the user chose a starting step, `--from <step id>`. Hefesto SHALL state, before launching, that LibreLane continues after the last step that wrote `state_out.json`. Resuming SHALL be refused for a run whose recorded installation version or PDK commit differs from the current ones, unless the user confirms.

Compatibility: native

(LibreLane ref: librelane/flows/flow.py:596-655)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Resume a cancelled run
- **GIVEN** run `RUN_2026-11-06_14-03-09` cancelled during step 37
- **WHEN** the user resumes it
- **THEN** the argument vector SHALL contain `--run-tag RUN_2026-11-06_14-03-09`, and no new run directory SHALL be created

### Requirement: Run directory layout
Hefesto SHALL rely only on these parts of a LibreLane run directory `<design dir>/runs/<tag>/`:

| Path | Meaning |
|---|---|
| `resolved.json` | The fully resolved configuration of the run |
| `flow.log`, `warning.log`, `error.log` | Flow log at verbose level, and warnings and errors only |
| `<NN>-<step slug>/` | One directory per executed step. `NN` is the step ordinal, zero-padded to the number of digits of the flow's step count. The slug is the step id in lower case, with characters other than letters, digits, `_`, `-`, `.` and spaces removed, and runs of spaces and dots replaced by `-` |
| `<NN>-<slug>/config.json`, `state_in.json` | Step inputs; their presence means the step has started |
| `<NN>-<slug>/state_out.json` | Step outputs, including a `metrics` object; its presence means the step has finished |
| `<NN>-<slug>/runtime.txt` | Step wall-clock time |
| `<NN>-<slug>/<slug>.log` | Step tool log |
| `<NN>-<slug>/reports/` | Step reports |
| `final/` | Final views by view id, plus `metrics.json` and `metrics.csv`; present when the flow reached its end |

Any of these MAY be missing; Hefesto SHALL then show the information it has and SHALL NOT fail. Hefesto SHALL write only one file inside a run directory, `hefesto-run.json` (Requirement "Reproducibility record"); this name does not start with a digit, so LibreLane does not treat it as a step directory.

Compatibility: native

(LibreLane ref: librelane/flows/flow.py:215-221, librelane/flows/flow.py:604-680, librelane/flows/flow.py:720-735, librelane/steps/step.py:1146-1199, librelane/steps/step.py:1235, librelane/flows/sequential.py:400-411, librelane/state/state.py:193-218)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Step directory naming
- **GIVEN** the Classic flow with 80 steps, whose 39th step has id `OpenROAD.GlobalRouting`
- **WHEN** that step starts
- **THEN** Hefesto SHALL recognize the directory `39-openroad-globalrouting` as step 39 (verify against LibreLane)

### Requirement: Progress reporting
While a run is active, Hefesto SHALL show:
- the current step's ordinal, id and elapsed time, and the total step count of the flow when it is known;
- a progress fraction equal to (number of step directories containing `state_out.json`) ÷ (total step count); steps the user skipped count as finished;
- the live tail of `flow.log`, and counts of lines in `warning.log` and `error.log`;
- metrics as they appear, read from the newest `state_out.json`.

Progress SHALL be derived by watching the run directory and reading the process's standard output and error; Hefesto SHALL NOT require any LibreLane plug-in. Progress updates SHALL NOT block the user interface (capability `hefesto-architecture`, Requirement "Concurrency model"). Standard output and error SHALL also be saved to `hefesto-stdout.log` in Hefesto's per-run data, not in the run directory.

Compatibility: native

(LibreLane ref: librelane/flows/flow.py:186-221, librelane/steps/step.py:130-160)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Progress fraction
- **GIVEN** an 80-step flow where 20 step directories contain `state_out.json`, and the user skipped 4 other steps
- **WHEN** progress is computed
- **THEN** the fraction SHALL be 24 ÷ 80 = 0.30

### Requirement: Run status
When the process ends, Hefesto SHALL classify the run as exactly one of:
- **succeeded**: exit status 0 and `final/metrics.json` exists;
- **completed with errors**: nonzero exit status, `final/` exists, and `flow.log` or `error.log` reports deferred errors (sign-off checks that failed after the final views were saved);
- **failed**: any other nonzero exit status, or exit status 0 without `final/`;
- **cancelled**: the user cancelled it (Requirement "Cancellation").

The status, exit status, start and end time SHALL be stored in `hefesto-run.json`.

Compatibility: native

(LibreLane ref: librelane/flows/sequential.py:400-411)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Sign-off check fails
- **GIVEN** a run whose LVS checker step fails after the final views were saved, and LibreLane exits with status 1
- **WHEN** the run ends
- **THEN** its status SHALL be "completed with errors", and its final views SHALL be available to capability `digital-flow-results`

### Requirement: Cancellation
The user SHALL be able to cancel an active run. Cancellation SHALL:
1. send an interrupt to the run's process group (or stop the container in container mode);
2. if the process has not exited after 10 s, send a termination request;
3. if it has not exited 10 s after that, kill the process group.

After cancellation the run directory SHALL be kept, the run SHALL be marked cancelled, and it MAY be resumed. Closing the app with an active run SHALL ask whether to cancel the run or keep it running detached; a detached run SHALL be re-attached for progress reporting the next time its design is opened, if its process is still alive.

Compatibility: native

(LibreLane ref: librelane/__main__.py:285-305)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Cancel during routing
- **GIVEN** an active run in step `OpenROAD.DetailedRouting` that ignores the interrupt
- **WHEN** the user cancels it
- **THEN** a termination request SHALL be sent 10 s after the interrupt, the process group SHALL be killed 10 s later if still alive, and the run SHALL be marked cancelled with its directory kept

### Requirement: Run directory management
Hefesto SHALL list the runs of a design by reading `<design dir>/runs/*/`, newest first by directory modification time, showing tag, status, start time, duration, LibreLane version, PDK and disk usage. The user SHALL be able to:
- rename a run's display label (the directory is not renamed);
- mark a run as kept, which excludes it from cleanup;
- delete a run, after a confirmation that names the directory and its size;
- clean up, which proposes deleting all runs that are not kept and are older than a user-chosen number of days, and deletes only after confirmation.

Hefesto SHALL NOT delete any run directory without explicit confirmation, and SHALL NOT start more than one run on the same design directory at a time. The default limit on simultaneous runs across all designs SHALL be 1, configurable in settings.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Second run on the same design
- **GIVEN** an active run on `/w/spm`
- **WHEN** the user starts another run on `/w/spm`
- **THEN** Hefesto SHALL refuse with a diagnostic naming the active run

### Requirement: Reproducibility record
Before launching, Hefesto SHALL write `hefesto-run.json` into the run directory, as UTF-8 JSON with two-space indentation and sorted keys, holding:
- `hefesto_run`: format version, 1;
- `hefesto_version`;
- `installation`: launch mode, executable path or flake reference, and LibreLane version; for nix, the locked flake revision when available; for container, the image reference reported by LibreLane when available;
- `argv`: the complete argument vector;
- `pdk`: variant, standard-cell library, store open_pdks commit, and the commit expected by LibreLane;
- `config`: the configuration file's path and SHA-256 at launch;
- `inputs`: the SHA-256 of every file the configuration names through `dir::` that exists at launch;
- `status`, `exit_status`, `started`, `ended` (ISO 8601 with offset), updated when the run ends.

Hefesto SHALL offer "rerun with the same settings", which launches a new run with the same argument vector except the run tag. It SHALL warn, listing each difference, when the installation version, PDK commit, configuration hash or any input hash differs from the record.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Rerun after an input changed
- **GIVEN** a recorded run whose input `src/spm.v` had SHA-256 `a1…` and the file has since changed
- **WHEN** the user chooses "rerun with the same settings"
- **THEN** Hefesto SHALL warn that `src/spm.v` changed, and SHALL launch only after confirmation

### Requirement: Consent and isolation
Running the flow executes third-party programs and design-supplied scripts (for example custom PDN or floorplan scripts named by the configuration). Therefore:
- opening a project or design SHALL NEVER start a run;
- the first run of each pair (installation version, design directory) SHALL require consent in a dialog that shows the executable, its version, the argument vector, the design directory, and a statement that the flow runs with the user's permissions and may access the network (nix and container modes may download software);
- consent SHALL be recorded per installation version and design directory, and SHALL be asked again when either changes;
- the AI agent and scripts SHALL be able to start a run only through the same command, and an agent-initiated run SHALL require the user's explicit approval (capability `agent-safety-and-approval`);
- the headless CLI SHALL run the flow only when given an explicit `--allow-external-flow` option;
- Hefesto SHALL NOT run the flow with elevated privileges, and SHALL refuse to launch if its own process is running as root;
- in container mode, Hefesto SHALL pass `--container-mount` only for the design directory, the PDK store root and the directories of macro views outside the design directory.

Compatibility: native

(LibreLane ref: librelane/__main__.py:380-410)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: First run asks for consent
- **GIVEN** a design directory never run before with installation version 3.0.17
- **WHEN** the user clicks Run
- **THEN** the consent dialog SHALL be shown, and the process SHALL start only if the user accepts

#### Scenario: Agent starts a run
- **WHEN** the AI agent proposes to run the flow on `/w/spm`
- **THEN** the run SHALL start only after the user explicitly approves that proposal

### Requirement: LibreLane mechanisms (informative)
Hefesto SHALL treat this requirement as a description of LibreLane, not as a constraint on Hefesto. LibreLane is a Python package (entry point `librelane`) distributed through Nix and PyPI, with an option to re-run itself in a container. Each step invokes its tool as a subprocess; Magic steps run `magic -dnull -noconsole -rcfile <MAGICRC> <wrapper.tcl>` with the step script passed in an environment variable; Netgen LVS runs `lvs` with `-blackbox -json`. Tools report metrics to LibreLane by printing lines `%OL_METRIC`, `%OL_METRIC_I` or `%OL_METRIC_F` followed by a name and value. Hefesto MAY differ from these mechanisms; they are recorded so that Hefesto's parsing and future compatibility work (Q-DIGITAL-7) have a reference.

Compatibility: informative

(LibreLane ref: librelane/steps/magic.py:175-183, librelane/steps/netgen.py:255, librelane/steps/step.py:130-160, librelane/flows/classic.py:40-121)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Magic invocation seen in a step log
- **GIVEN** a completed run
- **WHEN** the user opens the `COMMANDS` file of the `Magic.DRC` step directory
- **THEN** it SHALL show a `magic -dnull -noconsole -rcfile …` command line, which Hefesto displays without interpreting it
