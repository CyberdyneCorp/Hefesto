# Changelog

User-visible changes to Hefesto, newest first. Milestone details live in
`openspec/changes/` and `docs/roadmap.md`; this file lists what a user of a
release notices.

## Unreleased (M4: scripting)

### Changed

- **`hefesto-cli` runs `-c`, script files, standard input and
  `--interactive` as Tcl outside `--magic` mode too.** Each complete command
  runs in the project's sandboxed interpreter and its non-empty result is
  printed, as in a Tcl console (`-c "tech name"` prints the name). Before M4
  these lines went through Hefesto's own command reader with no Tcl
  substitution or result printing. In `--magic` mode only `puts` prints, as
  in Magic's batch mode. A Tcl error stops that source at its line, and a
  command that fails quietly still makes the run exit 1. See
  `docs/building.md`, `hefesto-cli`.
- **Lines typed in the desktop command bar and console are Tcl.** Braces,
  double quotes, `$` and `[...]` follow Tcl's quoting rules, as in Magic's
  Tcl console: `label {two words}` labels with two words, and a `$` or `[`
  meant literally needs braces or a backslash.
- **`shell` runs nothing, from any origin.** Magic's `shell` runs a command
  in an operating-system shell. Hefesto runs no external program (decision
  X-3), so `shell` now fails with `SCRIPT-SANDBOX-001` ("process execution
  is not permitted") when you type it in the console, the command bar or
  hefesto-cli, as it already did for scripts, key macros and plugins. The
  default `L` key macro (`shell ls`) reports the same error.
- **Key and button macros, key-binding files, menu items and manager
  buttons are Tcl too.** `$`, `[...]` and backslashes follow Tcl's rules in
  them. A key-binding file runs only its `macro` and `imacro` lines; any
  other command in it is reported (`UI-USAGE-001`) and does nothing. A
  quiet engine failure still shows the line as failed.
- **While a script runs, the layout takes no input.** A long PDK script, a
  menu callback or a console command now draws frames as it runs; after 2 s
  an Interrupt card (also Ctrl-C) stops it. Clicks, key macros and menu
  items in the meantime run nothing (`SCRIPT-TCL-004`); lines typed in the
  console or command bar wait and run after it. The edits of an
  interrupted console command are undone in one step.
- **A layout whose PDK scripts you declined is read-only.** It opens with
  the `minimum` technology, the editing tools and File › Save are off, and
  nothing writes it (`SCRIPT-TRUST-005`). "Review scripts…" asks again and
  reopens it with its technology when you allow the scripts.
- **Layer toolbar reorder dialog.** "Group selected" makes the selected
  layers adjacent in the first one's toolbar row, and "Move layer" moves a
  layer to a new position; Magic's two buttons only print the selection.
  Groups affect the order only, and the dialog selects one row at a time.
- **PCell dialog.** Create with the default instance name issues
  `magic::gencell_create <type> <lib> {k v …}`, because Magic's
  `magic::gencell <lib>::<type> {} k v …` only opens the dialog. A named
  instance issues `magic::gencell <lib>::<type> <inst> k v …`. The dialog
  stays above every window, not only Hefesto's, until the Raven toolkit has
  owned utility windows.

### Fixed

- **Moving, copying, deleting, rotating or arraying before anything was
  ever selected no longer crashes.** `box 0 0 0 0; delete` in a fresh
  session (or a device script that runs `array` first) crashed
  `hefesto-cli`; these commands now see an empty selection, as in Magic.
- **`cellname create` of an existing cell does nothing**, as in Magic,
  instead of failing with `cell "..." already exists` (a second generated
  device with the same parameters hit it).
- **Commands that need the box in the edit cell print Magic's text.**
  `paint`, `erase`, `label`, `port` and the other editing commands say `Box
  must be present` when there is no box; `box`, `select box` and `element
  inbox` keep `Box tool must be present`.
- **`readspice` and `mazeroute` are global Tcl commands**, as Magic's
  `readspice.tcl` and `mazeroute.tcl` define them, so scripts can call them
  unqualified.
- **`--diagnostics=magic` no longer prints the native-generator fallback
  note** (`PCELL-NATIVE-001`), which Magic has no equivalent of.

### Security

- `--trust-pdk <family|variant>` also trusts a copy of that PDK found
  outside the PDK store, for example a tree named by `-rcfile` or
  `PDKPATH`. This is a documented, accepted limit (S-12 in
  `docs/contributing/security.md`): name only PDKs you mean to run.
