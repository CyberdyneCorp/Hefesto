# M4 UI conformance

How the M4 desktop screens (menu bar, Place device and the PCell dialog, script consent, the GUI managers, the Tcl console) match the forge design system (`openspec/specs/hefesto-design-system/spec.md`, decision P-23) and the M4 boards drafted under [mockups/](mockups/) (`openspec/changes/m4-scripting/design.md` D13). Same format and rules as [m3-ui-conformance.md](m3-ui-conformance.md), [m2-ui-conformance.md](m2-ui-conformance.md) and [m1-ui-conformance.md](m1-ui-conformance.md), which stay the records for the earlier screens; the items carried from M2 (design D17) are closed in [m2-ui-conformance.md](m2-ui-conformance.md) by CARRY.

Every row names one board element, how the shell implements it, and any gap with its reason. Where Raven cannot match a board, the shell does the closest faithful thing and the gap is recorded here; a gap that needs a Raven change is also reported as an open item (proposed upstream, never patched locally).

Owner: SHELL (seeded by the M4 scaffold, task 2.7; completed in task 10.6). The "M4 boards" section belongs to DESIGN (task 3.1) and "Desktop Alpha" is signed off by the user (design D18). Status values:

- **done**: seen in a capture and matching the board within the noted tolerance.
- **partial**: built, with a gap recorded.
- **later**: present but disabled; the function arrives in the milestone named.
- **blocked**: built in the shell, but the core function it needs is not available yet (a `CORE-NOTIMPL-004` stub during M4).
- **todo**: not built yet.

Nothing is claimed from code reading alone: a row is **done** only after a capture of the product app (`hefesto ... --capture`) was compared with its board, as in M1–M3.

## How this was checked

To be written by SHELL (task 10.6): machine, build, PDK version, the `--run` command lines and the captures in `docs/images/app/m4-*.png`, as in M3.

## M4 boards (DESIGN)

Owner of this section: DESIGN (task 3.1). Each SHELL screen state of tasks.md section 10 maps to a board; the user approves the boards before SHELL builds a screen (design D13).

| Board | States | Status |
|---|---|---|
| `M4-Menu-Bar.dc.html` | menu bar File/Edit/Cell/Window/Layers/Drc/Options (in-window on Linux, native on macOS), PDK `Devices` menus, intercepted `DRC Routing` and `Import SPICE` items, skipped-statement report | drafted, waiting for the user's review |
| `M4-PCell-Dialog.dc.html` | Place device (tool rail, menus); dialog new, edit, invalid value, dependency, headless error; instance name; Create / Apply / Close | drafted, waiting for the user's review |
| `M4-Script-Consent.dc.html` | consent prompt (script origin, PDK version; store and auto-discovered variants), trusted PDKs in the PDK panel, declined read-only state, sandbox-denial diagnostic, long-running script with interrupt | drafted, waiting for the user's review |
| `M4-Managers.dc.html` | cell, library and technology managers, text helper, layer toolbar and reorder dialog (the DRC manager is the M3 DRC panel) | drafted, waiting for the user's review |
| `M4-Console.dc.html` | Tcl console: multi-line input, stdout/stderr, interrupt, window command entry | drafted, waiting for the user's review |

Mapping of the SHELL tasks to boards (DESIGN, 2026-11-11): 10.1 → `M4-Menu-Bar`; 10.2 → `M4-PCell-Dialog`; 10.3 → `M4-Script-Consent`; 10.4 → `M4-Managers` (DRC manager: `DRC-Browser.dc.html` of M3); 10.5 → `M4-Console`. The `platform::FilePicker` host of 10.5 is the platform's native file dialog (as in M1), so it has no board. Renders: `docs/images/ui/m4-*.png` (dark) and `m4-*-light.png`.

## Core interfaces the screens render

Landed by the scaffold (tasks.md 2.6; contracts in `openspec/changes/m4-scripting/design.md` "As-landed scaffold contracts"). Each screen issues logged commands only (design D13).

| Screen | Core model or host | Header | Owner of the model |
|---|---|---|---|
| Menu bar | `gui::WrapperMenuModel`, `script::MenuModel`, `pcell::ToolkitMenus`; `script::ScriptEvents::on_menu_model_changed` | `hefesto/gui/managers.hpp`, `hefesto/script/pdk/widgets.hpp`, `hefesto/pcell/framework/menus.hpp` | GUIMGR, PDKINT, PCELL |
| PCell dialog | `pcell::DialogModel` (in `EditorState::script.gencell.dialog`) | `hefesto/pcell/framework/dialog.hpp` | PCELL |
| Script consent | `script::ConsentHost` (SHELL implements), `script::TrustStore`, `ScriptEvents::on_consent_requested` / `on_denied` | `hefesto/script/pdk/consent.hpp`, `hefesto/script/pdk/trust.hpp`, `hefesto/script/events.hpp` | PDKINT (TCL for denials) |
| Managers | `gui::CellManagerModel`, `LibraryManagerModel`, `TechManagerModel`, `DrcManagerModel`, `TextHelperModel`, `ToolbarModel` | `hefesto/gui/managers.hpp` | GUIMGR |
| Tcl console | `script::Interpreter::evaluate` / `request_interrupt`, `script::OutputRouting` | `hefesto/script/interp.hpp`, `hefesto/script/binding/output.hpp` | TCL, BINDING |
| File prompts | `platform::FilePicker` (SHELL implements) | `hefesto/platform/file_picker.hpp` | INTEGRATION |

## Menu bar (M4-Menu-Bar)

| Element | Implemented as | Status / gap |
|---|---|---|
| (rows added by SHELL, task 10.1) | | todo |

## Place device and PCell dialog (M4-PCell-Dialog)

| Element | Implemented as | Status / gap |
|---|---|---|
| Tool rail "Place device" (disabled "arrives in M4" since M2) | | todo |
| (rows added by SHELL, task 10.2) | | todo |

## Script consent (M4-Script-Consent)

| Element | Implemented as | Status / gap |
|---|---|---|
| (rows added by SHELL, task 10.3) | | todo |

## Managers (M4-Managers)

| Element | Implemented as | Status / gap |
|---|---|---|
| (rows added by SHELL, task 10.4) | | todo |

## Tcl console (M4-Console)

| Element | Implemented as | Status / gap |
|---|---|---|
| (rows added by SHELL, task 10.5) | | todo |

## Desktop Alpha

The manual checklist of design D18, run through the GUI on Linux and macOS and signed off by the user:

| Step | Linux | macOS |
|---|---|---|
| Open a sky130A, gf180mcuD and ihp-sg13g2 design; the consent prompt names the PDK version | todo | todo |
| The PDK's `Devices` menus appear and their items open the Place device dialog | todo | todo |
| Place a native-generator device and a Tcl-only device through the dialog | todo | todo |
| DRC-check the result; the DRC browser lists the errors | todo | todo |
| Save and reopen; the cells and properties are unchanged | todo | todo |

## Raven gaps and spec/board differences

Reported as open items by SHELL (task 10.6).
