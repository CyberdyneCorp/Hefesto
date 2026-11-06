# Tapeout Shuttle Packages

## Purpose
Defines how Hefesto exports a design as a submission package for an open shuttle program. A package contains:
- the files the program requires, under the required names and folder layout
- the program's prechecks, reproduced so that the user sees the same pass or fail before submitting

Programs change their rules from run to run, so each program is described by a versioned program profile. Profiles are data, not code. They are distributed and updated through the same signed catalog as PDKs (capability `hefesto-pdk-manager`).

The checks themselves are capability `tapeout-signoff-checklist`. Fill and density are capability `tapeout-fill-and-density`. Seal rings are capability `tapeout-seal-ring`. GDS, LEF and DEF output are capabilities `gds-write`, `lef-write` and `def-write`. OASIS output is capability `oasis-write`.

Reference sources (read-only, never executed):
- Tiny Tapeout tt-support-tools @ `9d13ff3e3e6202090740110438c6a10c1d62ca0f` (`tt ref:`)
- ttsky-verilog-template @ `a42e6662fab316e93b4e26399e21d41664a460b7` (`tt-template ref:`)
- caravel_user_project @ `f534826abc72af4f9d467446b8b82c9471df68da` (`caravel ref:`)
- mpw_precheck @ `0941bdc1b62b5c3f99c8683bd11199d330af2ef3` (`mpw_precheck ref:`)
- wafer.space gf180mcu-precheck @ `9b4e7dab6af073d02b81ecc368069540d2f67db9` (`ws-precheck ref:`)
- gf180mcu-project-template @ `d99f6e1ba14e549e80d5d83e113fbd82ed37ed5c` (`ws-template ref:`)

Program requirements beyond these repositories (IHP open MPW, ChipFoundry's continuation of Caravel, future wafer.space runs) were not confirmed from primary sources. See Q-TAPEOUT-15.

## Requirements

### Requirement: Program profile format
A program profile SHALL be a JSON document with these fields:
- `id`, `version`, `title`, `url`
- `pdk`: family and allowed variants. Optionally, a required PDK version hash or a minimum version hash.
- `slots`: a list of `{name, width_um, height_um}` die areas, or the generator rule for them
- `top`: one of these naming rules
  - `{"literal": NAME}`
  - `{"pattern": GLOB, "equals_file_stem": bool}`
  - `{"from_field": PATH}`
- `layers`:
  - `valid`, a list of layer/datatype pairs or names resolved through the PDK `.lyp`
  - `forbidden`
  - `boundary` (layer name)
  - `pin_layers`
- `requires`: flags `seal_ring`, `fill`, `lvs`, `origin_zero`, `dbu_0_001`
- `checks`: the ordered checklist items with severity, waivable flag and mirrored program check name, including the KLayout decks with their `-rd` variables
- `package`: the file layout (see Requirement: Package layout templates)
- `metadata_schema`: for programs that need a metadata file
- `notes`: free text, with deadlines when known

Hefesto SHALL validate a profile against its schema before use. It SHALL refuse a profile whose `pdk` is not manufacturable (capability `hefesto-pdk-manager`). Each sign-off run SHALL record the profile's `id` and `version`. When the catalog offers a newer profile, Hefesto SHALL mark existing results stale for that program.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Profile update
- **GIVEN** a project signed off against `tinytapeout-sky` profile version 7
- **WHEN** version 8 is downloaded
- **THEN** the project's sign-off SHALL be shown as stale, with a summary of what changed in the profile

### Requirement: Built-in program profiles
Hefesto SHALL ship initial profiles for the programs below. A catalog update may revise them.

| id | PDK | top rule | die area | required files |
|---|---|---|---|---|
| `tinytapeout-<pdk>` | sky130A, gf180mcuD, ihp-sg13g2, ihp-sg13cmos5l | `tt_um_*`, equal to the file stem and to `info.yaml` `top_module` | the tile size chosen in `info.yaml` (`tiles`) from the program's tile table | `info.yaml`, `gds/<top>.gds` (or `.oas`), `lef/<top>.lef`, `src/project.v`, `docs/info.md` |
| `caravel-user-project` | sky130A/B, gf180mcuC/D | `user_project_wrapper` (or `user_analog_project_wrapper`) | user area (0,0)–(2920 µm, 3520 µm) | `gds/<top>.gds` (compressed in repo), `lef/<top>.lef`, `def/<top>.def`, `mag/<top>.mag`, `verilog/gl/<top>.v` (with power pins), `verilog/rtl/user_defines.v`, `README.md`, `LICENSE` |
| `waferspace-gf180mcu` | gf180mcuD | `chip_top` by default, equal to the file stem unless overridden | slot `1x1`, `0p5x1`, `1x0p5` or `0p5x0p5` from a 3932 µm × 5122 µm die with 60 µm saw street | one layout file `<top>.gds`, `.gds.gz` or `.oas` |
| `ihp-openmpw` | ihp-sg13g2 | user-chosen | program-defined (≈ ≤ 2 mm²) | design folder with GDS and documentation, per the run's `TO_<run>` repository |

The Tiny Tapeout tile table SHALL be profile data. For example, the sky130A 1x1 tile die area is 161.000 µm × 111.520 µm, from the program's `tt_block_1x1_pg.def`. Profile data also includes the forbidden top metal, the pin layers and the power-pin names `VGND`, `VDPWR` and `VAPWR`.

Compatibility: behavioral

(tt ref: precheck/precheck.py:408-490, precheck/tech_data.py:178-246; tt-template ref: info.yaml:1-56; caravel ref: Makefile:240-280; mpw_precheck ref: README.md:40-90; ws-precheck ref: README.md:1-80, scripts/klayout/check_size.py:8-110)

#### Scenario: Tiny Tapeout tile size
- **GIVEN** a sky130A Tiny Tapeout project with `tiles: "1x1"`
- **WHEN** the profile is applied
- **THEN** the required die area SHALL be 0 0 161.000 111.520 µm, and met5 SHALL be forbidden

#### Scenario: Caravel area
- **WHEN** a Caravel project is checked
- **THEN** every shape outside (0,0)–(2920 µm, 3520 µm) of `user_project_wrapper` that differs from the golden empty wrapper SHALL be reported (XOR item)

### Requirement: Tiny Tapeout metadata
For Tiny Tapeout profiles, Hefesto SHALL create and edit `info.yaml` with `yaml_version: 6`. The file SHALL contain:
- `project`:
  - `title`, `author`, `discord`, `description`, `language`
  - `clock_hz` (integer)
  - `tiles`: one of the profile's tile names
  - `top_module`
  - `source_files`: a list of paths under `src/`
  - for analog designs: `analog_pins` (integer) and `uses_vapwr` (bool)
- `pinout`: exactly the keys `ui[0]`…`ui[7]`, `uo[0]`…`uo[7]` and `uio[0]`…`uio[7]`, plus analog pin keys when `analog_pins > 0`

Hefesto SHALL validate the following before export:
- `top_module` starts with `tt_um_`
- the GDS file stem equals `top_module`
- `uses_vapwr` set together with `analog_pins = 0` is an error, reported as "Projects with VAPWR power need at least one analog pin"
- every pinout key is present

Unknown keys SHALL be preserved.

Compatibility: behavioral

(tt-template ref: info.yaml:1-56; tt ref: precheck/precheck.py:440-480)

#### Scenario: VAPWR without analog pins
- **GIVEN** `uses_vapwr: true` and `analog_pins: 0`
- **WHEN** the package is validated
- **THEN** export SHALL stop with "Projects with VAPWR power need at least one analog pin"

### Requirement: Tiny Tapeout pin template selection
For Tiny Tapeout, the pin template DEF SHALL be chosen as the program's tools choose it. `def_root` is `tech/<pdk>/def`.
- **Analog** (`analog_pins > 0`): use `<def_root>/analog/tt_analog_<tiles>`.
  - If `uses_vapwr` is set, add the suffix `_pgvaa.def`. If that file does not exist, use `_3v3.def`.
  - Otherwise add the suffix `.def`.
- **Digital, on ihp-sg13g2, ihp-sg13cmos5l or gf180mcuD**: use `<def_root>/tt_block_<tiles>_pgvdd.def`.
- **Digital, otherwise**: use `<def_root>/tt_block_<tiles>_pg.def`.

The template files SHALL be part of the profile data.

Compatibility: behavioral

(tt ref: precheck/precheck.py:462-478)

#### Scenario: IHP digital tile
- **GIVEN** an ihp-sg13g2 digital project with `tiles: "2x2"`
- **WHEN** the template is selected
- **THEN** it SHALL be `tt_block_2x2_pgvdd.def`

### Requirement: Package layout templates
A profile's `package` SHALL list each output path, using placeholders `{top}`, `{pdk}` and `{slot}`, together with:
- its generator: `gds`, `gds.gz`, `oas`, `lef`, `def`, `mag`, `verilog-blackbox`, `verilog-gl`, `metadata` or `copy`
- whether it is required

Exporting SHALL write every listed file into the target folder at those relative paths, and nothing else. It SHALL also write a `MANIFEST.sha256` file, sorted by path. Each line is `<sha256>  <path>`. For a Git-based program (Tiny Tapeout, Caravel), the target folder MAY be an existing clone of the program template. Exporting into it SHALL overwrite only the listed files. Exporting SHALL NOT commit, push or upload. On iPadOS, the package SHALL be exported as a folder or a `.zip` archive through the share sheet or the document picker.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Export into template clone
- **GIVEN** a clone of the Tiny Tapeout analog template that contains `.github/` workflows
- **WHEN** the package is exported into it
- **THEN** `info.yaml`, `gds/tt_um_x.gds`, `lef/tt_um_x.lef` and `src/project.v` SHALL be written, and `.github/` SHALL be unchanged

### Requirement: Final layout assembly
Before writing the package GDS, Hefesto SHALL assemble the final top cell in this order:
1. the user top cell
2. the seal ring, when the recipe and profile require one
3. the fill cell, when required, regenerated if stale
4. program identification cells, when the profile requires them

For wafer.space Chip-on-Board, step 4 means the cells `gf180mcu_ws_ip__qrcode_id`, `gf180mcu_ws_ip__shuttle_id`, `gf180mcu_ws_ip__project_id` and `gf180mcu_ws_ip__marker`. There SHALL be exactly one instance of each, at the template's positions. Their content is filled in by the program.

The GDS SHALL then be written with capability `gds-write`. Read-only vendor cells SHALL be copied verbatim. The database unit SHALL be 0.001 µm. The output SHALL be compressed when the profile's generator is `gds.gz`. The sign-off checklist SHALL run on the written file, read back from disk, and not on the in-memory layout.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15; ws-precheck ref: README.md:5-20)

#### Scenario: Missing CoB marker
- **GIVEN** a wafer.space profile with Chip-on-Board selected and no `gf180mcu_ws_ip__marker` instance
- **WHEN** the package is exported
- **THEN** export SHALL fail, naming the missing cell

### Requirement: Generated companion views
For profiles that require them, Hefesto SHALL generate:
- the LEF abstract of the top cell (capability `lef-write`), with pins on the profile's pin layers and power pins marked `USE POWER` or `USE GROUND`
- a Verilog black-box module `src/project.v`, named after the top, whose ports are exactly the LEF signal pins plus the power pins, with directions from the port `class` properties (capability `layout-labels-ports`)
- DEF and `.mag` copies where the profile lists them

When the user supplies a gate-level netlist (Caravel `verilog/gl/<top>.v`), Hefesto SHALL check that it contains the power pins. It SHALL NOT synthesize one.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Black-box Verilog
- **GIVEN** an analog Tiny Tapeout top with ports `ua[0]`, `ui_in[7:0]`, `uo_out[7:0]`, `VGND` and `VDPWR`
- **WHEN** the package is exported
- **THEN** `src/project.v` SHALL declare `module tt_um_x` with exactly those ports

### Requirement: Reproduced program prechecks
For each program profile, Hefesto SHALL run the program's precheck items through the sign-off checklist, under the program's own check names and in the program's order. For example:
- **Tiny Tapeout**: Magic DRC (sky130A only), KLayout FEOL, BEOL and offgrid (sky130A), pin label overlapping drawing, KLayout SG13G2 or SG13CMOS5L DRC (IHP), zero area, top name, forbidden layer, prBoundary, Pin check, Boundary check, Power pin check (sky130A, gf180mcuD), Layer check, Cell name check, urpm/nwell (sky130A), KLayout GF180MCU DRC and Antenna (gf180mcuD), Analog pin check, Verilog syntax check
- **Caravel**: License, Makefile, Default, Documentation, Top Cell, Consistency, GPIO-Defines, XOR, Magic DRC, KLayout FEOL, BEOL, Offgrid, Metal Minimum Clear Area Density, Pin Label Purposes Overlapping Drawing, ZeroArea, Spike, Illegal Cellname, OEB, LVS
- **wafer.space**: CheckTopLevel, CheckSize, GenerateID or pad mask (CoB), Density, ZeroArea, Antenna, Magic DRC (non-gating), KLayout DRC

Items that Hefesto can evaluate natively SHALL run in-process, with the semantics in capability `tapeout-signoff-checklist`. These are the structural, geometric, layer, name, boundary, pin, density, Magic DRC and LVS items. Items that need a KLayout deck SHALL follow the KLayout position in that capability. Repository-policy items are outside layout scope. They include License, Makefile, Documentation, GPIO-Defines, Verilog syntax and OEB. They SHALL be shown as `external`, with a link to the program's own precheck.

Compatibility: behavioral

(tt ref: precheck/precheck.py:489-600; mpw_precheck ref: check_manager/__init__.py:587-630; ws-precheck ref: precheck.py:437-470)

#### Scenario: Same names as the program
- **WHEN** a Tiny Tapeout sky130A package is checked
- **THEN** the results table SHALL list `Magic DRC`, `KLayout FEOL`, `KLayout BEOL`, … under exactly the program's names, and in the program's order

### Requirement: Submission readiness summary
After export, Hefesto SHALL show a readiness summary with:
- the package folder
- the manifest hash
- the profile id and version
- the sign-off result (`ready` or not, with blocking items)
- the remaining external steps, for example "push to GitHub and let the Tiny Tapeout action run", "upload on platform.wafer.space" or "open a pull request on IHP-GmbH/TO_<run>"

Hefesto SHALL NOT store program credentials. It SHALL NOT submit on the user's behalf.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-15)

#### Scenario: Not ready summary
- **GIVEN** an exported package whose KLayout DRC item is `not-run` on iPadOS
- **WHEN** the summary is shown
- **THEN** it SHALL say that the package is not ready, name the KLayout DRC item, and explain that the program's CI runs it after the push
