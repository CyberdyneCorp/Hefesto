# Hefesto Onboarding and Help

## Purpose
Defines how new users get started and how every user finds help (decision P-16). This covers:
- a new-project wizard
- project templates and example designs per PDK
- interactive tutorials, adapted from Magic's tutorial series
- help text for every command, available in the app and in the CLI

## Requirements

### Requirement: New-project wizard
Hefesto SHALL provide a new-project wizard. The wizard SHALL let the user:
- choose a PDK and a PDK version from the PDK manager, which offers to install the PDK if it is missing
- choose a template
- set the project location
- set the main project settings: units, `magic-compat`, and the agent provider (disabled by default)

The wizard SHALL create a project folder as specified by capability `hefesto-settings`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-16)

#### Scenario: First project on iPad
- **GIVEN** a fresh iPad installation with no PDK
- **WHEN** the user starts the wizard and chooses sky130A with the "analog block" template
- **THEN** the wizard SHALL install sky130A through the PDK manager, and SHALL create a project with a top schematic, a top layout cell and a testbench

### Requirement: Templates and example designs
For each supported PDK, Hefesto SHALL ship these project templates:
- empty layout
- analog block: schematic, layout and testbench
- standard-cell style cell
- shuttle submission

Each template for a shuttle submission SHALL contain the files required by the corresponding program in capability `tapeout-shuttle-packages`. Hefesto SHALL also ship example designs that pass DRC and LVS with the shipped PDK version, for example an inverter, a current mirror and a 5-transistor OTA. CI SHALL keep the examples verified.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-16)

#### Scenario: Examples stay clean
- **WHEN** CI runs against the pinned PDK versions
- **THEN** every example design SHALL be DRC-clean and SHALL give an LVS unique match

### Requirement: Interactive tutorials
Hefesto SHALL provide interactive tutorials. The series SHALL adapt Magic's tutorials to Hefesto's user interface, covering:
- basics
- selection and cells
- hierarchy
- DRC
- extraction
- routing

It SHALL add tutorials on these subjects:
- schematics and simulation
- LVS
- 3D
- tapeout
- the AI agent

Each tutorial SHALL guide the user step by step, check each step against the actual layout state, and work with both desktop input and touch.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-16)

#### Scenario: Step verification
- **WHEN** a tutorial step asks the user to paint a 2×2 µm metal1 square and the user paints 2×3 µm
- **THEN** the tutorial SHALL explain the difference and SHALL NOT advance until the layout matches

### Requirement: Command help
Every command of the command API SHALL have structured help: a synopsis, its options with defaults, examples, and links to related commands. The same help text SHALL be available in each of these places:
- through `help <command>`
- in the command bar, as completion hints
- in a searchable help browser
- in `hefesto-cli --help <command>`

The help SHALL be generated from the same command metadata that the dispatcher uses, so it cannot drift from the implementation.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-16)

#### Scenario: Help matches behavior
- **WHEN** a new option is added to the `box` command's metadata
- **THEN** it SHALL appear in `help box` and in the help browser without any separate documentation change
