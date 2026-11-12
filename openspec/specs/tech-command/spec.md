# Technology Command

## Purpose

Defines the `tech` command. It queries the current technology (name, file name, version, internal scale, planes, layers and DRC values), loads a new technology, and changes which layers are locked. The loading semantics are in capability `tech-loading-and-scaling`. Locking semantics for the paint tables are in capability `tech-compose-and-connect`. Name and type-list parsing is in capability `tech-layers-and-planes`. The values returned by `tech drc` are defined by the `drc-*` capabilities.

Results are returned as script results, as in Magic's Tcl build, which is normative where the builds differ (`docs/spec-conventions.md`). Magic's non-Tcl build prints them to the console instead; that printed form is given where it differs and is `informative`. The information returned is `behavioral`.

## Requirements

### Requirement: Option parsing

The command `tech [option args...]` SHALL select its option by case-insensitive unique-prefix match against this ordered option list. An exact match wins.

Compatibility: behavioral

`load`, `help`, `name`, `filename`, `version`, `lambda`, `planes`, `layers`, `drc`, `locked`, `unlocked`, `revert`

- **No option.** `tech` with no option behaves as `tech help`.
- **Ambiguous.** An ambiguous prefix, such as `l`, `la` or `lo`, reports `Ambiguous techinfo option: "<opt>"` and then `  Type ":tech help" for help.`.
- **Unknown.** An unknown option reports `"<opt>" isn't a valid tech option.  Type ":tech help" for help.`.
- **Wrong argument count.** This reports `wrong number of arguments to command "tech"` and the help hint.
- **Namespace prefix.** A leading `::magic::` or `magic::` on the option is ignored.

The manual names the lock options `layers lock|unlock|revert`. The command actually takes `locked`, `unlocked` and `revert` as top-level options. `lock` and `unlock` work as prefixes (see Q-TECH-29).

(Magic ref: commands/CmdTZ.c:168-215, commands/CmdTZ.c:635-645, utils/lookup.c:58-130)

#### Scenario: Prefix selects lock
- **GIVEN** any technology
- **WHEN** `tech lock poly` is executed
- **THEN** it acts as `tech locked poly`

#### Scenario: Ambiguous option
- **GIVEN** any technology
- **WHEN** `tech la` is executed
- **THEN** `Ambiguous techinfo option: "la"` is reported

### Requirement: tech help

`tech help` SHALL print `Tech commands have the form "tech option",` and then `where option is one of:`. After that it prints one line per option: four spaces and then the option's help text, in option-list order.

Compatibility: informative

(Magic ref: commands/CmdTZ.c:624-631)

#### Scenario: Help lists twelve options
- **GIVEN** any technology
- **WHEN** `tech help` is executed
- **THEN** 12 option lines are printed after the 2 header lines

### Requirement: tech name, filename and version

The command SHALL return these technology properties:
- `tech name`: the technology name (see capability `tech-file-format`).
- `tech filename`: the full path of the loaded technology file. It is empty or null if the last load failed.
- `tech version`: the version string and the description string.

Compatibility: behavioral

Output forms:
- **Magic's non-Tcl build (informative):** `Technology name is "<n>"`, `Technology filename is "<f>"`, `Technology version is: "<v>"`, and `Description: <d>` on the next line.
- **Result:** the name, the file name, or the version followed by the description. With `magic-compat` on, the version result is built as Magic builds it: the raw version string, then the description appended as one Tcl list element (`Tcl_AppendElement`: a separating space unless the version is empty or ends in whitespace, and the description quoted when needed). The version string itself is not quoted, so a multi-word `version` line, whose words Magic joins with LF, does not form a proper list element. With `magic-compat` off, the result is a proper two-element list {version, description}.

(Magic ref: commands/CmdTZ.c:217-226, commands/CmdTZ.c:249-258)

#### Scenario: Version query
- **GIVEN** the example technology of capability `tech-file-format`
- **WHEN** `tech version` is executed
- **THEN** the result is the list {`1.0`, `Tiny demo process`}

#### Scenario: Multi-word version
- **GIVEN** a technology with `version 1.0 beta` and `description "a b"`
- **WHEN** `tech version` is executed with `magic-compat` on
- **THEN** the result is `1.0`, LF, `beta {a b}`
- **AND** with `magic-compat` off it is the list {`1.0`LF`beta`, `a b`}, shown as `{1.0`LF`beta} {a b}`

### Requirement: tech lambda

`tech lambda` SHALL report the internal scale fraction (I internal units = L lambda; see capability `tech-loading-and-scaling`).

Compatibility: behavioral

- **Result:** the two-element list {L, I}.
- **Magic's non-Tcl build (informative):** it prints `Internal units per Lambda = <L> / <I>`. The label is the inverse of the printed ratio (see Q-TECH-26).
- **Setting the fraction:** `tech lambda N [D]`, where N is an integer, first sets I := N and L := D. If D is omitted, L := 1. If D is given but is not an integer, L is unchanged. The fraction is then reduced and reported as above.
  - Setting it this way rescales no table and no layout (see Q-TECH-26). With `magic-compat` OFF, setting the fraction SHALL be refused with an error that points to `scalegrid`.

(Magic ref: commands/CmdTZ.c:227-243, commands/CmdTZ.c:259-271)

#### Scenario: Query after scalegrid
- **GIVEN** `scalegrid 1 2` has been executed from 1:1
- **WHEN** `tech lambda` is executed
- **THEN** the result is {1 2}

### Requirement: tech planes

`tech planes` SHALL list every plane from 0 to K−1, including the built-in planes, by long name, in plane-number order.

Compatibility: behavioral

- **Magic's non-Tcl build (informative):** it prints `Defined planes (<K>) are:`, then one line per plane: three spaces and the plane name.
- **Result:** a list of the names.

(Magic ref: commands/CmdTZ.c:244-247, commands/CmdTZ.c:272-276)

#### Scenario: Example technology planes
- **GIVEN** the example technology
- **WHEN** `tech planes` is executed
- **THEN** the result is {router designRuleCheck designRuleError mhint fhint rhint well active metal1 metal2}

### Requirement: tech layers

`tech layers` SHALL list types. Each argument form behaves as follows:

Compatibility: behavioral

1. **`tech layers` (no argument).** Print `Layer names are:`. Then, for each user type 9..U−1 in number order, print one line: four spaces, then all of its non-alias names in byte-wise sorted order, joined by ` or `. After the user types, list the built-ins `magnet`, `fence` and `rotate` the same way, with all of their names.
2. **`tech layers *`.** Return a list with one name per type, covering user types 9..U−1 and then magnet, fence and rotate. For each type the name chosen is the longest of its non-alias names (all names for built-ins), and the first in sorted order if several are equally long. Names are separated by single spaces.
3. **`tech layers <type-list>`.** The same as `*`, but only for the types in the parsed list. Unknown names are reported.

Stacked types are never listed. Any other argument count is a wrong-argument-count error.

The manual says the list form returns each type's primary name. The code returns the longest name (see Q-TECH-30).

(Magic ref: commands/CmdTZ.c:279-296, database/DBtechname.c:433-551)

#### Scenario: Formatted listing
- **GIVEN** the example technology
- **WHEN** `tech layers` is executed
- **THEN** the line for type 10 is `    p or poly or polysilicon`

#### Scenario: List form chooses the longest name
- **GIVEN** the example technology
- **WHEN** `tech layers *` is executed
- **THEN** the result begins `nwell polysilicon ndiffusion ntransistor metal1 metal2 polycontact ndcontact m2contact pwell` and ends `magnet fence rotate`

### Requirement: tech locked, unlocked and revert

The lock options SHALL change the current active (editable) layer set. The tech-file active set is fixed at load time (see capability `tech-layers-and-planes`). Each option behaves as follows:

Compatibility: behavioral

- **`tech locked` (no argument).** List, as with `tech layers <list>`, the types in the all-but-space-and-DRC set (6..255) that are not active.
- **`tech locked <list>`.** Let M be the parsed list, or the user layer set (space and types 9..U−1) for `*`. Then:
  1. Remove M from the active set.
  2. Lock every user contact in M (see capability `tech-compose-and-connect`, "Contact locking").
  3. For every stacked type whose component set intersects M: remove it from the active set and lock it.
- **`tech unlocked` (no argument).** List the active types.
- **`tech unlocked <list>`.** Let M be the parsed list, or the user layer set for `*`. Then:
  1. Add M to the active set.
  2. Unlock every user contact in M.
  3. For every stacked type whose component contacts are all active: add it and unlock it.
  4. Restrict the active set to types 6..255.
- **`tech revert` (no argument).** Set the active set equal to the tech-file active set, then adjust contacts (below).
- **`tech revert <list>`.** Let M be the parsed list. For each type in M, set its active bit to its value in the tech-file active set. Then adjust contacts (below).
  - With `*`, M is the tech-file active set itself. So `tech revert *` re-activates the layers that were active in the technology file, but in Magic does not re-lock the layers that were locked there (see Q-TECH-25). Hefesto SHALL also re-lock them, as `tech revert` with no argument does.
- **Contact adjustment after revert:**
  - Every user contact is unlocked if active and locked otherwise.
  - Every stacked type is made active and unlocked if all its component contacts are active.
  - Otherwise, if some component contact is active, the stacked type is made inactive and locked.
  - Otherwise it is left unchanged.
- **Other counts.** Any other argument count is a wrong-argument-count error.

(Magic ref: commands/CmdTZ.c:297-442)

#### Scenario: Lock and list
- **GIVEN** the example technology, where pwell is locked by the technology file
- **WHEN** `tech locked metal1` and then `tech locked` are executed
- **THEN** the listing contains metal1 and pwell

#### Scenario: Locking a contact protects it from residue erase
- **GIVEN** the example technology
- **WHEN** `tech locked pc` is executed
- **THEN** E[metal1](pc, metal1) = pc, and pc+m2c becomes inactive

#### Scenario: Full revert
- **GIVEN** the example technology after `tech unlocked pwell`
- **WHEN** `tech revert` is executed
- **THEN** pwell is locked again

### Requirement: tech drc

`tech drc <rule> <layer> [<layer2>]` SHALL query the current DRC rule set and return a distance in the current display units. The rule keyword is tested by case-sensitive prefix comparison:

Compatibility: behavioral

| Rule keyword (must begin with) | Layers | Returns |
|---|---|---|
| `width` | t1 | minimum width of t1 |
| `spac` | t1 [t2] | minimum spacing between t1 and t2 (t2 defaults to t1) |
| `surr` | t1 t2 (required) | minimum surround of t1 by t2 |
| `direc` | t1 t2 (required) | minimum one-orientation surround of t1 by t2 |

- **Unresolved layer.** If a layer name does not resolve, report the resolution error and `No such layer <n>`, and return nothing.
- **Missing second layer.** For `surr` and `direc`, a missing t2 reports `Requires two layer types.`.
- **Unknown keyword.** An unknown rule keyword returns nothing, silently.
- **Too few arguments.** Fewer than 4 words, counting `tech drc`, is a wrong-argument-count error.
- **Output.** The result is `<v>`. Informative: Magic's non-Tcl build prints `Minimum width is <v>`, `Minimum spacing is <v>`, `Minimum surround is <v>`, or `Minimum surround (in one orientation) is <v>`.

The values and their unit formatting are defined by the `drc-*` capabilities and the display-units capability.

(Magic ref: commands/CmdTZ.c:444-542)

#### Scenario: Width query
- **GIVEN** a technology whose metal1 minimum width is 3 lambda at 1:1, with display units in lambda
- **WHEN** `tech drc width metal1` is executed
- **THEN** the reported value is 3

### Requirement: tech load

`tech load <file> [-noprompt] [-override] [-nooverride]` SHALL load a technology as specified in capability `tech-loading-and-scaling`. The steps run in this order:

Compatibility: behavioral

1. **Options.** Trailing words are scanned from the end while they begin with `-` and at least 3 words remain:
   - `-override` clears the command-line override.
   - `-noprompt` suppresses the confirmation.
   - `-nooverride` is accepted and has no effect.
   - Any other `-` word stops the scan.
   - After the scan exactly `tech load <file>` must remain. Otherwise report `Usage: tech load <filename> [-noprompt] [-override]`.
2. **Command-line override.** If a command-line technology override is active (during startup; see capability `tech-loading-and-scaling`), do nothing and report nothing.
3. **Confirmation.** Unless `-noprompt` was given, if any non-internal cell has paint (types 6..255 on planes 3..K−1), ask `Technology file (re)loading may invalidate the existing layout.  Continue? ` with the choices `no` and `yes` (default `no`). Answering `no` cancels the command.
4. **Pre-check.** Run the readability pre-check. On failure the command returns ok status with the result set to `Technology file does not exist or is not readable\n` (no period, trailing LF) and prints no diagnostic line of its own. A missing file still prints the `Could not find file` message from the search.
5. **Load.** Perform the full load. On failure the command returns ok status with the result set to `Error in loading technology file\n` (trailing LF), after the loader's own error messages.

These are the results of Magic's Tcl build, which Hefesto follows (capability `scripting-api`, "Command result and error conventions"). Magic's non-Tcl build prints `Technology file does not exist or is not readable.` (with a period) and `Error in loading technology file` as error text instead. Hefesto's own diagnostic stream (text or JSON) carries the failure as diagnostic `TECH-FILE-002` or `TECH-LOAD-001` with the non-Tcl wording. With `--diagnostics=magic`, only the result is printed, as Magic does.

Hefesto SHALL present the confirmation in a way that suits the platform. A session that cannot ask (no prompt host, a headless session, or a script session whose `tk_dialog` gives no integer, capability `scripting-api` "Prompts and dialogs through the script layer") SHALL go ahead with `magic-compat` ON, as Magic's Tcl build does with TxDialog's -1, and SHALL treat it as declined with `magic-compat` OFF, reporting TECH-CMD-003 (`Technology file (re)loading may invalidate the existing layout.  Continue? (no: this session cannot ask; use -noprompt)`), unless `-noprompt` was given (Q-SCRIPT-48). An answered `no` cancels without a message in both settings.

(Magic ref: commands/CmdTZ.c:544-622)

#### Scenario: Reload without prompt
- **GIVEN** a session with painted layout
- **WHEN** `tech load tiny -noprompt` is executed
- **THEN** no confirmation is asked and the technology is reloaded

#### Scenario: Reload in a session that cannot ask
- **GIVEN** a `-dnull` script session with painted layout
- **WHEN** `tech load tiny` is executed without `-noprompt`
- **THEN** with `magic-compat` ON the technology is reloaded and `Existing layout may be invalid.` is reported
- **AND** with `magic-compat` OFF nothing is loaded and TECH-CMD-003 is reported

#### Scenario: Unknown flag
- **GIVEN** any session
- **WHEN** `tech load tiny -quiet` is executed
- **THEN** `Usage: tech load <filename> [-noprompt] [-override]` is reported and nothing is loaded

#### Scenario: Result of a failed load
- **GIVEN** no file `nothere.tech` and a file `bad.tech` that fails to load
- **WHEN** `puts [tech load nothere -noprompt]` and `puts [tech load bad -noprompt]` run
- **THEN** stdout shows `Technology file does not exist or is not readable` followed by an empty line, then `Error in loading technology file` followed by an empty line
