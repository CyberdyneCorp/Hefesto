# Startup and Configuration

## Purpose
Defines how Magic starts: command-line arguments, `CAD_ROOT`-based search paths, the startup (rc) file sequence, default technology selection, display selection, batch mode and exit codes. The C core (`utils/main.c`) runs four init stages (`mainInitBeforeArgs` → `mainDoArgs` → `mainInitAfterArgs` → `mainInitFinal`). In the Tcl build, `magic.tcl` drives those stages through `magic::initialize` and `magic::startup`.

## Requirements

### Requirement: Staged initialization
The system SHALL initialize in the order `mainInitBeforeArgs`, `mainDoArgs`, `mainInitAfterArgs`, `mainInitFinal`. The system SHALL set `MagicIsInitialized` only after all four stages succeed. (src: utils/main.c:1297-1314)

#### Scenario: Non-Tcl binary start
- **GIVEN** a non-Tcl build
- **WHEN** `magic` is run
- **THEN** `main()` SHALL call `magicMain()`, which runs `magicMainInit`, then the command loop `TxDispatch(NULL)`, then `MainExit(0)` (src: magic/magicTop.c:51-56, utils/main.c:1285-1295)

#### Scenario: Tcl build start
- **GIVEN** a Tcl build
- **WHEN** the `magic` shell script is run
- **THEN** it SHALL exec `magicexec` (Tk), `magicdnull` (no Tk, `batch_mode=true`) or `tkcon.tcl`
- **AND** each of these SHALL source `magic.tcl`, which loads `tclmagic`, then calls `magic::initialize` and `magic::startup` (src: tcltk/magic.sh.in:33-72, tcltk/magic.tcl.in:11-18, tcltk/tclmagic.c:693-799,960-1058)

### Requirement: Command-line options
The C argument parser SHALL accept the following options (src: utils/main.c:252-452):
- `-g port`, `-i port`: graphics and tablet ports
- `-d type`, also attached as `-dTYPE`: display type
- `-m type`: monitor type
- `-T tech`: technology, which overrides the default
- `-r`: crash recovery
- `-rcfile file`: alternate startup file name; the default is `.magicrc`
- `-D`: debug
- Tcl builds only: `-w…` (no wrapper window) and `-n…`, whose fourth character selects `noconsole` / `nowindow` / `norcfile`

Any other non-option argument SHALL be treated as a file to load. The suffix `.mag` SHALL be stripped from such a name. Files ending in `.lef`/`.def`, `.cif`, `.gds*` and `.tcl` SHALL be loaded by their format.

#### Scenario: Unknown option
- **WHEN** an unrecognized `-x` option is given
- **THEN** the system SHALL print "Unknown option" and a usage line
- **AND** argument processing SHALL return failure, which ends startup with exit status 1 (src: utils/main.c:370-379)

#### Scenario: Ambiguous -n option
- **WHEN** an `-n` option shorter than 4 characters is given in a Tcl build
- **THEN** the system SHALL report "Ambiguous option" (src: utils/main.c:347-368)

#### Scenario: Tcl script argument ends option parsing
- **WHEN** a `.tcl` file name appears on the command line
- **THEN** argument parsing SHALL stop, and the remaining arguments SHALL be left to the script (src: utils/main.c:382-448)

### Requirement: Tcl launcher-level options
The Tcl `magic.tcl` script SHALL handle the following options itself (src: tcltk/magic.tcl.in:158-237):
- `--version`: print `MAJ.MIN.REV` and exit 0
- `--commit`: print the git SHA and exit 0
- `--prefix`: print `$CAD_ROOT` and exit 0
- `-nowrapper`: disable the wrapper GUI
- `-dnull`: imply no wrapper
- `-r` / `-recover`: run `crash recover` after startup

#### Scenario: Version query
- **WHEN** `magic --version` is run on an installed Tcl build
- **THEN** the system SHALL print the version (for example `8.3.694`) and exit with status 0, without loading a technology

### Requirement: CAD_ROOT and search paths
The system SHALL expand `$CAD_ROOT` in the following order (src: utils/path.c:297-327, tcltk/tclmagic.c:1663-1669):
1. In the Tcl build, the Tcl variable `CAD_ROOT`
2. Otherwise, the environment variable
3. Otherwise, the compile-time `CAD_DIR` (`${LIBDIR}`)

The expanded value SHALL be cached on first use. The system search path SHALL be `. $CAD_ROOT/magic/sys $CAD_ROOT/magic/sys/current` (src: utils/paths.h:40-44).

#### Scenario: Relocated install
- **GIVEN** the environment variable `CAD_ROOT=/opt/eda/lib`
- **WHEN** magic starts
- **THEN** technology files, display styles and the system `.magicrc` SHALL be looked up under `/opt/eda/lib/magic/sys`

### Requirement: Technology file lookup
`TechLoad` SHALL search `.` and then the system path for `<name>.tech`. It SHALL honor an explicit non-`.tech` extension first, and SHALL fall back to `<name>.tech27`. (src: utils/tech.c:425-540)

#### Scenario: Load by name
- **WHEN** `tech load sky130A` is executed and `sky130A.tech` exists in `$CAD_ROOT/magic/sys`
- **THEN** that file SHALL be loaded

### Requirement: Startup file sequence
`mainInitFinal` SHALL process startup files in this order (src: utils/main.c:834-1240):
1. Tcl builds only: `$CAD_ROOT/magic/sys/.initrc`, which sources `site.pre` if present
2. The `-T` technology, if one was given
3. The system `$CAD_ROOT/magic/sys/.magicrc`, which ends by sourcing `site.def`
4. Unless `-norcfile` is given: `~/<rcname>`, then `./<rcname>`. The legacy fallbacks are `.magic` and `magic_setup`.

In Tcl builds the local file SHALL be skipped when the current directory is the home directory, so the same file is not read twice.

#### Scenario: Tech override protects -T choice
- **GIVEN** magic was started with `-T mytech`
- **WHEN** a startup file runs `tech load othertech` without `-override`
- **THEN** the system SHALL silently ignore that load while startup files are processed (src: commands/CmdTZ.c:548-579)

#### Scenario: Bad local startup file
- **WHEN** the local startup file raises a Tcl error
- **THEN** the system SHALL print "Bad local startup file…, continuing without." and SHALL continue startup

### Requirement: Default technology selection
If no technology was loaded by `-T` or by a startup file, the system SHALL use the `tech` line of the first file given on the command line (`DBGetTech`). If that does not determine a technology, the system SHALL load the built-in `minimum` technology. No code path selects `scmos` as the default. (src: utils/main.c:1138-1166)

#### Scenario: No technology anywhere
- **WHEN** magic starts with no `-T`, no rc file that loads a tech, and no file arguments
- **THEN** the technology SHALL be `minimum`

#### Scenario: Tech inferred from a layout file
- **WHEN** `magic inv.mag` is run and `inv.mag` contains `tech sky130A`
- **THEN** the system SHALL load the `sky130A` technology before it reads the cell

### Requirement: Display type selection
If no `-d` option is given, the system SHALL choose `XWIND` when the `DISPLAY` environment variable is set and `NULL` otherwise. The `-d` value SHALL be matched case-insensitively by prefix against these names (src: graphics/grMain.c:96-114,381-426):
- `XWIND`, `X11`, `8BIT`, `16BIT`, `24BIT` (X11)
- `OPEN_GL`, `OGL`, `OPENGL` (OpenGL)
- `CAIRO`, `XR` (Cairo)
- `NULL`

#### Scenario: Unknown display
- **WHEN** `-d FOO` is given
- **THEN** the system SHALL list the known display types, suggest "Use '-d NULL'", and fail startup with exit 1 (src: graphics/grMain.c:269-305)

### Requirement: Batch operation
`magic -dnull -noconsole` SHALL run without graphics and read commands from standard input (for example, `magic -noc -d NULL < script.tcl`). In `magicdnull`, `batch_mode` SHALL be set and interrupt handling SHALL be disabled. (src: tcltk/magicdnull.c:22-61, utils/main.c:612-621, tcltk/magicexec.c:22-25)

#### Scenario: Scripted run
- **WHEN** `echo "version ; quit" | magic -d null -noconsole -T scmos` is run
- **THEN** the version line SHALL be printed and magic SHALL exit

### Requirement: Exit codes
The system SHALL exit with:
- 1 on a bad argument, a graphics init failure, a Tcl `magic::initialize` failure, or end of input in non-Tcl mode
- 2 when the `minimum` technology fails during init
- the status given to `quit [status] [-noprompt]`

Window clients MAY veto `quit` unless `-noprompt` is given. (src: utils/main.c:1292,626,724,1166; windows/windCmdNR.c:261-309; textio/txCommands.c:1694-1697)

#### Scenario: Quit with status
- **WHEN** `quit 3 -noprompt` is executed
- **THEN** magic SHALL exit with status 3 without asking to save modified cells
