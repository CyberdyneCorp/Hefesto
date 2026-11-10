# Golden Test Cases: TECH area

These cases are to be captured later by running Magic 8.3.694 (commit fc5daa9e) headless, with `magic -dnull -noconsole`. `tiny.tech` is the valid example file in capability `tech-file-format`, requirement "Complete annotated example". Unless stated otherwise, every case starts from a fresh session with `tech load tiny -noprompt`.

The paint and erase table dumps can be captured with a small Tcl helper that loops over planes, h and t. The helper paints t over h on a 1×1 box in a scratch cell and reads back the type with `what` or `select`. Another way is to run magic's internal `*showtech`-style debug output with a debug build. Record which method was used.

Cases whose load fails (G01, G04, G05, G06, G07, G10, G12) capture only the error output and the load result. They do not query the technology afterwards: Magic keeps the partly loaded tables and Hefesto keeps the previous technology (atomic load, Q-TECH-10), a documented deviation.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| TECH-G01 | tech-file-format | tiny.tech variant with indented `#` comments, mid-line `#` tokens in a connect line, and CRLF endings | load result; connect table; error text | Physical line reading and comment lines; Tokenization and quoting |
| TECH-G02 | tech-file-format | tiny.tech with a continuation `metal\`+`1` inside `types`, and a comment line inside a continuation | type names table (`tech layers`) | Line continuation |
| TECH-G03 | tech-file-format | `version` section with `description "a b"` and `version 1.0 beta` | `tech version` result (exact string, including LF) | Tokenization and quoting; version section |
| TECH-G04 | tech-file-format | tiny.tech with the header `drc # x` | error output; load result | Section structure |
| TECH-G05 | tech-file-format | tiny.tech with a stray `end` before `planes` | error output; load result (expect failure: planes missing) | Section structure; Section registry and prerequisites |
| TECH-G06 | tech-file-format | tiny.tech with `types` placed before `planes` | full error text, including the prerequisite listing | Section registry and prerequisites |
| TECH-G07 | tech-file-format | tiny.tech with a `connect` line `m1` (1 token) | error lines; list of sections with errors; `tech load` message | Error reporting and recovery |
| TECH-G08 | tech-file-format | tiny.tech split into `tiny.tech` + `tiny-drc.tech` (include in the same dir, run from another cwd); also `include common.inc` with only `common.inc` present | which files are opened (strace or error output); load result | include directive |
| TECH-G09 | tech-file-format | tiny.tech with a missing include inside `connect` | warning text; subsequent error; load result | include directive (Q-TECH-2) |
| TECH-G10 | tech-file-format | `repeat 3` / `m1 m1` / `endrepeat` in connect, plus `repeat 0` and `repeat x` variants | count of handler invocations (add a deliberate error line inside the block and count messages) | repeat directive |
| TECH-G11 | tech-file-format | `tech` sections: `format 34`, `format 33`, `formatX 40`, `format abc`, `a b c`, none | `extract` fringehalo state after load (`extract help`/option query); messages | tech section; Format version gating |
| TECH-G12 | tech-file-format | `requires 8.3.700`, `requires magic-8.2.1`, `requires 8.3` | load result and message text | version section |
| TECH-G13 | tech-layers-and-planes | tiny.tech | `tech planes`; `tech layers`; `tech layers *` | Built-in planes; planes section; types section; Built-in tile types |
| TECH-G14 | tech-layers-and-planes | tiny.tech; Tcl `magic::tech layers <x>` and `paint <x>` in a scratch cell for x in {p, po, poly, polys, ndif, m, cp, CP, EP, f, pc/metal1, poly/metal1} | resolved type, or error text | Name lookup by unique prefix; Single type-name resolution |
| TECH-G15 | tech-layers-and-planes | tiny.tech; `tech layers` on the lists `m1`, `pc`, `*m1`, `~(pc,ndc)/active`, `0`, `0,poly`, `(poly,ndiff`, `allm1` | returned list (stacked types are not listed, so also capture the selection by painting and `what`) | Type-list language; Type aliases |
| TECH-G16 | tech-layers-and-planes | tiny.tech plus aliases `diff ndiff`, `poly ndiff`, `allm1 *m1` twice, and types-section `alias x *m1` | messages; `tech layers` output; resolution of `diff` | Type aliases |
| TECH-G17 | tech-layers-and-planes | types line with an unknown plane `metal3 metal3,m3`, followed by a valid line `metal2 foo` | resolution of `m3` after the load (Q-TECH-11); errors | types section |
| TECH-G18 | tech-contacts-and-stacking | tiny.tech with `stackable` (no args) instead of `stackable pc m2c pm12c` | type names of the generated types (via a `.mag` save containing them); `pm12c` resolution; number of types | stackable statement; Stacking eligibility; Stacked type properties |
| TECH-G19 | tech-compose-and-connect | tiny.tech; paint ndiff over a 4×4 pc in a scratch cell | resulting tiles on active and metal1 (`.mag` save) | Finalization step 3a (Q-TECH-23) |
| TECH-G20 | tech-compose-and-connect | tiny.tech; full dump of P[p](h,t) and E[p](h,t) for p in 6..9, h and t in 0..19 | dump table (CSV) | all compose requirements |
| TECH-G21 | tech-compose-and-connect | scmos-like tech with contact `pdc pdiff metal1` and explicit `erase pdc pwell pdiff` | E[active](pdc, pwell) after the load (Q-TECH-20) | Explicit paint and erase rules; Contact locking |
| TECH-G22 | tech-compose-and-connect | tiny.tech; paint m2c over pc; paint pc+m2c on empty space; erase pc+m2c | `.mag` save of the resulting tiles per plane | Finalization step 3a; Finalization step 3c |
| TECH-G23 | tech-compose-and-connect | scmos.tech (from the distribution) | full P/E dump for tech planes | all compose requirements (regression on a real tech) |
| TECH-G24 | tech-compose-and-connect | sky130A.tech (open_pdks) | full P/E dump for tech planes; connect table | all compose and connect requirements |
| TECH-G25 | tech-compose-and-connect | tiny.tech with a contact compose rule `compose pdc pdiff m1` added (with a pdc contact) | P/E entries for pdc, pdiff, m1 on active | Contact compose and decompose rules; Finalization step 4 |
| TECH-G26 | tech-compose-and-connect | tiny.tech; `select` net from a poly box connected to pc, then to m1 | selected net contents (shows connect is not transitive across non-contact types) | connect section; Connectivity finalization |
| TECH-G27 | tech-compose-and-connect | tiny.tech; `tech locked pc`; erase metal1 over a pc tile | resulting tiles | Contact locking; tech locked |
| TECH-G28 | tech-compose-and-connect | load tiny.tech, then a second tech with a contact compose rule, then tiny.tech again | P/E dump after the third load compared with TECH-G20 (Q-TECH-19) | Contact compose and decompose rules |
| TECH-G29 | tech-styles-section | tiny.tech with `nfet 6 7`, `poly 1 nosuchstyle 20`, `m2c 20 21 33`, run in a graphics-enabled session | style-to-type mapping (via the debug `*showtech` output or a rendered image) | Type to style mapping; Contact style propagation |
| TECH-G30 | tech-styles-section | tiny.tech with a `styletype` naming a family that does not exist, in a `-dnull` session and in a graphics session | load result; messages | styletype statement (Q-TECH-24) |
| TECH-G31 | tech-loading-and-scaling | files `SCN4M_SUBM.20.tech`, `tiny.tech`, `old.tech27`; `tech load SCN4M_SUBM.20`, `tech load tiny.tech`, `tech load old`, `tech load nothere` | `tech filename`; error text | Technology file name resolution |
| TECH-G32 | tech-loading-and-scaling | a tech file beginning with `version` | `tech load` message; current tech unchanged | Readability pre-check |
| TECH-G33 | tech-loading-and-scaling | tiny.tech with a broken connect line, loaded after scmos | `tech name`; `tech filename`; `tech layers` (shows the partial state, Q-TECH-10) | Full technology load |
| TECH-G34 | tech-loading-and-scaling | cell with metal2 paint; reload a tech with fewer planes | messages; saved `.mag` contents | Effect on existing layout |
| TECH-G35 | tech-loading-and-scaling | `scalegrid 1 2`, then `tech load tiny -noprompt` (with a drc width rule of 3) | printed `Scaled tech values` line; `tech drc width metal1` in internal units | Post-load adjustments; Internal grid scale |
| TECH-G36 | tech-loading-and-scaling | box (0,0)-(3,5) and a tile edge at x = −3; `scalegrid 1 2`; then `scalegrid 2 1`; also `scalegrid 2/4`, `scalegrid 1:3`, `scalegrid 0 2` | box coordinates; tile coordinates (`.mag`); printed report lines | scalegrid command |
| TECH-G37 | tech-loading-and-scaling | cifoutput with `scalefactor 5`, `gridlimit 5`; `scalegrid 1 2`, `scalegrid 1 20` | accept/refuse messages | Grid limit check |
| TECH-G38 | tech-loading-and-scaling | cifoutput with `options set-minimum-grid` | `tech lambda` after the load | Post-load adjustments |
| TECH-G39 | tech-loading-and-scaling | startup `magic -T tiny` with a `.magicrc` containing `tech load scmos`; and startup with an `inv.mag` argument (`tech tiny`) and no -T | `tech name` after startup | Startup technology selection |
| TECH-G40 | tech-loading-and-scaling | `PDK_ROOT` pointing at a test tree with `sky130A/libs.tech/magic/sky130A.tech`; `load` a cell with `tech sky130A` from the minimum tech | messages; `tech filename`; `path cell` | Automatic load from a cell's technology |
| TECH-G41 | tech-command | `tech`, `tech help`, `tech la`, `tech foo`, `tech name extra` | console output | Option parsing; tech help |
| TECH-G42 | tech-command | tiny.tech; `tech lambda`, `tech lambda 2`, `tech lambda 3 2` (Tcl and non-Tcl builds) | results; printed lines | tech lambda (Q-TECH-26) |
| TECH-G43 | tech-command | tiny.tech; `tech locked`, `tech locked metal1`, `tech unlocked pwell`, `tech revert *`, `tech revert` | each listing | tech locked, unlocked and revert (Q-TECH-25) |
| TECH-G44 | tech-command | scmos.tech; `tech drc width m1`, `tech drc spacing m1`, `tech drc surround m2c m1`, `tech drc direc m2c m1`, `tech drc surround m1` | returned values; messages | tech drc |
| TECH-G45 | tech-command | painted layout; `tech load tiny` answered `no`; `tech load tiny -quiet`; `tech load tiny -noprompt` | dialog presence; usage text; result | tech load |

## Authoring status (M1, M2)

Case directories under `cases/` hold the inputs; `expected/` comes from the CI capture (golden-capture.yml).
Generated by `tools/golden/authoring/m1_cases.py` (21): TECH-G08, TECH-G09, TECH-G11, TECH-G18, TECH-G19, TECH-G20, TECH-G21, TECH-G22, TECH-G23, TECH-G24, TECH-G25, TECH-G26, TECH-G27, TECH-G28, TECH-G30, TECH-G34, TECH-G35, TECH-G36, TECH-G37, TECH-G38, TECH-G45.
Hand-written earlier (19): G01-G07, G10, G12-G17, G31, G32, G41-G43.

Generated by `tools/golden/authoring/m3_cases.py` (M3, 1): TECH-G44.

Not authored (reason):

- TECH-G29: needs a graphics session (style mapping)
- TECH-G33: partial state after a failed load differs by design (Q-TECH-10)
- TECH-G39: startup .magicrc scripts do not run in hefesto-cli yet (app-startup, 4.3 deviation)
- TECH-G40: needs PDK_ROOT in the environment of both tools
