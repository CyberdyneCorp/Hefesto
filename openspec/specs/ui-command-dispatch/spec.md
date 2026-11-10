# UI Command Dispatch

## Purpose
Defines how user actions become commands and reach the code that runs them. This covers:
- the window-client model and per-client command tables
- command lookup, prefix rules and `help`
- command-line parsing: `;` splitting, quoting and limits
- interactive keys, key macros and `.` repeat
- script files
- the commands that drive the dispatcher itself: `macro`, `imacro`, `logcommands`, `pushbutton`, `setpoint`, `sleep`, `echo`, `send`, `source`, `history`

Coordinate parsing inside commands is owned by `cmd-coordinates-and-units`. Window and view commands are in `ui-windows-and-views`. Tools and default bindings are in `ui-tools-and-input`. Tcl-level command wrapping is owned by capability `scripting-api`.

## Requirements

### Requirement: Window client model
The system SHALL organize interaction around window clients. A client SHALL have a unique name and a sorted command table, and SHALL provide these handlers, any of which MAY be absent except the command handler:
- create a window (may refuse)
- delete a window (may refuse)
- redisplay an area
- execute a command or button event
- update after each command
- exit check, which may veto `quit`
- reposition

The built-in clients SHALL be:

| Client | Visibility | Role |
|---|---|---|
| `*window` | hidden: names starting with `*` are never listed or opened by users | global window commands |
| `layout` | visible | default client |
| `color` | visible | color-map editor |
| `netlist` | visible, optional module | netlist menu |
| `wind3d` | visible, optional module | 3-D view |

A client name typed by a user SHALL be accepted as any unique prefix of a registered name, matched case-sensitively. When the system lists window types (for usage messages), it SHALL list every non-hidden client name, one per line, each preceded by a tab.

Compatibility: behavioral
(Magic ref: windows/windMain.c:224-341, windows/windMain.c:396-420, windows/windInt.h:86-87)

#### Scenario: Client prefix
- **WHEN** `specialopen col` is executed
- **THEN** a `color` window SHALL be opened

#### Scenario: Hidden client
- **WHEN** `specialopen *window` is executed
- **THEN** the usage text `Usage: specialopen [leftx bottomy rightx topy] type [args]` and the list of valid window types SHALL be printed, and no window SHALL open

### Requirement: Command registration and help text
Each command SHALL be registered with a help string whose first whitespace-delimited word is the command name. The rest of the string is a one-line synopsis and MAY contain newlines. A client's table SHALL be kept in ascending byte order (`strcmp`) of the full help string. Commands whose names begin with `*` SHALL be wizard commands (see ui-wizard-and-debug).

The `*window` client SHALL register: `center`, `closewindow`, `cursor`, `grow`, `help`, `imacro`, `logcommands`, `macro`, `openwindow`, `over`, `pushbutton`, `quit`, `redo`, `redraw`, `reset`, `scroll`, `setpoint`, `sleep`, `specialopen`, `underneath`, `undo`, `updatedisplay`, `version`, `view`, `windowborder`, `windowcaption`, `windownames`, `windowpositions`, `windowscrollbars`, `xview`, `zoom`. It SHALL also register the wizard commands `*bypass`, `*crash`, `*files`, `*grstats`, `*pause`, `*winddebug` and `*winddump`. Informative: Magic's non-Tcl build omits `*bypass` and adds `echo`, `send` and `source`.

Compatibility: behavioral
(Magic ref: windows/windMain.c:472-553, windows/windClient.c:680-828)

#### Scenario: Ordering
- **GIVEN** commands registered in the order `zoom`, `center`, `view`
- **WHEN** `help` lists them
- **THEN** they SHALL appear in the order `center`, `view`, `zoom`

### Requirement: Command name lookup
A typed command word SHALL be matched against a table as follows. Only the part of each table entry before its first space SHALL count.
- Letters SHALL match case-insensitively. Other characters SHALL match exactly.
- If the word equals an entry name, that entry SHALL be chosen at once, even if earlier entries matched as prefixes.
- Otherwise, if the word is a prefix of exactly one entry name, that entry SHALL be chosen.
- If the word is a prefix of two or more entry names, the result SHALL be "ambiguous".
- If the word matches no entry, the result SHALL be "not found".
- An empty word is a prefix of every entry, so it is ambiguous whenever the table has two or more entries.

A leading `::magic::` or `magic::` SHALL be stripped before matching.

Compatibility: exact
(Magic ref: utils/lookup.c:58-126)

#### Scenario: Unique abbreviation
- **WHEN** `getc foo` is typed in a layout window
- **THEN** the `getcell` command SHALL run with argument `foo`

#### Scenario: Exact name beats abbreviation
- **GIVEN** a table with `ext` and `extract`
- **WHEN** `ext` is looked up
- **THEN** `ext` SHALL be chosen and the lookup SHALL NOT be ambiguous

#### Scenario: Case-insensitive
- **WHEN** `ZOOM 2` is typed
- **THEN** the `zoom` command SHALL run

### Requirement: Command dispatch across global and client tables
A text command SHALL be dispatched in these steps:
1. Determine the target window W. Use the command's window if it was given. Otherwise use the window id recorded with the command. Otherwise use the window under the command's point. If there is still no window, there is no W.
2. Determine the client C. With no W, C is `layout`. If the command's point is outside W's drawing area and W accepts window commands, C is `*window`, so border clicks go to the window manager. Otherwise C is W's client.
3. While any mouse button is held, the client that received the first press SHALL receive all commands and button events until every button is up.
4. Look up the word in the `*window` table, unless W exists and does not accept window commands. Also look up the word in C's table.
5. If either lookup is ambiguous, report `That command abbreviation is ambiguous.` and stop.
6. If both lookups are "not found", report `Unknown command:` followed by the command, and stop. If a button is held, also report `'C' window is waiting for B button to be released.`, where B is the first of left, right, middle that is held.
7. If the `*window` lookup found `help`, run `help` (see Requirement: help command).
8. If only one table matched, run that table's command.
9. If both matched, compare just the two entries with the rules of Requirement: Command name lookup. An exact name beats an abbreviation. If that is still ambiguous, report `That command abbreviation is ambiguous`. Otherwise prefer the client's command over the `*window` command.

A dispatched command whose argument count is zero SHALL be a no-op.

Magic's `Did you point to the correct window?` message and its `window is grabbing all input` line are unreachable in both of its builds, so Hefesto SHALL NOT print them; the output for an unknown command SHALL be exactly Magic's (see Q-UI-9, resolution: replicate).

Compatibility: behavioral (messages informative)
(Magic ref: windows/windSend.c:86-352, windows/windSend.c:354-400; Hefesto decision: docs/decisions/open-questions/UI.md#Q-UI-9)

#### Scenario: Unknown command
- **WHEN** `foo` is typed in a layout window
- **THEN** `Unknown command:` and the command text SHALL be reported
- **AND** no `Did you point to the correct window?` line SHALL follow (see Q-UI-9)

#### Scenario: Client overrides global
- **GIVEN** both the `*window` client and the `wind3d` client register `zoom`
- **WHEN** `zoom 2` is typed in a 3-D window
- **THEN** the `wind3d` `zoom` command SHALL run

#### Scenario: No window
- **GIVEN** a command arrives with no window and an unknown point
- **WHEN** it is dispatched
- **THEN** it SHALL be interpreted by the `layout` and `*window` tables

### Requirement: help command
`help [pattern]` SHALL print the `*window` table under the heading `Global Commands`. If the target client is not `*window`, it SHALL then print that client's table under the heading `<Client> Commands`, where the client name has its first letter capitalized. Each heading SHALL be followed by a line of 15 dashes.

Selection rules:
- Wizard entries SHALL be excluded.
- With a pattern P, only entries whose full help string matches the glob `*P*` SHALL be listed. P is truncated to 195 characters.
- `help wizard` SHALL list only wizard entries, under the heading `Wizard <Client> Commands`, followed by 22 dashes.
- More than one argument SHALL print `Usage:  help [pattern]`.

Compatibility: behavioral (format informative)
(Magic ref: windows/windSend.c:402-452)

#### Scenario: Pattern help
- **WHEN** `help window` is typed in a layout window
- **THEN** the listing SHALL include the `closewindow`, `openwindow` and `windowborder` synopses and SHALL exclude `zoom`

### Requirement: Command line splitting and quoting
A command line SHALL be split into commands and words as follows:
- Leading whitespace SHALL be skipped.
- Words are separated by runs of whitespace.
- An unquoted `;` SHALL end the current command. Parsing SHALL continue after it, skipping leading whitespace.
- `"` or `'` SHALL start a quoted span that ends at the next matching quote character. Inside the span, whitespace and `;` are literal. Inside the span, `\` SHALL make the following character literal and SHALL itself be removed. Outside quotes, `\` is literal.
- Quoted spans SHALL join with adjacent unquoted text into one word (`a"b c"d` is the single word `ab cd`). An empty quoted span SHALL produce an empty word.
- A missing closing quote SHALL report `Unmatched " in string, I'll pretend that there is one at the end` and parsing SHALL continue.
- An empty command, such as between `;;`, is a no-op.

Limits:
- A command line SHALL have no length limit and a command SHALL have no word-count limit, in every mode: no text SHALL be truncated and no length or word-count error SHALL be reported (see Q-UI-10, resolution: fix).
- Informative: Magic's Tcl build reports `Error: number of command arguments exceeds 200!` for more than 200 words and `Error: command length exceeds 2048 characters!` at 2048 bytes. Its non-Tcl build copies each command into a 2048-byte buffer, silently dropping text after the first 2047 bytes, and reports `Too many arguments.` and then `Unable to completely parse command line '<line>'` for a command reaching 200 words, dropping that command and all later commands on the line. Lifting these limits only turns former errors into successes, so no Magic script or PDK depends on them.

Compatibility: exact
(Magic ref: utils/parser.c:55-160, textio/txCommands.c:1095-1153, textio/txCommands.c:1491-1594, textio/txcommands.h:38-39; Hefesto decision: docs/decisions/open-questions/UI.md#Q-UI-10)

#### Scenario: Two commands with quoting
- **WHEN** the line `label "a;b" n ; box 0 0 1 1` is parsed
- **THEN** there SHALL be two commands: [`label`, `a;b`, `n`] and [`box`, `0`, `0`, `1`, `1`]

#### Scenario: Empty quoted word
- **WHEN** `macro x ""` is parsed
- **THEN** argv SHALL be [`macro`, `x`, ``] with three words

#### Scenario: Backslash inside quotes
- **WHEN** `echo "say \"hi\""` is parsed
- **THEN** the second word SHALL be `say "hi"`

#### Scenario: Long command lines are not limited
- **WHEN** a command of 300 words and 5000 bytes is parsed
- **THEN** it SHALL be dispatched with all 300 words intact
- **AND** no length or word-count error SHALL be reported

### Requirement: Interactive keystroke handling
Keystrokes SHALL be handled as follows in a layout window, outside a pending text-entry line:
- `:` or `;` SHALL open a command prompt that reads one line of text (prompt `:`). A non-empty line SHALL be stored as the `.` macro of the layout client's current table and then executed as a command line.
- A Return key with no macro SHALL only reprint the prompt.
- Any other key SHALL be looked up as a macro for the current window's client. For layout windows, that is the current tool's table. Then:
  - an ordinary macro SHALL be executed as a command line
  - an interactive macro (`imacro`) SHALL open the command prompt pre-filled with the macro text for the user to complete, and the completed line SHALL be stored as `.` and executed
  - an undefined key SHALL report `Unknown macro or short command: '<name>'`, where `<name>` is the key's display name (see Requirement: Macro key names)
- Interactive macros whose text starts with `:` SHALL open the prompt with that `:` removed.

Informative: Magic's non-Tcl build always looks key macros up in the layout client's current-tool table, whatever window has focus (see Q-UI-11).

Compatibility: behavioral
(Magic ref: textio/txCommands.c:1199-1363, graphics/grTOGL1.c:765-850)

#### Scenario: Repeat last long command
- **GIVEN** the user types `:paint m1` and presses Return
- **WHEN** the user presses `.`
- **THEN** `paint m1` SHALL run again

#### Scenario: Undefined key
- **WHEN** the user presses `j` with no macro bound
- **THEN** `Unknown macro or short command: 'j'` SHALL be reported

### Requirement: Macro tables
Macros SHALL be stored per table name. For the `layout` client, the table name SHALL be the current tool name (see ui-tools-and-input), so each tool has its own key and button bindings. For other clients, the table name SHALL be the client name. A macro entry SHALL hold:
- the command text
- an optional help text
- an interactive flag

Defining a key that already has an entry SHALL replace the text and the flag, and SHALL clear the help text unless new help is given. Deleting a key SHALL remove its entry.

Compatibility: behavioral
(Magic ref: utils/macros.c:96-195, utils/macros.c:405-461)

#### Scenario: Per-tool binding
- **GIVEN** the box tool is current and `macro Button1 "box move bl cursor"` is defined
- **WHEN** the tool is switched to `wiring` and `macro Button1` is queried
- **THEN** the wiring tool's own binding (or none) SHALL be reported, not the box tool's

### Requirement: Macro key names
A key name SHALL be parsed as zero or more modifier prefixes followed by a key:
- Modifier prefixes: `Meta_` or `Alt_` (meta), `Control_` or `^` (control; `^` only when another character follows), `Capslock_` (lock), `Shift_` (shift). A key name wrapped in single quotes SHALL have the quotes removed.
- An optional `XK_` prefix before the key SHALL be ignored.
- A single remaining character is a character key.
- `<del>` is code 0x7F.
- `ButtonN` SHALL mean pointer button N, where 1 = left, 2 = middle, 3 = right, 4 = wheel up and 5 = wheel down.
- Any other name SHALL be a named key, using X11 keysym names such as `F1`, `KP_Left`, `space`, `colon`.

A single character with Control or Shift SHALL be normalized:
- The character SHALL be upper-cased.
- With Shift, the key SHALL be the upper-case character.
- With Control only, the key SHALL be the control code (character − `A` + 1).
- Unless Meta or Lock is also present, the modifiers SHALL be dropped, except that Control together with Shift is kept.

So `^A`, `Control_a` and the single control character 0x01 SHALL name the same key, and `Shift_a` SHALL equal `A`.

Without extended key support (Magic's non-X builds), only single characters and `^X` forms SHALL be accepted. Other names SHALL print `Extended macros are unavailable with graphics type "T".` once.

Display names SHALL be:
- control codes below 0x20: `^` + (code + `@`)
- 0x7F: `<del>`
- other ASCII characters: themselves
- named keys: the modifier prefixes `Meta_`, `Control_`, `Capslock_`, `Shift_` (in that order), then `XK_`, then the keysym name

Hefesto SHALL embed the X11 keysym name table, with Magic's `Meta_`/`Control_`/`Capslock_`/`Shift_` prefixes and 16-bit keysym masking, on every platform, and SHALL map each platform's native key codes to it, so that the key names in `.magicrc` and PDK macro files work unchanged (see Q-UI-12, resolution: replicate). Informative: Magic falls back to single-character, `^X` and `0x` names when no X display is open, which matters only for headless golden captures of `macro` listings.

Compatibility: behavioral (key-name strings exact; host keysym mapping [platform], see Q-UI-12)
(Magic ref: utils/macros.c:514-761, graphics/grTOGL1.c:550-603; Hefesto decision: docs/decisions/open-questions/UI.md#Q-UI-12)

#### Scenario: Control shorthand
- **WHEN** `macro ^R "clockwise 180"` and `macro Control_r` are both used
- **THEN** both SHALL address the same key, Ctrl-R

#### Scenario: Shifted letter
- **WHEN** `macro Shift_s "select more"` is defined
- **THEN** pressing Shift+S (the character `S`) SHALL run `select more`

### Requirement: macro and imacro commands
`macro` and `imacro` SHALL accept `macro [client|tool] [list|help|search TEXT|-reverse]… [key [value [helptext]]]`.

Selecting the table:
- An optional first word that exactly names a client or a registered tool SHALL select that table. Otherwise the table is chosen by the window the command came from (for layout, the current tool). With no window, the `layout` current-tool table is used.
- If the first remaining word is not a valid key but the word after it is, the command SHALL silently do nothing. This lets bindings for clients that are not compiled in fail quietly.

Listing:
- With no key, all entries SHALL be listed. `imacro` lists only interactive macros. `help` shows help text in place of the macro text when help exists. `search TEXT` keeps only entries whose shown text contains TEXT.
- Without `list`, the format SHALL be `Macro 'K' contains "TEXT"`, or `Interactive macro 'K' contains "TEXT"` for interactive macros.
- With `list`, the result SHALL be a flat list of key/text pairs, with the text before the key in each pair under `-reverse`. Informative: Magic's non-Tcl build prints `K = "TEXT"` lines instead.
- If nothing is listed and `list` was not given, the system SHALL print `No macros are defined for this client.`.
- An unknown table SHALL report `Cannot get macro list from current window.` and list the known tables.

Defining and querying:
- `key` alone SHALL print that binding. With `list`, it SHALL print the bare text.
- `key value` SHALL define the binding. `imacro` defines an interactive binding. An empty value SHALL delete it. With `help`, the value SHALL set the help text of an existing binding.
- `key value helptext` SHALL define the binding with help.
- An unparseable key SHALL report `Unrecognized macro name K`.
- `copy NAME` SHALL copy every entry of the current table into table NAME and register NAME as a tool name if it is new (see Q-UI-33).

Compatibility: behavioral
(Magic ref: windows/windCmdAM.c:997-1384, doc/html/macro.html)

#### Scenario: Delete binding
- **GIVEN** `macro z "zoom .5"` is defined in the box tool
- **WHEN** `macro z ""` is executed
- **THEN** pressing `z` SHALL report `Unknown macro or short command: 'z'`

#### Scenario: Client-qualified binding
- **WHEN** `macro color plus "color next"` is executed from a startup file
- **THEN** pressing `+` in a color window SHALL run `color next`

#### Scenario: Unavailable client fails quietly
- **GIVEN** a startup file written for a host that registers a client named `wind3d`, which the running application does not register
- **WHEN** `macro wind3d L "level up"` is read from that startup file
- **THEN** no error SHALL be printed

### Requirement: Script files and source
`source FILE` SHALL be the `source` command of the scripting interpreter (capability `scripting-api`), as in Magic's Tcl build. Informative: Magic's non-Tcl build registers its own `source` command, which opens FILE through the path `.` and then the system library path, executes it as a command stream, and reports `Couldn't read from FILE.` if the file cannot be opened.

Startup files and `source`d files SHALL be read by the scripting interpreter with its standard rules, as in Magic's Tcl build, which is normative (D-7). Command files that Hefesto reads through the command dispatcher SHALL follow the same Tcl line rules:
- A physical line ending in `\` + newline SHALL be joined with the next line as a standard Tcl continuation: the `\`, the newline and the leading whitespace of the next line SHALL be replaced by a single space (see Q-UI-13, resolution: fix).
- Lines that are empty, or whose first non-blank character is `#`, SHALL be skipped.
- A line beginning with a single `:` (not `::`, which is a namespace marker) SHALL have that character removed. There is no special test for a `;` after it.
- Each remaining line SHALL be parsed as a command line.

Informative: Magic's non-Tcl build reads command files with `txGetFileCommand`, which keeps the `\` of a continuation and removes only the newline, and removes the first character of any line that begins with `:`, also when the second character is `;`. That reader is reached only through the non-Tcl `source` command and non-Tcl startup files, so Hefesto does not reproduce these quirks (see Q-UI-13).

Command lines from scripts SHALL NOT be written to the command log. A point forced by `setpoint` SHALL stay in effect for all later commands of the same script. An interrupt SHALL abort the script with `[Read-in of command file aborted]`. End of file SHALL end the script. End of input on the interactive terminal SHALL print `EOF encountered on input stream -- Magic exiting.` and exit with status 1.

Compatibility: behavioral
(Magic ref: windows/windCmdSZ.c:363-400, textio/txCommands.c:1364-1438, textio/txCommands.c:1364-1418, textio/txCommands.c:1595-1769, tcltk/tclmagic.c:1290-1308, utils/main.c:899-1127; Hefesto decision: docs/decisions/open-questions/UI.md#Q-UI-13)

#### Scenario: Continuation and comments
- **GIVEN** a script whose lines are `# setup`, `box 0 0 \`, `    10 10`
- **WHEN** it is sourced, or read as a command file through the dispatcher
- **THEN** a single `box` command SHALL run, with argv [`box`, `0`, `0`, `10`, `10`], whatever the `magic-compat` setting

### Requirement: Command logging
`logcommands [start|stop|update|suspend|resume] [FILE]` SHALL control a command log. Keywords SHALL be matched by unique prefix. A non-keyword first argument is a file name only in the legacy form `logcommands FILE update`, which SHALL start logging and turn on update mode; `logcommands FILE` alone SHALL print the usage `Usage: logcommands [start|stop|update|suspend|resume [filename]]` and start nothing, as in Magic 8.3.694. With no argument, `logcommands` SHALL stop logging.

Starting:
- If a log is already open, report `There is already a log file open!`.
- If the file cannot be created, report `Could not open file 'F' for writing.`.
- Otherwise print `Logging commands to file "F"`.
- Write the header lines `# Magic command log file`, `# Using technology: T`, `# Title: <window caption>` and `# Date: <ctime>`.
- If the command came from a window, write `view Xi Yi Xi Yi` with the window's surface area. If a box exists, write `box values Xi Yi Xi Yi` with its root coordinates.

Logging rules:
- Each interactive command SHALL be written as its words joined by single spaces. Any word containing a space SHALL be wrapped in `"`.
- Commands whose name (after any `::`) starts with `logc`, or is `*bypass` or `setpoint`, SHALL NOT be logged. Neither SHALL `wire show`.
- Button events SHALL be written as `pushbutton left|middle|right down|up`.
- In update mode, `updatedisplay` SHALL follow each logged command.
- No logged line SHALL start with `:`; this includes the `#` header and the `view` and `box values` lines, as in Magic's Tcl build, which is normative (D-7). Informative: in Magic's non-Tcl build, every logged line except the `box values` line starts with `:`; Hefesto does not reproduce this (see Q-UI-14, resolution: other, nothing to fix).
- `suspend` and `resume` SHALL pause and continue logging. `update` SHALL toggle update mode, printing `Forcing display refresh after logged commands.` or `No display refresh after logged commands.`.
- `stop` SHALL print `Ending command logging to file.`.

Compatibility: exact (log file content); behavioral (messages)
(Magic ref: windows/windCmdAM.c:917-996, textio/txCommands.c:703-992; Hefesto decision: docs/decisions/open-questions/UI.md#Q-UI-14)

#### Scenario: Logged quoting
- **GIVEN** command logging to `log.txt`
- **WHEN** `label "my net" n` is executed interactively
- **THEN** the log SHALL contain the line `label "my net" n`

### Requirement: pushbutton and setpoint
`pushbutton BUTTON ACTION` SHALL synthesize a button event at the current command point. BUTTON is matched by prefix against `left`, `middle`, `right`, and ACTION against `down`, `up`. The event SHALL be dispatched as if it came from the pointer to the window's client: in a layout window it SHALL reach the box tool's button handler whatever tool's macro table is current (in Magic's Tcl build `tool` only swaps macro tables; the other tools act through their macros), and in a color window the color client's button handler. Bad usage SHALL report `Usage: pushbutton button action`.

`setpoint [x y [WID|name]]` SHALL force the point and window id of following commands. In interactive use the forced point SHALL apply to the next command only. In a script it SHALL stay in effect until the script ends. The values are window pixel coordinates with y measured from the window's bottom edge.

With no arguments, `setpoint` SHALL report the current point. Formats:
- the result SHALL be the list `x y X Y`
- informative: Magic's non-Tcl build prints `Point is at screen coordinates (x, y) in window W.` then `Point is at layout coordinates (X, Y)` with a window, and `Point is at screen coordinates (x, y).` without one

Non-integer x or y SHALL print `Usage: setpoint [x y [window ID|name]]`.

Compatibility: behavioral
(Magic ref: windows/windCmdNR.c:203-260, windows/windCmdSZ.c:214-294, textio/txCommands.c:1595-1769)

#### Scenario: Scripted click
- **GIVEN** a layout window with id 0
- **WHEN** a script runs `setpoint 100 120 0` and then `pushbutton left down` and `pushbutton left up`
- **THEN** the box tool's button handler SHALL receive a left press and release at window pixel (100,120), also when the wiring tool's macro table is current

### Requirement: sleep, echo and send
`sleep N` SHALL pause for N whole seconds, and SHALL NOT pause when N ≤ 0, whatever the `magic-compat` setting. Magic pauses N−1 seconds (none for N ≤ 1); this only changes timing, never output, so Hefesto does not reproduce it (see Q-UI-15, resolution: fix). An interrupt SHALL end the pause early. A wrong argument count SHALL report `Usage: sleep seconds`.

Informative: `echo` and `send` are Magic commands only in its non-Tcl build; in the Tcl build, which is normative, these names belong to the scripting environment. In the non-Tcl build, `echo [-n] words…` prints the words separated by single spaces and then a newline; if the first word is `-n`, the newline is suppressed, and the output starts with a space when more words follow. Hefesto does not reproduce this leading space, because `echo` is not a Magic command in the normative Tcl build (see Q-UI-15). `send TYPE command…` sends the command to the first window found of client TYPE (unique prefix; hidden clients refused), or with no window if none exists; a wrong usage prints `Usage: send type command` and the list of valid window types.

Compatibility: behavioral (sleep); informative (echo, send)
(Magic ref: windows/windCmdSZ.c:319-362, windows/windCmdAM.c:646-688, windows/windCmdSZ.c:1055-1100; Hefesto decision: docs/decisions/open-questions/UI.md#Q-UI-15)

#### Scenario: sleep of one second pauses one second
- **WHEN** `sleep 1` is executed
- **THEN** the command SHALL return after about 1 second

#### Scenario: sleep pauses the full count
- **WHEN** `sleep 3` is executed
- **THEN** the command SHALL return after about 3 seconds

### Requirement: Command history
With line-editing support, the command prompt SHALL keep a history of entered lines. History expansion SHALL follow GNU Readline rules. `history [n] [r] [NUM] [help]` SHALL list the last NUM entries, or all entries. Each entry SHALL be printed as tab, the index in `%4d`, a space and the line. `n` SHALL suppress the numbers, `r` SHALL reverse the order, and a zero or non-numeric NUM SHALL print usage. Without line editing there is no history, and the prompt supports only erase-char, kill-line, erase-word, reprint and literal-next.

Compatibility: informative
(Magic ref: commands/CmdFI.c:1472-1552, textio/txInput.c:997-1110, dbwind/DBWcommands.c:597-601)

#### Scenario: Reverse listing
- **GIVEN** the history contains `box`, then `paint m1`
- **WHEN** `history r 2` is executed
- **THEN** the listing SHALL show the `paint m1` entry before the `box` entry

### Requirement: Command counter and post-command update
After each dispatched command, the system SHALL:
1. increment a global command counter, except for `*bypass`
2. close the current undo event group for clients that use undo
3. run every client's update handler
4. update all windows (see ui-windows-and-views)

A command that ends with an interrupt SHALL print `[Interrupted]`.

Compatibility: behavioral
(Magic ref: textio/txCommands.c:1491-1594, textio/txCommands.c:1595-1769, dbwind/DBWprocs.c:984-1010, dbwind/DBWprocs.c:1068-1073)

#### Scenario: Highlights after command
- **WHEN** `box 0 0 10 10` completes
- **THEN** the box highlight SHALL be redrawn before the next prompt

### Requirement: Host command interface mechanics
Hefesto SHALL provide the dispatcher semantics independently of Magic's host mechanics. In Magic, commands reach the dispatcher from different hosts:
- the non-Tcl build: a terminal reader, `select()` on input devices, and an X11 event pump
- the Tcl build: Tcl `proc`s that call the dispatcher, Tk key bindings, and the tkcon console

Hefesto SHALL provide the same command semantics from its own command console and its scripting surface. It MUST NOT depend on a terminal. The scripting surface is the sandboxed Tcl interpreter that Hefesto embeds on every platform (decision X-3; capability `scripting-api`), but dispatching a command SHALL NOT require going through Tcl.

Compatibility: informative
(Magic ref: textio/txCommands.c:993-1094, textio/txCommands.c:1595-1769, tcltk/tclmagic.c)

#### Scenario: No terminal on iPadOS
- **GIVEN** Hefesto running on iPadOS
- **WHEN** the user types `zoom 2` into the in-app command console
- **THEN** the command SHALL be dispatched exactly as from Magic's terminal
