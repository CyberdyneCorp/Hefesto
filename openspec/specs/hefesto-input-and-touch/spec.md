# Hefesto Input and Touch

## Purpose
Defines how mouse, keyboard, trackpad, touch and Apple Pencil input map onto Magic's tools, macros and commands. Every interaction ends up as a command of the single command API (see capability `hefesto-architecture`). The interaction model is the same on every platform.

## Requirements

### Requirement: Desktop input parity with Magic
On Linux and macOS with a mouse and keyboard, the following SHALL behave as specified by capability `ui-tools-and-input`:
- the default key bindings
- the button bindings of each tool (box, wiring, netlist, rsim)
- user macros

Compatibility: behavioral

(Hefesto decision: docs/decisions/cross-cutting.md#X-5)

#### Scenario: Box tool buttons
- **WHEN** the left mouse button is clicked in a layout window with the box tool active
- **THEN** the box SHALL move by its lower-left corner to the cursor, as in Magic

### Requirement: Touch and Pencil mapping
On iPadOS, every tool action and every default key or button binding SHALL be reachable without a mouse or a hardware keyboard. The mapping SHALL be:
- one-finger drag: the tool's primary action
- two-finger drag: pan
- pinch: zoom
- long press: context menu with the actions of the tool's secondary and tertiary buttons
- Apple Pencil: precise primary actions
- an on-screen command bar: typed commands

A floating modifier key SHALL switch the next touch to middle-button or right-button semantics. Hardware keyboards and pointers on iPad SHALL get desktop bindings.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-5)

#### Scenario: Paint by touch
- **GIVEN** an iPad with no keyboard
- **WHEN** the user sets the box by dragging, then picks a layer and taps "paint" in the context menu
- **THEN** the same `paint` command SHALL be issued that Magic's middle-button action issues

### Requirement: Command entry on every platform
Every platform SHALL provide a command entry for typing any command of the command API. It SHALL support history and completion of command names, cell names and layer names.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-3)

#### Scenario: Typed command on iPad
- **WHEN** a user types `drc why` in the command bar on iPad
- **THEN** the DRC explanation SHALL be shown exactly as on desktop
