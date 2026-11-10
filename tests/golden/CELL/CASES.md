# Golden Cases: CELL area

These cases are to be captured by running Magic `fc5daa9e` headless (`magic -dnull -noconsole`, Tcl build, locking enabled, zlib). The default technology is `scmos`, unless a case names `sky130A` (open_pdks install) or `fake` (a minimal test technology with a stacked contact `m3c2` whose residues are `m2c` and `m3c`).

Capture conventions:
- **"file"**: the exact bytes of the written `.mag` file.
- **"console"**: stdout and stderr.
- **"db dump"**: the output of `cellname list`, `cellname flags`, `cellname filepath`, `instance list`, `getcell` paths, `property` and `cellname timestamp` for all cells.

Timestamps that depend on wall-clock time are captured by scripting `cellname timestamp` or by masking them.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| CELL-RD-01 | cell-file-format | scmos; minimal file `magic`/`tech scmos`/`timestamp 5`/`<< end >>`; `load a` | console, db dump | Overall file grammar; Header parsing |
| CELL-RD-02 | cell-file-format | first line `MAGIC` | console (error text), db dump | Header parsing |
| CELL-RD-03 | cell-file-format | second line `<< metal1 >>` (no tech line) under technologies scmos and sky130A | db dump (paint, stamp 0) | Header parsing |
| CELL-RD-04 | cell-file-format | `magscale 1 2` file loaded into a fresh session, then a second file without magscale holding `rect 0 0 3 5` | db dump (`box` values in internal units, `scalegrid` state), console | Coordinate scaling on read |
| CELL-RD-05 | cell-file-format | `magscale 1 2` under a cifoutput style whose gridlimit forbids refinement; rect with odd and negative coordinates | db dump, console | Coordinate scaling on read |
| CELL-RD-06 | cell-file-format | CRLF line endings, `#` comment lines inside the paint, use and label sections | db dump | Line reading model |
| CELL-RD-07 | cell-file-format | `rect  1 2 3 4` (double space); `rect 1 2 3` followed by `rect 5 6 7 8` | db dump, console | Rectangle and triangle records |
| CELL-RD-08 | cell-file-format | `tri` records with corners `nw sw se ne`, plus `SE`, `n`, and empty corner text | db dump (`what`, split directions) | Rectangle and triangle records |
| CELL-RD-09 | cell-file-format | `fake`; section `<< m3c2 >>` with one rect | db dump (types on each plane) | Body section dispatch |
| CELL-RD-10 | cell-file-format | sections `<< checkpaint >>` and `<< error_p >>` with rects; then `drc why` | db dump, DRC state | Body section dispatch |
| CELL-RD-11 | cell-file-format | unknown layer section `<< nolayer >>`; alias section naming several types | console, db dump | Body section dispatch |
| CELL-RD-12 | cell-file-format | blank line between sections | console, db dump (partial content) | Line reading model; Format errors and partial reads |
| CELL-RD-13 | cell-file-format | file without `<< end >>` | console, db dump | Format errors and partial reads |
| CELL-RD-14 | cell-file-format | use blocks: locked, unlocked, no id, duplicate id, array, missing timestamp, degenerate box | console, db dump (instance ids, arrays, transforms, lock) | Use block parsing; Instance identifiers on read |
| CELL-RD-15 | cell-file-format | illegal transform `transform 2 0 0 0 1 0`; malformed array line | console | Use block parsing |
| CELL-RD-16 | cell-file-format | self use (`a` uses `a`); mutual use `a`↔`b` | console, db dump | Use block parsing |
| CELL-RD-17 | cell-file-format | labels: rlabel sticky and non-sticky, flabel with known and unknown font, old `label` form, 120-character text, text with inner spaces, label on unknown layer, label on a stacked type | db dump (label list with flags) | Label records |
| CELL-RD-18 | cell-file-format | port records with 2, 3, 4, 5 tokens; port before any label; second port on the same label; unknown use, class and shape | console, db dump (port attributes) | Port records |
| CELL-RD-19 | cell-file-format | elements: rectangle, line with flags, text with position and size flags, malformed element | db dump (`element` listing) | Element records |
| CELL-RD-20 | cell-file-format | properties: string, integer, dimension, double; `FIXED_BBOX` and `OBS_BBOX` as string; `MASKHINTS_x` with 8 and with 6 values; `GDS_START` and `GDS_END`; `GDS_FILE`; read with magscale 1 2 | db dump (`property` values and types), console | Properties section records |
| CELL-RD-21 | cell-file-format | property value of 5000 characters | db dump (value length and content) | Properties section records |
| CELL-RD-22 | cell-file-format | `inv.mag.gz` only (gzip); load `inv` | db dump (`cellname filepath`) | Compressed cell input |
| CELL-WR-01 | cell-file-format | scmos; draw poly, metal1, a triangle and two instances; `save` | file | Writer header and scale reduction; Writer paint sections; Writer use blocks |
| CELL-WR-02 | cell-file-format | `scalegrid 1 2`; cell with all-even coordinates; save. Then add one odd rect and save again | both files | Writer header and scale reduction |
| CELL-WR-03 | cell-file-format | `scalegrid 1 2`; flabel with odd size and offset, element at odd coordinates, even paint; save | file | Writer header and scale reduction |
| CELL-WR-04 | cell-file-format | metal1 A=(0,0)-(10,10), B=(20,5)-(30,15), plus a 4-rect staircase | file (record order) | Writer paint sections |
| CELL-WR-05 | cell-file-format | `fake`; paint `m3c2`, and a split tile with `m2c` on one side and `m3c2` on the other; save | file | Writer paint sections |
| CELL-WR-06 | cell-file-format | instance ids `a10`, `B1`, `a2`, `x`, `x2`, `A1`, `a1`; save on Linux and on macOS builds | file (use order on both hosts) | Writer use blocks; Natural ordering of names |
| CELL-WR-07 | cell-file-format | labels with ports: use only, class only, all three attributes, use=reset; then read back | file, db dump after reload | Writer labels and ports; Port records |
| CELL-WR-08 | cell-file-format | persistent elements (rectangle, line with flags, text NORTH small); save | file | Writer elements |
| CELL-WR-09 | cell-file-format | properties of every type; save with `property compat true` and with `property compat false`; reload both | files, db dump after reload | Writer properties |
| CELL-WR-10 | cell-file-format | round trip: load each `.mag` of the sky130_fd_sc_hd library and save it unmodified (timestamps fixed) | diff of saved vs. original files | Overall file grammar; all writer requirements |
| CELL-WR-11 | cell-file-format | `save top.tcl` of a cell with paint, a split tile, an instance, labels, ports and properties; then `source top.tcl` in a fresh session | script file, db dump after sourcing, `cellname flags top` | Command-script save variant; Command-script content |
| CELL-PR-01 | cell-library-resolution | parent `../mid/top` with `use inv inv_0 ../lib`; then a second parent referencing `../lib` | db dump (`cellname filepath`), console | Reading instance paths; Duplicate cell conflicts on read |
| CELL-PR-02 | cell-library-resolution | HOME=/home/alice; parent at `/home/bob/proj/top` with `~/lib` | db dump (child filepath) | Reading instance paths |
| CELL-PR-03 | cell-library-resolution | `use` path `$PDK_ROOT/...` with the Tcl variable set, and with it unset | db dump, console | Reading instance paths |
| CELL-PR-04 | cell-library-resolution | duplicate: `/a/inv.mag` and `/b/inv.mag` with different content; parents referencing each, run outside any git tree | console, db dump (`inv__0`) | Duplicate cell conflicts on read |
| CELL-PR-05 | cell-library-resolution | duplicate where `/b` exists but has no `inv.mag`; and where `/b` does not exist | console | Duplicate cell conflicts on read |
| CELL-PR-06 | cell-library-resolution | same file through a symbolic link and through a real path | console (no conflict expected), db dump | Duplicate cell conflicts on read; Path normalization algorithms |
| CELL-PR-07 | cell-library-resolution | duplicate check run with the working directory inside a git work tree, for two unrelated directories | console (git equivalence message) | Duplicate cell conflicts on read |
| CELL-PR-08 | cell-library-resolution | missing child referenced by two parents; expand twice | console (single message), db dump (not-found flag, filepath `inv.mag`) | Missing cell handling |
| CELL-PR-09 | cell-library-resolution | bad recorded path with the cell present in `addpath` directory; then the same with `load -dereference` | console, db dump | Cell file lookup order; Dereferencing |
| CELL-PR-10 | cell-library-resolution | both `x.mag` and `x.mag.gz` present (different content), locking on and off | db dump (which content), `cellname filepath` | Cell file lookup order |
| CELL-PR-11 | cell-library-resolution | `load /b/inv` after `/a/inv` was loaded; `getcell /b/inv` | console, db dump (`inv_a`) | Path conflicts on explicit load and placement |
| CELL-PR-12 | cell-library-resolution | relative cell paths, then `cd ../b` (Tcl `cd`) | `cellname filepath` before and after | Rebasing paths on working-directory change |
| CELL-PW-01 | cell-library-resolution | child in a subdirectory of the parent directory; save the parent | file (use path `lib`) | Writing instance paths |
| CELL-PW-02 | cell-library-resolution | child in a sibling directory under HOME | file (`../other`) | Writing instance paths |
| CELL-PW-03 | cell-library-resolution | child in the same directory; two instances | file (no path; path only on the first instance) | Writing instance paths |
| CELL-PW-04 | cell-library-resolution | parent in `/tmp`, child under HOME | file (`~/lib`) | Writing instance paths; Path variable substitution on write |
| CELL-PW-05 | cell-library-resolution | child under `$PDK_ROOT` (Tcl variable set), plus `PDK_ROOT` as a character prefix of a sibling directory | file (`$PDK_ROOT/...`, `$PDK_ROOT2/...`) | Path variable substitution on write |
| CELL-PW-06 | cell-library-resolution | child recorded relative to the working directory, parent saved into a directory reached through a symbolic link | file (absolute fallback path, `~` or not) | Writing instance paths; Path variable substitution on write |
| CELL-LIB-01 | cell-library-resolution | fresh session, PDK_ROOT pointing to an open_pdks install; `load` of a sky130A cell | console (`Loading technology`), `tech name`, `addpath`/cell path | Automatic technology discovery |
| CELL-LIB-02 | cell-library-resolution | as LIB-01, but with `sky130A.magicrc` removed | console (`Cell path is now`), cell path | Automatic technology discovery; Standard-cell library path discovery |
| CELL-LIB-03 | cell-library-resolution | technology mismatch with paint loaded; `load -force` | console | Automatic technology discovery; Technology check on read |
| CELL-IO-01 | cell-io-safety | cell file mode 0444 owned by the user; file owned by another user with 0664 (group member); symbolic-link cell file | `cellname flags` (readonly), console | Read-only cell detection |
| CELL-IO-02 | cell-io-safety | two Magic processes load the same cell | console of the second (`locked by pid`), `edit` refusal | Advisory locking between sessions; Locking mechanism |
| CELL-IO-03 | cell-io-safety | `save` of a read-only cell with an edit cell; with no edit cell; `writeall force` with a modified read-only cell | console, files on disk | Write refusal for protected cells |
| CELL-IO-04 | cell-io-safety | save over an existing 0640 file; inspect the directory during the save (strace) | resulting mode, syscall trace (mkstemp, rename) | Atomic save through a temporary file |
| CELL-IO-05 | cell-io-safety | save into a directory without create permission where the target exists and is writable | file content after the save, console | Fallback write without a temporary file |
| CELL-IO-06 | cell-io-safety | save onto a full filesystem (loop device) | console, original file intact | I/O error handling during write; Post-write verification |
| CELL-IO-07 | cell-io-safety | modify two cells, one read-only; `crash save`; inspect the file | backup file bytes | Crash backup file format; Crash backup policy |
| CELL-IO-08 | cell-io-safety | kill -9 a session after `crash save`; start a new session and run `crash recover` (answer read) | console, db dump, backup file removed, `cellname flags` (modified?) | Crash recovery; Backup and archive reading |
| CELL-IO-09 | cell-io-safety | `archive write lib.mag`, `archive writeall all.mag`, `archive read lib`, `archive readref lib` | archive bytes, db dump after each read | Archive files; Backup and archive reading |
| CELL-IO-10 | cell-io-safety | backup file whose location contains `.magic/`; and a location longer than 256 characters | console | Backup and archive reading |
| CELL-TS-01 | cell-timestamps | hierarchy top→mid→leaf; edit leaf; `writeall force` | stamps of all three files; `cellname flags` before and after | Generating new stamps; Marking cells for a new stamp |
| CELL-TS-02 | cell-timestamps | edit leaf alone (out of context), save; then load top | console (`Processing timestamp mismatches`, list), DRC check areas | Timestamps on cell read; Deferred mismatch processing |
| CELL-TS-03 | cell-timestamps | two parents with different use timestamps for the same unread child | `cellname timestamp child` after each read | Timestamps on instance read |
| CELL-TS-04 | cell-timestamps | file with `timestamp 0` | console (zero timestamp warning), flags | Timestamps on cell read |
| CELL-TS-05 | cell-timestamps | `cellname timestamp a 5`, modify `a`, save | file stamp, `cellname flags` | Fixed timestamps |
| CELL-TS-06 | cell-timestamps | LEF-imported macro used in a parent; modify the parent; save all | stamps (LEF cell unchanged, parent new) | Fixed timestamps; Generating new stamps |
| CELL-TS-07 | cell-timestamps | `gds readonly true` cell (vendor GDS) used by a parent; edit the parent; save | stamps (vendor stamp from the GDS hash) | Generating new stamps |
| CELL-TS-08 | cell-timestamps | read-only child pending a stamp (via DRC) under a modified parent; save | stamps | Generating new stamps |

## Authoring status (M1)

Case directories under `cases/` hold the inputs; `expected/` comes from the CI capture (golden-capture.yml).
Generated by `tools/golden/authoring/m1_cases.py` (42): CELL-RD-01, CELL-RD-02, CELL-RD-03, CELL-RD-04, CELL-RD-05, CELL-RD-06, CELL-RD-07, CELL-RD-08, CELL-RD-09, CELL-RD-10, CELL-RD-11, CELL-RD-12, CELL-RD-13, CELL-RD-14, CELL-RD-15, CELL-RD-16, CELL-RD-17, CELL-RD-18, CELL-RD-19, CELL-RD-20, CELL-RD-21, CELL-RD-22, CELL-WR-01, CELL-WR-02, CELL-WR-03, CELL-WR-04, CELL-WR-05, CELL-WR-06, CELL-WR-07, CELL-WR-08, CELL-WR-09, CELL-WR-10, CELL-WR-11, CELL-PR-01, CELL-PR-08, CELL-PR-09, CELL-PR-10, CELL-PR-11, CELL-PW-01, CELL-PW-03, CELL-LIB-03, CELL-TS-04.

Generated by `tools/golden/authoring/m3_cases.py` (M3, 5): CELL-TS-01, CELL-TS-02, CELL-TS-03, CELL-TS-05, CELL-TS-08. CELL-RD-10 (m1_cases.py) gains `drc why` over its error tile in M3.

Not authored (reason):

- CELL-PR-02: needs HOME=/home/alice and absolute paths
- CELL-PR-03: needs a Tcl variable (PDK_ROOT)
- CELL-PR-04: needs absolute /a and /b directories
- CELL-PR-05: needs absolute /b
- CELL-PR-06: needs symbolic links in input/
- CELL-PR-07: needs a git work tree in the work dir
- CELL-PR-12: needs Tcl cd
- CELL-PW-02: needs HOME
- CELL-PW-04: needs HOME and /tmp
- CELL-PW-05: needs a Tcl variable (PDK_ROOT)
- CELL-PW-06: needs symbolic links
- CELL-LIB-01: needs PDK_ROOT in the environment
- CELL-LIB-02: needs PDK_ROOT in the environment
- CELL-IO-01..10: need file modes/owners, two processes, strace, full filesystems, crash/archive commands
- CELL-TS-06: needs lef read (LEF/DEF, M6)
- CELL-TS-07: needs gds readonly (GDS, M5)
