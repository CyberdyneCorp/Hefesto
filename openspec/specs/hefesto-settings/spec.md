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
- project settings, including `magic-compat`
- open documents and window layout

The file SHALL be text, with deterministic key order, so that it diffs well under version control.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-8)

#### Scenario: Reopen project
- **WHEN** a project folder is opened on another device
- **THEN** the same PDK, search paths and settings SHALL be applied

### Requirement: Preference sync
User preferences SHALL sync through the platform account mechanism (iCloud key-value or document storage on Apple platforms) where available, and SHALL be exportable and importable as a file on all platforms.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-8)

#### Scenario: Mac to iPad
- **WHEN** a user changes their macro bindings on a Mac
- **THEN** the same bindings SHALL appear on their iPad after sync
