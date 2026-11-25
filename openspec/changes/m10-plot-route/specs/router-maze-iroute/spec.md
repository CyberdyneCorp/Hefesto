## MODIFIED Requirements

### Requirement: iroute availability and dispatch
The `iroute` command SHALL require a maze router style named `irouter`; without it every invocation SHALL report `Need irouter style in mzrouter section of technology file to use irouter.` and do nothing. With `magic-compat` OFF, a technology without a style named `irouter` gets the derived style of requirement "Derived irouter style", so the command is available; with it ON nothing is derived. With no argument it SHALL route from the cursor to the box. Otherwise the first argument is matched case-insensitively as a unique prefix (exact match wins) against: `contacts`, `help`, `layers`, `route`, `saveParameters`, `search`, `spacings`, `verbosity`, `version`, `wizard`. Ambiguous: `Ambiguous iroute subcommand: "%s"`; unknown: `Unrecognized iroute subcommand: "%s"` followed by `Valid iroute irSubcommands are:  ` and the list. When verbosity is 0 all informational output of the command is suppressed (errors still shown). The interactive router parameters are a session copy of the `irouter` style, re-initialised whenever the technology is reloaded.

Compatibility: behavioral
(Magic ref: irouter/irCommand.c:2573-2670, irouter/irMain.c:120-160) (See Q-LEGACY-37.)

#### Scenario: Missing style
- **GIVEN** a technology without an `irouter` mzrouter style, and `magic-compat` ON
- **WHEN** `iroute` is executed
- **THEN** `Need irouter style in mzrouter section of technology file to use irouter.` is reported

#### Scenario: Prefix match
- **WHEN** `iroute sa out.ir` is executed
- **THEN** it is interpreted as `iroute saveParameters out.ir`

### Requirement: Route context and endpoints
Routing SHALL use as reference window the window set by `wizard window` (if its id exists; otherwise `Couldn't find route window (%d),using command window as reference.`) or the command window; without a layout window: `Point to a layout window first.`; if the edit cell is not being edited in that window's root: `Nothing being edited in route window.`. Everything visible (expanded) in that window is seen by the router, the result is painted into the edit cell.

Start: `-spoint` coordinates transformed from edit to root coordinates; cursor point (must be in a layout window showing the route cell: `Can not use cursor as start:  cursor not in layout window.` / `...cursor not in routecell.`); label: searched first among selected labels, then all labels in the route cell hierarchy (`Warning: Start label '%s' not unique.`, `Start label '%s' not found.`), the start point is the label rectangle's lower-left corner and the start layer is the first active route layer connected to the label's type.

Destination: box (`Can not use box for dest:  No Box.`, `...box not in route cell.`), rectangle, label (same search, rectangle = label rectangle, analogous messages with `Destination`), or selection (a destination area for each selected tile of a type connecting to each permitted active layer). For a label destination only the first active route layer connected to the label type is used; otherwise every permitted active layer.

Start layers when not determined by a label: from the permitted active layers, find route contacts (both layers active) and layers touching the start point. No layer present → all permitted active layers; exactly one layer → that layer; exactly one contact → its two layers; otherwise print `Multiple nodes present at start point:` with the candidates and ask `Connect to %s? [yes] ` for each (empty answer = yes; the last candidate is taken automatically). A session that cannot ask (a script host, a headless batch run, an agent session) SHALL print the same lines and take the empty answer, in both `magic-compat` settings, so the first candidate is connected and the command never waits for input (see Q-LEGACY-40).

Compatibility: behavioral
(Magic ref: irouter/irRoute.c:95-470, irouter/irRoute.c:505-760, irouter/irRoute.c:900-1192) (See Q-LEGACY-40.)

#### Scenario: Label not found
- **WHEN** `iroute route -slabel nosuch` is executed
- **THEN** `Start label 'nosuch' not found.` is reported and nothing is painted

#### Scenario: Several nodes in a batch run
- **GIVEN** a headless batch session and a start point touched by metal1 and metal2 and by the contact between them, all active
- **WHEN** `iroute route -spoint 10 10 -dbox` is executed
- **THEN** `Multiple nodes present at start point:` and the first `Connect to` prompt are printed, the first candidate is connected, and the command finishes without reading input

## ADDED Requirements

### Requirement: Derived irouter style
With `magic-compat` OFF and no style named `irouter` in the technology's `mzrouter` section, Hefesto SHALL derive one when the technology is loaded or the grid is rescaled:
- route layers: the types named by the `routing` lines of the technology's `lef` section, in order, the first line lowest; at even positions (0, 2, …) hCost 32 and vCost 64, at odd positions hCost 64 and vCost 32; jogCost 256, hintCost 1, overCost 1; all active;
- route contacts: the types named by the `cut` lines of the `lef` section whose two residue types are both derived route layers, in order, cost 1024, active;
- widths, lengths and spacings: derived from DRC exactly as for a written style (capability `router-maze`, "Width and spacing derivation from DRC");
- every other parameter: the style defaults (capability `router-maze`, "Style defaults and lookup").

A technology with no `routing` line gets no derived style and keeps the missing-style error. The first `iroute` of a session that uses the derived style prints, once, `Using the irouter style derived from the technology's lef section.` (suppressed at verbosity 0). `saveParameters` writes the derived style's parameters like any other; sourcing that file in a session with `magic-compat` ON fails with the missing-style error. A style named `irouter` in the technology always takes precedence.

Compatibility: native

(Hefesto decisions: docs/decisions/cross-cutting.md#X-2, docs/decisions/product.md#P-13) (See Q-LEGACY-37, Q-AGENT-1.)

#### Scenario: sky130A with magic-compat off
- **GIVEN** the sky130A technology, whose `mzrouter` section is empty, and `magic-compat` OFF
- **WHEN** `iroute layers` is run
- **THEN** the table lists li, metal1 to metal5 and the `lef` section's remaining routing types in that order, metal1 with hCost 64 and vCost 32, and widths equal to the DRC minimum widths

#### Scenario: sky130A with magic-compat on
- **GIVEN** the sky130A technology and `magic-compat` ON
- **WHEN** `iroute layers` is run
- **THEN** `Need irouter style in mzrouter section of technology file to use irouter.` is reported

### Requirement: Route timeout
`iroute route -timeout <n>` SHALL end the search after n seconds of wall-clock time as an interrupt does (the results of requirement "Route results" for an interrupt). The deadline SHALL belong to the one command: it is removed when the command ends on any path, including an option error after `-timeout` (Magic leaves its timer armed there; see Q-LEGACY-41), so no later command is interrupted by it. Golden cases and tests that need a time limit use the engine's bloom budget (capability `router-maze`, "Maze router engine interface") instead of `-timeout`.

Compatibility: behavioral

(Magic ref: irouter/irCommand.c:1683-1696, utils/signals.c:145-198) (See Q-LEGACY-41.)

#### Scenario: Option error after a timeout
- **GIVEN** `iroute route -timeout 1 -foo`
- **WHEN** it is executed and a 5-second `drc check` of a large cell is run next
- **THEN** `Bad option to 'iroute route':  '-foo'` is reported and the DRC check is not interrupted

### Requirement: iroute behaviour with magic-compat off
With `magic-compat` OFF, `iroute` SHALL differ from Magic in exactly these points; with it ON, each is as the other requirements of this capability state:
- **Already routed (Q-LEGACY-42).** When the start already connects to the destination, `Already routed.` is printed as information instead of the error `Route Failed.`; the scripting result is `Route already routed` in both settings.
- **Saved spacings (Q-LEGACY-43).** `saveParameters` writes each spacing value with the `i` suffix (internal units), e.g. `:iroute spacings metal2 metal2 4i`, so the file restores the same spacings under any `units` setting and after `scalegrid`.
- **Coordinates (Q-LEGACY-45).** `-spoint` and `-drect` accept every coordinate form of the standard coordinate parser (unit suffixes included), and the `-drect` rectangle is normalised to lower-left and upper-right corners.
- **Sourcing saved files (Q-LEGACY-44).** If the LEG-MZ-11 capture shows that Magic's Tcl `source` does not accept the leading `:` of `saveParameters` lines, the scripting layer accepts it with `magic-compat` OFF; if the capture shows that it does, it is accepted in both settings and this point is void.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2) (See Q-LEGACY-42, Q-LEGACY-43, Q-LEGACY-44, Q-LEGACY-45.)

#### Scenario: Suffixed coordinates
- **GIVEN** `magic-compat` OFF and an `irouter` style
- **WHEN** `iroute route -spoint 1um 1um -drect 10um 10um 2um 2um` is executed
- **THEN** the route starts at (1 µm, 1 µm) and ends in the rectangle (2 µm, 2 µm)-(10 µm, 10 µm); with `magic-compat` ON `Nonnumeric coordinate: 1um` is reported and nothing is routed

#### Scenario: Saved spacings after scalegrid
- **GIVEN** `magic-compat` OFF, `scalegrid 1 2`, and a metal2-to-metal2 spacing of 8 internal units
- **WHEN** `iroute saveParameters p.ir` is run, the technology is reloaded with the same grid, and `source p.ir` is executed
- **THEN** `iroute spacings metal2 metal2` prints `	8`
