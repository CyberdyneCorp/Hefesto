# Digital Flow Mixed-Signal Integration

## Purpose
Defines how hardened digital blocks (capability `digital-flow-block-import`) are used inside analog and top-level layouts in Hefesto: placing block instances, connecting their supply pins, checking the result with DRC, extraction and LVS while the blocks are black boxes, and exporting a Hefesto layout as a macro that LibreLane can consume in a top-level digital flow.

LibreLane reference: `librelane/librelane` @ `8494d4fea0ddd9bee12ea5e42a38b59438c2b128` (version 3.0.17). LibreLane references cite paths in that repository.

## Requirements

### Requirement: Placing block instances
A block SHALL be placed like any other subcell (capability `layout-hierarchy`), with these additions:
- the placement dialog SHALL accept a position in microns and a DEF orientation (`N`, `S`, `E`, `W`, `FN`, `FS`, `FE`, `FW`), and SHALL compute the transform so that the lower-left corner of the transformed block bounding box lands on the position, exactly as capability `def-read`, Requirement "Component placement transform" defines it;
- the position SHALL be checked as an exact decimal value: if either coordinate is not an integer multiple of the PDK's manufacturing grid, the placement SHALL be refused with a diagnostic that gives the nearest grid-aligned positions below and above; otherwise it SHALL be converted to internal units exactly;
- the default instance name SHALL be `u_<block>` followed by the lowest positive integer suffix `_<n>` that makes it unique in the parent when `u_<block>` is taken;
- the user MAY import instance placements from the `instances` of a LibreLane `MACROS` entry, whose `location` is in microns and whose `orientation` uses the same names (with `R0`, `MY`, `R90`, `MXR90`, `R180`, `MX`, `R270` and `MYR90` accepted as `N`, `FN`, `W`, `FW`, `S`, `FS`, `E` and `FE`).

Compatibility: native

(LibreLane ref: librelane/config/variable.py:69-104, docs/source/usage/using_macros.md)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Place with orientation S
- **GIVEN** block `spm` with bounding box (0, 0)–(20000, 16000) internal units, 1 internal unit = 0.005 µm, and manufacturing grid 0.005 µm
- **WHEN** it is placed at (100, 50) µm with orientation `S`
- **THEN** the transformed bounding box SHALL be (20000, 10000)–(40000, 26000) internal units

#### Scenario: Off-grid position
- **GIVEN** manufacturing grid 0.005 µm
- **WHEN** a block is placed at (10.0025, 0) µm
- **THEN** the placement SHALL be refused, and the diagnostic SHALL offer (10.000, 0) and (10.005, 0) µm

### Requirement: Placement halo and keep-out
Each block SHALL have a placement halo, a non-negative distance in microns stored in the block manifest with default 0. Hefesto SHALL show the halo as an outline around each instance's bounding box and SHALL report, as a warning in DRC results, any paint of the parent cell on a routing layer that lies within the halo and is not connected to one of the block's ports. The block's LEF obstructions SHALL take part in DRC against parent paint when the abstract representation is active; when the full representation is active, the full geometry SHALL be used. Interactions between a block and its parent SHALL be checked; the block's interior SHALL NOT be re-checked, because block cells are read-only (capability `digital-flow-block-import`, Requirement "Read-only block cells").

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Paint inside the halo
- **GIVEN** a block instance with a 2 µm halo, and a parent `metal2` wire 1 µm from the block edge that is not connected to any block port
- **WHEN** DRC runs on the parent
- **THEN** a halo warning SHALL be reported at the wire, in addition to any DRC rule violations

### Requirement: Supply hookup
For each block instance, Hefesto SHALL keep a supply map from each of the block's supply pins (capability `digital-flow-block-import`, Requirement "Pin mapping") to a net name of the parent. The supply map SHALL default to mapping each supply pin to the parent net of the same name when that net exists. Hefesto SHALL provide a supply check that, using capability `layout-connectivity`:
- reports each supply pin that is not connected to any parent geometry;
- reports each supply pin that is connected to a parent net other than its mapped net;
- reports each pair of supply pins with different mapped nets that are connected to each other (a short).

Hefesto MAY offer to draw a straight strap from a supply pin to a user-selected parent rail on a user-selected layer, as one undoable edit followed by a DRC check of the strap area.

Hefesto SHALL be able to write the supply map of a parent cell as LibreLane `PDN_MACRO_CONNECTIONS` entries, one string per instance and supply-pin pair, in the form `<instance regex> <vdd net> <gnd net> <vdd pin> <gnd pin>`, where the instance regex is the instance name with regex metacharacters escaped and anchored with `^` and `$`.

Compatibility: native

(LibreLane ref: librelane/steps/openroad.py:1234-1237)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Supply pin on the wrong net
- **GIVEN** instance `u_spm` with supply map `VPWR`→`vccd1`, `VGND`→`vssd1`, and `VPWR` connected by metal to parent net `vdda1`
- **WHEN** the supply check runs
- **THEN** it SHALL report that `u_spm/VPWR` is connected to `vdda1` instead of `vccd1`

#### Scenario: PDN_MACRO_CONNECTIONS output
- **GIVEN** instance `u_spm.0` with supply map `VPWR`→`vccd1` and `VGND`→`vssd1`
- **WHEN** the supply map is written for LibreLane
- **THEN** the entry SHALL be `^u_spm\.0$ vccd1 vssd1 VPWR VGND`

### Requirement: Extraction with blocks
When a layout containing block instances is extracted (capabilities `extract-commands` and `extract-hierarchy`), each block's top cell SHALL be treated as an abstract view, as for a cell whose `LEFview` property is true, regardless of its active representation. Its netlist definition SHALL be a subcircuit whose ports are the block's ports in port index order, with no devices, so that SPICE output (capability `netlist-spice-hierarchy`) writes a subcircuit call for each instance and an empty definition for the block. The user MAY enable transistor-level extraction per block; it SHALL then be extracted from the full representation like any read-only cell, and SHALL be refused with a diagnostic when the block has no GDS view.

Compatibility: native

(LibreLane ref: librelane/steps/magic.py:575-640)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Block written as a subcircuit call
- **GIVEN** parent `top` with instance `u_spm` of block `spm` whose ports, in index order, are `VPWR`, `VGND`, `a`, `y`, connected to parent nets `vccd1`, `vssd1`, `in1`, `out1`
- **WHEN** `top` is extracted and written as hierarchical SPICE
- **THEN** the netlist SHALL contain an empty subcircuit `spm` with ports `VPWR VGND a y` and the call `Xu_spm vccd1 vssd1 in1 out1 spm`

### Requirement: LVS with black-boxed blocks
LVS of a layout containing blocks (capability `lvs-command`) SHALL, by default, compare each block as a black box (capability `lvs-hierarchy`, Requirement "Black boxes and flattening prohibitions"): the layout side uses the empty subcircuit of Requirement "Extraction with blocks", and the schematic side SHALL use an empty subcircuit with the same name. Hefesto SHALL build the schematic-side port list, in this order of preference, from the block's `spice` or `cdl` view (top `.subckt` ports), its `pnl` view (module ports), or its LEF pins in index order. When only the `nl` view (unpowered netlist) is available, supply pins SHALL be taken from the LEF and a warning SHALL state that the netlist has no supply pins.

Black-box comparison checks only pin connections. Hefesto SHALL therefore show, next to the LVS result, the block's own sign-off evidence: the `design__lvs_error__count`, `magic__drc_error__count` and `klayout__drc_error__count` values from the block's `metrics` attachment, or "no sign-off evidence" when it has none.

The user MAY instead request hierarchical LVS of a block, which compares the block's full-representation extraction against its `spice`, `cdl` or `pnl` view; Verilog views SHALL be read as specified by capability `lvs-netlist-input`.

Compatibility: native

(LibreLane ref: librelane/steps/netgen.py:140-160, librelane/steps/netgen.py:255)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Pin swap at a block
- **GIVEN** a schematic where instance `u_spm` connects `a` to `in1` and `y` to `out1`, and a layout where `u_spm/a` is wired to `out1` and `u_spm/y` to `in1`
- **WHEN** LVS runs with `spm` black-boxed
- **THEN** LVS SHALL fail, and the mismatch SHALL be reported at the pins of `u_spm`

#### Scenario: No sign-off evidence
- **GIVEN** a block imported from loose files without `metrics.json`
- **WHEN** LVS of the parent completes
- **THEN** the result SHALL show "no sign-off evidence" for that block

### Requirement: Schematic symbols for blocks
Hefesto SHALL generate a schematic symbol for a block for the built-in schematic editor (decision P-12), with one pin per block port: inputs on the left, outputs on the right, inouts and analog pins on the right below the outputs, power pins on top and ground pins at the bottom, each group in port index order. Bus ports SHALL become one bus pin `name[msb:lsb]` when their indices are contiguous, and individual pins otherwise. The symbol's netlist template SHALL produce a subcircuit call whose port order matches Requirement "LVS with black-boxed blocks".

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-12)

#### Scenario: Bus pin on a symbol
- **GIVEN** block ports `data[0]` to `data[7]` (direction input) and `y` (output)
- **WHEN** the symbol is generated
- **THEN** it SHALL have one input bus pin `data[7:0]` on the left and one output pin `y` on the right

### Requirement: Export for top-level digital flows
Hefesto SHALL export a cell of the user's layout as a macro package for LibreLane. The package SHALL be a folder `<cell>/` containing:
- `gds/<cell>.gds`: GDS of the cell and its hierarchy (capability `gds-write`);
- `lef/<cell>.lef`: LEF of the cell (capability `lef-write`), written with `-hide` by default; the user MAY choose a full LEF (no `-hide`) and `-pinonly`, matching LibreLane's `MAGIC_WRITE_FULL_LEF` and `MAGIC_WRITE_LEF_PINONLY` choices;
- `vh/<cell>.vh`: a Verilog black-box module (Requirement "Verilog black-box module");
- `spice/<cell>.spice`: the extracted SPICE netlist of the cell, when the user includes it;
- `macro.json`: a `MACROS` entry for the cell, `{"<cell>": {"gds": ["dir::gds/<cell>.gds"], "lef": ["dir::lef/<cell>.lef"], "vh": ["dir::vh/<cell>.vh"], "spice": ["dir::spice/<cell>.spice"]}}`, omitting `spice` when it is not exported, written with four-space indentation and one trailing LF;
- the attachments of any blocks inside the cell are not re-exported; the package refers only to the exported cell.

Export SHALL fail, writing nothing, when any block inside the cell lacks a GDS view (see Q-DIGITAL-5), or when the pre-export checks of Requirement "Pre-export pin checks" report an error.

Compatibility: native

(LibreLane ref: librelane/config/variable.py:107-189, librelane/steps/magic.py:234-270, librelane/scripts/magic/lef.tcl:40-64)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Export an analog block
- **GIVEN** cell `ldo` with ports `vin`, `vout`, `en` and supply pins `vdd`, `vss`
- **WHEN** the user exports it without SPICE
- **THEN** the folder `ldo/` SHALL contain `gds/ldo.gds`, `lef/ldo.lef`, `vh/ldo.vh` and `macro.json`, and `macro.json` SHALL have no `spice` field

### Requirement: Verilog black-box module
The exported `.vh` file SHALL have this exact form, with LF line endings:

```
// Black-box module generated by Hefesto <version> from cell <cell>
(* blackbox *)
module <cell> (
`ifdef USE_POWER_PINS
    inout <supply 1>,
    ...
    inout <supply k>,
`endif
    <dir> <name 1>,
    ...
    <dir> <name n>
);
endmodule
```

Rules:
- supply ports (use POWER or GROUND) SHALL be listed in port index order inside the `ifdef USE_POWER_PINS` block, each followed by a comma;
- the other ports SHALL follow in order of their lowest port index, separated by commas, with no comma after the last;
- `<dir>` SHALL be `input`, `output` or `inout` from the port direction, and `inout` when the direction is unknown, `INOUT`, `FEEDTHRU` or tristate output;
- ports named `name[i]` or `name<i>` whose indices form a contiguous range SHALL be written once as `<dir> [msb:lsb] name`, with msb the highest index; other bits SHALL be written as escaped identifiers `\name[i] ` (backslash, name, trailing space);
- a cell with no non-supply ports SHALL NOT be exported, with a diagnostic.

Compatibility: native

(LibreLane ref: docs/source/usage/using_macros.md)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Module text
- **GIVEN** Hefesto version `1.0.0`, cell `ldo` with ports in index order `vdd` (POWER), `vss` (GROUND), `en` (input), `vout` (output)
- **WHEN** the `.vh` file is written
- **THEN** its contents SHALL be:
  `// Black-box module generated by Hefesto 1.0.0 from cell ldo` / `(* blackbox *)` / `module ldo (` / `` `ifdef USE_POWER_PINS `` / `    inout vdd,` / `    inout vss,` / `` `endif `` / `    input en,` / `    output vout` / `);` / `endmodule`, each followed by LF

### Requirement: Pre-export pin checks
Before export, Hefesto SHALL check the cell and report each finding:
- **error**: two ports with the same name and different port indices that are not connected;
- **error**: a port whose label rectangle has zero area, since LEF pins need geometry;
- **error**: a cell bounding box or port rectangle whose coordinates are not multiples of the manufacturing grid;
- **error**: a supply net that is not a port, when the user has selected it as a supply pin;
- **warning**: a port on a layer that has no LEF routing layer name in the technology (capability `lef-tech-section`);
- **warning**: a port that does not touch the cell bounding box, since a router must reach it from inside the macro area;
- **warning**: a block inside the cell whose supply check (Requirement "Supply hookup") reports a problem.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Port off the manufacturing grid
- **GIVEN** manufacturing grid 0.005 µm and port `en` with rectangle (1.0025, 0)–(1.5, 0.5) µm
- **WHEN** export is requested
- **THEN** an error SHALL name `en` and its off-grid coordinate, and no file SHALL be written

### Requirement: Platform availability
Placement, supply checks, extraction and LVS of layouts that contain blocks SHALL work on every platform, because they use only the stored block views. Writing `PDN_MACRO_CONNECTIONS` entries and exporting macro packages for LibreLane are part of the digital flow integration and SHALL be available only in the desktop app; on iPadOS they SHALL follow capability `hefesto-ipad-feature-scope`. See Q-DIGITAL-1.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: LVS with a block on iPad
- **GIVEN** a project with block `spm` placed in `top`, opened on iPadOS
- **WHEN** the user runs LVS of `top`
- **THEN** LVS SHALL run with `spm` black-boxed and give the same result as on the desktop
