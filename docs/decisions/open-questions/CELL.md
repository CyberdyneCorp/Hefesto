# Open Questions: CELL area

Capabilities: `cell-file-format`, `cell-library-resolution`, `cell-io-safety`, `cell-timestamps`.
Magic reference: `fc5daa9e` (8.3.694).

### Q-CELL-1: Blank lines in .mag files are format errors
- Kind: quirk
- Magic behavior: The line reader keeps the LF, so an empty line is never treated as "blank". In a paint section, the label/element/property sections, or the body loop, an empty line falls through to the `use` parser. That fails with `Expected "use" line`, and the rest of the file is abandoned (partial read).
- Evidence: database/DBio.c:4187-4214, database/DBio.c:998-999, database/DBio.c:3889-3896
- Options: replicate | fix (skip empty lines on read; never write them)
- Recommendation: Accept empty lines on read (strictly more tolerant; Magic never writes them). Keep the writer identical.
- Resolution: fix — empty lines are skipped on read and never written; Magic never writes them, so only hand-edited files differ, where Magic abandons the rest of the file (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-2: Silent truncation when grid refinement is refused
- Kind: quirk
- Magic behavior: If a file is finer than the database and the output-style grid limit forbids refinement, coordinates are divided with truncation toward zero. The "Geometry may be lost" warning is printed only when n > 1, so for `magscale 1 2` files there is no warning.
- Evidence: database/DBio.c:794-833
- Options: replicate | fix (always warn when d > 1 remains)
- Recommendation: Replicate the geometry. Always warn.
- Resolution: fix — the geometry is truncated exactly as Magic does; the "Geometry may be lost" warning is printed whenever a divisor greater than 1 remains (messages are informative) (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-3: Rectangle scanner tolerance
- Kind: quirk
- Magic behavior: The scanner skips exactly 4 (`rect`) or 3 (`tri`) characters after the first letter, so `rect  1 2 3 4` reads as (0,1,2,3). Between numbers, the whitespace skip crosses LF, so a short line consumes the next line. Only the lowercase `s`/`e` letters after the fourth number decide the triangle corner. There is no overflow check.
- Evidence: utils/getrect.c:57-145
- Options: replicate | fix (token-based parse, error on short lines)
- Recommendation: Replicate for files Magic itself writes (identical results). For malformed input, prefer a format error and log a warning. Decide after the golden cases CELL-RD-07/08.
- Resolution: compat — Magic's fixed-skip scanner, which reads files Magic writes identically; with `magic-compat` OFF, records are parsed as whitespace-separated tokens on one line and a short or malformed record is a format error (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-4: Which duplicate instance id is renamed
- Kind: ambiguity
- Magic behavior: Duplicate ids are detected while enumerating the subcell plane, not in file order. The instance visited second loses its id and gets `<child>_<k>`.
- Evidence: database/DBcellname.c:2605-2700
- Options: replicate (needs Magic's bin-plane order) | fix (file order: the later block is renamed)
- Recommendation: Use file order and document it. Duplicates never occur in Magic-written files.
- Resolution: fix — the later instance in file order loses its id; reproducing Magic's subcell-plane visiting order buys nothing, because Magic-written files never hold duplicate ids (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-5: Label text truncated to 99 characters on read
- Kind: bug
- Magic behavior: The writer emits the full label text, but the reader uses `%99[^\n]`, so longer texts lose characters on every round trip.
- Evidence: database/DBio.c:3911-3962, database/DBio.c:4088
- Options: replicate | fix (read the full remainder of the line)
- Recommendation: Fix on read (keep the full text). The writer is unchanged. Flag the difference in the compatibility notes.
- Resolution: fix — data loss (named in X-2): the full label text is kept on read; the writer is unchanged (X-2 tier 1, 2026-11-07).

### Q-CELL-6: Port records that cannot be read back
- Kind: bug
- Magic behavior: The reader accepts only 2, 4 or 5 tokens. The writer emits nothing for the port uses `reset`, `scan` and `tieoff`. A port with one of these uses is therefore written as `port N dirs <class>` (3 tokens), which the reader rejects, and the whole port is lost. The error text for an unknown class names the use token.
- Evidence: database/DBio.c:3986-3990, database/DBio.c:4846-4866, database/DBio.c:4059-4061
- Options: replicate | fix (write `reset`/`scan`/`tieoff`; accept 3 tokens and first letters `r`/`t` for use)
- Recommendation: The writer must stay byte-identical for representable ports. For reset/scan/tieoff, write the use names (Magic writes a lossy line anyway). Accept them on read.
- Resolution: fix — data loss (named in X-2): port uses `reset`, `scan` and `tieoff` are written by name and read back by full word; a 3-token record (Magic's lossy output) is accepted with its third token read as the class; ports with other uses are written byte-identically (X-2 tier 1, 2026-11-07).

### Q-CELL-7: Property types and empty values do not round-trip
- Kind: quirk
- Magic behavior: In the default compatibility mode, every property is written as `string`. Non-reserved integer, double and dimension properties therefore come back as strings. Properties with an empty value (an empty string or an empty mask-hint plane) are written as `type KEY` followed by nothing and are skipped on read.
- Evidence: database/DBio.c:5039-5042, database/DBio.c:3235-3249, database/DBprop.c:37
- Options: replicate | fix
- Recommendation: Replicate the output exactly (it is what PDKs see). Document the type loss. Optionally preserve the type in Hefesto's in-memory model when the value parses.
- Resolution: compat — every property is written as `string` and empty values are skipped on read, as in Magic (it is what PDKs see); with `magic-compat` OFF, properties are written with their own type and an empty value is written and read back as empty (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-8: Malformed numeric property values corrupt memory
- Kind: bug
- Magic behavior: When a token of an `integer`, `double` or `dimension` value fails to parse, the code stores a string property and breaks only the inner counting loop. It then builds a numeric record sized for the tokens counted so far, while the second pass writes every token, which overflows the allocation. The string property is then replaced.
- Evidence: database/DBio.c:3490-3661
- Options: replicate (impossible safely) | fix (store as string and stop)
- Recommendation: Fix: store the raw value as a string property.
- Resolution: fix — heap overflow; the raw value is stored as a string property and parsing stops (X-2 tier 1, 2026-11-07).

### Q-CELL-9: Values not covered by the write-scale reducer
- Kind: bug
- Magic behavior: The reducer is the GCF of technology paint, labels, dimension and plane properties, and instance geometry. It ignores built-in type tiles (DRC check/error tiles, router hints), flabel size and offsets, and element coordinates. These values are still divided by the reducer with truncation.
- Evidence: database/DBio.c:4248-4291, database/DBio.c:4820-4830, database/DBio.c:5995-6003, dbwind/DBWelement.c:216-227
- Options: replicate | fix (fold these values into the GCF)
- Recommendation: Replicate the output for compatibility. Note that DRC tiles may shift by one unit. Revisit if golden cases show user-visible damage.
- Resolution: compat — the values outside the reducer are divided with truncation, as in Magic; with `magic-compat` OFF, they are folded into the reducer's GCF (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-10: Byte-identical paint output depends on tile decomposition
- Kind: ambiguity
- Magic behavior: `rect` records are the tiles of the corner-stitched plane in pre-order traversal. The record set and order therefore depend on the exact maximal-horizontal-strip decomposition, including non-canonical states left by some operations.
- Evidence: database/DBio.c:4771-4773, database/DBtiles.c:521-639
- Options: replicate the plane algorithm | canonicalize before writing (differs from Magic when Magic is non-canonical)
- Recommendation: The layout capabilities must guarantee identical decomposition for identical editing histories. For byte-identity tests, compare after reading into a canonical plane. Track this in the layout area.
- Resolution: compat — tile order: Hefesto reproduces Magic's maximal-horizontal-strip decomposition and traversal, so `rect` records are byte-identical (X-1, 2026-11-07).

### Q-CELL-11: Element order and element name scope
- Kind: quirk
- Magic behavior: Persistent elements are written in hash-table iteration order, which depends on Magic's hash function and table growth. Element names are global across all cells. Coordinates are formatted into a 10-byte buffer, which overflows for values with 10 or more characters.
- Evidence: dbwind/DBWelement.c:186-227, database/DBio.c:3747
- Options: replicate the hash order | fix (sort by name)
- Recommendation: Sort elements by name with the natural comparator. Elements are rare in PDK data. Document that the order differs.
- Resolution: decided by X-1 (2026-11-05). Elements are written sorted by name with the natural comparator, and golden comparison canonicalizes the order (capability `cell-file-format`). The 10-byte buffer overflow is tier-1 (always fix, X-2).

### Q-CELL-12: Natural comparator is not a strict weak order
- Kind: bug
- Magic behavior: A proper prefix compares greater than its extension in both argument orders, and case or leading zeros compare equal. The sorted order then depends on the C library `qsort` (glibc merge sort vs. BSD/macOS introsort), so Magic's own output differs between hosts for ids like `x`/`x2` or `A1`/`a1`.
- Evidence: database/DBcellname.c:676-707
- Options: replicate (requires a host-specific sort) | fix (prefix sorts first; ties broken by byte comparison)
- Recommendation: Fix with a total order that matches Magic whenever Magic's comparator is consistent (all ordinary cases).
- Resolution: fix — decided by D-9 (2026-11-07). Total order: Magic's comparison, a natural prefix sorts first, and remaining ties (case, leading zeros) are broken by unsigned byte comparison (`cell-file-format`, requirement "Natural ordering of names").

### Q-CELL-13: Failed .tcl save falls back to a .mag write
- Kind: bug
- Magic behavior: If `x.tcl` cannot be opened, the code continues as a normal save of `x.tcl.mag` and records `x.tcl` as the cell path. Writing a script also strips `.tcl` from the cell name in place, without updating the name table.
- Evidence: database/DBio.c:5635-5666, database/DBio.c:5155-5158
- Options: replicate | fix (fail the save)
- Recommendation: Fix: report the failure and write nothing.
- Resolution: fix — a failed script save reports the failure and writes nothing, instead of a `.tcl.mag` file (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-14: Command-script writer defects
- Kind: bug
- Magic behavior:
  - A split tile emits `splitpaint` for both sides with the same direction.
  - Instances lose orientation, arrays, lock state and path.
  - Label size is converted from eighths (`>>3`), but offsets are not.
  - A text label with spaces is not quoted.
  - Port uses reset, scan and tieoff produce a line without a value or newline.
  - Dimension properties are printed in the current display units, but the script declares `units internal`.
- Evidence: database/DBio.c:5354-5568
- Options: replicate | fix
- Recommendation: The script is a Tcl artifact, which is informative for Hefesto. If implemented, emit a correct script and do not replicate the defects.
- Resolution: fix — the script loses instance orientation, arrays, lock state and paths (data loss); Hefesto writes a correct script and does not replicate any of the listed defects (X-2 tier 1, 2026-11-07).

### Q-CELL-15: Split tile written twice when both sides match a type
- Kind: bug
- Magic behavior: The paint search reports a split tile once per matching side. The writer ignores which side was reported and always prefers the left side, so when both sides contain the type (directly or as a stacked-contact residue), the left-side `tri` record appears twice.
- Evidence: database/DBio.c:5952-5973, database/DBtiles.c:562-604
- Options: replicate | fix (write each side once)
- Recommendation: Replicate the output. The duplicate is harmless on read and required for byte identity.
- Resolution: compat — the left-side record is written twice, as in Magic (harmless on read, needed for byte identity); with `magic-compat` OFF, each side is written once (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-16: Compressed-file naming and preference
- Kind: bug
- Magic behavior: There are three related problems.
  - A cell read from `x.mag.gz` records the path `x.mag.gz`, so the next save writes an uncompressed `x.mag.gz.mag`.
  - With locking enabled, every open prefers `<candidate>.gz` over the candidate, so `x.mag.gz` shadows `x.mag`.
  - Saves always write plain `x.mag`, so a stale `x.mag.gz` keeps winning on the next load.
- Evidence: database/DBio.c:1634-1641, database/DBio.c:5671-5677, utils/flock.c:117, utils/path.c:117-130
- Options: replicate | fix (strip `.mag.gz` from the recorded path; prefer the newer file; or save compressed when read compressed)
- Recommendation: Fix. Record the base path. When both files exist, prefer `.mag` and warn. Never write `x.mag.gz.mag`.
- Resolution: fix — data loss (named in X-2): the recorded path drops `.mag.gz`, a save never writes `x.mag.gz.mag`, and when both `x.mag` and `x.mag.gz` exist `x.mag` is preferred with a warning (X-2 tier 1, 2026-11-07).

### Q-CELL-17: Variable sources for path expansion and substitution [platform]
- Kind: platform
- Magic behavior: `$VAR` in search paths and `use` paths, and the `$PDK_*` substitution on write, use Tcl global variables in the Tcl build and the process environment otherwise. `CAD_ROOT` falls back to the install directory. `~user` uses the password database. Automatic technology discovery reads environment variables and then Tcl variables.
- Evidence: utils/path.c:270-300, database/DBio.c:2585-2610, database/DBio.c:6059-6088, database/DBio.c:614-672
- Options: platform alternative
- Recommendation: Hefesto defines a single "path variables" table, seeded from the environment (desktop) or app settings (iPadOS), plus PDK configuration. Substitution on write must reproduce Magic's `$PDK_ROOT/...` text whenever the same variable values are configured. On iPadOS, `~` maps to the app container, and `~user` is unsupported (treated as an unknown user).
- Canonical entry for Q-UI-26 and the path-variable part of Q-TECH-28. Platform owner: capability `hefesto-files-and-pdks`.
- Resolution: platform — path variables come from one table in the settings, seeded from the environment and the PDK startup scripts on the desktop and from app settings on iPadOS; `~` is the app container there and `~user` is unknown (X-4, hefesto-files-and-pdks, 2026-11-07).

### Q-CELL-18: Gzip search-path retry uses the recorded path instead of the cell name
- Kind: bug
- Magic behavior: When the recorded path fails, the uncompressed search-path retry uses the cell name, but the `.mag.gz` retry uses the recorded path, which is usually a relative or absolute directory path.
- Evidence: database/DBio.c:1512-1518
- Options: replicate | fix (use the cell name)
- Recommendation: Fix.
- Resolution: fix — the gzip search-path retry uses the cell name, like the uncompressed retry (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-19: Undefined variable in a use path
- Kind: quirk
- Magic behavior: If `$PDK_ROOT` is undefined, the path keeps the literal `$PDK_ROOT/...`. Since it does not start with `/` or `~`, it is joined to the parent directory, which yields a bogus path that then fails and falls back to the search paths.
- Evidence: database/DBio.c:2589-2642
- Options: replicate | fix (report "undefined variable" and fall back directly to the search paths)
- Recommendation: Fix, with the same eventual outcome (search-path fallback) and a clear message.
- Resolution: fix — an undefined variable is reported and the lookup goes straight to the search paths, which is where Magic ends up after its bogus parent-relative path fails (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-20: PDK substitution is a character prefix, not a path prefix
- Kind: bug
- Magic behavior: With `PDK_ROOT=/usr/share/pdk`, the directory `/usr/share/pdk2/x` is written as `$PDK_ROOT2/x`.
- Evidence: database/DBio.c:6077-6087
- Options: replicate | fix (require `/` or the end of the string after the prefix)
- Recommendation: Fix. It only affects pathological layouts.
- Resolution: fix — the PDK prefix must be followed by `/` or the end of the string, so `/usr/share/pdk2` is never written as `$PDK_ROOT2`; only pathological layouts differ (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-21: Home substitution checks the wrong string
- Kind: bug
- Magic behavior: The `~` substitution tests whether the child's recorded directory (not the candidate string) starts with HOME. For the absolute fallback path computed from a relative recorded path, the test fails and the absolute path is written without `~`.
- Evidence: database/DBio.c:6095-6108, database/DBio.c:6216-6222
- Options: replicate | fix (test the candidate string)
- Recommendation: Replicate (it affects written bytes). Revisit with golden case CELL-PW-06.
- Resolution: compat — the child's recorded directory is tested, as in Magic (it affects written bytes); with `magic-compat` OFF, the candidate string s is tested (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-22: Directory identity uses inode only [platform]
- Kind: platform
- Magic behavior: The duplicate check in use-line reading compares only `st_ino` of the two directories, with no device number, so directories on different filesystems can match by accident. Elsewhere, file identity uses device and inode. iPadOS security-scoped URLs may not expose stable inodes.
- Evidence: database/DBio.c:2712-2728, database/DBio.c:2081-2092
- Options: replicate | fix (device + inode) | platform alternative (file resource identifiers)
- Recommendation: Use the platform file identity (device + inode, or the URL resource identifier on Apple platforms).
- Resolution: platform — directory identity uses the platform file identity: device and inode, or the file resource identifier on Apple platforms (X-4, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-23: Git equivalence check is defective [platform]
- Kind: bug
- Magic behavior: `execlp("git", "-C <dir>", "rev-parse", "HEAD")` passes `-C <dir>` as argument zero, so git runs in Magic's working directory for both checks. When Magic runs inside any git work tree, conflicts are declared equivalent. The read buffer is not NUL-terminated, so the comparison may read garbage. iPadOS has no fork or exec.
- Evidence: database/DBio.c:2750-2836
- Options: replicate | fix (query each directory's repository HEAD via libgit2 or a subprocess) | drop
- Recommendation: Implement the intended semantics (the same HEAD commit for the repositories that contain the two directories) where available. Treat the check as inconclusive on iPadOS.
- Resolution: fix — the intended test is implemented: the repositories containing E and D have the same HEAD commit; where git is unavailable (always on iPadOS) the test is inconclusive; Magic's defective call declares any conflict equivalent when run inside a git work tree (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-24: Duplicate-cell rename is not persistent
- Kind: quirk
- Magic behavior: After renaming the original definition to `N__k`, the renamed definition keeps its file path (`.../N`). Its parents are marked modified. If they are saved, they reference `N__k` with a directory path in which no `N__k.mag` exists, so the next session falls back to the search paths or fails.
- Evidence: database/DBio.c:2866-2897, database/DBcellname.c:2122-2145
- Options: replicate | fix (warn on save, or keep a name-to-file mapping)
- Recommendation: Replicate the in-memory outcome. Warn when saving a parent that references a renamed duplicate.
- Resolution: fix — the in-memory outcome stays as in Magic; Hefesto warns when it saves a parent that references a renamed duplicate, so the broken reference is visible (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-25: Symbolically linked cell files are always read-only
- Kind: quirk
- Magic behavior: The writability test uses `lstat`, so a `.mag` file that is a symbolic link is "not a regular file" and the cell opens read-only. Saving is also refused.
- Evidence: database/DBio.c:138-150
- Options: replicate | fix (follow the link; write through it or replace the target)
- Recommendation: Replicate (users rely on symbolic links to mark library cells as protected). Document it.
- Resolution: n/a — intended behaviour: users rely on symbolic links to mark library cells as protected, so the spec keeps Magic's read-only rule (D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-26: Session locking on sandboxed platforms [platform]
- Kind: platform
- Magic behavior: There is one open descriptor and one `fcntl` write lock per loaded editable cell. Locks are per process and are released when any descriptor of the file is closed. Magic reports the lock holder's process ID.
- Evidence: utils/flock.c:98-215
- Options: platform alternative
- Recommendation: On Linux and macOS, use OFD locks (`F_OFD_SETLK`) or `fcntl` with careful descriptor ownership, so that they interoperate with Magic sessions on the same files. On iPadOS, use NSFileCoordinator/NSFilePresenter for in-app coordination. There is no cross-process locking with Magic there, so treat every file as unlocked and rely on the atomic save.
- Canonical entry for Q-CMDEDIT-33. Platform owner: capability `hefesto-files-and-pdks`.
- Resolution: platform — `hefesto-files-and-pdks` requirement "Locking and recovery replacements": desktop locks interoperate with Magic sessions, iPadOS uses platform file coordination and atomic saves (X-4, 2026-11-07).

### Q-CELL-27: In-place fallback write corrupts the file
- Kind: bug
- Magic behavior: When no temporary file can be created, the cell is appended twice in append mode, because rewinding does not affect `O_APPEND` writes. The file ends up as the old content plus two copies of the new content, and the size check then flags an I/O error. Readers still see the old cell, because reading stops at the first `<< end >>`.
- Evidence: database/DBio.c:5799-5858
- Options: replicate | fix (fail the save with a clear message, or write in place with truncation after validating the space)
- Recommendation: Fix: fail the save and keep the old file intact.
- Resolution: fix — the fallback write corrupts the file; the save fails with a clear message and the old file stays intact (X-2 tier 1, 2026-11-07).

### Q-CELL-28: Order of cells in backup and archive files
- Kind: ambiguity
- Magic behavior: Cells are emitted in cell-hash-table iteration order.
- Evidence: database/DBio.c:6473
- Options: replicate the hash | fix (sorted by name, or parents before children)
- Recommendation: Sort by name with the natural comparator. The reader is order-independent.
- Status: confirmed by X-1 as amended (2026-11-06, D-1): backup and archive cell order is name-keyed but never affects results, so it keeps the sort by name with the natural comparator. The tie-break question remains open in D-9.
- Resolution: fix — decided by D-9 (2026-11-07). Backup and archive cells are written sorted by name with the total natural order of `cell-file-format`; golden comparison canonicalizes the order.

### Q-CELL-29: Wrong file name in the explicit crash-save message
- Kind: bug
- Magic behavior: `crash save <file>` prints `Created database crash recovery file` with the previous backup name, which may be null, before switching to `<file>`.
- Evidence: database/DBio.c:6453-6455
- Options: replicate | fix
- Recommendation: Fix (message text is informative).
- Resolution: fix — the confirmation message names the new file (message text is informative) (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-30: Crash backup location and liveness test [platform]
- Kind: platform
- Magic behavior: Backups are `$TMPDIR` or `/tmp/MAG<pid>.XXXXXX`. Recovery tests whether a process is alive by sending it SIGCONT (`kill(pid, SIGCONT)`), which affects a stopped process. Backups are triggered by a Tcl timer and by SIGTERM.
- Evidence: database/DBio.c:6419-6437, database/DBio.c:1137-1179, utils/signals.c:265-278, tcltk/tools.tcl:57-118
- Options: platform alternative
- Recommendation: Use a per-user application-support directory with a session lock file instead of process-ID probing. Keep the `MAG<pid>.*` reading capability for importing Magic backups on desktop. On iPadOS, back up on the app-background notification and with the timer.
- Canonical entry for Q-UI-31, Q-SCRIPT-35 and Q-CMDEDIT-35. Platform owner: capability `hefesto-files-and-pdks`. The desktop location was decided by D-8 (docs/decisions/audit/DECISIONS.md).
- Resolution: platform — decided by D-8 (2026-11-07). Crash backups go to an app-private directory (Application Support on macOS and iPadOS, `$XDG_STATE_HOME/hefesto` on Linux; never caches), with a per-session lock file for liveness and the Magic-readable file format; on the desktop, Magic's `$TMPDIR/MAG*` backups are offered for import (capability `cell-io-safety`).

### Q-CELL-31: Cell name derived from the first ".mag" in a backup location
- Kind: bug
- Magic behavior: The name is cut at the first `.mag` anywhere in the location, so `/home/u/.magic/x` gives an empty name and aborts the restore. The record line buffer is 256 bytes, so longer locations break parsing.
- Evidence: database/DBio.c:1244-1292
- Options: replicate | fix (strip a trailing `.mag` only; unlimited line length)
- Recommendation: Fix.
- Resolution: fix — only a trailing `.mag` is stripped from the location, and record lines have no length limit, so recovery no longer aborts on such paths (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-32: Recovery does not restore cell file paths
- Kind: quirk
- Magic behavior: The `file` location is used only for the name. Top-level cells recovered from a backup have no recorded path, so they are saved to the working directory rather than to their original location.
- Evidence: database/DBio.c:1294-1300
- Options: replicate | fix (set the recorded path from the location when none is set)
- Recommendation: Fix. The location is exactly the recorded path that Magic wrote.
- Resolution: fix — when a recovered cell has no recorded path, its path is set from the location, which is the path Magic wrote; recovered cells are saved back where they came from (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-33: Recovered cells are not marked modified
- Kind: bug
- Magic behavior: Reading a backup clears the modified flag (as for any read), and the backup file is then deleted. Quitting without saving silently loses the recovered work.
- Evidence: database/DBio.c:1004, database/DBio.c:1200-1201
- Options: replicate | fix (mark recovered cells modified)
- Recommendation: Fix for crash recovery. Archive reads may keep Magic's behavior.
- Resolution: fix — silent data loss; cells recovered from a crash backup are marked modified; archive reads keep Magic's behaviour (X-2 tier 1, 2026-11-07).

### Q-CELL-34: Archive write and read use different file names
- Kind: bug
- Magic behavior: `archive write X` writes the file `X` exactly. `archive read X` opens `X.mag` through Path, so `archive read X.mag` looks for `X.mag.mag`.
- Evidence: database/DBio.c:1248-1263, database/DBio.c:6458
- Options: replicate | fix (try `X` first, then `X.mag`)
- Recommendation: Fix as described. It is backward compatible.
- Resolution: fix — `archive read X` tries `X` first and then `X.mag`, so the name used to write is the name that reads; backward compatible (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-35: 32-bit timestamps
- Kind: quirk
- Magic behavior: Stamps are C `int` and are written with `%d`. After 2038-01-19, `time()` truncation produces negative or wrapped values.
- Evidence: database/DBtimestmp.c:91, database/DBtimestmp.c:229, database/DBio.c:4727-4733
- Options: replicate | fix (64-bit internally; write `%d` of the low 32 bits)
- Recommendation: Store 64-bit, but read and write as signed 32-bit to stay compatible. Revisit before 2038.
- Resolution: fix — stamps are held as 64-bit values internally but read and written as signed 32-bit, so files stay identical to Magic's; revisit before 2038 (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-36: Explicit timestamp does not become fixed
- Kind: bug
- Magic behavior: `cellname timestamp <cell> <value>` executes `flags &= ~CDGETNEWSTAMP | CDFIXEDSTAMP`, which only clears the pending flag. The comment and the flags listing suggest that the stamp was meant to become fixed.
- Evidence: commands/CmdCD.c:1556-1562
- Options: replicate | fix (set the fixed flag)
- Recommendation: Ask the Hefesto team, since this is used for library preparation. Default: replicate, and offer an explicit "fix timestamp" option.
- Resolution: compat — the explicit stamp does not become fixed, as in Magic (it changes the saved timestamp); with `magic-compat` OFF, `cellname timestamp` also sets the fixed-stamp attribute; library preparation should confirm this (X-2 tier 2, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-37: `save` with no edit cell clears read-only on the root cell
- Kind: quirk
- Magic behavior: When no edit cell exists (for example, the root is read-only), `save` clears the root's read-only flag before saving. A read-only, unlocked cell can then be overwritten when the file permissions allow it.
- Evidence: commands/CmdRS.c:207-218
- Options: replicate | fix (allow only "save as" to a new name)
- Recommendation: Allow "save as". Refuse an in-place overwrite of a read-only cell.
- Resolution: fix — a read-only root cell is never overwritten in place; `save` with a new name (save as) still works (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).

### Q-CELL-38: Manual page differs from the code about the use box and the missing tech line
- Kind: doc-mismatch
- Magic behavior: mag(5) says that the `box` covers the whole array in parent coordinates, but the code writes the child definition's bounding box in child coordinates. mag(5) says that a missing `tech` line defaults to `nmos`, but the code reads the file in the current technology without any check.
- Evidence: doc/man/mag.5:137-139, doc/man/mag.5:37-38, database/DBio.c:6282-6285, database/DBio.c:576
- Options: follow the code
- Recommendation: Follow the code (as specified).
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-CELL-39: Duplicate check compares the use path with PWD
- Kind: bug
- Magic behavior: In the duplicate test, a non-`~` path is "OK" when it equals `$PWD`, regardless of where the existing cell came from. If `PWD` is unset, the case-A test dereferences a null pointer, and the case-B test is skipped entirely.
- Evidence: database/DBio.c:2688-2703, database/DBio.c:2907-2924
- Options: replicate | fix (compare with the existing directory; use the OS working directory)
- Recommendation: Use the OS working directory instead of `$PWD`, and keep the "equals current directory" rule. Never crash.
- Resolution: fix — NULL dereference when `PWD` is unset; the process working directory is used instead of `$PWD`, keeping the "equals current directory" rule (X-2 tier 1, 2026-11-07).

### Q-CELL-40: Technology and library discovery depend on directory order
- Kind: ambiguity
- Magic behavior: The search is depth-first in `readdir` order (filesystem dependent). The first `T.tech` found wins (except for a directory named exactly T, which is searched first). Standard-cell paths are appended in traversal order. Names shorter than 4 characters are compared out of bounds.
- Evidence: database/DBio.c:223-320, database/DBio.c:340-391
- Options: replicate | fix (sort entries by name)
- Recommendation: Sort directory entries by name for determinism, keeping the exact-name-first rule.
- Resolution: fix — platform-dependent order: directory entries are visited sorted by name, after an entry named exactly T; names shorter than 4 characters are compared safely (X-1, 2026-11-07).

### Q-CELL-41: Use paths and names containing whitespace
- Kind: quirk
- Magic behavior: Use lines are split on whitespace and written unquoted, so a path, cell name or id that contains spaces does not round-trip. Property values and label texts that contain newlines also break the file.
- Evidence: database/DBio.c:3150, database/DBio.c:6247-6254, database/DBio.c:5071-5074
- Options: replicate | fix (reject such names at creation)
- Recommendation: Reject whitespace in cell names, ids and paths, and newlines in labels and properties, at the editing layer.
- Resolution: fix — names that cannot round-trip are refused when created: whitespace in cell names, instance ids and paths, and newlines in label texts and property values (X-2 tier 3, D-5, approved 2026-11-07, 2026-11-07).
