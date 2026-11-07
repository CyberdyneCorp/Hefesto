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

Panels SHALL be collapsible. On the desktop, the window SHALL be built from Raven widgets: dock area, splitters, tabs and tree/table views. Panels SHALL be dockable, and the UI theme SHALL use Raven's theme system, including light and dark themes. On iPad, panels SHALL be SwiftUI sidebars or popovers that adapt to the size class. Visual styling follows capability `hefesto-design-system`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-11, docs/decisions/product.md#P-23)

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

### Requirement: Desktop platform integration through Raven
On the desktop, the shell SHALL use Raven's platform features for these purposes:
- **Tables:** result and list browsers SHALL use Raven's Table widget with virtual rows, sortable and resizable columns, keyboard and screen-reader support. These include the DRC results, LVS mismatches, cell and library managers, net browser, and simulation measurements.
- **Menu bar:** on macOS, the application menu SHALL appear in the system menu bar.
- **Job completion:** when a long job finishes while the window is not focused, Hefesto SHALL post a system notification (macOS Notification Center, freedesktop notifications on Linux). Long jobs include DRC, extraction, LVS, simulation sweeps or Monte Carlo, tapeout packaging, and agent proposals. A setting SHALL turn this off.
- **Job progress:** a running long job SHALL show its progress on the Dock icon (macOS) or the launcher entry (Linux).
- **Developer inspector:** Raven's widget inspector MAY be enabled in development builds. It SHALL be absent from release builds.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-11)

#### Scenario: Extraction finishes in the background
- **GIVEN** a full-chip extraction is running and the user has switched to another application
- **WHEN** the extraction finishes
- **THEN** a system notification SHALL report the result, and the Dock or launcher progress indicator SHALL be cleared

#### Scenario: Large DRC result list
- **WHEN** a DRC run reports 200,000 errors
- **THEN** the DRC browser SHALL list them in a virtual Table, without creating a widget per row, and SHALL sort by rule or by count

### Requirement: Accessibility and localization
The GUI SHALL support:
- the platform screen readers, for all controls and panels: on the desktop through Raven's accessibility support (VoiceOver on macOS, AT-SPI on Linux), on iPad through SwiftUI accessibility
- full keyboard navigation
- Dynamic Type on iPad; on the desktop, Raven's text scale
- on the desktop, Raven's high-contrast themes, color filters for color blindness, and reduced motion, applied to the UI chrome only (layer colors on the canvas follow `hefesto-design-system`)
- localizable user-interface strings

Input methods for Chinese, Japanese and Korean text depend on Raven, which does not connect them yet on the desktop (decision P-11, risks). Until it does, entering such text in labels is unsupported on the desktop.

Layout-data text and command names SHALL NOT be localized.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-2, docs/decisions/product.md#P-11)

#### Scenario: VoiceOver on layer palette
- **WHEN** VoiceOver focuses a layer palette entry
- **THEN** it SHALL announce the layer name and its visibility state
