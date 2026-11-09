# Proposal: M2 Interactive editing and design system

## Why

M1 made Hefesto open, display and save real PDK layouts, but it is still a viewer: only the command subset the M1 golden cases needed exists (`box`, `paint`, `erase`, `label`, `setlabel` by box, `getcell`, `flatten`, ...), there is no selection, no undo, no macros or key bindings, and the desktop tool rail is disabled past the box. M2 (docs/roadmap.md, "M2") makes Hefesto a usable editor with Magic's full editing command set and a modern desktop UI on the forge design system. DRC (M3), Tcl and PCells (M4) and every later milestone edit layouts through these commands, the selection and the undo log, so they come first. The 42 open questions that blocked M2 are resolved and the M2 specs aligned with them (`scripts/check_milestone_readiness.py --milestone M2`: ready).

## What Changes

- **Coordinates, box and paint (EDIT-CORE).** The full `cmd-coordinates-and-units` rules (expressions, suffixes, `units`, `snap`, `grid`, pointer snapping, value formatting, root/edit frames) in one shared parser; every `box` option and the box tool gestures; `paint`, `erase`, `fill`, `corner`, `contact`, `polygon`, `splitpaint`, `spliterase`, `drop` with the full layer-list syntax; `xor`, `random`, `shell`, `straighten` (surface; plowing is M12).
- **Selection (SELECT).** The selection model (a flattened root-coordinate copy), every `select` option and qualifier with point cycling, chunk/region/net growth, floating selections, `what`, `identify`, `delete`, `copy`, `move`, `stretch`, rotations and flips, `orient`, `array`; selection highlights drawn in the renderer.
- **Connectivity (SELECT).** The flat and hierarchical connectivity searches of `layout-connectivity`, used by `select net`, net highlighting and later by DEF/extraction tools, including the equivalent-port rule.
- **Hierarchy, labels and properties (HIER).** The rest of `cmd-hierarchy` (`xload`, `edit`, `down`, full `getcell`/`dump` placement syntax, `flush`, `cellname`/`instance` options, edit-cell switching) and of `cmd-labels-properties` (`setlabel` on the selection, `findlabel`, `goto`, `element`, `port make` numbering).
- **Undo (UNDO).** The `layout-undo` event log with one step per command, every recorded event kind (paint, split tiles, labels, uses, box, selection, edit cell, wiring, color map), the history limit setting (default 1000), `undo`/`redo` with every form, and property/flatten-in-place undo with `magic-compat` off (D-10).
- **Command dispatch and input (DISPATCH).** Window clients with per-client command tables, Magic's lookup and `help`, line splitting, macro tables per tool with X11 keysym key names (Q-UI resolution), `macro`/`imacro`, the default key bindings, the tool registry and `tool`, `logcommands`, `pushbutton`/`setpoint`, `sleep`/`echo`/`send`, `history`, the post-command update; the feedback/highlight layer and the `feedback` command; the box/edit stacks, `suspendall`/`resumeall`, `ruler`, `measure`, `genkey` and macro auto-increment as native commands; the `color` window client; the wizard commands needed for debugging (including the tile-structure overlay).
- **Wiring tool (WIRE).** The technology `wiring` section, `wire` and every subcommand with exact painted geometry, and the wiring tool buttons.
- **Desktop editor (SHELL).** Every tool on the rail live (box, select, paint, wire, label, measure), pointer and keyboard input through the macro tables, a command bar with history and completion (commands, cells, layers), the selection inspector, layer palette paint/visibility/lock, dockable panels, the color window, a crash-recovery confirmation dialog, the remaining design-system pieces (SVG icon set, component patterns), and the deferred M1 UI gaps (multi-line results in the status bar, problem-count reset, minimap coverage of outline/cross styles, top-ruler font).
- **UX design (DESIGN).** New boards for the M2 screens and states in the reference design artifact before implementation (`hefesto-design-system` "Screen patterns follow the reference mockups"), and the internal usability review that the roadmap names as an exit criterion.
- **Integration (INTEGRATION).** Shared build files, `EditorState` members (selection, undo log, macro tables, tools, feedback), golden authoring and CI capture of the CMDEDIT cases, the UI command cases and the MODEL/TECH/MISC cases that waited for selection or undo, libFuzzer targets for the new readers, README and roadmap.

### Non-goals

- DRC (`drc` command, continuous checking, DRC browser): M3. Paint changes only record the areas DRC will need (`Common effects of edit-cell paint changes`).
- Tcl, `source` through Tcl, `magic::` namespace procs, PCells, the Tk GUI managers (`ui-gui-managers`): M4. The `tools.tcl` helpers of `ui-box-stack-and-measure` are implemented as native core commands now and become `magic::` names in M4.
- Plowing (`straighten` geometry, `plow`), the `netlist` and `rsim` tools' actions, routing: later milestones. Their commands and tool tables exist and report unavailability.
- GDS/CIF/LEF/DEF/extraction: later milestones. Golden cases that need them stay unauthored.
- iPadOS touch and Pencil mapping (`hefesto-input-and-touch` iPad requirements): M14. M2 delivers the desktop requirements of that capability and keeps the gesture mapping table in the core so the iPad shell reuses it.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

None. M2 implements the existing requirements of the 19 capabilities listed in docs/roadmap.md ("M2"). Every open question cited by these specs is resolved (Resolution lines in `docs/decisions/open-questions/{CMDEDIT,UI,MODEL,SCRIPT,LEGACY}.md`, commit "Resolve the 42 open questions that block M2") and the specs were aligned in the following commit. The change therefore sets `skip_specs: true`, following the M0 and M1 precedent (m0-foundations design D10). A workstream that finds a requirement that must change raises it as an open item; INTEGRATION then adds a spec delta to this change.

## Impact

- **New code:** `src/core/{include/hefesto,src}/{undo,select,connect,ui,wiring}/` and new files in `cmd/`, `db/`, `view/`, `display/`, `editor/`; `src/render/vulkan/` (selection and highlight batches); `src/shell/desktop/` (tools, input, inspector, color window, dialogs, icons); `assets/icons/` (SVG); `tests/unit/<area>/`, `tests/data/<area>/`.
- **Shared changes:** `editor::EditorState` gains the selection, undo log, macro tables, tool registry, box/edit stacks and feedback list; `cmd::Dispatcher` gains window-client tables and the post-command update; `db` mutations report to an undo recorder.
- **Dependencies:** none expected. SVG icons are data drawn by Raven; key names use an embedded X11 keysym table (data, no Xlib).
- **Golden cases:** CMDEDIT (80), the UI command cases of `ui-command-dispatch`, `ui-tools-and-input`, `ui-feedback-and-highlights`, `ui-color-window`, `ui-wizard-and-debug`, and the unauthored MODEL (undo, select/what, move), TECH (`select net`) and UI (edit, per-window select) cases; `setlabel` per-label forms through the CMDEDIT-LBL cases.
- **Docs:** `docs/design/m2-ui-conformance.md`, the design artifact boards for M2, diagnostic code documents, building and contributing updates, roadmap status.
