# Hefesto Settings

## Purpose
Defines how Hefesto replaces Magic's `.magicrc` and environment-variable configuration with layered settings. The layers are application defaults, user preferences, PDK startup data and per-project settings. Settings sync across a user's devices where the platform allows it.

## Requirements

### Requirement: Layered settings resolution
Settings SHALL be resolved in this order, from lowest to highest precedence:
1. built-in defaults
2. PDK startup data (the effects of the PDK's `.magicrc` or profile)
3. user preferences
4. project settings
5. command-line or session overrides

Each setting SHALL report which layer supplied its effective value.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-4)

#### Scenario: Project overrides user
- **GIVEN** the user preference `units` is `lambda` and the project sets `units` to `microns`
- **WHEN** the project is opened
- **THEN** the effective `units` SHALL be `microns`, reported as coming from the project layer

### Requirement: Magic startup-file compatibility
On every platform, Hefesto SHALL be able to execute Magic-style startup files, in the order defined by capability `app-startup`. These files are the system, user, local and PDK `.magicrc` files. They run through the embedded Tcl interpreter, subject to the sandbox of capability `hefesto-scripting`. Settings changed by these files SHALL land in the corresponding settings layer.

Compatibility: behavioral

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Legacy user .magicrc
- **GIVEN** a user's `~/.magicrc` that defines macros and sets `snap internal`
- **WHEN** Hefesto starts on Linux
- **THEN** the macros and the snap setting SHALL be active

### Requirement: Project files
A project SHALL be a folder with a `hefesto.project` file. The file SHALL record:
- the PDK name and version
- the cell search paths, relative to the project
- project settings, including the reference-tool compatibility settings `magic-compat` and `netgen-compat` (capability `hefesto-magic-compat-mode`), `script-strict` (capability `hefesto-diagnostics`) and the global nets and power names (requirement "Global nets and power names")
- open documents and window layout

The file SHALL be text, with deterministic key order, so that it diffs well under version control.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-8)

#### Scenario: Reopen project
- **WHEN** a project folder is opened on another device
- **THEN** the same PDK, search paths and settings SHALL be applied

### Requirement: Global nets and power names
Hefesto SHALL provide one project-level setting, "global nets and power names", that every layout netlister and LEF/DEF writer uses to decide which nets are global and which nets are power, ground and substrate. It has four fields:

| Field | Value | Built-in default | Replaces Magic's Tcl variable |
|---|---|---|---|
| `globalNets` | list of net names, unique, compared exactly (case-sensitive) | empty | the keys of the array `globals` |
| `vdd` | one net name, or unset | `vdd!` | `$VDD` |
| `gnd` | one net name, or unset | `gnd!` | `$GND` |
| `sub` | one net name, or unset | unset | `$SUB` |

The built-in `vdd` and `gnd` defaults are the values that Magic's system startup file sets (capability `scripting-pdk-integration`, requirement "Startup files are scripts").

**Seeding from the PDK.** The setting follows the layered resolution of requirement "Layered settings resolution". After the startup files have run (capability `app-startup`), Hefesto SHALL read the Tcl variables they set and store them in the layer of the file that set them; a PDK `.magicrc` or PDK script therefore seeds the PDK layer:
- every element name of the array `globals` is added to `globalNets`, in ascending byte order of the names (a Tcl array has no order of its own);
- `$VDD`, `$GND` and `$SUB`, when set, give `vdd`, `gnd` and `sub`.

A variable that no startup file sets leaves the lower layer's value in place. The project layer, edited in the project settings or with the setting commands, overrides the PDK layer.

**Scripts see the setting.** In the embedded Tcl interpreter, the variables `globals`, `VDD`, `GND` and `SUB` SHALL always read as the effective value of the setting (an unset field reads as an unset variable). A script that sets or unsets one of them after startup changes the session override layer; it never rewrites `hefesto.project`.

**Users.** The consumers are:
- capability `ext-flattening-names`, requirement "Global and ground detection": global names and the compatibility-mode ground;
- capability `ext-flattening-hierarchy`, requirement "Global name merging";
- capability `netlist-spice-options`, requirement "Global card in hierarchical mode": the CDL global list;
- capability `lef-write`, requirement "Power and ground pin detection";
- capability `def-write`, requirement "Net classification".

Each of these uses the trailing-`!` rule only where Magic's Tcl build applies it, as an explicit additional rule stated in that requirement. No consumer reads Tcl variables directly.

Compatibility: native

(Hefesto decision: docs/decisions/audit/DECISIONS.md#D-14, docs/decisions/cross-cutting.md#X-4)

#### Scenario: Seeded from the sky130 startup file
- **GIVEN** a project with no global-net settings of its own, using a PDK whose `.magicrc` runs `set VDD VPWR`, `set GND VGND` and `set SUB VSUBS`
- **WHEN** the project is opened
- **THEN** the effective setting SHALL be `vdd` = `VPWR`, `gnd` = `VGND`, `sub` = `VSUBS` and `globalNets` empty, each reported as coming from the PDK layer

#### Scenario: Globals array becomes a sorted list
- **GIVEN** a PDK script that runs `set globals(VPWR) 1` and then `set globals(VGND) 1`
- **WHEN** startup completes
- **THEN** `globalNets` SHALL be `VGND`, `VPWR`

#### Scenario: Project overrides the PDK
- **GIVEN** the PDK sets `$GND` to `VGND` and the project sets `gnd` to `VSS`
- **WHEN** a script reads `$GND`
- **THEN** it SHALL read `VSS`, and LEF output SHALL write `USE GROUND` for a default-use pin labeled `VSS`

### Requirement: Preference sync
User preferences SHALL sync through the platform account mechanism (iCloud key-value or document storage on Apple platforms) where available, and SHALL be exportable and importable as a file on all platforms.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-8)

#### Scenario: Mac to iPad
- **WHEN** a user changes their macro bindings on a Mac
- **THEN** the same bindings SHALL appear on their iPad after sync
