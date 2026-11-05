# Command System

## Purpose
Every Magic action is a text command. Each window client registers its own command table:
- the global `*window` client
- `layout`
- `color`
- `netlist`
- `wind3d`, available only in Tcl + OpenGL builds

Commands come from typed lines, key and button macros, startup files, and Tcl. Each command is dispatched to the client of the window under the cursor. Coordinates in commands can carry unit suffixes.

## Requirements

### Requirement: Command registration and lookup
Commands SHALL be registered per client, in sorted order. Lookup SHALL be case-insensitive and SHALL accept unique prefixes. In the Tcl build, the `magic::` and `::magic::` prefixes SHALL be stripped before lookup.

Commands whose names begin with `*` are wizard commands. `help` SHALL hide them unless `help wizard` is given. (src: windows/windMain.c:223-576, utils/lookup.c:57-126, windows/windSend.c:401-452)

#### Scenario: Abbreviation
- **WHEN** `getc foo` is typed
- **THEN** the `getcell` command SHALL run

#### Scenario: Ambiguous abbreviation
- **WHEN** the typed prefix matches more than one command
- **THEN** the system SHALL report "That command abbreviation is ambiguous."

### Requirement: Dispatch to window clients
A command SHALL be looked up in both the global table and the table of the client that owns the target window. If both tables match:
- an exact name SHALL win over an abbreviation
- if both are exact or both are abbreviations, the window's own client SHALL win

With no window, the `layout` client SHALL be used. While a mouse button is held, the client that received the press SHALL receive all input. (src: windows/windSend.c:85-384)

#### Scenario: Unknown command
- **WHEN** `foo` is typed in a layout window
- **THEN** the system SHALL report "Unknown command: foo" and "Did you point to the correct window?"

### Requirement: Coordinate units and parsing
Coordinates SHALL accept these suffixes:
- `l` (lambda)
- `i` (internal)
- `g` (user grid)
- `nm`, `um`, `mm`, `cm`, `u`, `micron`, `centimicron`/`cu`

Coordinates MAY be simple `+ - * /` expressions, which SHALL be evaluated left to right. A value without a suffix SHALL use the `snap` setting while `units` is `default`, and SHALL use `units` otherwise. Defaults SHALL be: snap = lambda, units = default. (src: commands/CmdSubrs.c:122-321, dbwind/DBWtools.c:72,82)

#### Scenario: Micron coordinates
- **WHEN** `box 0 0 1um 2um` is run
- **THEN** the box SHALL span 1 µm × 2 µm, whatever the lambda value

#### Scenario: Unparseable value
- **WHEN** a coordinate cannot be parsed
- **THEN** the system SHALL report "Coordinate value cannot be parsed:  assuming 0"

### Requirement: Units and snap settings
`units default|internal|lambda|grid|user|microns|list|print|noprint` SHALL control how values are reported. `snap internal|lambda|grid|user|on|off|list` SHALL control box snapping and the unit used for coordinates without a suffix. If `snap internal` is run while units are still `default`, the units SHALL also change, with a deprecation warning. (src: commands/CmdTZ.c:873-1004, commands/CmdRS.c:953-1021)

#### Scenario: Report in microns
- **WHEN** `units microns print` is set and `box` is run
- **THEN** the dimensions SHALL be reported with a `um` suffix

### Requirement: Macros
`macro [client|tool] [list|help|search text|-reverse|copy tool] [key [value [help]]]` SHALL bind keys and buttons to command strings. For the layout client, macro tables SHALL be kept per tool. An empty value SHALL delete a binding.

Key names SHALL accept the modifier prefixes `Meta_`, `Alt_`, `Control_`, `^`, `Capslock_` and `Shift_`, followed by an X keysym name or a single character. The last long command typed SHALL be stored as macro `.`. (src: windows/windCmdAM.c:1044-1384, utils/macros.c, textio/txCommands.c:1275,1308)

#### Scenario: Default key bindings
- **GIVEN** the default system `.magicrc` with the new macro set
- **WHEN** the user presses `s` in a layout window
- **THEN** `select` SHALL run (src: magic/proto.magicrc.in:11-198)

### Requirement: Command lines and scripts
A line MAY contain several commands separated by `;`. Limits SHALL be 200 arguments and 2048 characters per command. Typing `:` or `;` in a layout window SHALL open the command prompt. Any other key SHALL be looked up as a macro. In script files read by the non-Tcl build:
- `\` SHALL continue a line
- `#` lines and blank lines SHALL be skipped

(src: textio/txcommands.h:38-39, textio/txCommands.c:1266-1520)

#### Scenario: Too many arguments
- **WHEN** a command has more than 200 arguments
- **THEN** the system SHALL report "Error: number of command arguments exceeds 200!"

### Requirement: Undo and redo commands
`undo [count|print|enable|disable]` and `redo [count]` SHALL step through the undo log. (src: windows/windCmdSZ.c:634-700)

#### Scenario: Undo paint
- **WHEN** paint is added and then `undo` is run
- **THEN** the paint SHALL be removed
