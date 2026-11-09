# Golden Test Cases — UI area

To be captured later by running Magic 8.3.694 (commit fc5daa9e) headless. Unless stated otherwise, every case uses:
- the Tcl build
- `magic -dnull -noconsole -norcfile -T scmos`
- the 512×484 null screen
- one initial layout window
- commands fed on stdin

"Log" means the text written to stdout/stderr. A case marked *interactive* needs the X11 build with a scripted event injector such as `xdotool`. Cases about style files may run without a display, using a test harness that only calls the file readers, or may compare Magic's error output.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| UI-G-001 | ui-command-dispatch | `getc nonexist` and `getcell nonexist` | log; confirm both run the same command | Command name lookup |
| UI-G-002 | ui-command-dispatch | `ext` vs `extr`, plus an ambiguous prefix `e` | log (`That command abbreviation is ambiguous.`) | Command name lookup; Command dispatch across global and client tables |
| UI-G-003 | ui-command-dispatch | `foo` in a layout window | full log of the unknown-command message (no "Did you point…" line) | Command dispatch across global and client tables |
| UI-G-004 | ui-command-dispatch | `ZoOm 2`, then `view get` | view rectangle | Command name lookup |
| UI-G-005 | ui-command-dispatch | `help`, `help window`, `help wizard` in layout and color windows | full listing text | help command |
| UI-G-006 | ui-command-dispatch | non-Tcl build: `:echo a;b ; echo "c;d" ; echo 'x y'` | printed words | Command line splitting and quoting |
| UI-G-007 | ui-command-dispatch | non-Tcl build: one command with 199, 200 and 201 words (`echo w1 … wN`) | which ones run; messages | Command line splitting and quoting |
| UI-G-008 | ui-command-dispatch | non-Tcl build: a 3000-character `echo` line, then a second command after `;` past byte 2048 | output length; whether the second command ran | Command line splitting and quoting |
| UI-G-009 | ui-command-dispatch | `macro list` and `macro help` with the default system `.magicrc` (box tool) | full list | macro and imacro commands; Default key bindings |
| UI-G-010 | ui-command-dispatch | `macro z`, `macro z ""`, `macro z` | outputs | macro and imacro commands; Macro tables |
| UI-G-011 | ui-command-dispatch | `macro ^R`, `macro Control_r`, `macro Shift_a "box"`, `macro A` | outputs (same key) | Macro key names |
| UI-G-012 | ui-command-dispatch | `tool wiring ; macro Button1 ; tool box ; macro Button1` | outputs per tool | Macro tables |
| UI-G-013 | ui-command-dispatch | `macro copy mytool ; tool mytool ; macro list` | list contents; tool registered | macro and imacro commands |
| UI-G-014 | ui-command-dispatch | `macro wind3d L "level up"` in a build without 3-D | no output | macro and imacro commands |
| UI-G-015 | ui-command-dispatch | *interactive* X11: type `:paint m1`, press Return, then `.` | resulting paint; `macro .` contents | Interactive keystroke handling |
| UI-G-016 | ui-command-dispatch | `logcommands log.txt ; box 0 0 10 10 ; label "a b" n ; setpoint 1 1 ; logcommands stop` in Tcl and non-Tcl builds | log.txt byte-exact | Command logging |
| UI-G-017 | ui-command-dispatch | `logcommands log.txt update ; box 0 0 1 1 ; logcommands` | log.txt (`updatedisplay` lines) | Command logging |
| UI-G-018 | ui-command-dispatch | `setpoint` (no args) in the null display after `setpoint 100 100 0` | printed result | pushbutton and setpoint |
| UI-G-019 | ui-command-dispatch | `setpoint 120 100 0 ; pushbutton left down ; pushbutton left up ; box` (box tool) | box values | pushbutton and setpoint; Box tool buttons |
| UI-G-020 | ui-command-dispatch | non-Tcl: `echo -n a b`, `echo a b`, `sleep 1` timing, `sleep 3` timing | exact output bytes; elapsed times | sleep, echo and send |
| UI-G-021 | ui-command-dispatch | non-Tcl `source s.mag-cmds` with comments, `\` continuation, a leading `:` line, and a line `x;box 0 0 1 1` | resulting box; argv of continued line (via `*winddebug`) | Script files and source |
| UI-G-022 | ui-command-dispatch | non-Tcl: `send color color 3`, then `windownames all` | output and which window received the command | sleep, echo and send |
| UI-G-023 | ui-windows-and-views | at startup, `view get ; windowpositions ; windownames all` | rectangles; window ids | View mapping; windowpositions and windownames |
| UI-G-024 | ui-windows-and-views | `load` a cell with bbox (0,0)-(100,50), then `view get` | surface area | Loading a cell into a window; View mapping |
| UI-G-025 | ui-windows-and-views | the same cell: `view ; view get ; zoom 2 ; view get ; zoom .5 ; view get` | rectangles | view, xview and zoom |
| UI-G-026 | ui-windows-and-views | `zoom 0`, `zoom 20`, `zoom 25`, `zoom -1`, `zoom` | messages | view, xview and zoom |
| UI-G-027 | ui-windows-and-views | `scroll r .1 w ; view get ; scroll n ; view get ; scroll e 10 ; view get ; scroll x 1 w` | rectangles; messages | scroll |
| UI-G-028 | ui-windows-and-views | `center 50 50 ; view get ; center h 0.25 ; view get ; center 1.5 2` | rectangles; error | center and findbox |
| UI-G-029 | ui-windows-and-views | `box 0 0 10 10 ; findbox zoom ; view get ; findbox ; view get` | rectangles | center and findbox |
| UI-G-030 | ui-windows-and-views | `openwindow` ×31 | error after the limit; ids | Window identity and limits |
| UI-G-031 | ui-windows-and-views | `openwindow ; closewindow ; openwindow ; windownames all` | id reuse | Window identity and limits; Opening and closing windows |
| UI-G-032 | ui-windows-and-views | `windowcaption`, `windowcaption off ; openwindow`, `windowborder`, `windowscrollbars on` | messages; caption state of the new window | Window chrome flags |
| UI-G-033 | ui-windows-and-views | cells `top` (contains `inv`), `edit` into `inv`; two windows: `top` and `nand2` | `windowcaption` result in each window | Window captions |
| UI-G-034 | ui-windows-and-views | a 200-character cell name; `load` it | caption text (ellipsis) | Window captions |
| UI-G-035 | ui-windows-and-views | `specialopen *window ; specialopen foo ; specialopen ; specialopen 1 2 layout ; specialopen la c ; windownames all` (`specialopen color 12` waits for ui-color-window, M2) | usage text; window list | specialopen, over, underneath and grow |
| UI-G-036 | ui-windows-and-views | `expand` in window 0 only, with two windows on `top`; then the `what`/`select` behavior per window | expansion bits per window | Per-window expansion state |
| UI-G-037 | ui-windows-and-views | `expand t`, `expand toggle`, `unexpand`, `xview` | expansion state | Per-window expansion state; view, xview and zoom |
| UI-G-038 | ui-windows-and-views | `see no metal1 ; see allSame ; see no allSame ; see no a b c` | messages; window flags (via a screenshot in X11) | Layer visibility (see) |
| UI-G-039 | ui-windows-and-views | `grid 10 20 5 0 ; grid box ; grid state ; grid ; grid state ; grid 0 ; grid -1 ; grid 1 2 3` | messages | Grid display and grid command |
| UI-G-040 | ui-windows-and-views | `grid multiple 300 ; grid multiple` | message; stored value | Grid display and grid command |
| UI-G-041 | ui-windows-and-views | `crosshair 100 100` with windows on `top`, `top` and `inv` (X11 screenshot) | which windows show it | Crosshair |
| UI-G-042 | ui-windows-and-views | X11 OpenGL screenshot of an scmos test cell: edit cell with an expanded child, ports with n/e directions, a rendered-font label at 45°, an unexpanded instance, grid on | PNG for visual diff | Redisplay order; Label display; Subcell display; Grid display |
| UI-G-043 | ui-windows-and-views | the same layout with `see allSame` and with the window rooted at the child (not edit) | PNG | Redisplay order for layout windows |
| UI-G-044 | ui-windows-and-views | DRC errors in the root and in a child (expanded) | PNG; confirm child errors not shown | DRC error display |
| UI-G-045 | ui-windows-and-views | box drawings: a point box, a 3-pixel-wide box, a box outside the edit bbox | PNG | Box display |
| UI-G-046 | ui-tools-and-input | `tool ; tool ; tool ; tool ; tool foo ; tool w ; tool info ; tool type` (non-Tcl) | messages; current tool | tool command; Tool registry |
| UI-G-047 | ui-tools-and-input | *interactive* X11: box at (0,0)-(10,10); left click at (20,30); right click at (25,5); left+right corner-move sequence | box values after each | Box tool buttons |
| UI-G-048 | ui-tools-and-input | `cursor`, `cursor lambda`, `cursor window`, `cursor 22`, `cursor bogus` | outputs | cursor command |
| UI-G-049 | ui-tools-and-input | the generated system `.magicrc` (new macros, Tcl and non-Tcl variants) | full `macro list` per tool and client | Default key bindings |
| UI-G-050 | display-style-files | load `mos.OpenGL.dstyle`, `mos.24bit.dstyle` and `mos.7bit.dstyle`; dump the style table (index, mask, color, outline, fill, stipple, short and long name, num) | table dump | Style table model; Dstyle file grammar; Dstyle style-line field semantics |
| UI-G-051 | display-style-files | mutated dstyle files: 51 display styles; pale count ≠ layout count; unknown section; `layout_styles` after `pale_styles`; bad fill; color by name; `version` without a number | error messages; load result; exit status | Dstyle section count validation; Dstyle file grammar |
| UI-G-052 | display-style-files | only `mos.OpenGL.dstyle5` present | messages | Dstyle file name and lookup |
| UI-G-053 | display-style-files | load `mos.OpenGL.std.cmap` and `mos.7bit.std.cmap`; dump all entries | 256 RGB+name entries | Color map file grammar |
| UI-G-054 | display-style-files | mutated cmaps: a blank line, out-of-order, short file, only `.cmap1` present | messages; result | Color map file grammar |
| UI-G-055 | display-style-files | load a cmap and save it via the color window `save t d m` | written file byte-exact | Color map file writing; Color window commands |
| UI-G-056 | display-style-files | load `color.glyphs`, `bw.glyphs` and `DEFAULT.glyphs`; dump sizes, origins and pixel style indices | dump | Glyph file grammar; Cursor glyph indices |
| UI-G-057 | display-style-files | mutated glyphs: missing rows, extra rows, a whitespace cell, a short line, no size line | messages; result | Glyph file grammar |
| UI-G-058 | display-style-files | tech `styles` with numeric and named references, an invalid style name, and a contact with stacked types | type→style bindings; messages | Style loading triggered by styletype |
| UI-G-059 | display-style-files | X11 and OpenGL screenshots of a full-window fill in each stipple (1–16) | PNGs (orientation) | Stipple patterns |
| UI-G-060 | ui-color-window | `specialopen color 47 ; color ; color get ; color rgb ; color next ; color 0x1ff ; color last` | outputs; caption | Color window creation and layout; Color window commands |
| UI-G-061 | ui-color-window | `specialopen color 5 ; setpoint` at the red bar midpoint, `pushbutton left`, then `color rgb ; undo ; color rgb` | RGB values | Color window buttons; Color component update |
| UI-G-062 | ui-color-window | pumps: left and right press on the Hue `+` pump for color (184,73,83) | resulting RGB | Color component update; RGB/HSV conversion |
| UI-G-063 | ui-feedback-and-highlights | `box 0 0 4 4 ; feedback add "a" ; feedback add "b" medium 0 0 10 10 ; feedback add "c" outline 1 1 ; feedback count ; feedback find ; feedback find ; feedback find 5 ; feedback why` | outputs; box after each find | feedback command; Feedback area model |
| UI-G-064 | ui-feedback-and-highlights | `feedback add "p" solid 0 0 10 0 10 10 0 10 ; feedback add "t" pale 0 0 10 0 0 10` | count; areas via `feedback find` | feedback command |
| UI-G-065 | ui-feedback-and-highlights | the areas above plus `feedback save f.txt` (text with quotes) | f.txt byte-exact | feedback command |
| UI-G-066 | ui-feedback-and-highlights | `feedback clear b ; feedback count ; feedback clear ; feedback count` | outputs | Clearing feedback |
| UI-G-067 | ui-feedback-and-highlights | `feedback add x 5 1 2` (numeric style), `feedback add x bogus`, `feedback add x solid_highlights` | messages | feedback command |
| UI-G-068 | ui-feedback-and-highlights | extract a cell with a known extraction warning, then `feedback count ; feedback find` | count; text | Feedback producers |
| UI-G-069 | app-startup | `magic -x`, `magic -n`, `magic -d FOO`, `magic -T` (missing argument), `magic -T -d` | stdout/stderr; exit status | Command-line options; Display type selection |
| UI-G-070 | app-startup | `magic -dnull -noconsole` with no tech and no files; `tech name` | technology name (`minimum`) | Default technology selection |
| UI-G-071 | app-startup | `magic -dnull -noconsole inv.mag`, where inv.mag has `tech scmos` | technology; loaded cell | Default technology selection; Command-line file arguments |
| UI-G-072 | app-startup | `magic -dnull -noconsole -T scmos` with `./.magicrc` doing `tech load minimum` | technology | Startup file order |
| UI-G-073 | app-startup | startup files that print markers: system `.magicrc`, `~/.magicrc`, `./.magicrc`; run from HOME and from another dir, Tcl and non-Tcl | marker order | Startup file order |
| UI-G-074 | app-startup | `-rcfile my.rc` and `-norcfile` variants; a missing `-rcfile` file | markers; messages | Startup file order; Command-line options |
| UI-G-075 | app-startup | `magic -dnull -noconsole a b` (two cells), then `cellname list window` and the loading messages | order; displayed cell | Command-line file arguments |
| UI-G-076 | app-startup | `quit 3 -noprompt`, `quit 300 -noprompt`, `quit x`, `quit 1 2` | exit statuses; messages | quit command and exit status |
| UI-G-077 | app-startup | `--version`, `--prefix`, `--commit` | stdout; exit 0 | Tcl launcher options |
| UI-G-078 | app-startup | non-Tcl: stdin EOF without quit | message; exit status 1 | Batch and headless operation |
| UI-G-079 | app-startup | create a fake `$TMPDIR/MAG99999.xyz` backup (dead pid) and a live-pid one; run `magic -r` | offered file; prompt text | Crash recovery at startup |
| UI-G-080 | app-startup | `version` | output format | version command |
| UI-G-081 | ui-wizard-and-debug | `*coord` with a box set; `*winddebug` twice; `*files` | outputs | Layout wizard commands; Window-client wizard commands |
| UI-G-082 | ui-wizard-and-debug | X11 screenshot with `*watch metal1`, `*watch metal1 types`, `*watch metal1 demo` | PNGs | Tile structure overlay |

## Authoring status (M1)

Case directories under `cases/` hold the inputs; `expected/` comes from the CI capture (golden-capture.yml).
Generated by `tools/golden/authoring/m1_cases.py` (15): UI-G-023, UI-G-024, UI-G-025, UI-G-026, UI-G-027, UI-G-028, UI-G-029, UI-G-030, UI-G-031, UI-G-032, UI-G-034, UI-G-037, UI-G-038, UI-G-039, UI-G-040.

Not authored (reason):

- UI-G-033: needs edit (edit cell inside a child) and per-window command routing
- UI-G-036: needs what/select per window
- UI-G-041..045: X11/OpenGL screenshots
- UI-G-050..059: style-file dumps need a reader harness (no command)
- Other UI rows belong to capabilities outside M1 viewer scope (design D8).
