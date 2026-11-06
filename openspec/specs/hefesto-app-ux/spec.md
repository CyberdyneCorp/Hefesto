# Hefesto App UX

## Purpose
Defines the information architecture of Hefesto's modern GUI: the main window, panels, browsers and multi-window behavior on desktop and iPad. Magic's Tk wrapper GUI is specified separately for reference (capability `ui-gui-managers`). Hefesto provides equivalent functions in a modern layout. Every interaction maps to commands of the command API.

## Requirements

### Requirement: Main window layout
Each layout document window SHALL contain:
- the layout canvas
- a layer palette showing the tech's layers with their display styles, plus visibility and lock toggles
- a cell hierarchy browser
- a property inspector for the current selection
- a command bar
- a status bar showing cursor coordinates in the current units, the box size, the edit cell and the DRC state

Panels SHALL be collapsible. On iPad, panels SHALL appear as sidebars or popovers that adapt to the size class.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-2)

#### Scenario: Layer palette paint
- **WHEN** the user selects a layer in the palette and invokes "paint"
- **THEN** the `paint <layer>` command SHALL be issued for the box area

### Requirement: Functional parity with Magic's GUI managers
Hefesto SHALL provide the following browsers and dialogs:
- a cell manager and a library manager
- a DRC results browser, which steps through errors in the order of `drc find` and shows the why-text
- a net browser, which selects, highlights and lists terminals
- a text and label helper
- device generator (PCell) dialogs driven by the PDK generators of capability `pcell-toolkit`

These SHALL cover every function of capability `ui-gui-managers`.

Compatibility: behavioral

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: PCell dialog
- **WHEN** the user opens the device generator for sky130 nfet
- **THEN** a dialog SHALL list the generator's parameters with their defaults and checks, and "create" SHALL place the generated cell

### Requirement: Multiple windows and documents
Hefesto SHALL support multiple documents and multiple windows per document. Each window has its own view, root cell and expansion state, as in Magic's multiple layout windows. On iPad, this SHALL use iPadOS scenes, including Stage Manager and external displays.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-2)

#### Scenario: External display on iPad
- **WHEN** an iPad is connected to an external display under Stage Manager
- **THEN** a second layout window SHALL be openable on that display

### Requirement: Accessibility and localization
The GUI SHALL support:
- the platform screen readers: VoiceOver on Apple platforms, AT-SPI on Linux, for all controls and panels
- full keyboard navigation
- Dynamic Type, or the platform font scaling equivalent
- localizable user-interface strings

Layout-data text and command names SHALL NOT be localized.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-2)

#### Scenario: VoiceOver on layer palette
- **WHEN** VoiceOver focuses a layer palette entry
- **THEN** it SHALL announce the layer name and its visibility state
