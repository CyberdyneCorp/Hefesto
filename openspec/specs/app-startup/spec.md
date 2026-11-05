# Application Startup

## Purpose
Defines how the application starts and ends:
- command-line options and file arguments
- installation-root and search-path resolution
- the order of startup files
- default technology selection
- display selection and batch (headless) mode
- crash recovery at startup
- `quit`, `version` and exit status

Host mechanics (environment variables, Tcl launcher scripts, terminals, `/tmp`) are `informative`. Platform decisions for macOS, Linux and iPadOS are logged as `[platform]` open questions. Display-style loading is in `display-style-files`. Technology-file loading is owned by the technology capability.

## Requirements

### Requirement: Startup sequence
Startup SHALL run in this order:
1. Initialize the text console.
2. Guess the display type, then apply any legacy `displays` file entry.
3. Parse the command line.
4. Initialize the database, macros and signal handling. In batch mode, interrupt handling is not installed.
5. Initialize the display. Failure ends startup with status 1.
6. Register the technology-section readers and load the built-in `minimum` technology. Failure ends startup with status 2.
7. Initialize undo, windows, commands and the layout client (creating the initial window unless suppressed), then the color client, 3-D client, extractor, plow, selection, wiring, netlist menu, DRC, routers and simulator modules.
8. Process the startup files (see Requirement: Startup file order).
9. Choose and load the technology (see Requirement: Default technology selection).
10. Either recover a crash file (`-r`) or load the command-line files.
11. Set the box to the edit cell's bounding box.
12. Set the undo fence so that startup actions cannot be undone.

The application SHALL be considered initialized only after all steps succeed. In the non-Tcl build it then reads commands from the terminal and window until `quit` or end of input.

Compatibility: behavioral
(Magic ref: utils/main.c:511-556, utils/main.c:557-834, utils/main.c:835-1258, utils/main.c:1286-1317)

#### Scenario: Undo fence
- **GIVEN** a startup file that paints metal1
- **WHEN** startup completes and the user types `undo`
- **THEN** `Nothing more to undo` SHALL be printed and the paint SHALL remain

### Requirement: Command-line options
The core option parser SHALL accept the following. Options that take an argument SHALL accept it attached (`-dNULL`) or as the next word. An argument that starts with `-` SHALL be rejected with `Bad name after '-X' option: 'ARG'` and startup SHALL fail with status 1.

| Option | Meaning |
|---|---|
| `-g PORT` | graphics port; also sets the tablet port unless `-i` was given |
| `-i PORT` | tablet port |
| `-d TYPE` | display type (see Requirement: Display type selection) |
| `-m TYPE` | monitor type (default `std`); selects the color map variant |
| `-T TECH` | technology; overrides every other choice during startup |
| `-r`, `-re…` | recover a crash file |
| `-rc…` / `-rcfile FILE` | use FILE as the user startup file name instead of `.magicrc` |
| `-D` | debug mode |
| `-w…` | (Tcl build) no initial window |
| `-noc…` | (Tcl build) no console |
| `-now…` | (Tcl build) no initial window |
| `-nor…` | (Tcl build) read no user startup files |

`-n` with fewer than 4 characters SHALL report `Ambiguous option -n…:  use -nowindow, -noconsole, or -norcfile` and fail with status 1. Any other option SHALL report `Unknown option: '-X'` and a usage line, and fail with status 1.

Compatibility: behavioral (option letters exact)
(Magic ref: utils/main.c:253-473, utils/main.c:474-510)

#### Scenario: Unknown option
- **WHEN** the application is started with `-x`
- **THEN** `Unknown option: '-x'` and the usage line SHALL be printed and the exit status SHALL be 1

#### Scenario: Custom startup file
- **WHEN** the application is started with `-rcfile my.rc`
- **THEN** `~/my.rc` and then `./my.rc` SHALL be read in place of the `.magicrc` files

### Requirement: Command-line file arguments
Each non-option argument SHALL be recorded as a file to load, unless the initial window is suppressed. In that case Magic ignores file arguments (see Q-UI-25). Recording rules:
- A trailing `.mag` SHALL be stripped.
- The type SHALL be chosen by the last `.` extension, case-insensitively: `.lef` → LEF, `.def` → DEF, `.cif` → CIF, any extension starting with `.gd` → GDS, `.tcl` → script (Tcl build). Anything else is a layout cell.
- In the Tcl build, a `.tcl` argument SHALL end option and file processing. Later arguments belong to the script.

After startup files and technology loading:
- The recorded files SHALL be processed in reverse command-line order, each announced with `Loading "F" from command line.`.
- A layout cell SHALL be loaded into every layout window.
- LEF and DEF files SHALL be read.
- A script SHALL be evaluated.
- CIF and GDS arguments SHALL be announced but not read (see Q-UI-25).

Because every layout load replaces the window contents, the window SHALL end up showing the first layout file named on the command line.

Compatibility: behavioral
(Magic ref: utils/main.c:388-452, utils/main.c:1176-1228)

#### Scenario: Two cells
- **WHEN** the application is started with `a.mag b`
- **THEN** `b` SHALL be loaded first and then `a`
- **AND** the window SHALL show `a`

#### Scenario: Suffix stripped
- **WHEN** the application is started with `inv.mag`
- **THEN** the cell `inv` SHALL be loaded

### Requirement: Installation root and search paths
The installation root (`CAD_ROOT`) SHALL be resolved from, in order:
1. the scripting variable `CAD_ROOT` (Tcl build)
2. the environment variable `CAD_ROOT`
3. the built-in install prefix

Path entries SHALL expand `~`, `~user` and `$VAR`/`${VAR}`. Expansions SHALL be cached for the session.

The search paths SHALL be:
- system library path: `. $CAD_ROOT/magic/sys $CAD_ROOT/magic/sys/current`
- cell library path: `$CAD_ROOT/magic/<tech> $CAD_ROOT/magic/sys/current $CAD_ROOT/magic/tutorial` when a technology is known at that point, otherwise `$CAD_ROOT/magic/sys/current $CAD_ROOT/magic/tutorial`
- system startup file: `$CAD_ROOT/magic/sys/.magicrc`
- pre-startup file (Tcl build): `$CAD_ROOT/magic/sys/.initrc`

Hefesto SHALL let the user or the PDK configuration designate an installation root containing `magic/sys`. On iPadOS this root SHALL be inside the app's sandbox (see Q-UI-26).

Compatibility: behavioral (host mechanics informative)
(Magic ref: utils/path.c:193-388, utils/paths.h:30-73, utils/main.c:557-834, tcltk/tclmagic.c:1663-1669)

#### Scenario: Relocated install
- **GIVEN** `CAD_ROOT=/opt/eda/lib`
- **WHEN** the application starts
- **THEN** `mos.OpenGL.dstyle`, technology files and the system startup file SHALL be looked up under `/opt/eda/lib/magic/sys`

### Requirement: Startup file order
Startup files SHALL be processed in this order:
1. (Tcl build) the pre-startup file `$CAD_ROOT/magic/sys/.initrc`, which by default sources `site.pre` if present.
2. The `-T` technology, if given. If it cannot be loaded, report `Failed to load technology "T"` or `Error loading technology "T"`, and the `-T` override SHALL be cancelled.
3. (Non-Tcl build) print `Using technology "NAME", version V.` and the description, when a technology is loaded.
4. The system startup file `$CAD_ROOT/magic/sys/.magicrc`. The default one ends by sourcing `site.def` (Tcl).
5. Unless startup files were disabled (`-norcfile`):
   1. The home file `~/<rcname>` when rcname is not absolute. If it is missing, `~/.magic` is tried; in the non-Tcl build only when rcname is `.magicrc`. `~/.magic` prints `Note:  Use of the file name "~/.magic" is deprecated.  Please change this to "~/.magicrc".`.
   2. The local file `<rcname>`, resolved from the current directory. In the Tcl build it SHALL be skipped when the current directory is the home directory, unless rcname is absolute. The non-Tcl build reads it even then (see Q-UI-27). If it is missing and rcname is `.magicrc`, `./.magic` (with a deprecation note) and then `magic_setup` are tried. A missing non-default rcname SHALL report `Startup file "F" not found or unreadable!` (non-Tcl).

A script error in a startup file SHALL be reported and startup SHALL continue. For local files, `Bad local startup file "F", continuing without.` SHALL also be printed. Startup files SHALL be executed as command streams (see ui-command-dispatch).

While startup files run with a `-T` technology in force, `tech load` without `-override` SHALL be silently ignored.

Compatibility: behavioral
(Magic ref: utils/main.c:835-1135, magic/proto.initrc:1-4, magic/proto.magicrc.in:315-318, commands/CmdTZ.c:544-580)

#### Scenario: Tech override protects -T
- **GIVEN** the application is started with `-T sky130A`, and `./.magicrc` contains `tech load gf180mcuD`
- **WHEN** startup completes
- **THEN** the technology SHALL be `sky130A`

#### Scenario: Bad local file
- **GIVEN** the local `.magicrc` raises a script error (Tcl build)
- **WHEN** the application starts
- **THEN** `Bad local startup file ".magicrc", continuing without.` SHALL be printed and startup SHALL complete

### Requirement: Default technology selection
After the startup files, the technology SHALL be chosen as follows:
1. If no technology was loaded by `-T` or by a startup file, and a file argument was given, the system SHALL read the `tech` line of the first file argument and try to load that technology.
2. If still no technology is loaded, the built-in `minimum` technology SHALL be loaded again and kept.
3. If `minimum` cannot be loaded, startup SHALL fail and the process SHALL exit with status 255.

No path selects `scmos` by default, although the manual page claims it does (see Q-UI-28).

Compatibility: behavioral
(Magic ref: utils/main.c:1130-1170, doc/man/magic.1)

#### Scenario: Tech from layout
- **GIVEN** `inv.mag` contains the line `tech sky130A` and no startup file loads a technology
- **WHEN** the application is started with `inv.mag`
- **THEN** `sky130A` SHALL be loaded before `inv` is read

#### Scenario: Nothing specified
- **WHEN** the application starts with no `-T`, no technology in any startup file, and no file arguments
- **THEN** the technology SHALL be `minimum`

### Requirement: Display type selection
With no `-d`, the display type SHALL come from the legacy `displays` file if it matches. Otherwise it SHALL be `XWIND` when the `DISPLAY` environment variable is set, and `NULL` otherwise.

A `-d` value SHALL be upper-cased and leading whitespace skipped. It SHALL then be matched against the compiled-in names `XWIND`, `X11`, `8BIT`, `16BIT`, `24BIT`, `OPEN_GL`, `OGL`, `OPENGL`, `CAIRO`, `XR`, `NULL`, in that order. A name matches when the given value starts with it (see Q-UI-29).

An unmatched value SHALL report:
- `Unknown display type:  V`
- `These display types are available in this version of Magic:`
- the names, one per line
- `Use '-d NULL' if you don't need graphics.`

Startup SHALL then fail with status 1. A display that fails to initialize SHALL report `The graphics display couldn't be correctly initialized.` and fail the same way.

Hefesto's display selection SHALL be its own GPU backend or headless mode. It SHALL accept `-d NULL` (and any value starting with `NULL`) as headless, and SHALL treat the other names as requests for its default GUI backend (see Q-UI-29).

Compatibility: behavioral (host backends informative)
(Magic ref: graphics/grMain.c:102-146, graphics/grMain.c:246-328, graphics/grMain.c:382-445)

#### Scenario: Prefix rule
- **WHEN** the application is started with `-d nullx`
- **THEN** the headless display SHALL be chosen, because `NULLX` starts with `NULL`

#### Scenario: Unknown display
- **WHEN** the application is started with `-d FOO`
- **THEN** the known types and `Use '-d NULL' if you don't need graphics.` SHALL be printed and the exit status SHALL be 1

### Requirement: Batch and headless operation
With the `NULL` display, the application SHALL draw nothing and SHALL run every non-graphical command normally. The null display has a fixed 512×484-pixel screen, so window and view commands still compute views.

In batch mode, interrupt handling SHALL be disabled. Batch mode is the `magicdnull` executable selected by `-dnull`, or the Tcl variable `batch_mode`.

Commands SHALL be read from standard input:
- In the non-Tcl build, standard input is read as keystrokes, so command lines must start with `:`. End of standard input SHALL end the program with `EOF encountered on input stream -- Magic exiting.` and status 1.
- In the Tcl build, the interpreter reads standard input until `quit` or end of input.

Hefesto SHALL provide an equivalent headless mode on Linux and macOS that takes a command script from standard input or from a file argument. iPadOS has no headless mode (see Q-UI-30).

Compatibility: behavioral
(Magic ref: utils/main.c:557-834, graphics/grNull.c:370-380, textio/txCommands.c:1595-1769, tcltk/magic.sh.in:33-72)

#### Scenario: Scripted run
- **GIVEN** the Tcl build of Magic, or Hefesto's headless mode
- **WHEN** `echo "box 0 0 10 10 ; quit -noprompt" | <app> -dnull -noconsole -T scmos` is run
- **THEN** the box SHALL be set and the process SHALL exit with status 0

### Requirement: Tcl launcher options
The Tcl launcher SHALL handle these options itself (informative):
- `--version` prints `MAJOR.MINOR.REVISION` and exits 0
- `--commit` prints the source revision and exits 0
- `--prefix` prints `CAD_ROOT` and exits 0
- `-now`, `-nowrap` and `-nowrapper` disable the GUI wrapper
- `-dnull`, as one word, implies no wrapper and runs the batch executable
- `-noc…` runs without the console
- `-r`, `-re` and `-recover` run `crash recover` after startup
- the option arguments of `-rc…`, `-d`, `-g`, `-m`, `-i` and `-T` are passed through
- other words are cells to preload

A failing initialization SHALL exit with status 1. Hefesto SHALL support `--version`, printing its own version and the Magic compatibility version, and SHALL exit 0.

Compatibility: informative
(Magic ref: tcltk/magic.tcl.in:155-238, tcltk/magic.tcl.in:453-465, tcltk/magic.sh.in:33-72)

#### Scenario: Version query
- **WHEN** the application is run with `--version`
- **THEN** a version string SHALL be printed and the exit status SHALL be 0, without loading a technology

### Requirement: Crash recovery at startup
With `-r` and an initial window, startup SHALL look for a crash backup file instead of loading command-line files. It SHALL search the temporary directory (`$TMPDIR`, or the system default) for files that:
- have names starting with `MAG`
- are owned by the current user
- are not tied to a running process (a decimal process id after `MAG` and before the first `.` names a live process)

It SHALL pick the one with the most recent status-change time and ask `Recover from backup file F?` with the answers `read` (default) and `cancel`. On `read`, the backup SHALL be loaded and the backup file removed. If a backup file is in use by the current session, report `Error:  Backup file in use for current session.`.

On normal exit, the session's own backup file SHALL be removed. The backup file format and periodic backups are owned by the database capability. Hefesto SHALL keep backups in an application-private location (see Q-UI-31).

Compatibility: behavioral (location and process checks [platform])
(Magic ref: utils/main.c:1170-1185, database/DBio.c:1114-1235, utils/main.c:190-252)

#### Scenario: Live session skipped
- **GIVEN** `/tmp/MAG1234.xyz` is owned by the user and process 1234 is running
- **WHEN** another instance starts with `-r`
- **THEN** that file SHALL NOT be offered for recovery

### Requirement: quit command and exit status
`quit [STATUS] [-noprompt]` SHALL end the application:
- `-noprompt` SHALL be recognized only as the last word.
- STATUS is an integer. A non-integer SHALL report `Invalid exit_status: X`, and the word stays as an extra argument.
- Any remaining extra argument SHALL report `Usage: quit [exit_status] [-noprompt]` and quitting SHALL be cancelled.
- Without `-noprompt`, every client's exit check SHALL run first. Any check MAY veto the quit:
  - the layout client asks whether to discard modified cells
  - the color client asks about an unsaved color map
  - the netlist client asks about an unsaved netlist
- On exit the application SHALL close graphics, remove its crash backup, flush output, restore the terminal, and exit with STATUS (default 0).

Magic does not range-check STATUS, so the host truncates it to 8 bits (see Q-UI-32).

Exit statuses produced by the application itself SHALL be:

| Status | Cause |
|---|---|
| 0 | normal `quit`, or a second Ctrl-C while an interrupt is pending |
| 1 | bad option, display failure, Tcl initialization failure, or end of input on the non-Tcl terminal |
| 2 | `minimum` technology failure during initialization |
| 255 | `minimum` technology failure after the startup files |

Compatibility: behavioral
(Magic ref: windows/windCmdNR.c:262-334, utils/main.c:190-252, utils/main.c:1286-1317, graphics/grTOGL1.c:600-610)

#### Scenario: Quit with status
- **WHEN** `quit 3 -noprompt` is executed with modified cells
- **THEN** the process SHALL exit with status 3 without asking

#### Scenario: Veto
- **GIVEN** a modified cell
- **WHEN** `quit` is executed and the user declines to discard changes
- **THEN** the application SHALL keep running

### Requirement: version command
`version` SHALL print `Version V revision R.  Compiled on DATE.`. Any argument SHALL print `Usage: version`. Hefesto SHALL report its own version and SHALL state the Magic version it is compatible with (8.3.694 for this specification).

Compatibility: informative
(Magic ref: windows/windCmdSZ.c:762-797)

#### Scenario: Version output
- **WHEN** `version` is executed
- **THEN** a single line beginning with `Version ` SHALL be printed

### Requirement: Platform startup surface
On iPadOS, where there is no command line, environment or arbitrary file system, Hefesto SHALL provide equivalents for:
- choosing the installation root and PDK
- choosing the technology (the `-T` equivalent)
- enabling or disabling user startup files (the `-norcfile` equivalent)
- opening files at launch (file arguments), including files handed over by other apps
- crash recovery on the next launch

These SHALL keep the precedence order defined in this capability. The mechanisms are [platform] decisions (see Q-UI-26 and Q-UI-31).

Compatibility: behavioral
(Magic ref: utils/main.c:253-473, utils/main.c:835-1258)

#### Scenario: PDK default on iPad
- **GIVEN** Hefesto on iPadOS with a PDK selected as default in settings, and a `.mag` file opened from the Files app
- **WHEN** the app launches
- **THEN** the selected technology SHALL be loaded first, and then the opened cell SHALL be shown
