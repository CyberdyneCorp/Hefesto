# Router: Interactive Maze Router Command (iroute)

## Purpose

Defines the `iroute` command: point-to-point routing with the maze router (capability `router-maze`), start/destination specification, endpoint layer selection, result codes, painting and selection of the route, all parameter subcommands (`contacts`, `layers`, `search`, `spacings`, `verbosity`, `version`, `wizard`), and the exact `saveParameters` file format, which is a sourceable command script and therefore a compatibility surface.

## Requirements

### Requirement: iroute availability and dispatch
The `iroute` command SHALL require a maze router style named `irouter`; without it every invocation SHALL report `Need irouter style in mzrouter section of technology file to use irouter.` and do nothing. With no argument it SHALL route from the cursor to the box. Otherwise the first argument is matched case-insensitively as a unique prefix (exact match wins) against: `contacts`, `help`, `layers`, `route`, `saveParameters`, `search`, `spacings`, `verbosity`, `version`, `wizard`. Ambiguous: `Ambiguous iroute subcommand: "%s"`; unknown: `Unrecognized iroute subcommand: "%s"` followed by `Valid iroute irSubcommands are:  ` and the list. When verbosity is 0 all informational output of the command is suppressed (errors still shown). The interactive router parameters are a session copy of the `irouter` style, re-initialised whenever the technology is reloaded.

Compatibility: behavioral
(Magic ref: irouter/irCommand.c:2573-2670, irouter/irMain.c:120-160)

#### Scenario: Missing style
- **GIVEN** a technology without an `irouter` mzrouter style
- **WHEN** `iroute` is executed
- **THEN** `Need irouter style in mzrouter section of technology file to use irouter.` is reported

#### Scenario: Prefix match
- **WHEN** `iroute sa out.ir` is executed
- **THEN** it is interpreted as `iroute saveParameters out.ir`

### Requirement: iroute route options
`iroute route [options]` SHALL accept, each matched case-insensitively as a unique prefix of the option names (which include the leading `-`):

| Option | Args | Meaning |
|---|---|---|
| `-dbox` | – | destination = box (default) |
| `-dlabel` | name | destination = label `name` |
| `-dlayers` | layer-list | restrict destination layers to the active route layers in the list |
| `-drect` | xl yl xh yh | destination rectangle (edit-cell coordinates, standard coordinate parsing) |
| `-dselection` | – | destination = selected paint |
| `-scursor` | – | start at the cursor (default) |
| `-slabel` | name | start at label `name` |
| `-slayers` | layer-list | restrict start layers |
| `-spoint` | x y | start point (edit-cell coordinates) |
| `-timeout` | seconds | interrupt the search after the given integer time |

Errors: `Bad option to 'iroute route':  '%s'`, `Ambiguous option to 'iroute route':  '%s'`, `Missing label.`, `Missing layer list.`, `No active route layers in destination list!` / `... start list!`, `Incomplete coordinates.`, `Nonnumeric coordinate: %s` (the first `-drect` field reports `Non coordinate: %s`), `No timeout value given.`, `Noninteger timeout value: %s`. Any error aborts the command without routing. When multiple start/destination options are given the last one wins. The documented form `iroute route node_name...` is not implemented (see Q-LEGACY-34).

Compatibility: behavioral
(Magic ref: irouter/irCommand.c:1445-1740)

#### Scenario: Route between labels
- **GIVEN** labels `A` and `B` on metal1 in the edit cell and an `irouter` style with metal1 active
- **WHEN** `iroute route -slabel A -dlabel B` is executed
- **THEN** a route is painted into the edit cell, selected, and `Done Routing.` is printed

#### Scenario: Bad option
- **WHEN** `iroute route -foo` is executed
- **THEN** `Bad option to 'iroute route':  '-foo'` is reported and nothing is routed

### Requirement: Route context and endpoints
Routing SHALL use as reference window the window set by `wizard window` (if its id exists; otherwise `Couldn't find route window (%d),using command window as reference.`) or the command window; without a layout window: `Point to a layout window first.`; if the edit cell is not being edited in that window's root: `Nothing being edited in route window.`. Everything visible (expanded) in that window is seen by the router, the result is painted into the edit cell.

Start: `-spoint` coordinates transformed from edit to root coordinates; cursor point (must be in a layout window showing the route cell: `Can not use cursor as start:  cursor not in layout window.` / `...cursor not in routecell.`); label: searched first among selected labels, then all labels in the route cell hierarchy (`Warning: Start label '%s' not unique.`, `Start label '%s' not found.`), the start point is the label rectangle's lower-left corner and the start layer is the first active route layer connected to the label's type.

Destination: box (`Can not use box for dest:  No Box.`, `...box not in route cell.`), rectangle, label (same search, rectangle = label rectangle, analogous messages with `Destination`), or selection (a destination area for each selected tile of a type connecting to each permitted active layer). For a label destination only the first active route layer connected to the label type is used; otherwise every permitted active layer.

Start layers when not determined by a label: from the permitted active layers, find route contacts (both layers active) and layers touching the start point. No layer present → all permitted active layers; exactly one layer → that layer; exactly one contact → its two layers; otherwise print `Multiple nodes present at start point:` with the candidates and ask `Connect to %s? [yes] ` for each (empty answer = yes; the last candidate is taken automatically).

Compatibility: behavioral
(Magic ref: irouter/irRoute.c:95-470, irouter/irRoute.c:505-760, irouter/irRoute.c:900-1192)

#### Scenario: Label not found
- **WHEN** `iroute route -slabel nosuch` is executed
- **THEN** `Start label 'nosuch' not found.` is reported and nothing is painted

### Requirement: Route results
After the search the command SHALL: if a path was found, copy its paint into the edit cell (one undoable operation), replace the selection with the route, request redisplay and DRC re-check of the changed area, and print `Done Routing.`; if the search was interrupted after a complete path was found, print `Search Interrupted!` and `Using best path found prior to interrupt.` and paint that path; if no path: `Route Failed.`; if interrupted: `Route Interrupted!`. The scripting result (when available) SHALL be one of `Route success`, `Route best before interrupt`, `Route failure`, `Route unroutable`, `Route interrupted`, `Route already routed`.

Compatibility: behavioral
(Magic ref: irouter/irRoute.c:360-470, irouter/irCommand.c:1700-1740)

#### Scenario: Already connected
- **GIVEN** the start point lies on material already connected to the destination
- **WHEN** `iroute` runs
- **THEN** nothing is painted and the result is `Route already routed`

### Requirement: Layer and contact parameter tables
`iroute layers` and `iroute contacts` SHALL show or set route-type parameters. Layer parameters (in order): `active`, `width`, `length`, `hCost`, `vCost`, `jogCost`, `hintCost`, `overCost`. Contact parameters: `active`, `width`, `length`, `cost`. Argument forms (`T` = type or `*`, `P` = parameter or `*`):

- no args, `*`, or `* *` [values...]: table of all parameters for all types;
- `T` or `T *` [values...]: all parameters for type T;
- `* P` [values...]: parameter P for all types;
- `T P` [value...]: one parameter.

Values given after the parameter selection are consumed in table order (wrapping around); if their count does not come out even: `Warning:  Number of parameter values didn't match number of parameters.`. Tables print a heading `%-12.12s ` (`layer` or `contact`) then `%8.8s ` per parameter name, a dash line of the same widths, then per type `%-12.12s ` long type name followed by the values. Booleans print as `%8.8s ` of `YES`/`NO` (see Q-LEGACY-36 for a spurious error printed when only displaying); integers as `%8d `. Booleans accept unique prefixes of `yes no true false 1 0 on off`. Non-integer integers report `Noninteger value for integer parameter ("%.20s") ignored.`. Unknown type: `Unrecognized layer (type): "%.20s"`; type not a route layer/contact: `Unrecognized route layer or contact: "%.20s"` / `Unrecognized route-contact: "%.20s"`.

Compatibility: exact
(Magic ref: irouter/irCommand.c:200-520, irouter/irCommand.c:780-1070, irouter/irCommand.c:1177-1440, utils/set.c)

#### Scenario: Layer table row
- **GIVEN** route layer metal2 active, width 3, length 3, costs 32 64 256 1 1
- **WHEN** `iroute layers metal2` is executed
- **THEN** the row reads `metal2       ` followed by `     YES        3        3       32       64      256        1        1 `

### Requirement: Search, verbosity, version and wizard parameters
The command SHALL support:
- `iroute search [rate|width] [value]`: window rate and width (integers; printed `  %s=` then `%.0f `).
- `iroute verbosity [n]`: n >= 0; prints `\t1 (Brief messages)` for 1, `\t%d (Lots of statistics)` for >= 2, nothing for 0. Default 1.
- `iroute version`: prints `\tIrouter version 1.0`.
- `iroute wizard [param [value]]` with params (in order) `bloom` (bloom delta cost, integer), `bloomLimit` (integer; 0 = none), `boundsIncrement` (non-negative integer or a prefix of `AUTOMATIC`), `estimate` (boolean), `expandEndpoints` (boolean), `penalty` (decimal; stored with 0.5 resolution, printed `%f`), `penetration` (max walk length; integer or `AUTOMATIC`), `window` (route window: non-negative window id, `COMMAND` = the command's window, or `.` = the current window). Each prints `  %s=` followed by the value.

Compatibility: behavioral
(Magic ref: irouter/irCommand.c:530-780, irouter/irCommand.c:1756-1856, irouter/irCommand.c:2135-2355)

#### Scenario: Penalty resolution
- **WHEN** `iroute wizard penalty 1.3` is executed
- **THEN** `  penalty=1.000000` is printed (1.3 × 2 truncated to 2, divided by 2)

### Requirement: Spacings subcommand
`iroute spacings` SHALL: with no argument print, for each route type, `%s:  ` followed by `%s=%d ` for every type with spacing >= 0 and `SUBCELL=%d ` when set, then a blank line; with `CLEAR` set all spacings (including SUBCELL) to -1; with a route type print that type's line; with a route type and another type (or a prefix of `subcell`) print `\t%d` or `\tNIL`; with additional `type value` pairs set them, values being non-negative integers, `-1`, or prefixes of `n`, `nil`, `none`, `null` (all = NIL), echoing ` %s=%d` / ` %s=NIL`. Odd pair counts report `Type and value args don't pair evenly.`.

Compatibility: behavioral
(Magic ref: irouter/irCommand.c:1856-2130)

#### Scenario: Clear spacings
- **WHEN** `iroute spacings CLEAR` then `iroute spacings metal1` are executed
- **THEN** the second command prints `metal1:  ` with no spacing entries followed by a blank line

### Requirement: saveParameters file format
`iroute saveParameters <file>` SHALL write a command script that restores all interactive-router parameters when sourced. With no file: `Must specify save file!`; with extra args: `Too many args on ':iroute saveParameter'`; unopenable: `Could not open file '%s' for writing.`. The file content is, exactly:

```ebnf
save_file   = header , "\n" , ":iroute verbosity 0\n" , { contact_line } , { layer_line } ,
              search_lines , ":iroute spacings CLEAR\n" , { spacing_line } , wizard_lines ,
              ":iroute verbosity " , int , "\n" ;
header      = "# Irouter version 1.0\n" , "#\n" ,
              "# This is a Magic command file generated by the Magic command\n" ,
              "#\t:iroute saveParameters\n" ,
              "# To restore these parameter settings, use the Magic `:source' command.\n" ;
contact_line = ":iroute contact " , longname , " * " , bool , int8 , int8 , int8 , "\n" ;   (* active width length cost *)
layer_line   = ":iroute layer " , longname , " * " , bool , int8 , int8 , int8 , int8 , int8 , int8 , int8 , "\n" ;
               (* active width length hCost vCost jogCost hintCost overCost *)
search_lines = ":iroute search rate " , dl , "\n" , ":iroute search width " , dl , "\n" ;
spacing_line = ":iroute spacings " , longname , " " , ( longname | "SUBCELL" ) , " " , int , "\n" ;
wizard_lines = ":iroute wizard bloom " , dl , "\n" , ":iroute wizard bloomLimit " , int8 , "\n" ,
               ":iroute wizard boundsIncrement " , auto , "\n" , ":iroute wizard estimate " , bool , "\n" ,
               ":iroute wizard expandEndpoints " , bool , "\n" , ":iroute wizard penalty " , float6 , "\n" ,
               ":iroute wizard penetration " , auto , "\n" , ":iroute wizard window " , ( "COMMAND" | int ) , "\n" ;
bool   = printf("%8.8s ", "YES"|"NO") ;   int8 = printf("%8d ") ;   dl = printf("%.0f ") ;
auto   = "AUTOMATIC" | int8 ;              float6 = printf("%f") ;
```

Contacts and layers appear in technology-file order; spacing lines are emitted per route type (route-type order) for every type index with spacing >= 0 in increasing type number, then the SUBCELL line (see Q-LEGACY-35). The literal subcommand words `contact` and `layer` are written (they prefix-match `contacts`/`layers`).

Example (scmos `irouter`, widths/spacings from DRC, abridged):

```
# Irouter version 1.0
#
# This is a Magic command file generated by the Magic command
#	:iroute saveParameters
# To restore these parameter settings, use the Magic `:source' command.

:iroute verbosity 0
:iroute contact m2contact *      YES        4        4     1024 
:iroute layer metal2 *      YES        3        3       32       64      256        1        1 
:iroute search rate 500 
:iroute search width 10000 
:iroute spacings CLEAR
:iroute spacings metal2 metal2 4
:iroute wizard bloom 1 
:iroute wizard bloomLimit        0 
:iroute wizard boundsIncrement AUTOMATIC
:iroute wizard estimate      YES 
:iroute wizard expandEndpoints      YES 
:iroute wizard penalty 1024.000000
:iroute wizard penetration AUTOMATIC
:iroute wizard window COMMAND
:iroute verbosity 1
```

Compatibility: exact
(Magic ref: irouter/irCommand.c:2356-2470, irouter/irInternal.h:40, utils/set.c)

#### Scenario: Round trip
- **GIVEN** `iroute layers metal2 hCost 40` was executed
- **WHEN** `iroute saveParameters p.ir` is run, the technology is reloaded, and `source p.ir` is executed
- **THEN** `iroute layers metal2 hCost` prints `      40 `

### Requirement: iroute help
`iroute help` SHALL print `\niroute - route from cursor to box\n\n`, then `iroute %s - %s` for each subcommand with its description, then `\niroute help <subcmd> - print usage info for subcommand.\n\n`; `iroute help <subcmd>` prints the description and `\nusage:\niroute %s\n` with the usage text.

Compatibility: informative
(Magic ref: irouter/irCommand.c:1072-1176, irouter/irCommand.c:2480-2570)

#### Scenario: Help for route
- **WHEN** `iroute help route` is executed
- **THEN** the route usage lists `-sLayers`, `-sCursor`, `-sLabel`, `-sPoint`, `-dLayers`, `-dBox`, `-dLabel`, `-dRect` and `-dSelection`

### Requirement: Interactive router diagnostics
The wizard command `*iroute` with subcommands `debug`, `help` and `parms` SHALL be treated as developer diagnostics.

Compatibility: informative
(Magic ref: irouter/irTestCmd.c:215-314)

#### Scenario: Diagnostics have no layout effect
- **WHEN** `*iroute parms` is executed
- **THEN** the parameters are printed and the layout is unchanged
