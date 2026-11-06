# Agent Device and Cell Generation

## Purpose
Lets the agent build devices and cells from a description or from a SPICE subcircuit. Examples: "nfet W=2 µm L=0.15 µm with 4 fingers", "a 1 pF MIM capacitor", "an inverter cell matching this subckt". Wherever the PDK provides a device generator, the agent SHALL use it, so that device geometry comes from the PDK's own validated generators rather than from model-invented polygons.

## Requirements

### Requirement: Prefer PDK generators
To create a device type, the agent SHALL check the PDK's device generators (capabilities `pcell-toolkit` and `hefesto-scripting`) and SHALL use a generator whenever one exists. It SHALL map the requested parameters onto the generator's parameters and validate them with the generator's check procedure. The agent SHALL draw device geometry polygon by polygon only for structures that no generator covers. Every such polygon SHALL then pass DRC.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Transistor via generator
- **GIVEN** sky130A is loaded
- **WHEN** the user asks for "nfet W=2 µm L=0.15 µm, 4 fingers"
- **THEN** the proposal SHALL instantiate the sky130 nfet generator with w=0.5 and nf=4, the per-finger width according to the generator's convention, and SHALL show the generator's parameter set for review

#### Scenario: Out-of-range parameter
- **WHEN** the requested L is below the PDK minimum
- **THEN** the agent SHALL report the generator's check failure and propose the nearest legal value; it SHALL NOT bypass the check

### Requirement: Cells from a SPICE subcircuit
Given a SPICE `.subckt`, the agent SHALL generate a cell. The cell's ports SHALL match the subckt pins in name and order. It SHALL contain one device per subckt device, created with the matching generator, and wiring that realizes the subckt's connectivity.

The proposal SHALL be marked "verified" only if:
- DRC is clean
- LVS (capability `lvs-command`) between the extracted layout and the subckt reports a unique match

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Inverter from subckt
- **GIVEN** `.subckt inv A Y VDD VSS` with one nfet and one pfet
- **WHEN** the agent generates the cell
- **THEN** the proposal SHALL contain a cell `inv` with ports A, Y, VDD and VSS, and LVS SHALL report a unique match

### Requirement: Style constraints
The user MAY give layout constraints: cell height, track pitch, power-rail layer and width, pin layers and positions, symmetry. The agent SHALL honor them, and SHALL report any constraint that it could not meet.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-10)

#### Scenario: Fixed cell height
- **WHEN** the user requires a cell height of 2.72 µm, with power rails on met1
- **THEN** the generated cell's bounding box height SHALL be 2.72 µm, with VDD and VSS rails on met1 at the top and bottom edges
