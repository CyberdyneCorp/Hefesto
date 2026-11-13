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

The passes below give the machine, build, PDK version, the command lines and the captures in `docs/images/app/m4-*.png`, as in M3. The latest is the SCREENS polish pass (2026-11-12), which captured the menu bar, the PCell dialog, the text helper, the reorder dialog, both consent variants and the PDK trust rows.

**SHELL-B pass (managers and Tcl console, tasks 10.4–10.5)**, 2026-11-11, macOS 27 arm64 (Apple M3 Pro, MoltenVK, Retina 2×, 1600×1000 logical), build `build/m4r2-shellb` (Debug, `-DHEFESTO_WARNINGS_AS_ERRORS=ON`, desktop on), branch `m4-round2`. sky130A at the new pin (ciel `5740c4a8`, `sky130A.tech` given with `-T`; its rc file is not used, so no consent prompt); `CAD_ROOT` points at a folder holding Magic's `mos.OpenGL.*` styles and glyph files from `build/ref/magic`. Two small cells `inv` (m1 and poly) and `nand` (li) were saved first, then:

```
CAD_ROOT=<dir> hefesto -T <pin>/sky130A/libs.tech/magic/sky130A.tech --width 1600 --height 1000 [--console] \
  --run 'load top; box 0 0 0 0; getcell inv; box 4um 0 4um 0; getcell inv; box 8um 0 8um 0; getcell nand; select clear; view; <shown>' \
  --capture out.ppm --capture-frames 30
```

| Capture | `<shown>` | Board tile |
|---|---|---|
| [m4-managers-cells.png](../images/app/m4-managers-cells.png) | `cellmanager create; techmanager create` | M4-Managers 1 (Cells tab) and 3 (technology manager) |
| [m4-managers-libraries.png](../images/app/m4-managers-libraries.png) | `libmanager create; techmanager create` (`--console`) | M4-Managers 2 (Libraries tab) |
| [m4-console.png](../images/app/m4-console.png) | `tech name; box values 0 0 84 330; cellmanager create; techmanager create` (`--console`) | M4-Console 1 (transcript, header, input) |

Not reachable from `--run` (no key reaches the window) and covered by tests instead: typed Tcl in the console (multi-line Return / Shift-Return, `puts`, results, errors and sandbox denials, history, queueing, interrupt: `tests/unit/shell/console_test.cpp`, with a real project interpreter), the Interrupt card (shown 2 s into a run; `ConsoleRun` tests with a fake clock), the text helper and reorder windows (utility windows opened by `magic::update_texthelper` and `magic::reorderToolbar`; their models are `text_helper_view` / `toolbar_views` tests), the native file dialog (`file_picker_host_test.cpp`).

**Integration pass (round 2)**, 2026-11-11, same Mac, build `build/m4` (Debug, `-Werror`, desktop on), branch `m4-round2` with every workstream merged. sky130A at ciel `5740c4a8`; `CAD_ROOT` as above; the cell is `sky130_fd_sc_hd__inv_1` copied from the pinned `libs.ref`. Captures (not committed; the window capture shows the main window only, and screen recording is not permitted on this Mac, so the native menu bar and the separate PCell dialog window could not be captured):

```
# consent prompt and declined state: the PDK's rc file through -rcfile; capture mode answers Esc (Don't run)
CAD_ROOT=<cad> PDK_ROOT=<pin> hefesto -rcfile <pin>/sky130A/libs.tech/magic/sky130A.magicrc --width 1600 --height 1000 --console sky130_fd_sc_hd__inv_1 --capture main-consent-declined.ppm --capture-frames 40
# PDK scripts run (the rc copied to ./.magicrc, a local startup file: no consent) and a gencell dialog requested
CAD_ROOT=<cad> PDK_ROOT=<pin> hefesto --width 1600 --height 1000 --console sky130_fd_sc_hd__inv_1 --run 'magic::gencell sky130::sky130_fd_pr__nfet_01v8' --capture main-pdk-loaded-pcell.ppm --capture-frames 40
```

Differences from the boards seen in these captures:

| Board element | Seen | Difference |
|---|---|---|
| M4-Script-Consent 1: centered modal alert with a shield icon over a scrim | Raven `Rdialog` drawn as a full-width card across the window, no icon, no scrim | Raven dialog style (gap); content and answers match |
| M4-Script-Consent 2: origin block with "Then runs sky130A.tcl + 8 part files" and "digest verified" | Script, PDK, Release rows; the path is middle-ellipsized; "digest not checked" | no "Then runs" row; the release digest is not looked up for a `-rcfile` start (resolved in the SHELL closing pass: both rows shown, `m4-consent-origin.png`) |
| M4-Script-Consent 3: Don't run left, Trust and Run once right | the three buttons together on the right, Run once highlighted | button grouping (Raven dialog layout) |
| M4-Script-Consent 6: read-only chip, header "minimum · read-only", empty canvas "Nothing to draw", editing tools disabled | the banner and "Review scripts..." show, but the header still reads "sky130A", the sky130A layers are drawn, the tools are enabled | the declined layout keeps the current technology (no TECH entry point to switch to `minimum`) and nothing stops editing or saving yet (SHELL-A/PDKINT open items); the banner text claims `minimum` (round 3: resolved; header, chip, tools and Save follow the board, see the Script consent table) |
| M4-Script-Consent 7: denial toast "Show" | a SCRIPT-TRUST-001 warning in the status bar | the decline is a warning, not a denial: as designed |
| M4-Console 1 | the Console tab shows the rc's `puts` lines in order ("Sourcing design .magicrc…", "Loading sky130A Device Generator Menu…") and a typed `magic::gencell` with its result | matches; the run line comes from `--run` (echoed with `%`) |
| M4-PCell-Dialog | `magic::gencell sky130::sky130_fd_pr__nfet_01v8` returns 0 and opens the dialog in its own utility window | the dialog window is not in a `--capture` image (main window only) |
| M4-Menu-Bar | the PDK's device menus are in the menu model after startup (`gui::adopt_wrapper`, unit-tested) | the macOS menu bar is the system bar, outside the window capture: not captured |

**Integration pass (round 3)**, 2026-11-11, same Mac, build `build/m4` (Debug, `-Werror`, desktop on), branch `m4-round3`. sky130A at ciel `5740c4a8`; `CAD_ROOT` as above; `HOME` a scratch folder. The desktop takes the PDK store from `PDK_ROOT`, and the sky130A rc file reads `$PDK_ROOT/sky130A` (a `ciel enable` layout), so the captures use a scratch store whose `ciel` links to `.cache/pdks/ciel` and whose `sky130A` holds a copy of `libs.tech/magic`; trust was given with `hefesto-cli -c "pdk trust sky130A <version>"` and the store's trust records were restored afterwards. Captures (not committed, in the integration scratchpad `m4r3-shots/`):

```
CAD_ROOT=<cad> HOME=<home> PDK_ROOT=<store> hefesto -rcfile <store>/ciel/sky130/versions/<v>/sky130A/libs.tech/magic/sky130A.magicrc \
  --width 1600 --height 1000 sky130_fd_sc_hd__inv_1 --console --run '<shown>' --capture out.ppm [--capture-frames 40 | --capture-running]
```

| Capture | State | `<shown>` | Board tile |
|---|---|---|---|
| m4-trusted-console.png | trusted | `magic::cellmanager create; magic::techmanager create` | M4-Managers 1 and 3 (technology description without braces) |
| m4-interrupt-card.png | trusted, `--capture-running` | a 4 s `orig_clock` loop | M4-Script-Consent 7 (Interrupt card) |
| m4-consent-over-declined.png | not trusted, `PDK_ROOT=.cache/pdks` | `cellmanager create` | M4-Script-Consent 1–3 (store variant) over 6 |
| m4-declined-readonly.png | not trusted, scratch store | `magic::cellmanager create` | M4-Script-Consent 6 and the auto-discovered (outside the store) prompt |

**SHELL closing pass**, 2026-11-11, same Mac, build `build/mc-shell` (Debug, `-Werror`, desktop on), branch `m4-close-shell`. `CAD_ROOT` as above, `HOME` a scratch folder, the cell `sky130_fd_sc_hd__inv_1` copied from the pinned `libs.ref`. Captures (committed, 1600 px wide):

```
# store prompt: PDK_ROOT=.cache/pdks (provenance records with SHA-256 digests), not trusted; capture mode answers Esc
CAD_ROOT=<cad> HOME=<home> PDK_ROOT=.cache/pdks hefesto -rcfile .cache/pdks/ciel/sky130/versions/5740c4a8…/sky130A/libs.tech/magic/sky130A.magicrc \
  --width 1600 --height 1000 sky130_fd_sc_hd__inv_1 --console --capture m4-consent-origin.ppm --capture-frames 40
# a long-running script: an edit, then a pure-Tcl loop of several seconds, captured while it runs
CAD_ROOT=<cad> HOME=<home> hefesto -T <pin>/sky130A/libs.tech/magic/sky130A.tech --width 1600 --height 1000 sky130_fd_sc_hd__inv_1 \
  --run 'box 0 0 300 60; paint metal2; for {set i 0} {$i < 40000000} {incr i} {}' --capture m4-script-dimmed.ppm --capture-running
```

| Capture | Board tile | Seen |
|---|---|---|
| [m4-consent-origin.png](../images/app/m4-consent-origin.png) | M4-Script-Consent 1–2 (store variant) | Script, PDK, "Release: ciel 5740c4a · ● digest verified" and "Then runs: sky130A.tcl + 8 part files (device generators)", as on the board; still Raven's dialog card (no shield, no scrim, buttons grouped right) |
| [m4-script-dimmed.png](../images/app/m4-script-dimmed.png) | M4-Script-Consent 7 and note 8 | the canvas dimmed under the Interrupt card (the layout at about 35 %), the metal2 painted by the running script already drawn (canvas and minimap refreshed in the pumped frame), the header current. Differences: the card sits at the canvas's top right (board: top centre); the side panels (Layers, Inspector) are not rebuilt while the script runs and show their state from before it until it ends (their widgets may belong to the callback that started the script) |

Not reachable from `--run` (no key or pointer reaches the window) and covered by tests: Tab completion in the console (`console_test.cpp`, with a real interpreter), press-and-hold on Undo (`PressHold`, `screen_models_test.cpp`), the undo history named after commands (`screen_models_test.cpp`), the Label popover's flip at the canvas edge (`label_popover_point`, now fed by `LayoutCanvas::label_point`).

Differences seen in round 3: the prompt is still Raven's dialog card (no shield, no scrim; round 2 gap); `(UNNAMED)` is listed in the Cells manager (core fix, M4 closing round CODE: a session with a display now deletes the untouched `(UNNAMED)` on `load`, as Magic does under X11; NULL-graphics sessions keep it as `magic -dnull` does; re-capture pending); the unqualified `cellmanager` is not a global command, as in Magic (`cellmgr.tcl` defines `magic::cellmanager` after the global import; the menus call it qualified), so `--run` lines use `magic::cellmanager`; the native macOS menu bar and the PCell dialog window are outside `--capture` (no in-window menu bar option on macOS). The CARRY captures of the same folder (`panels-*`, `key-sheet`, `wiring`) are recorded in [m2-ui-conformance.md](m2-ui-conformance.md).

**SCREENS polish pass**, 2026-11-12, same Mac (macOS 27 arm64, Apple M3 Pro, MoltenVK, Retina 2×), build `build/mp-screens` (Debug, `-DHEFESTO_WARNINGS_AS_ERRORS=ON`, desktop on), branch `m4-polish-screens`. sky130A at ciel `5740c4a8`; `CAD_ROOT` a folder holding Magic's `mos.OpenGL.*` styles and glyph files from `build/ref/magic`; `HOME` a scratch folder. Three new capture options make the remaining screens reachable (`hefesto --help`; `options_test.cpp`):

- `--menu-bar-in-window`: the menu bar is drawn in the window on macOS too (Raven's `Rmenu_bar_set_native(bar, false)`), so `--capture` sees it. The macOS default stays the system bar.
- `--press X,Y`: after `--run`, the left button is pressed at a window point and kept down (Raven event injection), once the menu bar has not changed for three frames (a rebuilt bar closes an open menu). A press on a title of the in-window bar opens that menu.
- `--capture-window main|pcell|texthelper|reorder`: `--capture` saves the named M4 utility window instead of the main window, once it is open.

Captures (committed, main window 1600 px wide, utility windows at 1×):

```
# trusted startup: the rc copied to ./.magicrc (local startup file, no consent), PDK_ROOT a scratch store whose sky130A holds a copy of libs.tech/magic
CAD_ROOT=<cad> HOME=<home> PDK_ROOT=<store> hefesto --menu-bar-in-window --width 1600 --height 1000 sky130_fd_sc_hd__inv_1 --press <x>,63 --capture out.ppm --capture-frames 50
CAD_ROOT=<cad> HOME=<home> PDK_ROOT=<store> hefesto --width 1600 --height 1000 sky130_fd_sc_hd__inv_1 --run '<shown>' --capture-window <window> --capture out.ppm --capture-frames 50
# consent prompts: PDK_ROOT=.cache/pdks; the store rc (store variant) or a copy of libs.tech/magic outside the store (outside variant); capture mode answers Esc
CAD_ROOT=<cad> HOME=<home> PDK_ROOT=.cache/pdks hefesto -rcfile <rc> --width 1600 --height 1000 sky130_fd_sc_hd__inv_1 --console --capture out.ppm --capture-frames 40
# PDK panel: PDK_ROOT=.cache/pdks, --pdk-manager; the running script: as in the SHELL closing pass (--capture-running)
```

| Capture | How | Board tile | Seen / difference |
|---|---|---|---|
| [m4-menu-bar-devices.png](../images/app/m4-menu-bar-devices.png) | `--press 385,63` | M4-Menu-Bar 1–2 (in-window bar, Devices 1 open) | File … Options, then Devices 1 and Devices 2 from the PDK; the Devices 1 menu lists nmos (MOSFET) … deep n-well region with Magic's separators. Differences: no divider, ember title tint or "Devices 1 · pdk1" source row (Raven gap, draft 10); the in-window bar has no accelerators (draft 8) |
| [m4-menu-bar-file.png](../images/app/m4-menu-bar-file.png) | `--press 20,63` | M4-Menu-Bar 3 (intercepted Import SPICE) | Open…, Save…, Flush changes, Import SPICE (PDK) between separators, Read/Write GDS disabled (M5), Save All and Quit, Quit, as on the board; "(PDK)" in the label instead of the board's chip (draft 10) |
| [m4-menu-bar-drc.png](../images/app/m4-menu-bar-drc.png) | `--press 263,63` | M4-Menu-Bar 3 (DRC Routing last in Drc) | DRC update, report, Find next error, Fast, Complete, Routing (PDK) last, as on the board |
| [m4-pcell-dialog.png](../images/app/m4-pcell-dialog.png) | `--run 'magic::gencell sky130::sky130_fd_pr__nfet_01v8' --capture-window pcell` | M4-PCell-Dialog 3–5 (new device) | header with muted keys and mono values, the generator chip text in its status color, rules between header, body and buttons, 12 px rows, 150 px mono entries, the select list 195 px, Reset left and Close / Create / Create and Close right. Differences: Raven controls are 28 px tall (board 22–24 px; theme metric shared by every control); "◆ Tcl generator" is drawn in the info color (board: neutral chip); rows are widgets, not a virtual grid; no warning outline (draft 12) |
| [m4-text-helper.png](../images/app/m4-text-helper.png) | `--run 'magic::update_texthelper' --capture-window texthelper` | M4-Managers 4 | Magic's fields with muted 12 px keys, mono entries sized to their values, the 3×3 justification grid as small buttons plus center / default, default / sticky, Port enable with its index, Apply and Okay left, Cancel right. Differences: the board's "New label" chip is a muted line; the preview line is empty with no label selected (it shows the `setlabel` / `port make` line once fields change); the selected justification is not drawn in the accent (Raven buttons have no checked state outside a toggle; draft 13) |
| [m4-reorder.png](../images/app/m4-reorder.png) | `--run 'magic::reorderToolbar' --capture-window reorder` | M4-Managers 6 | search and preset on one row, the toolbar table with Move up / Move down / Group with above / Delete layer in a column at its right, the ◆ presets note, Load and Save left, Confirm right. Differences: no drag handles and single selection (G-3); Move layer is two buttons |
| [m4-consent-alert.png](../images/app/m4-consent-alert.png) | store rc, `--console` | M4-Script-Consent 1–3 | the alert as on the board: shield icon in its bordered box, the question, the body, the origin block (inset, keys muted, values mono, "● digest verified" chip, "+ 8 part files (device generators)" muted), Don't run left, Trust sky130A 5740c4a and Run once right, 600 px, centred over Raven's veil (the window under it is darkened by 42 %: the top bar #15181C reads #0C0E10). Difference: the script path is middle-ellipsized and starts with `~` only under `$HOME` (this run's HOME was a scratch folder) |
| [m4-consent-outside.png](../images/app/m4-consent-outside.png) | the rc copied outside the store, `-rcfile` | M4-Script-Consent 4 | 436 px alert, "sky130A · version unknown", the ▲ warning chip "not in the PDK store · not verified", Don't run (default) and Run once. Differences: no "Found via" row because a `-rcfile` start is not auto-discovered (as designed: the row is for `implicit` requests); the default button is drawn filled (Raven's primary style) where the board shows the focus ring on an outlined button |
| [m4-pdk-trust.png](../images/app/m4-pdk-trust.png) | `--pdk-manager`, `PDK_ROOT=.cache/pdks` | M4-Script-Consent 5 | each installed version shows its install state and, on a line of its own, "◆ scripts ask before running" (or "● scripts trusted <date>" in the success color); the library count right of the title. Fixed on the way: the install state was centred over both lines and overlapped the scripts state since M4 added it. Difference: three lines instead of the board's two (the version, date and scripts state do not fit one line of the 300 px dock); Trust scripts / Revoke trust is the panel's action button for the selected row, not a button per row |
| [m4-managers-cells.png](../images/app/m4-managers-cells.png) | trusted startup, `--run 'magic::cellmanager create; magic::techmanager create'` | M4-Managers 1 and 3 | the Instance column narrowed to 74 px so `sky130_fd_sc_hd__inv_1` fits the 240 px dock. Differences: `(UNNAMED)` is still listed for a cell given on the command line (the CODE fix deletes it on `load`; a file argument is opened by the startup path, not `load`: CELL open item); the technology manager's CIF/extract/DRC values are Magic's style names (`sky130()`, `drc(fast)`) as menus, not the board's "sky130 · 0.01" select fields; the title wraps in the 300 px dock |
| [m4-script-dimmed.png](../images/app/m4-script-dimmed.png) | as in the SHELL closing pass | M4-Script-Consent 7–8 | the Interrupt card now at the canvas's top centre (board), the canvas dimmed. The in-window menu bar shows because `--capture-running` grabs one of the first frames, before Raven hands the bar to the system |

**M4 follow-up pass**, 2026-11-12, same Mac, build `build/m4` (Debug, `-DHEFESTO_WARNINGS_AS_ERRORS=ON`, desktop on), branch `m4-followup`. `(UNNAMED)` no longer appears for a cell given on the command line: the desktop opens its layout window before startup loads the file argument, with no root, and `load` created `(UNNAMED)` for that window without showing it in it, so Magic's rule (DBWloadWindow deletes the window's untouched `(UNNAMED)` while the display runs) never applied. The window now shows `(UNNAMED)` first, as Magic's DBWcreate does, and the load deletes it (`cell/commands.cpp` `adopt_startup_root`; regression test "a window opened without a cell loses its (UNNAMED) to the first load" in `tests/unit/cell/commands_test.cpp`). Capture:

```
CAD_ROOT=<cad> HOME=<home> hefesto -T <pin>/sky130A/libs.tech/magic/sky130A.tech <pin>/sky130A/libs.ref/sky130_fd_sc_hd/mag/sky130_fd_sc_hd__dfxtp_1.mag \
  --width 1600 --height 1000 --console --run 'magic::cellmanager create; puts stdout "CELLS: [cellname list allcells]"' --capture out.ppm --capture-frames 30
```

| Capture | Board tile | Seen |
|---|---|---|
| [m4-followup-cells.png](../images/app/m4-followup-cells.png) | M4-Managers 1 | the Cells tab lists `sky130_fd_sc_hd__dfxtp_1 (top cell)` only; the Console prints `CELLS: sky130_fd_sc_hd__dfxtp_1` |

## M4 boards (DESIGN)

Owner of this section: DESIGN (task 3.1). Each SHELL screen state of tasks.md section 10 maps to a board; the user approves the boards before SHELL builds a screen (design D13).

| Board | States | Status |
|---|---|---|
| `M4-Menu-Bar.dc.html` | menu bar File/Edit/Cell/Window/Layers/Drc/Options (in-window on Linux, native on macOS), PDK `Devices` menus, intercepted `DRC Routing` and `Import SPICE` items, skipped-statement report | approved by the user, 2026-11-11 |
| `M4-PCell-Dialog.dc.html` | Place device (tool rail, menus); dialog new, edit, invalid value, dependency, headless error; instance name; Create / Apply / Close | approved by the user, 2026-11-11 |
| `M4-Script-Consent.dc.html` | consent prompt (script origin, PDK version; store and auto-discovered variants), trusted PDKs in the PDK panel, declined read-only state, sandbox-denial diagnostic, long-running script with interrupt | approved by the user, 2026-11-11 |
| `M4-Managers.dc.html` | cell, library and technology managers, text helper, layer toolbar and reorder dialog (the DRC manager is the M3 DRC panel) | approved by the user, 2026-11-11 |
| `M4-Console.dc.html` | Tcl console: multi-line input, stdout/stderr, interrupt, window command entry | approved by the user, 2026-11-11 |

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
| Menu bar File/Edit/Cell/Window/Layers/Drc/Options, Magic's labels (double spaces collapsed) | `menu_bar_view` (`hefesto/shell/menu_bar_model.hpp`) over `gui::wrapper_menus(".layout1")`, rendered by Raven's `MenuBar` (`ui::MenuBar`, `m4_window.cpp`) under the top bar | done (`m4-menu-bar-devices.png`, `-file.png`, `-drc.png`, in-window bar on macOS through `--menu-bar-in-window`): labels and order as on the board; Linux capture not taken (no display on the CI runners) |
| macOS: native NSMenu; Linux: 30 px in-window bar, F10, ←/→/↓ | Raven `MenuBar`: the first MenuBar of a window is the system bar on macOS (takes no room), drawn in the window elsewhere; keyboard navigation is Raven's | partial: the in-window bar is captured on macOS (`--menu-bar-in-window`, the same Raven widget Linux draws); the native NSMenu itself is outside any window capture (screen recording is not permitted here); F10 focus depends on Raven's menu bar (not verified); ⌘Q in the app menu is Raven's, not `magic::quit` (gap) Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| Each item runs its logged command (Zoom Box → `magic::findbox zoom`, Rotate 90 degree → `magic::clockwise 90`) | `menu_item_script` evaluated in the project interpreter (`run_gui_script`, origin `gui`), echoed in the Console | partial (unit-tested; no capture) |
| Check items (Options, Display units) follow `Opts(...)` / `Winopts(...)` | checked state read from the interpreter's variables; choosing sets the variable to the toggled value, then runs the command (Tk checkbutton) | partial |
| Accelerator shown only on an exact key-macro match | `gui::apply_accelerators` kept in `MenuBarItem::accelerator` (tested: `z` = `zoom .5` does not match Zoom In) | gap: Raven menu items have no display-only shortcut text (`Rmenu_item_set_shortcut` also binds the key, which would steal the canvas macros and, on macOS, unmodified keys typed in text fields); not shown. Raven issue draft 8 in `docs/decisions/raven-go-no-go.md`. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| Read/Write GDS (M5), Netlist Window (M12), 3D Display (M10) disabled with their milestone | `later_milestone`; item disabled, milestone in its accessible description | partial |
| PDK `Devices 1/2` menus after a divider, ember tint, source row | toolkit menus (`pcell::ToolkitMenus`) after the wrapper menus (`MenuBarView::divider`), source "Devices 1 · pdk1" in the model | partial (`m4-menu-bar-devices.png`): Devices 1 / Devices 2 after the wrapper menus with the PDK's items and separators. Gap: Raven has no menu-title tint or divider in a MenuBar, and no non-item header row; PDK items in wrapper menus carry "(PDK)" in the label instead of the chip (draft 10) Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| Import SPICE in File, DRC Routing in Drc (intercepted widget calls) | `script::MenuModel` edits merged by label (native entries keep the wrapper's state) | done (`m4-menu-bar-file.png`, `m4-menu-bar-drc.png`): Import SPICE (PDK) between separators after Flush changes, DRC Routing (PDK) last in Drc |
| Item hover shows label and command in the status bar | the command is the item's accessible description; the status bar shows it when the item is chosen | gap: Raven has no menu-item hover callback. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| Rebuilt on `ScriptEvents::on_menu_model_changed` | the bar is recomputed between frames and rebuilt when its signature changes (`menu_bar_signature`) | partial |
| Skipped-statement report: one ▲ Diagnostics row per statement and one status message | `skipped_statement_row`, `skipped_statements_message` from `ScriptEventCollector` | implemented: `skipped_statement_row` names the file:line, the statement and `in <procedure> · evaluation continued` (`SkippedStatement::procedure`); the board's 'with line N+1' is not shown (the next statement's line is unknown) |
## Place device and PCell dialog (M4-PCell-Dialog)

| Element | Implemented as | Status / gap |
|---|---|---|
| Tool rail "Place device" (disabled "arrives in M4" since M2) | enabled; opens a popover of the PDK's device-menu items (`place_device_view`, `MainWindow::open_place_device`); says "No PDK device scripts" in the status bar when there are none or the scripts were declined | partial: the rail button is not drawn disabled in that state (it reports why when clicked) |
| Popover grouped by menu, filter, last device with ^P | menu headings, items, "Recent: <device>" last (`PlaceDeviceView`) | gap: Raven's popup menu has no filter field (the model filters); the ^P key text is not shown. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| Choosing an item issues its command; `*_draw` items paint at the box | the item's Tcl (`magic::gencell sky130::…`) runs in the project interpreter | partial |
| Dialog header: mode, device, library, cell name, editable instance, native/Tcl chip | `pcell_dialog_view` over `EditorState::script.gencell.dialog`; chip from `pcell::builtin_generators()` for the technology's exact PDK version | done (`m4-pcell-dialog.png`): muted keys, mono values, the editable instance; the chip is text in its status color (info for Tcl, success for native), not a filled chip |
| Parameter rows: entries, checkboxes, select lists, add_message text; body scrolls | Raven text inputs (mono, 150 px), checkboxes, combo boxes, labels in a ScrollBox; header and buttons fixed | partial (`m4-pcell-dialog.png`): 12 px rows, 150 px mono entries, 195 px select list, add_message text in the warning color, the body scrolls between ruled header and buttons. Gaps: rows are widgets, not a virtual grid (40-row devices not measured); Raven controls are 28 px tall (board 22–24) Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| Edits run the widget's handlers (Return / focus-out) | `set ::magic::<p>_val v ; <handlers>` on Return, or before a button runs (uncommitted text) | partial: Raven text inputs report Return, not focus-out. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| Create / Create and Close (default) / Apply / Okay / Reset / Close | Create: `magic::gencell_dialog [magic::gencell_create <type> <lib> {k v …}] <type> <lib> {}` (default name; named: `magic::gencell <lib>::<type> <inst> k v …`); Apply/Okay: `magic::gencell <lib>::<type> <inst> k v …`; Reset: the model's Reset script; Close drops the model | partial (end to end on the test PDK in `m4_pcell_dialog_test.cpp`) |
| Non-modal utility window above the layout window | a second Raven window, `Rwindow_set_topmost` | accepted (user decision 2026-11-11) until Raven has owned utility windows (raven-go-no-go.md issue draft 7): topmost keeps it above every window, not only Hefesto's; captured with `--capture-window pcell` |
| _check stderr as a ▲ banner, corrected field outlined with what was typed | banner from `DialogModel::messages`; "▲ you typed …" under the corrected field | partial: no warning outline (Raven text input has no per-widget outline) Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| Dependency updates announced politely | `PCellDialogView::announcement` through `ui::announce` (polite), "◆ updated to …" under the field | partial |
| Headless error: Console and status bar, no dialog | a failed script's message goes to the Console and the status bar | partial |
## Script consent (M4-Script-Consent)

| Element | Implemented as | Status / gap |
|---|---|---|
| Consent prompt: shield, question title, body, origin block (script, PDK and version, release and digest, then runs) | `consent_prompt` / `consent_dialog` (`hefesto/shell/script_consent_model.hpp`) shown by `MainWindow::ask_consent` through the M2 question dialog; `DesktopConsentHost` installed as the session's `ConsentHost` before startup (`shell.cpp`) | done (`m4-consent-alert.png`, `m4-consent-outside.png`): `consent_dialog` fills the M4 alert parts of `PromptDialog` (icon `shield`, a detail block of `DialogDetail` rows split by `consent_detail` into value, chip and note, width 560/400 px, Don't run as the leading button) and `Window::run_dialog` draws them over Raven's veil (`DialogLook` from the theme tokens); the script path is middle-ellipsized to one line from `~` (`home_relative`) with the whole path as the row's tooltip. Release row: "● digest verified" when the version's provenance (`pdk::Provenance`) records a SHA-256 for every asset of a Hefesto install (`release_digest_verified`; a ciel/volare install is "◆ digest not checked"); Then runs: `<variant>.tcl` + its `<variant>-*.tcl` part files next to the startup file (`startup_then_runs`, nothing evaluated) — seen in `m4-consent-origin.png` |
| Don't run / Trust <variant> <version> / Run once; Return = Run once, Esc = Don't run | answers mapped to `ConsentDecision`; dismissed (Esc) = decline | done (`m4-consent-alert.png`; Return/Esc unit-tested): Don't run left, the others right; the default is Raven's filled primary button, not an outlined button with a focus ring. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| Outside the PDK store: ▲ release row, no Trust, Don't run default | `ConsentContext::in_store` from the PDK store root; `implicit` from the published `ConsentRequest` | done (`m4-consent-outside.png`): ▲ warning chip, Don't run (default) and Run once; "Found via" shows for an auto-discovered script only (unit-tested) |
| Shown once per PDK version | `DesktopConsentHost` keeps the session's answer per `PdkIdentity` (and the core's `TrustStore` declines) | partial |
| Trusted PDKs in the PDK panel: ● scripts trusted / ◆ ask before running, Trust scripts / Revoke trust | row state from `Provenance::trusted`; one button runs `pdk trust|untrust <variant> <version>` | partial (`m4-pdk-trust.png`): `PdkRow::scripts_label` ("◆ scripts ask before running", "● scripts trusted 11 Nov 2026" in the success color) on its own line under the install state; three lines instead of two, one Trust scripts / Revoke trust button for the selected row instead of one per row |
| Declined: ▲ read-only chip, banner, disabled editing tools, Place device and save; Review scripts... | banner over the canvas with Review scripts... (asks again; a yes trusts or runs once and asks to reopen the project); no PDK menus; Place device says why | partial (round 3, `m4-declined-readonly.png`, `m4-consent-over-declined.png`): ▲ read-only chip, "minimum · read-only" subtitle, banner and Review scripts… (a yes trusts or runs once, then `cell::reopen_declined` reads the layout again, editable) are seen; Paint, Wire, Label and Place device are disabled with "<tool>: off, the layout is read-only"; File › Save… and Save as… are disabled; the core refuses every write (SCRIPT-TRUST-005). A `-rcfile` or pinned startup that declines now opens the file argument with `minimum` (round 3 fix). The Cells manager no longer lists Magic's empty `(UNNAMED)` cell next to the declined cell (M4 follow-up pass) |
| Sandbox denial: ■ Diagnostics row and status bar with Show | the `SCRIPT-SANDBOX-*` diagnostic is the row; `denial_status` names the last denial in the status bar | partial: a denial the script caught carries `DeniedOperation::caught` and its row ends " · caught by the script's catch"; the status bar reads "■ Sandbox blocked socket in fetch_models.tcl:12 · Show" and a click on it (Show) opens the Diagnostics tab (SHELL closing pass; "Show" is not drawn in the link color: the status bar paints one text run); a typed `load <cell>` no longer records a spurious denial (round 3 fix) |
| Long-running script card after 2 s with Interrupt (Ctrl-C) | `interrupt_card` model (unit-tested) | partial (`m4-interrupt-card.png`, `--capture-running`): the interpreter reports progress every 50 ms (also in a pure-Tcl loop), the window draws a frame per report, and after 2 s the card over the canvas shows "Running <script> · N s", the file and line, Interrupt ⌃C (also Ctrl-C), stopping it with SCRIPT-TCL-002; input in those frames runs nothing (SCRIPT-TCL-004). SHELL closing pass (`m4-script-dimmed.png`): while the card shows, the canvas is dimmed (`script_dim`: the canvas color at 65 % over it, the layout at 35 % as on the board), and each pumped frame brings the canvas, its highlights, the minimap and the header up to date. The card sits at the canvas's top centre (SCREENS polish pass, `Widget::place_top_center`). Gap: the side panels wait for the end of the script |
## Managers (M4-Managers)

| Element | Implemented as | Status / gap |
|---|---|---|
| 1 Cells tab of the left dock (Hierarchy renamed Cells) | `ManagerPanels` (manager_panels.cpp) over `CellsView` (manager_views.hpp): a virtual Raven Table, columns Cell / Instance, rows flattened from `gui::cell_manager_model`, children loaded on opening (`gui::cell_manager_children`); the left dock is now a Tabs strip Layers / Cells / Libraries and the Layers tab no longer shows the M1 hierarchy | done (m4-managers-cells.png) |
| 1 Target line | `Opts(target)` from the model ("default" when unset) | done |
| 1 Load / Edit / Expand / Place / Zoom / Refresh | `gui::cell_manager_commands` as Tcl-quoted lines run through `ScreenContext::run` (logged); Refresh issues `cellmanager update` | done: lines are Tcl in the project's interpreter, so the `load` tag (`clearstack; cellmanager`) fires from these buttons (test 'a manager button's line fires the command's Tcl tag') |
| 1 Select highlights the instance's row (`mgrselect %r`) | `CellsView::select_instance` from the `select` result and `instance list celldef` per path element; `magic::mgrselect` is defined as a no-op so the tag does not fail | done (unit-tested); not captured |
| 1 Tree keyboard | Raven Table arrows and Return (Return or double click opens/closes a node); `CellsView::key_left/right` model the Left/Right keys | partial: Raven's Table callbacks expose no Left/Right, so open/close is Return only (gap G-1) Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| 2 Libraries tab: one node per directory, only the first open, path variables unexpanded, Filter, Technology column | `LibrariesView` over `gui::library_manager_model`; `$PDKPATH`, `$PDK_ROOT`, `$PDK_PATH`, `$CAD_ROOT` folded back with `display_directory`; virtual Table (446 sky130_fd_sc_hd cells cost no widget each) | done (m4-managers-libraries.png) |
| 2 Load / Place / Pick / Refresh, refresh after `addpath path save writeall` | `gui::library_manager_commands`; `ManagerPanels::note_command` marks the tab after those commands and after `tech` | done |
| 3 Technology manager in the right dock | `TechView` painted fields (name, version, description, µm per λ, units per λ, CIF in/out, extract, DRC), units entries (Return issues `scalegrid`), Technology / CIF input / CIF output / Extract / DRC menus through the shared popover; shown and hidden by `techmanager create` (Options › Tech Manager), refreshed after `tech cif extract drc scalegrid load` | done (`m4-trusted-console.png`): the description is split as a Tcl list ("SkyWater SKY130: Open Source rules and DRC"); choosing a PDK sources its rc file through consent |
| 4 Text helper (Edit › Text …) | `TextHelperWindow` (utility_windows.cpp): a separate topmost Raven window with Magic's fields, the 3×3 justification grid plus center/default, port enable and index, preview line of `TextHelperView::preview`, Apply / Okay / Cancel; reads the selected label with `setlabel …`/`port …` in microns; `magic::update_texthelper` opens it (the `select` tag only refreshes it) | partial (`m4-text-helper.png`, `--run 'magic::update_texthelper' --capture-window texthelper`): the fields, grid, checks, port and buttons as on the board; the chosen justification is not marked (draft 13); the preview line is empty until a field changes. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| 5 Layer toolbar = the palette in toolbar order | `LeftPanel::set_toolbar_order` sorts the palette rows (`toolbar_order`); `palette_command` maps gestures onto `gui::toolbar_row_command` | partial: the palette keeps its M2 gestures (click = paint layer, eye = `see`/`see no`, lock); middle click, Shift-middle and the p/e/s/S/l/u keys are not delivered by the raven_ui `Painted` wrapper (gap G-2); Hide locked / hide special come from `Opts(hidelocked/hidespecial)` only through `maketoolbar`. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| 6 Reorder dialog (Layers › Reorder layers…) | `ReorderWindow` over `ReorderModel`: search, preset list (`gui::toolbar_preset_names`), Move up / Move down, Group with above, Delete layer, Load (`gui::load_toolbar_preset`, as data), Save (rewrites `set current_toolbar {…}`), Confirm (palette order in memory); opened by `magic::reorderToolbar` | partial (`m4-reorder.png`, `--run 'magic::reorderToolbar' --capture-window reorder`): search and preset on one row, the buttons in a column right of the table, Confirm right. Raven's Table is single-selection, so "Group selected" groups the selected row with the one above, and Move layer moves one row at a time (gap G-3); no drag handles. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| DRC manager | the M3 DRC panel (board note) | done (M3) |

## Tcl console (M4-Console)

| Element | Implemented as | Status / gap |
|---|---|---|
| 1 Console tab = the project's Tcl console, sharing the interpreter with the command bar | `ConsolePanel` (console_panel.cpp) over `TclConsole` / `TclConsoleSession`: evaluation in `EditorState::script.interp`; typed command-bar lines run through the same console (`MainWindow::run_console_line`), GUI command lines are echoed in its transcript; one history (the command bar's) | done (m4-console.png); typed Tcl unit-tested |
| 1 Header: filter All / stderr, Interrupt (enabled while running), Clear, Save… | ghost buttons; Save… writes the transcript through the native save dialog | done |
| 1 Transcript: % input, > continuation, stdout, results, ▲ stderr, ■ errors with code; last 10,000 lines; JetBrains Mono | virtual Raven Table (no header), `console_line_text` prefixes; sandbox denials name their `SCRIPT-SANDBOX-*` code | partial: one text color per row (Raven's Table has no per-cell color), so the kinds are told by their glyph only, not glyph plus color (gap G-4) Accepted for M4 (user decision 2026-11-12), closes with the Raven fix. |
| 1 Multi-line input, Return runs complete commands, Shift-Return new line, hint "1 open brace", ↑/↓ history, prompt | Raven text editor; the window's key hook decides Return (`info complete` in the interpreter), the hint is `console_input_status` | done for Tab (SHELL closing pass): Tab completes the word before the cursor from the interpreter's `info commands` (commands and procedures, in command position: start, newline, `;`, `[`, `{`) or `info vars` (after `$`), as typed (`magic::bo` → `magic::box`); one name is completed (a command gets a space), several extend to their common prefix and, when nothing can be added, are listed as one transcript line ("2 matches: PDKPATH · PDK_ROOT"); arguments are not completed (`tcl_completion.hpp`, `console_test.cpp`). Not captured (keys do not reach `--run` captures) |
| 2 `puts` and engine messages in order, literal `$` | `::puts` replaced by the console's (Tcl's kept as `::hefesto::tcl_puts`); engine diagnostics during an evaluation captured as lines | done (unit-tested) |
| 6–7 Running: ● running N s, input queued, Interrupt card over the canvas after 2 s, Ctrl-C | `ConsoleRun`; the run pumps frames (`Rdraw_frame`) after every engine command it dispatches, at most every 50 ms | partial: frames are pumped from the interpreter's progress reports (every 50 ms, also in a pure-Tcl loop); a typed line held while another script runs runs after it (`TclConsole::run_held`) |
| 8 Interrupted: "eval unwound · interrupted by the user (N edits kept; undo reverts them as one step)" | `interrupt_report`; N = undo steps closed during the run | partial: the edits of an interrupted console or command-bar command form one undo step named after it (`UndoLog::merge_since`); Tcl's text for an unwinding cancel is `eval unwound`, kept as Tcl reports it (board: `eval unwound`; user's call) |
| 9 Window Command Entry (`addcommandentry F`) | `addcommandentry` / `deletecommandentry` defined in the interpreter: the console's target becomes `F`, prompt `F>`, lines sent as `F <command>` | partial: one console with a target, not one Console tab per window (Hefesto has one layout window per main window) |
| File prompts (`promptload`, `promptsave`, intercepted `tk_getOpenFile`) | `DesktopFilePicker` set as `EditorState::file_picker`: the native open/save dialog (`Rfile_dialog_open/save`) with the request's filters, folder, file name and default extension | done (unit-tested; no board) |

## Desktop Alpha

The manual checklist of design D18, run through the GUI on Linux and macOS and signed off by the user:

| Step | Linux | macOS |
|---|---|---|
| Open a sky130A, gf180mcuD and ihp-sg13g2 design; the consent prompt names the PDK version | todo | partial: the three designs open (GUI captures from a store where all three PDKs are trusted, so the consent prompt was not shown) |
| The PDK's `Devices` menus appear and their items open the Place device dialog | todo | todo (the native menu bar is not in `--capture` frames) |
| Place a native-generator device and a Tcl-only device through the dialog | todo | partial: placed through `magic::gencell` in the headless `tests/e2e/desktop_alpha_*.tcl` runs (same cells as Magic), not through the dialog |
| DRC-check the result; the DRC browser lists the errors | todo | done: `drc listall why` equals Magic's on all three PDKs (headless); the DRC panel lists them (captures `*-drcmanager.png`), and its chip equals Magic's `drc list count total` for the reopened design (sky130A 6, ihp-sg13g2 14) |
| Save and reopen; the cells and properties are unchanged | todo | partial: `writeall modified` writes the same cells as Magic, byte-identical but for timestamps, and no library cell (headless); the reopened designs show the saved cells (captures) |

macOS runs of 2026-11-11 (branch `m4-alpha-fixes`): headless `hefesto-cli --trust-pdk <variant> --magic -dnull -noconsole -rcfile <rc> desktop_alpha_<pdk>.tcl` against Magic 8.3.694 on the same scripts (PDK mounted read-only), and desktop captures at 1600×1000 of the reopened designs (`select top cell; drc check; drc catchup`; `magic::drcmanager create`). Fixed on the way: `drc listall why` now replicates Magic's re-entrant why resolution (drc-engine "Interaction check over an area"); the selected cell's name and instance id sit at 2/3 and 1/3 of its box instead of both at the centre; the inspector's "Design rules nearby" card shows the DRC result under the box (rule margins: M11); `magic::drcmanager create` and Options › DRC Manager show and focus the DRC panel; the Console dock is 320 px with the hint beside the input, so the transcript keeps about nine lines. The user's sign-off is not recorded here.

## Raven gaps and spec/board differences

Reported as open items by SHELL (task 10.6).

User decision 2026-11-12: every remaining board difference that is a Raven limit (G-1..G-4, drafts 8 and 10–14, and the rows above that cite them) is **accepted for M4 (user decision 2026-11-12), closes with the Raven fix**; each such row says so. The issues still have to be filed upstream (`docs/decisions/raven-go-no-go.md`), and the control height (28 px against the boards' 22–24 px) stays a design question, accepted for M4 as it is.

SHELL-B (managers and console):

- **G-1** Raven Table: no Left/Right key callback, so tree nodes open and close with Return or a double click only (Raven feature request: key callback or tree rows in Table). Accepted for M4 (user decision 2026-11-12), closes with the Raven fix.
- **G-2** raven_ui `Painted` delivers click and context only: the toolbar's middle click, Shift-middle and p/e/s/S/l/u keys cannot reach the palette rows yet (needs a raw input callback in the wrapper; Raven's widget events carry them). Accepted for M4 (user decision 2026-11-12), closes with the Raven fix.
- **G-3** Raven Table is single-selection: the reorder dialog's Group selected / Move layer are one row at a time. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix.
- **G-4** Raven Table has no per-cell text color: console line kinds are glyph-only in the transcript. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix.
- **G-5** No nested frame from inside Raven's frame update: a run started from a frame callback (not an input event) cannot pump frames, and nothing pumps during a pure-Tcl loop (TCL progress hook needed).
- **Board difference** Magic's "Group selected" and "Move layer" buttons only print the selection (reorderLayers.tcl); the shell implements grouping (same toolbar row number) and moving, but the toolbar model has no group concept, so grouping affects order only.
- **Board difference** The board's Text helper and Reorder dialog are utility windows, as approved; `--run 'magic::update_texthelper'` / `'magic::reorderToolbar'` opens them and `--capture-window` saves them (SCREENS polish pass).

SCREENS polish pass (2026-11-12): what the captures still differ in, and the Raven issues drafted for it (numbered 10–14 after CARRY's issue 9; copied to `docs/decisions/raven-go-no-go.md` "M4 polish round: issues 10–14" for filing; not filed, the user files them). No Raven code was patched.

- **Draft 10** *MenuBar: a divider, a title tint and a non-item header row.* The board puts the PDK's menus after a divider with an ember title tint and a "Devices 1 · pdk1" source row at the top of the menu. `R_WIDGET_MENU_BAR` draws titles only and `R_WIDGET_MENU` holds items, separators and sub-menus. Request: `Rmenu_bar_add_divider(bar)`, `Rmenu_set_title_color(menu, r, g, b, a)` and `Rmenu_add_header(menu, text)` (a disabled, non-focusable caption row); on the native macOS bar the tint and divider may be ignored. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix.
- **Draft 11** *Table: per-cell text color, Left/Right keys, several selected rows.* G-1, G-3 and G-4 above in one issue: a cell color callback (or `RTableCell.color`), the key callback (or tree rows) for Left/Right, and `Rtable_set_selection_mode(table, R_SELECT_MULTIPLE)`. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix.
- **Draft 12** *Text input: an outline state.* A field corrected by the application (the PCell dialog's `_check`) needs a warning outline that stays until the next edit; `Rtext_input` has the theme's focus ring only. Request: `Rwidget_set_state_outline(widget, r, g, b, a)` or an `R_STATE_WARNING` style. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix.
- **Draft 13** *Buttons: a selected state in a group.* The text helper's justification grid marks the chosen square (ember fill); Raven buttons have no checked state outside a toggle and a segmented control has one row. Request: `Rbutton_set_checked(button, checked)` (a toggle look without a toggle behavior) or a grid of radio buttons. Accepted for M4 (user decision 2026-11-12), closes with the Raven fix.
- **Draft 14** *Popup menu: a filter field and right-aligned key text.* The Place device popover has a filter field and the ^P key of the last device; `Rmenu_open` shows items only (draft 8 covers key text in the bar). Accepted for M4 (user decision 2026-11-12), closes with the Raven fix.
- **Not a Raven gap** The consent prompt's scrim: Raven's dialog sheet already darkens the window (42 % black veil; measured in `m4-consent-alert.png`). The round-2/3 "no scrim" note came from the full-width card, which hid how dark the window behind it was.
- **Not a Raven gap** Control height: Raven's `R_METRIC_CONTROL_HEIGHT` is a theme metric; the forge theme sets it for the whole app (28 px), and the boards' compact 22–24 px rows in the PCell dialog and the managers would need a second density. Left as a design question, not an issue.
