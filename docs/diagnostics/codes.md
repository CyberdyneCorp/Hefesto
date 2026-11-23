# Diagnostic codes

Every diagnostic that Hefesto emits carries a stable code of the form
`<AREA>-<TOPIC>-<NNN>` (capability `hefesto-diagnostics`, requirement
"Structured diagnostic records"). Codes are never renumbered or reused once
released; a retired code stays in this table marked as retired.

The table below is the "core" area of the code table
(`src/core/src/diag/codes.cpp`). Since M1 each area owns its own codes and
document, so parallel work never edits the same table
(`openspec/changes/m1-core-viewer/design.md` D3; the M2 areas:
`openspec/changes/m2-interactive-editing/design.md` D1; the M3 areas:
`openspec/changes/m3-drc/design.md` D1; the M4 areas:
`openspec/changes/m4-scripting/design.md` D1; the M5 areas:
`openspec/changes/m5-mask-gds-cif/design.md` D1; the M6 areas:
`openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md` D1; the M8 area:
`openspec/changes/m8-schematic/design.md` D3; the M10 area:
`openspec/changes/m10-3d-view/design.md` D2; the M7 areas:
`openspec/changes/m7-extraction/design.md` D2):

| Area | Prefix | Source | Document |
|---|---|---|---|
| core | CLI, CMD, CORE, PLATFORM, SETTINGS | `src/core/src/diag/codes.cpp` | this file |
| tech | TECH | `src/core/src/tech/codes.cpp` | [codes/tech.md](codes/tech.md) |
| db | DB | `src/core/src/db/codes.cpp` | [codes/db.md](codes/db.md) |
| cell | CELL | `src/core/src/cell/codes.cpp` | [codes/cell.md](codes/cell.md) |
| display | DISPLAY | `src/core/src/display/codes.cpp` | [codes/display.md](codes/display.md) |
| view | VIEW | `src/core/src/view/codes.cpp` | [codes/view.md](codes/view.md) |
| pdk | PDK | `src/core/src/pdk/codes.cpp` | [codes/pdk.md](codes/pdk.md) |
| app | APP | `src/core/src/app/codes.cpp` | [codes/app.md](codes/app.md) |
| undo | UNDO | `src/core/src/undo/codes.cpp` | [codes/undo.md](codes/undo.md) |
| select | SELECT | `src/core/src/select/codes.cpp` | [codes/select.md](codes/select.md) |
| connect | CONNECT | `src/core/src/connect/codes.cpp` | [codes/connect.md](codes/connect.md) |
| ui | UI | `src/core/src/ui/codes.cpp` | [codes/ui.md](codes/ui.md) |
| wiring | WIRE | `src/core/src/wiring/codes.cpp` | [codes/wire.md](codes/wire.md) |
| drc | DRC | `src/core/src/drc/codes.cpp` (fragments `drc/<sub>/codes.inc`) | [codes/drc.md](codes/drc.md) |
| hints | HINTS | `src/core/src/hints/codes.cpp` | [codes/hints.md](codes/hints.md) |
| script | SCRIPT | `src/core/src/script/codes.cpp` (fragments `script/<sub>/codes.inc`) | [codes/script.md](codes/script.md) |
| pcell | PCELL | `src/core/src/pcell/codes.cpp` (fragments `pcell/<sub>/codes.inc`) | [codes/pcell.md](codes/pcell.md) |
| tools | TOOLS | `src/core/src/tools/codes.cpp` | [codes/tools.md](codes/tools.md) |
| gui | GUI | `src/core/src/gui/codes.cpp` | [codes/gui.md](codes/gui.md) |
| mask | MASK | `src/core/src/mask/codes.cpp` (fragments `mask/<sub>/codes.inc`) | [codes/mask.md](codes/mask.md) |
| gds | GDS | `src/core/src/gds/codes.cpp` (fragments `gds/<sub>/codes.inc`) | [codes/gds.md](codes/gds.md) |
| cif | CIF | `src/core/src/cif/codes.cpp` (fragments `cif/<sub>/codes.inc`) | [codes/cif.md](codes/cif.md) |
| lef | LEF | `src/core/src/lef/codes.cpp` (fragments `lef/<sub>/codes.inc`) | [codes/lef.md](codes/lef.md) |
| def | DEF | `src/core/src/def/codes.cpp` (fragments `def/<sub>/codes.inc`) | [codes/def.md](codes/def.md) |
| oasis | OASIS | `src/core/src/oasis/codes.cpp` (fragments `oasis/<sub>/codes.inc`) | [codes/oasis.md](codes/oasis.md) |
| extflat | EXTFLAT | `src/core/src/extflat/codes.cpp` (fragment `extflat/codes.inc`) | [codes/extflat.md](codes/extflat.md) |
| extract | EXTRACT | `src/core/src/extract/codes.cpp` (fragments `extract/<sub>/codes.inc`) | [codes/extract.md](codes/extract.md) |
| schem | SCHEM | `src/core/src/schem/codes.cpp` (fragments `schem/<sub>/codes.inc`) | [codes/schem.md](codes/schem.md) |
| view3d | VIEW3D | `src/core/src/view3d/codes.cpp` (fragments `view3d/<sub>/codes.inc`) | [codes/view3d.md](codes/view3d.md) |
| netlist | NETLIST | `src/core/src/netlist/codes.cpp` (fragments `netlist/<sub>/codes.inc`) | [codes/netlist.md](codes/netlist.md) |
| extresist | EXTRESIST | `src/core/src/extresist/codes.cpp` (fragments `extresist/<sub>/codes.inc`) | [codes/extresist.md](codes/extresist.md) |
| antenna | ANTENNA | `src/core/src/antenna/codes.cpp` (fragment `antenna/codes.inc`) | [codes/antenna.md](codes/antenna.md) |

Regenerate the core table with:

```sh
hefesto-cli --diagnostic-codes
```

The unit test `diagnostic code table matches docs/diagnostics/codes.md`
fails when the two disagree. To add a core code: add it to `codes.cpp` (sorted
by code) and to `hefesto/diag/codes.hpp`, then paste the command's output
between the markers below. Area codes follow the same steps in the area's
`codes.cpp` and document (`hefesto-cli --diagnostic-codes=<area>`).

## Renderings

- Text (CLI default, session log):
  `<severity>: <code>: <text>[ at <file>:<line>| in <cell> (<x>, <y>)]`
- JSON lines (`hefesto-cli --diagnostics=json`): one object per line, keys in
  the fixed order `severity`, `code`, `text`, `location`, `related`; absent
  fields are `null`. `location` is `{"file","line"}` or `{"cell","x","y"}`
  (internal units); `related` is `{"kind","name"}` with kind `cell`,
  `instance`, `net`, `label` or `rule`.

## Codes

<!-- BEGIN DIAGNOSTIC CODES (generated by `hefesto-cli --diagnostic-codes`) -->
| Code | Default severity | Meaning |
|---|---|---|
| `CLI-LOG-001` | warning | The session log file cannot be opened; the run continues without it. |
| `CLI-SCRIPT-001` | error | A script file named on the command line cannot be read. |
| `CLI-TECH-001` | warning | Retired in M1, never reused. Was: `-T <tech>` recorded but not loaded; `-T` now loads the technology at startup. |
| `CLI-USAGE-001` | error | Invalid command-line option or option argument. |
| `CMD-DISPATCH-001` | error | Unknown command name; the command was not run. |
| `CMD-INTERNAL-001` | error | A command handler raised an unexpected internal error; the command failed. |
| `CMD-PARSE-001` | error | Retired in M4, never reused. Was: command script has an unterminated double quote (M0 command-script reader; lines are Tcl now). |
| `CMD-PARSE-002` | error | Retired in M4, never reused. Was: command script has an unterminated brace group (M0 command-script reader; lines are Tcl now). |
| `CMD-PARSE-003` | error | Retired in M4, never reused. Was: extra characters after a closing quote or brace in a command script (M0 command-script reader; lines are Tcl now). |
| `CMD-REGISTRY-001` | error | A command with the same name is already registered. |
| `CMD-SESSION-001` | error | The command needs an editor session (technology, cells, windows) and none is attached. |
| `CMD-USAGE-001` | error | Wrong number or form of command arguments. |
| `CORE-NOTIMPL-001` | error | Retired in M1, never reused. Was: a declared feature not implemented yet (M1 scaffolding stub). |
| `CORE-NOTIMPL-002` | error | Retired in M2, never reused. Was: not implemented yet (an M2 command or function registered by the M2 scaffold before its workstream landed it). |
| `CORE-NOTIMPL-003` | error | Retired in M3, never reused. Was: not implemented yet (a `drc` option or M3 function stubbed by the M3 scaffold before its workstream landed it). |
| `CORE-NOTIMPL-004` | error | Retired in M4, never reused. Was: not implemented yet (an M4 command or function registered by the M4 scaffold before its workstream landed it). |
| `CORE-NOTIMPL-005` | error | Retired in M5, never reused. Was: an M5 command option or function declared by the M5 scaffold before its workstream landed it. |
| `CORE-NOTIMPL-006` | error | Retired in M6, never reused. Was: an M6 command option or function declared by the M6 scaffold before its workstream landed it. |
| `CORE-NOTIMPL-007` | error | Not implemented yet: an M8 schematic function declared by the M8 scaffold before its workstream landed it (retired at M8 exit). |
| `CORE-NOTIMPL-008` | error | Not implemented yet: an M7 extraction or netlisting command option or function declared by the M7 scaffold before its workstream landed it (retired at M7 exit). |
| `PLATFORM-IO-001` | error | A file cannot be read. |
| `PLATFORM-IO-002` | error | A file cannot be written. |
| `PLATFORM-IO-003` | error | A directory cannot be created, listed or removed. |
| `PLATFORM-IO-004` | error | A file or directory does not exist. |
| `PLATFORM-LOG-001` | warning | The session log cannot be opened; the session continues without it. |
| `PLATFORM-PDK-001` | error | A technology was not found in any PDK or technology root. |
| `PLATFORM-PROCESS-001` | error | An external process could not be started or waited for. |
| `PLATFORM-UNSUPPORTED-001` | error | The requested platform service is not available on this platform. |
| `SETTINGS-FILE-001` | error | A project settings file is malformed. |
| `SETTINGS-NAME-001` | error | Unknown project setting name. |
| `SETTINGS-VALUE-001` | error | Invalid value for a project setting. |
<!-- END DIAGNOSTIC CODES -->
