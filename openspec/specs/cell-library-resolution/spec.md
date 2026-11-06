# Cell Library Resolution

## Purpose

Defines how Hefesto maps a cell name, or an instance's recorded path, to a cell file. It covers:

- The file path recorded for each cell definition, and the search paths.
- How `use`-line paths are read, normalized and written: relative, absolute, PDK-variable and home-directory forms.
- Duplicate-cell conflicts between different files that share one cell name.
- Circular-use detection, dereferencing, and path rebasing when the working directory changes.
- Automatic technology discovery from PDK installation paths.

Everything here must reproduce Magic's results, so that a hierarchy saved by Magic loads the same cells in Hefesto and the reverse. The `.mag` grammar is in capability `cell-file-format`. Locking and read-only state are in `cell-io-safety`. The user commands `load`, `getcell`, `addpath`, `path` and `cellname` are in `cmd-hierarchy`.

Terms used below:

- **cell file path**: the per-definition recorded location, without the `.mag` extension unless stated otherwise.
- **search path**: an ordered list of directories.

## Requirements

### Requirement: Cell file path bookkeeping

Each cell definition SHALL carry an optional cell file path. The rules for setting and normalizing it are:
Compatibility: behavioral

- **Naming a definition.** A name that ends in `.mag` loses that suffix when it becomes a definition name.
- **After a successful read.** The cell file path becomes the opened file name. A trailing `.mag` is removed. A trailing `.mag.gz` is kept as-is (see Q-CELL-16).
- **After a failed lookup with no path recorded.** The cell file path becomes `<cellname>.mag`, with the suffix.
- **After a successful save.** The cell file path becomes the written file name with its last extension removed.
- **`~` and `$`.** These may appear at the start of a cell file path and are expanded only when a file is opened.

(Magic ref: database/DBcellname.c:1961-1984, database/DBio.c:1587-1600, database/DBio.c:1634-1654, database/DBio.c:5860-5867)

#### Scenario: Path recorded after read
- **GIVEN** the search path contains `/lib` and `/lib/inv.mag` exists
- **WHEN** cell `inv` is read without a recorded path
- **THEN** its cell file path SHALL become `/lib/inv`

### Requirement: Search path syntax and expansion

Hefesto SHALL interpret a search path as follows.
Compatibility: behavioral

- **Splitting.** A search path is a string of entries separated by any run of spaces, tabs or colons.
- **Expanding each entry before use:**
  - A leading `~` followed by a user name (up to `/`, `:`, whitespace or end) is replaced by that user's home directory.
  - A bare `~` is replaced by the `HOME` value.
  - An unknown user makes the entry unusable: it is skipped and nothing is tried for it.
  - A leading `$NAME` or `${NAME}` is replaced by the variable's value. If the variable is undefined, the entry is kept literally. The exception is `CAD_ROOT`, which falls back to the installation root.
  - Leading `./` sequences are removed. An entry `.` becomes the empty directory, meaning the current directory.
- **Forming a candidate.** The candidate is the expanded entry, then `/` (unless the entry is empty or already ends in `/`), then the file name and extension.
- **Names that bypass the search path.** These are opened directly: a name that begins with `/`, is exactly `.` or `..`, or begins with `./` or `../`. A name that begins with `~` or `$` is expanded and opened directly.
- **Stopping early.** While walking a search path, an open error other than "file does not exist" stops the whole lookup, which then fails.

The two cell search paths are:

- **Path** (default `.`, extended by `addpath` and `path`).
- **Library path**. The default is `$CAD_ROOT/magic/<techname> $CAD_ROOT/magic/sys/current $CAD_ROOT/magic/tutorial` when a technology was named at startup, and `$CAD_ROOT/magic/sys/current $CAD_ROOT/magic/tutorial` otherwise. It is extended by the standard-cell discovery described below.

The variable source is informative: Magic's Tcl build resolves `$NAME` against the interpreter's global variables, and the non-Tcl build against the process environment. See Q-CELL-17 [platform].

(Magic ref: utils/path.c:193-367, utils/path.c:389-430, utils/path.c:447-626, utils/main.c:516-517, utils/main.c:590-607, utils/paths.h:40-44)

#### Scenario: Current directory entry
- **GIVEN** Path is `. ~/lib`, HOME is `/home/u`, `./a.mag` does not exist and `/home/u/lib/a.mag` exists
- **WHEN** cell `a` is looked up
- **THEN** the candidates SHALL be `a.mag` and then `/home/u/lib/a.mag`, and the second SHALL be opened

### Requirement: Cell file lookup order

To read cell C, Hefesto SHALL try candidates in this order, stopping at the first successful open.
Compatibility: behavioral

**C has a recorded cell file path F.** A trailing `.mag` is first stripped from F for the lookup.

- *Not dereferenced:*
  1. F + `.mag`, opened directly. A relative F is relative to the working directory.
  2. F + `.mag.gz`, opened directly.
  3. C + `.mag` through Path, then through the library path.
  4. A gzip retry through Path and then the library path. Magic uses F rather than C as the name for this retry (see Q-CELL-18).
  5. If a search-path candidate (steps 3–4) succeeds, the following warnings are printed:
     - `Warning:  Parent cell lists instance of "<C>" at bad file path <F>.`
     - `The cell exists in the search paths at <found>.`
     - `The discovered version will be used.`
     The recorded path becomes the found file.
- *Dereferenced* (the definition is flagged to ignore recorded paths):
  1. Steps 3–4 first. These print no warnings.
  2. Then F + `.mag` and F + `.mag.gz`, opened directly, with the warning `Warning:  Dereferenced cell "<C>" not found in search paths;  using original location <F>.`

**C has no recorded path.**
1. C + `.mag` through Path, then through the library path.
2. C + `.mag.gz` through Path, then through the library path.

**Compressed counterpart preference.** When file locking is enabled (the default), every open first checks whether `<candidate>.gz` is readable and, if so, opens that file instead. The recorded name is still the candidate. With locking enabled, `x.mag.gz` therefore wins over `x.mag` (see Q-CELL-16).

**Recording the result.** On success, the cell is marked available, its not-found flag is cleared, and the recorded path is updated (see "Cell file path bookkeeping"). At full verbosity, Magic prints `Cell <C> read from path <dir>` or `Cell <C> read from current working directory`.

(Magic ref: database/DBio.c:1447-1662, utils/flock.c:98-215, utils/path.c:117-130)

#### Scenario: Bad recorded path falls back to search path
- **GIVEN** cell `inv` has recorded path `/old/inv`, which does not exist, and Path contains `/new`, which holds `inv.mag`
- **WHEN** `inv` is read without dereferencing
- **THEN** `/new/inv.mag` SHALL be read and the three bad-path warnings SHALL be printed
- **AND** the recorded path SHALL become `/new/inv`

#### Scenario: Dereferenced cell prefers search path
- **GIVEN** `inv` has recorded path `/proj/lib/inv`, `/proj/lib/inv.mag` exists, and Path contains `/pdk/mag`, which holds `inv.mag`
- **WHEN** `inv` is read dereferenced
- **THEN** `/pdk/mag/inv.mag` SHALL be read

### Requirement: Missing cell handling

When no candidate can be opened, Hefesto SHALL report the failure once per definition and keep the definition as an unavailable placeholder.
Compatibility: behavioral

- **Message.** If the definition has a recorded path, Magic prints `File <F> couldn't be read`. Otherwise it prints `Cell <C> couldn't be read` and records the path `<C>.mag`. When the caller asked for the error code, the system error text follows.
- **Not-found flag.** The definition is flagged not-found. Later attempts fail silently until the flag is cleared, which happens on a successful open or a backup read.
- **Placeholder.** The placeholder keeps the bounding box guessed from its parent's `box` line.

(Magic ref: database/DBio.c:1581-1605)

#### Scenario: Error printed once
- **GIVEN** a parent with two instances of a child whose file does not exist
- **WHEN** the hierarchy is expanded twice
- **THEN** exactly one `couldn't be read` message SHALL be printed for the child

### Requirement: Reading instance paths

For the path token P of a `use` line in a parent being read, Hefesto SHALL compute the child directory D as follows.
Compatibility: behavioral

1. **Trim.** Leading spaces and tabs are removed. A path that is only a newline counts as empty.
2. **Variable expansion.** If P begins with `$`, the variable name runs until `}`, `/`, whitespace or end, and may be enclosed in `{}`. If the variable is defined, the `$NAME` or `${NAME}` prefix is replaced by its value. Otherwise P is kept unchanged (see Q-CELL-19).
3. **Parent-relative join.** This step applies only on the first use of this child name in the file. If P is empty or does not begin with `/` or `~`, and the parent's cell file path contains `/`, let Pd be the parent's directory (the text before the last `/`):
   - If P is empty, D = Pd.
   - Otherwise D = collapse(Pd + `/` + P), using the collapse algorithm in "Path normalization algorithms".
4. **Home remapping.** If P begins with `~/` and the parent's cell file path is absolute and not under the current user's HOME, home-relative paths are remapped to the parent's home directory:
   - Let R be HOME up to and including its last `/` (for example `/home/`).
   - If the parent path starts with R, `~` is replaced by the parent path's prefix up to the next `/` after R.
5. **Otherwise** D = P.

If the resulting D is non-empty, the child's location is decided by "Duplicate cell conflicts on read". A path is used only on the first use of a child name within one file. Later instances inherit the definition.

(Magic ref: database/DBio.c:2409-2426, database/DBio.c:2585-2672)

#### Scenario: Relative path joined to parent directory
- **GIVEN** parent cell file path `../mid/top`, where `../mid` is not a symbolic link
- **WHEN** it contains `use inv  inv_0 ../lib` and `inv` is not yet known
- **THEN** `inv`'s recorded path SHALL be `../lib/inv.mag`

#### Scenario: Home directory of another user
- **GIVEN** HOME is `/home/alice` and the parent's cell file path is `/home/bob/proj/top`
- **WHEN** the parent contains `use inv  inv_0 ~/lib`
- **THEN** the child directory SHALL be `/home/bob/lib`

#### Scenario: No path means parent directory
- **GIVEN** parent cell file path `/p/top`
- **WHEN** it contains `use inv  inv_0` and `inv` is not yet known
- **THEN** `inv`'s recorded path SHALL be `/p/inv.mag`

### Requirement: Path normalization algorithms

Hefesto SHALL implement the following component-based path operations.
Compatibility: exact

**Common splitting rule.** A path is split on `/`. Empty components and `.` components are dropped. "Absolute" means the path starts with `/`.

- **collapse(p).**
  - Walk the components and append each one to an output list, with two exceptions.
  - For a `..` component, when the output is non-empty and its last component is not `..`: if the output joined so far is not a symbolic link (checked with no-follow status), drop the last output component instead of appending `..`.
  - A `..` directly after the root of an absolute path is dropped.
  - The result is joined with `/`, with a leading `/` if absolute. An empty result becomes `.`.
- **absComps(p)**, the absolute components of p. It fails if p starts with `~` or `$`.
  - A relative p is prefixed with the current working directory (as returned by the OS, which contains no symbolic links).
  - Leading `..` components of a relative path pop the working directory.
  - Any other `..` makes it fail.
- **relDir(pdir, cdir)**, the relative path from pdir to cdir.
  - Let m be the number of common leading components of absComps(pdir) and absComps(cdir).
  - It fails if either absComps fails, if m = 0, or if any of pdir's directories at depth > m is a symbolic link.
  - Otherwise the result is `..` repeated (len(pdir) − m) times, followed by cdir's components after m, joined by `/`. It is empty when the two are the same directory.
- **absDir(d).** It fails for `~` and `$`. An absolute d is returned unchanged. Otherwise the result is collapse(cwd + `/` + d), or cwd if d is empty.
- **isDirPrefix(pdir, cdir).** True when both are absolute or both are relative and pdir's components are a leading part of cdir's. An empty pdir is a prefix of any relative cdir.

(Magic ref: database/DBio.c:1729-2244)

#### Scenario: Collapse with a symbolic link
- **GIVEN** `/a/link` is a symbolic link
- **WHEN** collapse is applied to `/a/link/../b` and to `/a/dir/../b`, where `/a/dir` is a real directory
- **THEN** the results SHALL be `/a/link/../b` and `/a/b`

#### Scenario: Relative directory
- **GIVEN** no symbolic links are involved
- **WHEN** relDir(`/h/u/proj`, `/h/u/other`) is computed
- **THEN** the result SHALL be `../other`

### Requirement: Writing instance paths

When writing a parent cell P, Hefesto SHALL compute the path token of the use line for child definition C as follows. Let `cf` be C's cell file path and `pf` the parent's cell file path, or its name if no path is recorded.
Compatibility: exact

1. If C was already written earlier in this file, if `cf` is unset, or if `cf` contains no `/`, there is no preliminary path. Go to step 4.
2. **Strip common leading components.**
   - Start with s = `cf`. While the text of `cf` up to and including the next `/` after the current position equals the same-length prefix of `pf`, advance s past that `/`.
   - If `cf` is absolute and only the root `/` was stripped, restore s = `cf`.
   - Cut s at `cf`'s last `/`, keeping the directory part. If nothing remains, there is no preliminary path.
3. **Substitute.** The preliminary path is subst(s), as defined in "Path variable substitution on write".
4. **Make it relative to the parent.** If C was not already written, `cf` is set, and the preliminary path is absent or starts with none of `/`, `~`, `$`:
   - Let cdir be `cf`'s directory, or empty if `cf` has no `/`. Let pdir be `pf`'s directory: empty if `pf` has no `/`, and `/` if that is the only slash.
   - If not isDirPrefix(pdir, cdir), let q = relDir(pdir or `.`, cdir or `.`).
     - If q is empty, there is no path.
     - If q is non-empty, the path is q.
     - If q failed, the path is subst(absDir(cdir)), when absDir succeeds.
   - If isDirPrefix holds, the preliminary path is kept.
5. If a path results, it is written after the id, separated by one space, without quoting. C is then marked as written for the rest of this file. A path that contains whitespace therefore cannot be read back (see Q-CELL-41).

(Magic ref: database/DBio.c:6130-6243, database/DBio.c:2097-2134)

#### Scenario: Sibling directory under home
- **GIVEN** HOME is `/home/u`, parent `pf` is `/home/u/proj/top` and child `cf` is `/home/u/other/inv`, with no symbolic links
- **WHEN** the parent is written
- **THEN** the use line SHALL end with ` ../other`

#### Scenario: Subdirectory
- **GIVEN** parent `pf` is `/home/u/proj/top` and child `cf` is `/home/u/proj/lib/inv`
- **WHEN** the parent is written
- **THEN** the use line SHALL end with ` lib`

#### Scenario: Same directory
- **GIVEN** parent `pf` is `/a/top` and child `cf` is `/a/inv`
- **WHEN** the parent is written
- **THEN** the use line SHALL carry no path

#### Scenario: Unrelated absolute directory under home
- **GIVEN** HOME is `/home/u`, parent `pf` is `/tmp/top` and child `cf` is `/home/u/lib/inv`
- **WHEN** the parent is written
- **THEN** the use line SHALL end with ` ~/lib`

#### Scenario: Path written once
- **GIVEN** a parent with instances `inv_0` and `inv_1` of child `/lib/inv` and parent `pf` `/proj/top`
- **WHEN** the parent is written
- **THEN** only the `inv_0` use line SHALL carry the path `/lib`

### Requirement: Path variable substitution on write

The function subst(s) SHALL rewrite a directory string as follows.
Compatibility: exact

1. **PDK variables.** Check the variables `PDK_PATH`, `PDKPATH`, `PDK_ROOT` and `PDKROOT`, in that order. For the first one that is defined with value V such that s starts with the characters of V, the result is `$` + name + the rest of s. This is a plain character-prefix test, not a component test (see Q-CELL-20).
2. **Home directory.** Otherwise, if s is absolute and the child's cell file directory starts with HOME immediately followed by `/`, the result is `~` + the part of that directory after HOME. Magic tests the child's recorded directory here, not s (see Q-CELL-21).
3. **Otherwise** s is returned unchanged.

The variable source is informative. In Magic's Tcl build, step 1 reads interpreter globals, which are typically set by a PDK's startup script. Without Tcl, step 1 is skipped. See Q-CELL-17 [platform].

(Magic ref: database/DBio.c:6059-6111)

#### Scenario: PDK root substitution
- **GIVEN** variable `PDK_ROOT` = `/usr/share/pdk`, `PDK_PATH` and `PDKPATH` undefined, and a child at `/usr/share/pdk/sky130A/libs.ref/sky130_fd_sc_hd/mag/inv` used by parent `/home/u/top`
- **WHEN** the parent is written
- **THEN** the use line SHALL end with ` $PDK_ROOT/sky130A/libs.ref/sky130_fd_sc_hd/mag`

### Requirement: Duplicate cell conflicts on read

When a `use` line gives a non-empty child directory D for a child name N whose definition already exists, Hefesto SHALL decide whether the existing definition refers to the same file. Steps 1–4 below run only when the parent is not dereferenced, this is the first use of N in the file, and the paths differ textually. The procedure depends on the existing definition's recorded path.
Compatibility: behavioral

**Case A: the recorded path contains `/`.** Let E be the existing directory (the recorded path before its last `/`). The paths are equivalent ("OK") when any of the following holds:

1. D starts with `~` and E equals HOME + D[1:], or D equals the PWD environment value. Magic compares D with PWD, not with E, and fails if PWD is unset (see Q-CELL-39).
2. E and D, as directories, have the same inode number (see Q-CELL-22).
3. *Missing test.* Otherwise, if no file N.mag exists through D, D is noted as missing.
4. If D is not missing and both directories are in git work trees with the same HEAD commit, Magic prints `Cells <N> in <E> and <D> have matching git repository commits and can be considered equivalent.` and treats them as OK. Magic's invocation is defective (see Q-CELL-23).

If the paths are still not OK:

- **D is missing.** The existing definition is kept. Magic prints `Bad cell path in <parent>:  Instance of cell <N> specifies path <D>, which does not contain the cell.  Cell previously read from <E> will be used instead.`, or the variant `..., which does not exist.  ...` when D is not a directory.
- **D is not missing.** Magic prints `Duplicate cell in <parent>:  Instance of cell <N> is from path <D> but cell was previously read from <E>.`. The existing definition is renamed to `N__k`, where k is the smallest integer ≥ 0 not used as a definition name, with the message `Cell name conflict:  Renaming original cell to N__k.`. A new placeholder N is created with the child timestamp and box guess and recorded path `D/N.mag`. The renamed definition keeps its old file path, and its parents are marked modified (see Q-CELL-24).

**Case B: the recorded path has no `/`.** This check runs only when PWD is defined.

- The paths are OK if D equals PWD (or `~`-equals it), or if the same file is reachable as both the recorded path and D/N. File identity means equal device and inode, after trying the `.mag`, bare and `.mag.gz` names.
- Otherwise the missing test and the missing or duplicate outcomes are as in case A, with messages that say `the current directory` instead of E.

**Case C: no recorded path.** The recorded path becomes `D/N.mag`.

(Magic ref: database/DBio.c:2680-3016, database/DBio.c:2037-2092, database/DBcellname.c:95-169, database/DBcellname.c:2122-2145)

#### Scenario: Same cell through two relative paths
- **GIVEN** `inv` was read with recorded path `../lib/inv` and a second parent references it with D = `../mid/../lib`, which is the same directory
- **WHEN** the second parent is read
- **THEN** no conflict SHALL be reported and the existing definition SHALL be used

#### Scenario: True duplicate is renamed
- **GIVEN** `inv` was read from `/a/inv.mag`, `/b/inv.mag` is a different file, and the git test does not apply
- **WHEN** a parent with `use inv  inv_0 /b` is read
- **THEN** the original definition SHALL be renamed `inv__0`
- **AND** a new definition `inv` with recorded path `/b/inv.mag` SHALL be used by this parent

#### Scenario: Stale path ignored
- **GIVEN** `inv` was read from `/a/inv.mag` and directory `/b` exists but holds no `inv.mag`
- **WHEN** a parent with `use inv  inv_0 /b` is read
- **THEN** the `Bad cell path` message SHALL be printed and the instance SHALL use the existing definition

### Requirement: Path conflicts on explicit load and placement

When the user loads or places a cell by an explicit path whose base name equals an existing definition with a recorded path, Hefesto SHALL check whether both refer to the same file. The commands themselves are in capability `cmd-hierarchy`.
Compatibility: behavioral

- **Load.** The same-file test compares the inode of the file found for the requested name with the inode of the existing recorded path + `.mag`.
  - If they differ, the load fails with `File "<name>":  The cell was already read and points to conflicting location "<path>".`.
  - If the existing path cannot be checked, the existing definition has never been read, and the load did not demand failure, the recorded path is cleared, the requested location is used, and Magic warns `Existing cell <N> points to invalid path ...`.
- **Placement by path.** A path that differs both textually and by file identity creates a new definition `N_a`, `N_b`, … (the first unused one), with the warning `Cell file path mismatch.`

(Magic ref: dbwind/DBWprocs.c:380-454, commands/CmdCD.c:4960-4990)

#### Scenario: Load of a different file with the same name
- **GIVEN** `inv` is loaded from `/a/inv.mag`
- **WHEN** the user loads `/b/inv`, a different file
- **THEN** the load SHALL fail with the conflicting-location message

### Requirement: Dereferencing

A cell definition MAY be flagged as dereferenced. When the flag is set, Hefesto SHALL ignore recorded instance paths in favor of the search paths.
Compatibility: behavioral

- **Lookup order.** A dereferenced definition is looked up through the search paths first (see "Cell file lookup order").
- **Parent being read.** When the parent being read is dereferenced, conflict checks on its `use` paths are skipped. The paths are still recorded for children that have none, so the original location can be used as a last resort.
- **Propagation.** When a dereferenced definition's unavailable children are expanded, they inherit the flag.
- **Top-level cell.** A cell named directly on a load command is never dereferenced itself. The flag applies only to its descendants.
- **Flush.** Forced dereferencing during a flush clears the definition's recorded path.

(Magic ref: database/DBio.c:1361-1414, database/DBexpand.c:166-215, dbwind/DBWprocs.c:476-490, commands/CmdSubrs.c:421-427)

#### Scenario: Dereferenced hierarchy uses library copies
- **GIVEN** top cell `top` is loaded with dereferencing, its file references `use inv  inv_0 ../oldlib`, and Path contains `/pdk/mag`, which holds `inv.mag`
- **WHEN** `top` is expanded
- **THEN** `inv` SHALL be read from `/pdk/mag/inv.mag`

### Requirement: Rebasing paths on working-directory change

When the working directory changes from O to W, Hefesto SHALL rewrite every relative cell file path (one that does not start with `/`, `~` or `$`) so that it still names the same file.
Compatibility: behavioral

1. Split the path into components r. Let k be the number of leading `..` components.
2. If k exceeds the number of components of O, the path is left unchanged.
3. Otherwise let base = |O| − k, and let m be the number of common leading components of O[0..base) and W.
4. The new path is `..` repeated (|W| − m) times, then O[m..base), then r[k..], joined by `/` and relative.

Nothing is done when O equals W.

(Magic ref: database/DBio.c:2264-2362, tcltk/tclmagic.c:820-842)

#### Scenario: Change to a sibling directory
- **GIVEN** the working directory changes from `/home/u/a` to `/home/u/b`
- **WHEN** a definition has relative path `lib/inv`
- **THEN** its path SHALL become `../a/lib/inv`

### Requirement: Automatic technology discovery

When a cell's technology differs from the current one, the read does not ignore the technology, and no non-internal cell contains paint, Hefesto SHALL try to locate and load that technology T. The steps are:
Compatibility: behavioral

1. **Search roots.** Search for the file `T.tech` under each root in turn, stopping at the first hit:
   - the environment variables `PDK_PATH`, `PDKPATH`, `PDK_ROOT`, `PDKROOT`;
   - the fixed directory `/usr/local/share/pdk`;
   - in a scripting build, the interpreter variables `PDK_ROOT`, `PDKROOT`, `PDK_PATH`, `PDKPATH`.
2. **Directory search.** The search below each root is depth-first, with these rules:
   - The root is level 0. Levels above 10 are not entered.
   - Within a directory, an entry named exactly T is visited first, and the others follow in directory-listing order. `.` and `..` are skipped.
   - A non-directory entry named `T.tech` ends the search with the containing directory as the result. Other directory entries are searched recursively.
   - Symbolic links to directories are not followed where the directory listing reports entry types.
   - The result depends on directory-listing order (see Q-CELL-40).
3. **Startup script.** If found, and a `T.magicrc` file exists in that directory, it is executed first (scripting build only).
4. **Register the directory.** If found, the directory is appended to the system search path.
5. **Load.** Whether or not a directory was found, Magic prints `Loading technology T` and loads T by name as a full load, without the readability pre-check (capability `tech-loading-and-scaling`). When no directory was found, the normal technology file search rules apply. On failure Magic prints `Error in loading technology file`.
6. **open_pdks layout.** If the load succeeded, the found directory contains the substring `libs.tech`, and no startup script was executed, the found path is truncated to end at `libs.ref` (everything after `libs.tech` is discarded and the substring becomes `libs.ref`). Standard-cell discovery is then run on it.
7. **Hooks.** If the load succeeded, the callbacks registered for `tech load` are run (scripting build).

If the technology still differs from T after these steps, the read fails with a message suggesting `tech load`, or `cellname delete` followed by `load -force`.

(Magic ref: database/DBio.c:223-320, database/DBio.c:597-750)

#### Scenario: Open a PDK cell in a fresh session
- **GIVEN** an empty session in technology `minimum`, `PDK_ROOT` = `/opt/pdk`, `/opt/pdk/sky130A/libs.tech/magic/sky130A.tech` and `sky130A.magicrc` beside it
- **WHEN** a cell with `tech sky130A` is loaded
- **THEN** `sky130A.magicrc` SHALL be executed, technology `sky130A` SHALL be loaded, and the cell SHALL be read

#### Scenario: Paint present prevents switching
- **GIVEN** a loaded cell contains paint in technology `scmos`
- **WHEN** a cell with `tech sky130A` is loaded
- **THEN** no technology SHALL be loaded and the read SHALL fail

### Requirement: Standard-cell library path discovery

Given a root directory, Hefesto SHALL append to the library path every directory at depth ≤ 10 below the root (inclusive) that directly contains at least one non-directory entry whose name ends in `.mag`.
Compatibility: behavioral

- **Traversal.** The traversal is depth-first in directory-listing order.
- **When a directory is appended.** A directory is appended when its first `.mag` entry is met during its own listing. A subdirectory listed earlier is therefore appended before its parent.
- **Message.** If at least one directory was added, Magic prints `Cell path is now "<library path>"`.

(Magic ref: database/DBio.c:340-391, database/DBio.c:705-723)

#### Scenario: open_pdks reference libraries
- **GIVEN** `/opt/pdk/sky130A/libs.ref/sky130_fd_sc_hd/mag/` contains `.mag` files and no other directory under `libs.ref` does
- **WHEN** discovery runs on `/opt/pdk/sky130A/libs.ref`
- **THEN** exactly `/opt/pdk/sky130A/libs.ref/sky130_fd_sc_hd/mag` SHALL be appended to the library path

### Requirement: Host mechanisms for resolution

Hefesto SHALL treat the following description of Magic's host facilities as reference material only. Magic's resolution relies on them.
Compatibility: informative

- the process environment (`HOME`, `PWD`, `TMPDIR`, `PDK_*`);
- Tcl global variables;
- the password database for `~user`;
- `stat`/`lstat` device and inode numbers, and symbolic-link detection;
- `getcwd`;
- `readdir` entry types;
- running the external `git` program through `fork`/`execlp` and a pipe.

The `git` invocation passes `-C <dir>` as the program's argument zero, so the command actually run is `git rev-parse HEAD` in Magic's own working directory, and the output buffer is not terminated. Hefesto platforms without these facilities need the decisions logged under Q-CELL-17, Q-CELL-22 and Q-CELL-23 [platform].

(Magic ref: database/DBio.c:2750-2836, utils/path.c:193-367)

#### Scenario: No git available
- **GIVEN** the `git` program is not installed
- **WHEN** a duplicate-cell check reaches the git test
- **THEN** the git test SHALL be treated as inconclusive and the conflict handling SHALL continue
