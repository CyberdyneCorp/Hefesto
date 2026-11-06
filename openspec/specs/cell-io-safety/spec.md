# Cell I/O Safety

## Purpose

Defines the safety rules around reading and writing cell files:

- detecting read-only cells, and advisory locking between concurrent editor sessions;
- refusing writes that would damage protected files;
- atomic replacement through a temporary file, and post-write size verification;
- interrupt protection and I/O error handling;
- crash-backup files and crash recovery;
- multi-cell archive files.

For each item, the intent is specified as behavior Hefesto must match. Magic's host mechanisms (`fcntl` locks, `mkstemp`, `/tmp`, process IDs, signals) are recorded as informative, and the platform decisions for iPadOS are logged in `docs/decisions/open-questions/CELL.md`.

The `.mag` body grammar is in `cell-file-format`, file lookup in `cell-library-resolution`, and timestamps in `cell-timestamps`. The commands `save`, `writeall`, `crash` and `archive` are user commands in `cmd-hierarchy`. This spec owns their file-level effects.

## Requirements

### Requirement: Read-only cell detection

When a cell file is opened for reading, Hefesto SHALL mark the cell read-only (not editable) if any of the following holds.
Compatibility: behavioral

1. The opened path is not a regular file. The check does not follow a symbolic link in the final path component, so a symbolically linked cell file is read-only (see Q-CELL-25).
2. The process lacks write access to the file.
3. The process's effective user owns the file and the owner-write permission bit is clear.
4. The process's effective user does not own the file and neither the group-write nor the other-write bit is set.
5. The file is locked by another session (see "Advisory locking between sessions").

**Message.** When the reason is not a lock, Magic prints `Warning: cell <cell> from file <file> is not writeable`.

**Clearing the flag.** A cell that opens writable has its read-only mark cleared.

**Effects of read-only.** A read-only cell cannot become the edit cell. The edit command reports `File <file> is not writeable.  Edit not set.`. Read-only cells also never receive new timestamps (see `cell-timestamps`).

(Magic ref: database/DBio.c:138-170, database/DBio.c:1607-1625, commands/CmdE.c:160-167)

#### Scenario: File without write permission
- **GIVEN** `cell.mag` is owned by the current user with mode 0444
- **WHEN** cell `cell` is loaded
- **THEN** the cell SHALL be read-only and the "not writeable" warning SHALL be printed

#### Scenario: Group-writable file of another user
- **GIVEN** `cell.mag` is owned by another user with mode 0664, and the current user is in the file's group
- **WHEN** cell `cell` is loaded
- **THEN** the cell SHALL be editable

### Requirement: Advisory locking between sessions

When file locking is enabled, which is the default, Hefesto SHALL hold an exclusive advisory lock on the file of every editable cell it has loaded. Any cell whose file is locked by another session SHALL be opened read-only.
Compatibility: behavioral

**When the lock is held.** The lock is taken when the cell is read. It is held until one of the following happens:
- the cell is re-read;
- the cell is saved (the lock moves to the newly written file);
- the cell definition is deleted.

**Read-only result.** Opening a cell whose file the session cannot open for update gives the same read-only result as a lock held by another session.

**Locking setting.** Locking can be disabled and re-enabled at run time through the `locking` setting, which accepts `disable`, `no`, `false`, `off`, `0`, `enable`, `yes`, `true`, `on` or `1`, with unique-prefix matching. When locking is disabled, no locks are taken. Read-only detection still applies.

**Same-session reads.** Within one session, the same file is never reported as locked against itself.

(Magic ref: utils/flock.c:98-215, utils/path.c:58, commands/CmdLQ.c:580-640, database/DBio.c:1404-1411)

#### Scenario: Two sessions on one file
- **GIVEN** session A has loaded `cell.mag` editable
- **WHEN** session B loads `cell.mag`
- **THEN** B SHALL open the cell read-only and SHALL report that the file is locked
- **AND** B SHALL refuse to make the cell the edit cell

### Requirement: Locking mechanism

Hefesto SHALL treat this description of Magic's locking mechanism as reference material.
Compatibility: informative

**Taking the lock.**
- Magic opens the file read-write and queries for a conflicting whole-file `fcntl` write lock (`F_GETLK`).
- If there is none, it re-opens the file and sets the lock (`F_SETLK`). The descriptor stays open for as long as the lock is held.
- A failure to set the lock is reported with `perror` and is otherwise ignored.

**Conflict message.** When a conflicting lock exists, Magic prints `File <file> is already locked by pid <pid>.  Opening read-only.`. If the lock owner's process ID is unknown, the message is `File <file> is already locked by another process.  Opening read-only.`.

**Consequences of POSIX lock semantics.**
- Locks are per process, so the same process never conflicts with itself.
- Closing any descriptor of the file releases the lock.
- One descriptor stays open per loaded editable cell, which can exhaust the descriptor limit in large designs. This is why the `locking` command exists.

A build configured with `--disable-locking` has no locking at all.

See Q-CELL-26 [platform] for iPadOS and other sandboxed hosts.

(Magic ref: utils/flock.c:1-313, scripts/configure.in:1106-1118)

#### Scenario: Lock owner reported
- **GIVEN** process 4242 holds the lock on `cell.mag`
- **WHEN** another Magic process loads the cell
- **THEN** it SHALL print `File <cell.mag> is already locked by pid 4242.  Opening read-only.`

### Requirement: Write refusal for protected cells

Hefesto SHALL refuse to save a cell whose file is protected.
Compatibility: behavioral

**Refusal conditions.** A save is refused when any of the following holds:
- the cell is read-only (message `File <file>.mag is read_only and cannot be written`);
- the cell's file is locked by another session (message `File <file>.mag is locked by another user and cannot be written`);
- the target file exists but fails the writability test of "Read-only cell detection" (an OS error is reported).

**Effect of a refusal.**
- The save fails.
- Nothing is written.
- The cell's flags are unchanged.

**Write-all behavior.** When writing all modified cells without prompting, a read-only cell is skipped. If it was modified, Magic reports `Cell '<cell>' is not writeable;  modified cell not written.`

**Exception for the `save` command.** When there is no edit cell, `save` first clears the read-only mark of the window's root cell. A read-only root cell that is not locked can then be written. With an explicit new name, this works as "save as" (see Q-CELL-37).

(Magic ref: database/DBio.c:5695-5710, database/DBio.c:5739-5747, commands/CmdTZ.c:2290-2302, commands/CmdRS.c:207-218)

#### Scenario: Save of a read-only cell
- **GIVEN** cell `a` was opened read-only
- **WHEN** a save of `a` is requested
- **THEN** no file SHALL be written and the save SHALL report failure

### Requirement: Atomic save through a temporary file

When saving cell C to file name N, Hefesto SHALL write the new content without ever leaving a truncated or partially written N.
Compatibility: behavioral

**Choosing the file name.**
- If N does not end in `.mag`, the suffix is appended.
- A leading `~` or `$VAR` is expanded.
- N becomes C's recorded file path before the write.

**Target already exists.** The steps are:
1. Create a new uniquely named temporary file in the same directory, named N + `.` + 7 random characters.
2. Give it the existing file's permission bits.
3. Write the complete cell to the temporary file.
4. On success, release C's lock and rename the temporary file over N. On a write failure, delete the temporary file and fail, leaving N untouched.

**Target does not exist.** The cell is written directly to N.

**Failed rename.** If the rename fails, the save fails. Magic prints that the old copy is in N, that the new copy is in the temporary file, and that the user must copy it to a safe place.

**After success.**
- C's recorded file path becomes N without its extension.
- The written file is re-opened and re-locked, as in "Advisory locking between sessions".

(Magic ref: database/DBio.c:5609-5798, database/DBio.c:5860-5867)

#### Scenario: Interrupted write keeps the old file
- **GIVEN** `cell.mag` exists and writing the new content fails because the disk is full
- **WHEN** the save is attempted
- **THEN** `cell.mag` SHALL still hold its previous content and no temporary file SHALL remain

#### Scenario: Permissions preserved
- **GIVEN** `cell.mag` has mode 0640
- **WHEN** it is saved successfully
- **THEN** the new `cell.mag` SHALL have mode 0640

### Requirement: Fallback write without a temporary file

Hefesto SHALL handle the case where the target exists but no temporary file can be created in its directory. Magic attempts an in-place fallback, described here for reference.
Compatibility: informative

1. Open N for appending and append the whole cell, which reserves space. If this fails, truncate back to the original size and fail.
2. If the original size was non-zero, rewind and write the cell again, then truncate the file to the current position.

Because the file is open in append mode, the second write also goes to the end. The resulting file holds the old content followed by two copies of the new content, and the size check then fails. Hefesto should not replicate this (see Q-CELL-27).

(Magic ref: database/DBio.c:5799-5858)

#### Scenario: Directory not writable (verify against Magic)
- **GIVEN** the directory of an existing, writable `cell.mag` does not permit creating files
- **WHEN** the cell is saved by Magic
- **THEN** Magic SHALL report an I/O error from the size check and SHALL keep the cell marked modified

### Requirement: Post-write verification

After a successful write and rename, Hefesto SHALL re-open the written file and compare its size with the number of bytes it emitted for the cell.
Compatibility: behavioral

**Re-open fails.** The cell stays modified and Magic reports `Warning: Cannot open file "<file>" for writing!`.

**Sizes differ.** The cell stays modified and Magic reports `Warning: I/O error in writing file "<file>"`.

**Successful write.** The cell's modified, bounding-box-changed and child-timestamp-changed flags are cleared.

(Magic ref: database/DBio.c:5869-5910, database/DBio.c:4946-4957)

#### Scenario: Successful save clears flags
- **GIVEN** cell `a` is modified and has a changed child timestamp
- **WHEN** `a` is saved successfully
- **THEN** `a` SHALL be neither modified nor flagged for child timestamp changes

### Requirement: I/O error handling during write

Hefesto SHALL treat any failed output operation, flush failure or stream error while writing a cell as a write failure.
Compatibility: behavioral

- Magic prints `Warning: I/O error in writing file`.
- The function reports failure.
- The cell's flags are left unchanged.
- The save command then reports `Could not write file.  Cell not written.`.

User interrupts are deferred while a cell is being written, so a write is never abandoned halfway because of an interrupt.

(Magic ref: database/DBio.c:4699-4719, database/DBio.c:4948-4954, commands/CmdSubrs.c:754-759)

#### Scenario: Output failure
- **GIVEN** the output stream reports an error after the paint sections are written
- **WHEN** the cell is being written
- **THEN** the write SHALL fail and the cell SHALL remain modified

### Requirement: Crash backup file format

A crash backup or archive file SHALL be a sequence of cell records followed by an end record, as given by this grammar.
Compatibility: exact

```ebnf
backup_file = { file_record } , end_record ;
file_record = "file" , SP , location , NL , mag_body ;
end_record  = "end" , [ SP , root_cell ] , NL ;
```

**`location`.** The cell's recorded file path, or its name if it has none.

**`mag_body`.** A complete `.mag` cell exactly as written by the `.mag` writer of `cell-file-format`, from `magic` through `<< end >>`.

**`root_cell`.** The name of the root cell of the first layout window, if one exists.

**Cell order.** Magic writes the cell records in its cell hash-table enumeration order, which is arbitrary (see Q-CELL-28). Hefesto SHALL write them in a deterministic order, sorted by cell name with the natural comparator of capability `cell-file-format`; the reader is order-independent, and golden comparison SHALL canonicalize record order (decision X-1, `docs/decisions/cross-cutting.md#X-1`).

**Side effects of writing a record.** Writing a cell into a backup does not change any of the cell's flags. A pending new timestamp is suppressed for this write only, so no out-of-date warning is printed.

(Magic ref: database/DBio.c:6382-6533)

#### Scenario: Backup of one modified cell
- **GIVEN** the only modified editable cell is `top`, recorded at `/p/top`, and window 0 shows `top`
- **WHEN** a crash backup is written
- **THEN** the file SHALL be `file /p/top`, then the full `.mag` text of `top`, then `end top`

### Requirement: Crash backup policy

Hefesto SHALL maintain a per-session crash backup.
Compatibility: behavioral

**Cells included.** Only cells that meet all of the following are written:
- modified;
- available;
- editable;
- not internal;
- not flagged not-found.

**When there is nothing to write.** If no cell qualifies, nothing is written and no file is created.

**Session backup file.**
- The first backup in a session creates a uniquely named session backup file.
- Later backups rewrite the same file.

**Explicit backup target.** A backup requested with an explicit file name makes that file the session backup target. An empty name clears the target.

**Removal on normal exit.** The session backup file is removed when the session ends normally.

**Schedule.**
- Periodic backups run at a configurable interval. An interval of 0 or less stops them. Starting backups with no interval set uses 10 minutes, but the GUI tool setup run by the system startup file sets 1 minute before starting them, so 1 minute is the effective default in an interactive session (see Q-SCRIPT-18).
- Backups can be started, stopped, resumed and disabled. The `crashbackups` command that does this is specified in capability `ui-gui-managers`, requirement "Crash backup timer".
- A termination request writes a final backup before exiting.

(Magic ref: database/DBio.c:6399-6440, database/DBio.c:6542-6550, tcltk/tools.tcl:53-118, utils/signals.c:503-507, utils/main.c:201)

#### Scenario: Nothing modified
- **GIVEN** no cell is modified
- **WHEN** the periodic backup fires
- **THEN** no backup file SHALL be created or rewritten

### Requirement: Crash backup host mechanism

Hefesto SHALL treat this description of Magic's crash backup mechanism as reference material.
Compatibility: informative

**File location.** The session backup file is created atomically as `$TMPDIR/MAG<pid>.XXXXXX`, or `/tmp/MAG<pid>.XXXXXX` when TMPDIR is unset. Magic prints `Created database crash recovery file <file>`.

**Rewriting.** The file is rewritten in place, not atomically.

**Schedule.** Periodic backups are a Tcl `after` timer (`Opts(backupinterval)`, 600000 ms when unset at start; the tool setup sets 60000 ms) that runs `crash save`. SIGTERM triggers a backup and exit.

**Explicit name.** When an explicit name is given, the confirmation message prints the previous name rather than the new one (see Q-CELL-29).

See Q-CELL-30 [platform].

(Magic ref: database/DBio.c:6411-6456, tcltk/tools.tcl:57-118)

#### Scenario: Backup file name
- **GIVEN** TMPDIR is unset and the Magic process ID is 1234
- **WHEN** the first backup of the session is written
- **THEN** the file SHALL be created in `/tmp` with a name matching `MAG1234.XXXXXX`, where `XXXXXX` is replaced by six random characters

### Requirement: Crash recovery

Hefesto SHALL offer recovery of the most recent crash backup that belongs to the current user and was left by a session that is no longer running.
Compatibility: behavioral

**Preconditions.** Recovery is refused, with `Error:  Backup file in use for current session.`, if the current session already has a backup file.

**Choosing the file.**
- Without an explicit file, Hefesto scans the backup directory for candidates and selects the one with the newest status-change time among those that meet all of the following:
  - its name starts with `MAG`;
  - it is owned by the current user;
  - the process ID encoded between `MAG` and the first `.` does not belong to a running process.
- With an explicit file, that file is used.

**Confirmation.** The user is asked `Recover from backup file <file>?` with the choices `read` and `cancel`.

**Reading.** On `read`, the file is read as in "Backup and archive reading", with every cell dereferenced.

**After a successful read.** The backup file is deleted, and the session has no backup file afterwards.

(Magic ref: database/DBio.c:1113-1215, utils/main.c:1175-1182, commands/CmdCD.c:3755-3780)

#### Scenario: Recover after crash
- **GIVEN** a crashed session with process ID 1234 left `/tmp/MAG1234.abcdef`, owned by the user, and process 1234 is not running
- **WHEN** a new session requests recovery and the user answers `read`
- **THEN** the cells in the file SHALL be loaded and `/tmp/MAG1234.abcdef` SHALL be deleted

#### Scenario: Backup of a live session is skipped
- **GIVEN** `/tmp/MAG999.xyz123` belongs to the running process 999
- **WHEN** recovery scans for backups
- **THEN** that file SHALL NOT be offered

### Requirement: Backup and archive reading

When reading a backup or archive file, Hefesto SHALL process its records as follows.
Compatibility: exact

**Opening the file.**
- An archive file name is looked up with `.mag` appended, through Path.
- A crash backup file is opened as given.
- If the file cannot be opened, Magic prints `Cannot open <archive|backup> file "<name>"` and fails.
- An empty file gives `Bad <type> file <name>; can't read!` and fails.

**Record loop.** Repeat until a line starting with `end`:

1. The line MUST start with `file`. Otherwise Magic prints `Error in <type> file <name>; expected keyword "file", got "<line>"!` and fails.
2. Derive the cell name from the location:
   - remove the trailing newline;
   - cut at the first occurrence of `.mag` anywhere in the text (see Q-CELL-31);
   - take the part after the last `/`;
   - skip leading whitespace.
   An empty name fails.
3. Find or create the cell definition with that name, clear its not-found flag and mark it available.
4. Read the `.mag` body with technology mismatches ignored, and dereferenced as requested. A body read failure aborts the whole archive read, keeping the cells read so far.
5. Read the next line. End of file here gives `Error in <type> file <name>; partial restore only!` and fails.
6. Flag the cell's parents for timestamp changes and recompute its bounding box.

**Location is not restored.** The location in the `file` line is used only to derive the cell name. A cell's recorded path is not set from it (see Q-CELL-32).

**End record.** If text follows `end ` on the end record, that cell is loaded into every layout window.

**Modified flag.** Cells read from a backup are not marked modified (see Q-CELL-33).

(Magic ref: database/DBio.c:1235-1333)

#### Scenario: Two-cell archive
- **GIVEN** an archive with `file /p/top`, the body of `top` (which uses `inv`), `file /p/inv`, the body of `inv`, and `end top`
- **WHEN** it is read
- **THEN** cells `top` and `inv` SHALL be in memory with the archived content, and `top` SHALL be shown in the windows

### Requirement: Archive files

Hefesto SHALL support writing the whole database to a single archive file and reading it back, with four modes.
Compatibility: behavioral

**`write`** writes every available cell, modified or not, except:
- internal cells;
- not-found cells;
- read-only cells.

**`writeall`** also includes read-only (for example PDK) cells.

**`read`** loads the archive with every cell dereferenced.

**`readref`** loads it without dereferencing, so recorded paths are honored.

**Archive file name.**
- Writing uses the given file name exactly. Magic prints `Created database archive file <file>`.
- Reading appends `.mag` to the given name (see Q-CELL-34).

**Interaction with crash backups.** Writing an archive does not change the session's crash backup target, and the archive file is not removed at exit.

(Magic ref: commands/CmdAB.c:84-147, database/DBio.c:6399-6496)

#### Scenario: Archive includes unmodified cells
- **GIVEN** cells `top` (modified) and `inv` (unmodified, editable) are loaded
- **WHEN** `archive write lib.mag` is executed
- **THEN** `lib.mag` SHALL contain records for both `top` and `inv`

#### Scenario: Read-only cells only with writeall
- **GIVEN** a read-only PDK cell `pdkcell` is loaded
- **WHEN** an archive is written in `write` mode and another in `writeall` mode
- **THEN** only the `writeall` archive SHALL contain a record for `pdkcell`
