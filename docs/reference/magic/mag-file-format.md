# .mag File Format and Cell I/O

## Purpose
Specifies Magic's native text cell format (`.mag`, optionally gzip `.mag.gz`): how cell files are located (search paths, instance paths), file locking and read-only handling, atomic writes, crash-recovery backups, and automatic technology discovery from PDK paths.

## Requirements

### Requirement: File header
A .mag file SHALL begin with these lines, in order:
1. `magic`
2. `tech <name>`
3. an optional `magscale <n> <d>`
4. `timestamp <int>`

(src: database/DBio.c:523-784, 4716-4735)

#### Scenario: Bad first line
- **WHEN** a file whose first line is not `magic` is read
- **THEN** the system SHALL report "First line in file must be \"magic\"; instead saw: %s" and SHALL fail the read

#### Scenario: Technology mismatch
- **GIVEN** the current technology is `scmos`
- **WHEN** a cell with `tech sky130A` is read
- **AND** sky130A cannot be found and loaded automatically
- **THEN** the system SHALL report that the cell has a different technology, suggest `tech load` or `load -force`, and SHALL fail the read (src: database/DBio.c:741-750)

### Requirement: Automatic technology discovery
If a cell's technology differs from the current one and no loaded cell holds paint, the system SHALL search for `<tech>.tech` up to 10 directory levels deep under:
- `PDK_PATH`, `PDKPATH`, `PDK_ROOT`, `PDKROOT` (environment variables, then Tcl variables)
- `/usr/local/share/pdk`

If the file is found, the system SHALL source `<tech>.magicrc` (Tcl build), print "Loading technology %s", and load the technology. (src: database/DBio.c:224-320,597-725)

#### Scenario: Open a PDK cell in a fresh session
- **GIVEN** `PDK_ROOT=/usr/share/pdk` and `sky130A.tech` under it
- **WHEN** a sky130A cell is loaded into an empty session
- **THEN** the sky130A technology SHALL be loaded automatically

### Requirement: Body sections
After the header, the file SHALL contain:
- `<< layer >>` sections with `rect xbot ybot xtop ytop` and `tri xbot ybot xtop ytop <dir>` lines
- `use` blocks (see the next requirement)
- `<< labels >>` with `rlabel`, `flabel` and `port` lines
- an optional `<< elements >>` section
- `<< properties >>` with `string|integer|dimension|double <name> <value>` lines
- `<< end >>`

Stacked contact types SHALL be written as their residues. Lines starting with `#` SHALL be comments. (src: database/DBio.c:852-1062,4752-4906)

#### Scenario: Round trip
- **GIVEN** a cell containing paint, subcells, labels, ports and properties
- **WHEN** it is saved and read back
- **THEN** the geometry, instances, label text and attributes (except the port uses listed as open questions), and properties SHALL be preserved

#### Scenario: Format error
- **WHEN** any line fails to parse
- **THEN** the system SHALL report "File %s contained format error" and queue a full DRC recheck of the cell

### Requirement: Use records and instance path resolution
A `use` block SHALL consist of:
1. `use <cell> [<id>] [<path>]`, where an id prefixed `*` means the instance is locked
2. an optional `array` line
3. `timestamp`
4. `transform a b c d e f` (Manhattan only)
5. `box`

The path SHALL be written only on the first instance of each child. When writing, the system SHALL:
- write the path relative to the parent directory when possible
- substitute `$PDK_ROOT`-style variables and `~` for home
- sort instances by id

When reading, `$VAR` SHALL be expanded from Tcl variables. A relative path SHALL be resolved against the parent file's directory. (src: database/DBio.c:2386-3171,6060-6287)

#### Scenario: Conflicting cell paths
- **GIVEN** cell `inv` was already read from `/a/inv.mag`
- **WHEN** another parent references `inv` at path `/b`
- **AND** the two files are not the same (not the same inode, and not matching git commits)
- **THEN** the system SHALL report "Duplicate cell in …", rename the original to `inv__<i>`, and read the new one

#### Scenario: Circular hierarchy
- **WHEN** a cell (directly or indirectly) uses itself
- **THEN** the system SHALL report "Subcells are used circularly!" and SHALL skip that use

### Requirement: Locating cell files
To read a cell, the system SHALL try in order:
1. `cd_file.mag`
2. `cd_file.mag.gz` (with zlib)
3. the `Path` and `CellLibPath` search paths by cell name

A dereferenced cell (`CDDEREFERENCE`) SHALL try the search paths first. A missing cell SHALL be reported once: the system SHALL report "Cell %s couldn't be read" and set `CDNOTFOUND`. (src: database/DBio.c:1448-1662)

#### Scenario: Missing child
- **WHEN** a parent references a child file that does not exist
- **THEN** the error SHALL be printed once, and the child SHALL appear as an unloaded placeholder

### Requirement: File locking and read-only cells
Unless locking is disabled at configure time, opening a cell for editing SHALL take an `fcntl` write lock. A cell SHALL be opened read-only (`CDNOEDIT`) if its file is locked by another process or is not writable. In that case the system SHALL report "File <%s> is already locked by pid %d.  Opening read-only.", and a read-only cell SHALL NOT be made the edit cell. (src: utils/flock.c, database/DBio.c:1607-1633, commands/CmdE.c:160-167)

#### Scenario: Two sessions on one file
- **GIVEN** session A has `cell.mag` open
- **WHEN** session B loads `cell.mag`
- **THEN** B SHALL open it read-only and refuse to `edit` it

### Requirement: Safe writes
`save` / `writeall` SHALL write to a temporary file `<file>.XXXXXXX` that copies the original's permissions, and SHALL then `rename()` it over the original. After writing, the system SHALL verify the on-disk size. On mismatch it SHALL keep the cell modified and warn "I/O error in writing file".

Additional rules:
- A read-only or foreign-locked cell SHALL be refused ("File %s is read_only and cannot be written").
- A target ending in `.tcl` SHALL be written as a magic command script.

(src: database/DBio.c:4678-4958,5610-5917)

#### Scenario: Successful save
- **WHEN** `save` succeeds
- **THEN** `CDMODIFIED`, `CDBOXESCHANGED` and `CDSTAMPSCHANGED` SHALL be cleared

### Requirement: Crash recovery
The system SHALL periodically write modified cells to a backup file `$TMPDIR/MAG<pid>.XXXXXX`. On `magic -r` or `crash recover`, it SHALL offer the newest backup owned by the user whose process is no longer running ("Recover from backup file %s?"). After a successful recovery it SHALL remove the backup file. (src: database/DBio.c:1085-1333,6382-6549)

#### Scenario: Recover after crash
- **GIVEN** a magic process crashed and left `/tmp/MAG1234.abcdef`
- **WHEN** a new session runs `crash recover` and confirms
- **THEN** the modified cells SHALL be restored and the backup file deleted
