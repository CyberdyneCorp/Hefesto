# Open questions: LEFDEF (LEF/DEF input and output)

Each entry records Magic behavior (commit fc5daa9e) that looks accidental, ambiguous or platform-specific. The specs describe Magic's actual behavior and point to these IDs. ID ranges: 1-29 tech section and LEF read, 30-59 LEF write, 60-89 DEF read, 90-119 DEF write. Gaps in the numbering are intentional.

### Q-LEFDEF-1: Route width and spacing defaults are effectively never filled
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Route width and spacing are set to the sentinel -1 when DRC rules exist. Contact areas start at (-1,-1,-1,-1). The fill step runs only when the internal grid is not 1:1, and it runs after the sentinel has been scaled by n/d. A scaled sentinel generally no longer equals -1: it becomes -n/d when n > d and 0 when d > n, so it is not filled. With a 1:1 grid the fill step never runs. In practice the route width stays negative or zero unless a LEF LAYER section sets WIDTH. `def read` then falls back to that value for default wire widths, and `lef write -tech` omits WIDTH. Contact areas stay null, so the `contact` keyword behaves like `cut`, except on shrinking grids where the fill step does run.
- Evidence: lef/lefTech.c:350-368, 477-516, utils/tech.c:817-830, lef/defRead.c:198-216 (corrected 2026-11-13: Behaviour confirmed with corrections (Magic 8.3.694; lef/ unchanged between fc5daa9e and HEAD 9c83a9a6). Sentinels are set at lef/lefTech.c:340-366. LefTechScale (lefTech.c:416-462) scales route width and spacing with C truncation (w*n/d). So -1 stays -1 when d < n < 2d, which means the fill DOES run for ratios such as 3/2. It becomes <= -2 when n >= 2d (the spec's 2:1 scenario gives -2). It becomes 0, not -1, when n < d. Contact areas go through DBScalePoint (database/DBcellsrch.c:1457-1509), which floors, so they stay -1 and are filled only when n < d (the entry is right here). The fill runs only at utils/tech.c:826-829. `scalegrid` (commands/CmdRS.c:339), DBio.c:804 and CIFrdutils.c:249/284 rescale without refilling. Consequence the entry misses: in def read, a route width <= 0 (defRead.c:198-217, 366-392, 519-521) builds an inverted or empty rectangle (defRead.c:644-671), and DBPaintPlane drops it (database/DBpaint.c:266-267). So with a 1:1 grid and no prior `lef read` setting WIDTH, every regular-net wire, and every special-net wire with width 0, is silently not painted. lef write -tech (lefWrite.c:419-432) then omits WIDTH, SPACING and DIRECTION.)
- Options: replicate | fix (fill width, spacing and contact area from the DRC defaults after every tech load and grid change) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, with the DEF-read part fixed in both modes (X-2 tier 1, data loss). Hefesto keeps an 'unset' flag on route width and spacing and on the contact area instead of a numeric sentinel. With magic-compat ON, everything that is visible in Magic's own output stays as Magic does it: the scaled sentinel values, the fill only in the grid cases above, lef write -tech omitting WIDTH, SPACING and DIRECTION for unset routes, and unfilled contact-flavor cuts behaving like `cut` (no cut resizing). With it OFF, width, spacing and contact area are taken from the DRC defaults whenever they are unset, after every tech load and grid change (scalegrid included), and lef write -tech prints them. In BOTH modes, def read uses the DRC default width, or DEFAULT_WIDTH when there is no rule, wherever Magic would use a non-positive layer width. Magic paints nothing there, so there is no output to match. This keeps Q-LEFDEF-5 (already resolved: DRC default width and spacing) consistent. Resolve together with Q-LEFDEF-2 and Q-LEFDEF-31. Capture LEFDEF-002/003 to pin the ON values for 1:1, 3:2, 2:1 and shrinking grids. (corrected 2026-11-13, was fix, and record it in the compatibility notes)

### Q-LEFDEF-2: Default spacing written into the width field (read-side consequence)
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: This is the same defect as Q-LEFDEF-31, seen from the reader. When the fill step runs and the spacing is still -1, the DRC default spacing overwrites the route width and the spacing stays -1. `def read` then uses the spacing value as the default wire width.
- Evidence: lef/lefTech.c:510-512
- Verified 2026-11-13: Confirmed at lef/lefTech.c:510-512: `if (spacing == -1) lefl->info.route.width = DRCGetDefaultLayerSpacing(...)`. The fill runs only from utils/tech.c:826-829 (grid not 1:1). Because of the truncating scale (see Q-LEFDEF-1), spacing is still -1 at that point only when d < n < 2d. So this happens only for those grid ratios, not on 1:1 grids. def read then uses the spacing value as the default wire width (defRead.c:198-217). The wire is painted, but at the wrong width.
- Options: replicate | fix (assign to spacing) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, the same defect as Q-LEFDEF-31. With magic-compat ON, when the post-scale fill runs and spacing is still -1, the DRC default spacing overwrites the width and the spacing stays -1, as Magic does. def read then paints default wires at that width. With it OFF, the default spacing goes into the spacing field, and the width keeps its own default (Q-LEFDEF-1 OFF). Nothing is lost: the wire is painted at a wrong but positive width. That is an output-value quirk (tier 2), not tier 1. (corrected 2026-11-13, was fix together with Q-LEFDEF-1 and Q-LEFDEF-31)

### Q-LEFDEF-3: Secondary obstruction type and contact area carry over between LAYER statements
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: In a PORT or OBS block, the secondary obstruction type is updated only when a LAYER names a VIA-class entry that has an obstruction type. A later LAYER that names a routing layer keeps the previous secondary type, so its OBS rectangles also paint that via-obstruction type. Likewise, a LAYER that is resolved through the magic-name fallback does not reset the contact area, so a cut-size override from an earlier LAYER can be applied to it.
- Evidence: lef/lefRead.c:718-748, 1176-1224 (corrected 2026-11-13: Confirmed with a refinement (lef/lefRead.c:697-748 LefReadLayers; use at 1220-1291 and 1328-1331). The secondary type (lreturn/otherlayer) is updated only in obstruct mode, and only for a VIA-class entry whose obsType >= 0 (the dangling else at 720-723). PORT bodies call LefReadLayers with obstruct=FALSE (do_list TRUE), so in PORT the secondary type stays -1, and otherlayer is painted only when !do_list (1288-1289, 1328-1331). The secondary-type carry-over therefore affects OBS only. The contact-area carry-over affects both PORT and OBS. A name resolved through the magic-name fallback (742-750) never touches *rreturn. A later RECT on a contact type then uses the previous LAYER's cut size (1273-1286).)
- Options: replicate | fix (reset both at every LAYER statement) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto keeps both carry-overs as Magic has them. In OBS, a later routing-layer LAYER keeps the previous via's secondary obstruction type and paints it too. In PORT and OBS, a fallback-resolved contact layer reuses the previous cut-size override. With it OFF, every LAYER statement resets the secondary type to -1 and the contact area to null before resolution, so each layer paints only its own material at the file's size. The extra material changes output values but loses no data, so this is tier 2 under the standing rule. The previous 'fix' is downgraded to compat. (corrected 2026-11-13, was fix)

### Q-LEFDEF-4: Table names without an entry crash the reader
- Kind: bug
- Magic behavior: Some operations leave a name in the LEF table with a null value: generated-via removal after `def write`, a NONDEFAULTRULE `USEVIA` or `LAYER` lookup of an unknown name, and the DEF equivalents. A later geometry LAYER or a LEF LAYER section that names it then dereferences null and crashes.
- Evidence: lef/lefTech.c:164-171, lef/lefRead.c:732-741, 1960-1963, 3138-3148
- Options: replicate (impossible: crash) | fix (treat as absent)
- Recommendation: Fix: treat as absent, as the spec states.
- Resolution: fix — a table name with no entry is treated as absent (NULL dereference) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-5: New ROUTE entries from LEF LAYER sections have undefined parameters
- Kind: bug
- Magic behavior: A LEF `LAYER` whose name matches a magic routing type but is not in the tech `lef` section creates a ROUTE entry without initializing width, spacing, pitch or direction (uninitialized memory). Any value not set by the section is garbage for DEF read and LEF write.
- Evidence: lef/lefRead.c:3128-3137
- Options: replicate (impossible) | fix (initialize like a tech `routing` line: DRC defaults, pitch 0, horizontal)
- Recommendation: Fix: initialize with pitch 0, horizontal direction, and width and spacing as in Q-LEFDEF-1.
- Resolution: fix — a ROUTE entry created from a LEF LAYER section is initialised like a tech `routing` line: DRC default width and spacing, pitch 0, horizontal (uninitialised memory) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-6: Last SPACING statement in a LEF LAYER wins
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Every `SPACING` statement overwrites the stored spacing with its first number, ignoring RANGE, ENDOFLINE, PARALLELRUNLENGTH and similar qualifiers. The last statement wins, even when it describes a wide-wire rule.
- Evidence: lef/lefRead.c:2829-2840
- Verified 2026-11-13: Confirmed at lef/lefRead.c:2833-2840. LEF_LAYER_SPACING reads only the first token as a float, stores round(v/oscale) for ROUTE class, and LefEndStatement skips RANGE, ENDOFLINE, PARALLELRUNLENGTH and the rest. route.spacing is consumed only by lef write -tech (lefWrite.c:428-430). def read never uses it (grep of lef/*.c), so the effect is limited to -tech output.
- Options: replicate | fix (keep the first plain SPACING, or the minimum) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, every SPACING statement overwrites the stored spacing with its first number, and the last one wins, as Magic does (lef write -tech goldens). With it OFF, the stored spacing is the first value of the first SPACING statement that has no qualifier (nothing between the value and `;`). If the section has none, the smallest first value is used. That is the layer's minimum spacing, which is what a single SPACING line in the -tech output means. Nothing but lef write -tech reads the value. (corrected 2026-11-13, was replicate)

### Q-LEFDEF-7: Unsupported LEF geometry constructs
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: LEF 5.6+ generated via definitions (`VIA name ... VIARULE ... CUTSIZE ... ROWCOL ... END`) produce no geometry. `RECT MASK n`, `RECT ITERATE`, `POLYGON MASK`, and POLYGON in VIA definitions fail or are skipped. Via definitions are read only from LAYER/RECT pairs.
- Evidence: lef/lefRead.c:2749-2786, 868-936 (corrected 2026-11-13: Confirmed with line corrections. Via sections dispatch at lef/lefRead.c:2895-2915: LAYER through LefReadLayer, RECT through LefAddViaGeometry (2648-2687). VIARULE, CUTSIZE, LAYERS, CUTSPACING, ROWCOL, ORIGIN, OFFSET, PATTERN and POLYGON are not in layer_keys (2742-2777), so they go to the unknown-keyword path. `RECT MASK n ...` and `RECT ITERATE` fail in LefReadRect (867-936) with 'Bad port geometry: RECT requires 4 values.'. So a LEF via defined by a via rule gets no geometry. The parameterised generator already exists for DEF (LefGenViaGeometry, lefRead.c:~2550-2640, used by defRead.c:1996-2003).)
- Options: replicate | fix (support LEF 5.8 via rules and MASK) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto skips these constructs exactly as Magic does: the same messages and no geometry for rule-generated LEF vias. With it OFF, it supports them. A LEF VIA with VIARULE/CUTSIZE/LAYERS/CUTSPACING/ENCLOSURE/ROWCOL (and ORIGIN/OFFSET/PATTERN) is built by the same generator as DEF generated vias (shared with the Q-LEFDEF-62 OFF path). `RECT MASK n` paints the rectangle and ignores the mask number. `RECT ITERATE` expands its DO/STEP array. POLYGON in a VIA is decomposed and added. This follows the M5 precedent for dropped geometry (Q-MASK-7: compat). Magic's skipping changes the read result but is not tier 1. (corrected 2026-11-13, was replicate for compatibility, and track as a future extension)

### Q-LEFDEF-8: Via redefinition corrupts shared entries
- Kind: bug
- Magic behavior: When a redefined via name shares its entry with other names, the original entry's new canonical name is the first hash-table name that differs from the redefined name. That name can belong to an unrelated entry. The new entry's class is left uninitialized. When the entry is not shared, its extra-rectangle list is freed through the via union even if the entry is a ROUTE entry.
- Evidence: lef/lefRead.c:611-680
- Options: replicate (impossible for the uninitialized parts) | fix (choose an alias of the same entry; set the class to VIA)
- Recommendation: Fix.
- Resolution: fix — a redefined shared via keeps an alias of the same entry as its canonical name and the new entry's class is VIA (uninitialised class, free through the wrong union member) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-9: NONDEFAULTRULE duplicate renaming never increments
- Kind: bug
- Magic behavior: A duplicate rule name is renamed to `<name>_0`. If `<name>_0` also exists, the loop repeats `<name>_0` forever (infinite loop).
- Evidence: lef/lefRead.c:1897-1907
- Options: replicate (impossible: hang) | fix (try _0, _1, _2, ...)
- Recommendation: Fix with an incrementing index. The first rename (`_0`) stays as in Magic.
- Resolution: fix — duplicate NONDEFAULTRULE names try `_0`, `_1`, `_2`, … (infinite loop); the first rename stays `_0` as in Magic (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-10: NONDEFAULTRULE statements that leave `;` swallow the next statement
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `HARDSPACING ;`, `USEVIA n ;`, `USEVIARULE n ;`, `MINCUTS a b ;` and `PROPERTY a b ;` leave the `;` unread. The `;` is then treated as an unknown keyword and the statement after it is skipped. A `VIA` whose name already exists has its body parsed as rule statements instead of being skipped.
- Evidence: lef/lefRead.c:1928-1990
- Verified 2026-11-13: Confirmed at lef/lefRead.c:1928-1990. HARDSPACING breaks without reading (the END case even falls through into it, 1929-1937). USEVIA and USEVIARULE fall into LAYER (1960-1966): they read one name and also chain a rule entry for it. MINCUTS and PROPERTY read two tokens (1983-1989). In each case the following `;` hits the unknown-keyword path (1918-1924), whose LefEndStatement swallows the next statement. VIA with an existing name jumps to newrule without reading the body (1942-1958). Damage is bounded: an out-of-context top-level END only reports an error (3186-3192). Example: `HARDSPACING ;` before `LAYER met1 WIDTH 0.28 ;` loses that layer's width, and the layer's `END met1` then ends the whole rule early.
- Options: replicate | fix (consume through `;`; skip the bodies of known VIAs) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto reproduces Magic's token handling exactly (golden LEFDEF-020): the unread `;` is an unknown keyword that swallows the next statement, and the body of a known VIA is parsed as rule statements. With it OFF, every NONDEFAULTRULE statement is consumed through its `;`, and the body of an already-known VIA is skipped up to its `END name`. It is a parse quirk that changes the rule set, not a crash or undefined behaviour, so it is tier 2 under the standing rule. The previous 'fix' is downgraded to compat. (corrected 2026-11-13, was fix)

### Q-LEFDEF-11: `lef read -annotate` still creates new cells
- Kind: doc-mismatch
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The documentation says `-annotate` ignores macros that are not already cells. In the code, the option only stops renamed duplicate macros from being created. Every other unknown macro is created normally. Annotation mode is chosen only when the cell already exists, whether or not `-annotate` is given.
- Evidence: lef/lefRead.c:2127-2158, doc/html/lef.html (corrected 2026-11-13: Confirmed with corrected lines: lef/lefRead.c:2114-2158 (not 2127). doAnnotate is checked only in the duplicate-rename branch (2129-2135). Otherwise any unknown macro is created (2147-2155), and is_imported (annotation mode) is set only when DBCellLookDef finds the cell. doc/html/lef.html:37-39 says that -annotate ignores 'all other macros'. lefCmd.c:156 has the help text 'read a LEF file for cell annotation only'.)
- Options: replicate | fix (with -annotate, skip every macro whose cell does not exist) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, -annotate only suppresses the creation of renamed duplicate macros, and every other unknown macro is created, as Magic does. With it OFF, -annotate skips (LefSkipSection) every macro whose cell does not already exist, as the documentation and help text promise. Without -annotate, both modes choose annotation only because the cell exists. This differs from the earlier 'doc wrong, follow code' (n/a) resolutions such as Q-LEFDEF-16 and -17. Here the documented behaviour is well defined and is the obvious intent, so the standing compat rule applies. (corrected 2026-11-13, was fix to match the documentation)

### Q-LEFDEF-12: ORIGIN shifts geometry but not FIXED_BBOX
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Macro geometry is offset by −ORIGIN (and −FOREIGN point). The stored FIXED_BBOX is (FOREIGN pt) to (FOREIGN pt + SIZE), independent of ORIGIN. With a nonzero ORIGIN, the box and the geometry therefore disagree by ORIGIN. LEF says ORIGIN gives the offset from the macro geometry to the placement origin, so this may be a sign error.
- Evidence: lef/lefRead.c:2214-2228, 2364-2368 (corrected 2026-11-13: Partly wrong. In lef/lefRead.c:2214-2228, 2329-2340 and 2364-2385, geometry is painted at g - ORIGIN - F (F = FOREIGN point) and FIXED_BBOX = (F, F+SIZE). Measured against LEF's rule (geometry + ORIGIN lies in (0,0)-(SIZE)), the geometry is therefore off by 2*ORIGIN, not ORIGIN. The box is off by 2*F with respect to a FOREIGN point. Magic contradicts its own writer: lefWrite.c:1367-1369 writes ORIGIN = -boundary.ll with unshifted geometry and no FOREIGN point. So for a cell whose boundary lower-left B is not (0,0), Magic's write-then-read round trip puts the geometry at g+B inside a box (0,S). Also, ORIGIN takes effect only for PIN and OBS that follow it.)
- Options: replicate | fix (shift the box by the same offset) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto keeps Magic's frame: geometry at g - ORIGIN - F, and FIXED_BBOX (F)-(F+SIZE) (golden LEFDEF-012 and DEF placement goldens). With it OFF, the cell frame is the LEF macro's raw coordinate frame shifted by the FOREIGN point: geometry at g - F, FIXED_BBOX (-ORIGIN-F)-(SIZE-ORIGIN-F). This is LEF's geometry/box relation, and a Magic-written LEF (ORIGIN -B) then reads back to the original cell exactly. Both modes are identical when ORIGIN and F are zero, which covers the usual standard-cell libraries. def-read COMPONENTS placement must align the FIXED_BBOX lower-left with the placement point in OFF mode. Check this when def-read placement (Q-LEFDEF-85) is resolved. (corrected 2026-11-13, was replicate until LEFDEF-012 confirms Magic's output, then decide together with how `def read` places components using FIXED_BBOX)

### Q-LEFDEF-13: `DIRECTION OUTPUT TRISTATE` reads as OUTPUT
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Only the first token after DIRECTION is matched, so `OUTPUT TRISTATE` becomes output and the tristate port class is never produced by LEF read.
- Evidence: lef/lefRead.c:1546-1563, 1699-1707 (corrected 2026-11-13: Confirmed with corrected lines. The pin_classes table at lef/lefRead.c:1546-1563 contains 'OUTPUT TRISTATE' as one string, but LEF_DIRECTION (lefRead.c:1677-1685, not 1699-1707) matches a single token with LookupFull, so it can never match. 'OUTPUT' maps to PORT_CLASS_OUTPUT and LefEndStatement drops TRISTATE. lef write emits 'OUTPUT TRISTATE' (lefWrite.c:1046-1047), so the round trip loses the class.)
- Options: replicate | fix (map OUTPUT TRISTATE to tristate) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, `OUTPUT TRISTATE` reads as output, as Magic does. With it OFF, DIRECTION reads the token after OUTPUT: if it is TRISTATE, the class is tristate. Then LEF read, the database and lef write round-trip the class. This is an output-value quirk: the class changes, but it does not crash and nothing is lost on save. (corrected 2026-11-13, was fix)

### Q-LEFDEF-14: USE values not recognized on read
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The database has port uses reset, scan and tie-off, but LEF read accepts only DEFAULT, SIGNAL, ANALOG, POWER, GROUND and CLOCK. The other uses report `Improper USE statement`.
- Evidence: lef/lefRead.c:1565-1582, database/database.h.in:292-301 (corrected 2026-11-13: Confirmed. The pin_uses tables are at lef/lefRead.c:1565-1582, and LEF_USE (1686-1692) reports 'Improper USE statement' and leaves the use unchanged. The database has reset, scan and tieoff (database/database.h.in:299-301). LEF 5.8 PIN USE defines only SIGNAL, ANALOG, POWER, GROUND and CLOCK. SCAN, RESET and TIEOFF are DEF PINS values. So accepting them in LEF is an extension, not a standard fix.)
- Options: replicate | fix (accept SCAN, TIEOFF and RESET) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto accepts only DEFAULT, SIGNAL, ANALOG, POWER, GROUND and CLOCK, and reports 'Improper USE statement' for the rest, as Magic does. With it OFF, it also accepts SCAN, RESET and TIEOFF and maps them to the database uses, which tools that write DEF-style uses into LEF need. The change only adds values, and a valid LEF 5.8 file reads the same in both modes. Resolve together with Q-LEFDEF-36, whose OFF writer never emits these values. (corrected 2026-11-13, was fix)

### Q-LEFDEF-15: PATH and VIA pin/obstruction geometry ignored
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `WIDTH`/`PATH` and `VIA` statements in PORT and OBS produce no paint, so pins drawn as paths or via instances vanish.
- Evidence: lef/lefRead.c:1219-1224, 1318-1323 (corrected 2026-11-13: Confirmed with corrected lines. In LefReadGeometry, WIDTH and PATH just call LefEndStatement (lef/lefRead.c:1219-1224), and VIA does the same (1336-1338, not 1318-1323). PATH- or VIA-based PORT and OBS geometry therefore produces no paint and no port label.)
- Options: replicate | fix (paint paths with the current WIDTH, and instantiate via definitions) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, WIDTH, PATH and VIA in PORT and OBS are ignored, as Magic does. With it OFF, `WIDTH w ;` sets the current path width, and `PATH pts ;` paints each segment on the current layer w wide, extended by w/2 at both ends (LEF path semantics). `VIA x y name ;` places the named via definition's rectangles at (x,y). In PORT, both also produce port labels, as RECTs do. In annotation mode they go through the same connectivity check. Pins drawn this way otherwise vanish; that changes the read result but is tier 2, as in Q-MASK-7. (corrected 2026-11-13, was replicate for now, and track as an extension)

### Q-LEFDEF-16: `-import` has no effect
- Kind: doc-mismatch
- Magic behavior: The documentation says `-import` loads `.mag` files for FOREIGN cells. The code accepts the flag and does nothing with it.
- Evidence: lef/lefCmd.c:244-245, lef/lefRead.c:2345-2350, doc/html/lef.html
- Options: replicate | fix (implement the import)
- Recommendation: Replicate. Accept and ignore the flag.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-LEFDEF-17: Cut growing uses the output style, not the input style
- Kind: doc-mismatch
- Magic behavior: The code comment says vias keep their declared size when a cifinput style exists. The code instead resizes cuts whenever a cif output style exists, regardless of the input style.
- Evidence: lef/lefRead.c:2453-2483
- Options: replicate | fix
- Recommendation: Replicate. The spec follows the code.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-LEFDEF-18: POLYGON on an unknown layer
- Kind: bug
- Magic behavior: A POLYGON after a LAYER that resolved to no type is still painted with type -1 (undefined behavior). A RECT in the same situation is skipped.
- Evidence: lef/lefRead.c:1293-1317, 1045-1065
- Options: replicate (impossible) | fix (skip, as for RECT)
- Recommendation: Fix: skip the polygon.
- Resolution: fix — a POLYGON after a LAYER that resolved to no type is skipped, as a RECT is (paint with type -1) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-19: Empty CLASS or SYMMETRY statement
- Kind: bug
- Magic behavior: `CLASS ;` or `SYMMETRY ;` builds the property from uninitialized buffer contents (it reads past an empty string).
- Evidence: lef/lefRead.c:2194-2207, 2241-2253
- Options: replicate (impossible) | fix (store an empty string, or skip)
- Recommendation: Fix: do not set the property.
- Resolution: fix — an empty CLASS or SYMMETRY statement sets no property (read past an empty string) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-20: Ignored layers in VIA definitions report errors
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: A VIA definition RECT on a layer declared `ignore` reports `No layer defined for RECT.` once per rectangle. The same layer in a macro OBS is skipped silently.
- Evidence: lef/lefRead.c:2652-2660, 920-925 (corrected 2026-11-13: Confirmed with corrected lines. In a VIA section, RECT always calls LefAddViaGeometry (lef/lefRead.c:2903-2905), which calls LefReadRect even when curlayer < 0 (2656). LefReadRect reports 'No layer defined for RECT.' at line 910 (not 920-925). In macro geometry, LefReadRect is skipped when curlayer < 0 (1230), so OBS on an ignored layer is silent. LefReadLayers prints no 'Don't know how to parse' message for IGNORE layers (742-747).)
- Options: replicate | fix (silent skip) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, a VIA-definition RECT on an `ignore` layer reports 'No layer defined for RECT.' once per rectangle, as Magic does. With it OFF, it is skipped silently, as in macro OBS. The message is still reported for a layer that truly failed to resolve. Only messages change, and geometry is the same in both modes. (corrected 2026-11-13, was fix)

### Q-LEFDEF-21: LEF annotation alters existing layout and non-port labels
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: In annotation mode, accepted PORT rectangles are painted into the existing cell, which changes the layout. When a pin has both port and non-port labels of the same name and the scan stops at a non-port label before annotating anything, that non-port label is converted into a port with the LEF geometry. The code comment says the opposite is intended.
- Evidence: lef/lefRead.c:1236-1258, 1634-1704 (corrected 2026-11-13: Confirmed with corrected lines. The label logic is at lef/lefRead.c:1704-1782 (not 1634-1704). If any port label has the pin's name (hasPort), the scan stops at the first non-port label with that name, and when nothing was annotated before it, that label is passed to LefReadPort as R and converted into a port. This contradicts the comment at 1711-1715. Accepted RECTs are painted into the existing cell (1238-1290) after the connectivity check, and POLYGONs are painted without any check (1293-1335). Painting the RECT's layer over connected material of another type, for example a contact, or over subcell material, changes the layout.)
- Options: replicate | fix (annotate labels only and never paint; leave non-port labels alone) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto replicates the label logic and the painting, as the golden cases capture. With it OFF, annotation never paints into an existing cell: it only creates or updates labels from the accepted geometry, and POLYGONs get the same coverage check as RECTs. When a port label with the name exists, non-port labels with that name are left untouched, as Magic's comment intends. Annotation is meant to add pin information, and the ON paint is a quirk that changes the layout. It is tier 2, not data loss. (corrected 2026-11-13, was replicate the label logic, which the golden cases capture)

### Q-LEFDEF-22: LEF read is not undoable
- Kind: platform
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Undo is disabled for the whole read and the message `This action cannot be undone.` is printed.
- Evidence: lef/lefRead.c:3018-3020, 3195
- Verified 2026-11-13: Confirmed. lef/lefRead.c:3018-3019 prints 'This action cannot be undone.' and calls UndoDisable, and UndoEnable is at 3203 (entry said 3195). def read does the same (lef/defRead.c:2550-2551, 2765).
- Options: replicate | platform alternative (make the read one undoable transaction)
- Recommendation: platform. On every platform and in both modes, Hefesto records a `lef read` as one undoable transaction (layout-undo), covering created cells, annotated labels and paint, and does not print 'This action cannot be undone.'. File content and the read result are unchanged, so magic-compat is not involved. Apply the same decision to `def read` (defRead.c:2550-2551) so the two readers behave alike.

### Q-LEFDEF-23: Polygon port labels from multi-plane types
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: When a polygon layer occupies several planes (contact types), its decomposition is painted on each plane, but only the rectangles from the last plane become port labels. The lists from earlier planes are leaked.
- Evidence: lef/lefRead.c:1045-1065
- Verified 2026-11-13: Confirmed at lef/lefRead.c:1045-1071. LefPaintPolygon paints on every plane the type occupies, and each plane's rlist overwrites the previous one, so earlier lists leak. The decomposition comes from CIFPolyToRects on the point path (cif/CIFrdpt.c:604-636), which is purely geometric. Every plane therefore gets the same rectangles, and the last plane's list equals any other plane's.
- Options: replicate | fix
- Recommendation: replicate. Hefesto creates port labels from one decomposition of the polygon. That is observably identical to Magic, because every plane receives the same rectangles, and it does not leak. Nothing differs between modes, so magic-compat is not involved.

### Q-LEFDEF-24: File search uses Magic's cell search path
- Kind: platform
- Magic behavior: LEF and DEF files are opened through the cell search path (Path, then CellLibPath, with `~` expansion). iPadOS has sandboxed files and no such path variables.
- Evidence: lef/lefWrite.c:150-235, utils/path.c:615-646
- Options: replicate on desktop | platform alternative (document picker or security-scoped bookmarks on iPadOS)
- Recommendation: Use the search path on Linux and macOS and the sandbox picker on iPadOS. The read semantics do not change.
- Resolution: platform — LEF and DEF files are found through the cell search path on Linux and macOS and through the document picker or granted folders on iPadOS; read semantics do not change (X-4, hefesto-files-and-pdks, 2026-11-07).

### Q-LEFDEF-30: LEF write order depends on hash-table iteration
- Kind: ambiguity
- Magic behavior: Several parts of the output follow the order of Magic's string hash tables, which depends on the hash function and the bucket count. These are the order of `-tech` LAYER blocks, of PROPERTYDEFINITIONS keys and of SITE blocks. The winner when several layer records map the same magic type follows the same order.
- Evidence: lef/lefWrite.c:303-313, lef/lefWrite.c:316-370, lef/lefWrite.c:385-479, lef/defWrite.c:2052-2068
- Options: replicate (reimplement Magic's hash function, table growth and iteration order) | fix (use tech-file declaration order for layers and first-seen order for properties and sites)
- Recommendation: Replicate for byte compatibility of the `-tech` and PROPERTYDEFINITIONS output. Golden cases LEFDEF-112 and LEFDEF-113 should capture the order.
- Resolution: decided by X-1 as amended (2026-11-06, D-1): replicate. Hefesto reproduces Magic's string hash order exactly (capability `hefesto-output-determinism`, requirement "Magic string hash table").

### Q-LEFDEF-31: Default route spacing overwrites width
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Tech defaults are filled in after scaling. When the spacing is still unset (-1), Magic assigns the DRC default spacing to the route *width* and leaves the spacing at -1. As a result, `lef write -tech` prints `WIDTH <default spacing>` and no SPACING line, unless `lef read` has set the values.
- Evidence: lef/lefTech.c:507-511 (corrected 2026-11-13: The line is right (lef/lefTech.c:510-512), but the claim that it applies 'with default initialization' is wrong. The fill runs only from utils/tech.c:826-829 (grid not 1:1), and with truncating scaling spacing is still -1 then only for d < n < 2d (see Q-LEFDEF-1). On a 1:1 grid, width and spacing stay -1, so lef write -tech (lefWrite.c:419-432) writes no WIDTH, no SPACING and no DIRECTION. The `WIDTH <default spacing>` output occurs only for those grid ratios.)
- Options: replicate | fix (assign the spacing to the spacing field) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, resolved together with Q-LEFDEF-1 and Q-LEFDEF-2. With magic-compat ON, the -tech writer prints what Magic stores: no WIDTH, SPACING or DIRECTION for unset routes, and `WIDTH <default spacing>` with no SPACING when the fill ran with spacing unset. With it OFF, it prints the filled DRC default width and spacing and the DIRECTION line. Only the -tech output differs. (corrected 2026-11-13, was replicate in the `-tech` writer for compatibility, and flag it to the `lef-tech-section` owner)

### Q-LEFDEF-32: CUT AREA value and keyword
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The default contact rectangle is (-w,-w)-(w,w), where w is the DRC width, so the printed `CUT AREA` is 4*w^2*oscale^2. `CUT AREA` is also not a standard LEF LAYER keyword; standard LEF uses `AREA` or `WIDTH`.
- Evidence: lef/lefTech.c:495-503, lef/lefWrite.c:410-418
- Verified 2026-11-13: Confirmed. The fill sets the contact area to (-w,-w,w,w) in half internal units (lef/lefTech.c:495-503). The writer multiplies the half-unit sides (lef/lefWrite.c:410-415), so CUT AREA = 4*w^2*oscale^2, four times the true w x w cut area. The area is positive only when filled (shrinking grids, n < d) or set by a LEF VIA. VIA-class entries that have a via cell are skipped (401-402). So on a 1:1 grid the contact-flavor cuts print only `TYPE CUT ;`. `CUT AREA` is not a LEF 5.8 LAYER statement.
- Options: replicate | fix (emit `WIDTH` from the true cut size) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto prints `CUT AREA <4*w^2*oscale^2>` when the stored half-unit area is positive, as Magic does. With it OFF, it prints the standard `WIDTH <side*oscale> ;` for a square cut, where side = (xtop-xbot)/2 internal units, and nothing for a non-square or null area. With Q-LEFDEF-1 OFF, every contact-flavor cut then gets its DRC width. Other tools treat Magic's keyword as unknown, so ON keeps byte identity and OFF gives legal LEF. (corrected 2026-11-13, was replicate)

### Q-LEFDEF-33: SITE size uses the wrong cell's FIXED_BBOX
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: When a site cell has the fixed-bbox flag, the SITE SIZE is read from the `FIXED_BBOX` property of the cell being written (the root), not from the site cell.
- Evidence: lef/lefWrite.c:346-362
- Verified 2026-11-13: Confirmed at lef/lefWrite.c:345-362. When siteDef has CDFIXEDBBOX, the code calls DBPropGet(def, ...) with def being the cell being written, not siteDef. If the root has no FIXED_BBOX, the site's cd_bbox is used.
- Options: replicate | fix (read the site cell's FIXED_BBOX) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, a site cell with the fixed-bbox flag takes its SITE SIZE from the FIXED_BBOX of the cell being written, if that cell has one, as Magic does. With it OFF, the size comes from the site cell's own FIXED_BBOX, falling back to its bounding box. Sites created by lef read carry FIXED_BBOX = SIZE, so OFF writes the true site size. (corrected 2026-11-13, was fix, with an opt-in compatibility mode)

### Q-LEFDEF-34: Header written only for the edit root; LEF database units are stale otherwise
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `lef write` writes VERSION, UNITS, sites and properties only when the target cell is the edit root. When a subcell instance is selected, the file has no header. In that case the LEF database units used for rounding are left over from the previous header write, or are 1000 if no header has been written.
- Evidence: lef/lefCmd.c:430-433, lef/lefWrite.c:2384-2396, lef/lefWrite.c:66, lef/lefWrite.c:279 (corrected 2026-11-13: Behaviour confirmed, with one wrong ref. isRoot is computed at lef/lefCmd.c:442-443 (`selectedUse->cu_def == EditRootDef`), not 430-433. lefWrite.c:2384-2396 writes the header only if isRoot. LEFdbUnits is a file-static set to 1000 (lefWrite.c:66) and recomputed only in lefWriteHeader (279-292). lefPrint rounds with it (97-125), so without a header the precision is whatever the last header in the session used.)
- Options: replicate | fix (always write the header, and always recompute the units) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. Both modes keep the header-less output for a selected subcell, which Magic produces on purpose (and which concatenates cleanly). With magic-compat ON, number formatting uses U from the last header written in the session, or 1000, as Magic does. With it OFF, U is recomputed from the current cifoutput style on every write, header or not. U only changes rounding precision, so this is an output-value quirk. (corrected 2026-11-13, was always recompute the units, which is harmless)

### Q-LEFDEF-35: Name legalization is ineffective
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The routine meant to replace `;# -*$\n` with `_` detects a bad character only if it is the last character of the name. Its result is also never used. Pin and macro names are therefore written verbatim, even when this makes the LEF illegal (for example `a#1` or names containing spaces).
- Evidence: lef/lefWrite.c:976-1018, lef/lefWrite.c:1785-1788
- Verified 2026-11-13: Confirmed. MakeLegalLEFSyntax (lef/lefWrite.c:976-1003) breaks only the inner loop, so the bad-character test sees only the last character. Its result LEFtext (1785) is never printed: `END %s` uses reflab->lab_text, and it is freed at 1788. The PIN line (1034) and MACRO and FOREIGN lines use raw names. `-`, `*` and `$` are legal in LEF names; only space, `;`, `#` and newline break parsing.
- Options: replicate (verbatim) | fix (sanitize, which breaks name matching with DEF and netlists)
- Recommendation: replicate. In both modes, names are written verbatim, as Magic does. Magic defines no corrected behaviour that works: the only call site would have sanitised just the END line, which would contradict the PIN line. Sanitising only LEF would also break name matching with DEF, SPICE and the GDS FOREIGN cell, so a corrected mode would have to rename across every writer, which is outside M6. Hefesto adds a warning when a written name contains a character that breaks LEF tokenising (space, `;`, `#`, newline).

### Q-LEFDEF-36: Unhandled port class/use values give an empty keyword
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The port uses reset, scan and tieoff produce `    USE  ;`. The undefined class encodings 0x60 and 0x70 produce `    DIRECTION  ;`. Both lines are malformed LEF.
- Evidence: lef/lefWrite.c:1040-1090, database/database.h.in:282-301
- Verified 2026-11-13: Confirmed at lef/lefWrite.c:1035-1081. The switch has no case for PORT_CLASS 0x60 or 0x70, or for PORT_USE_RESET, SCAN or TIEOFF (database/database.h.in:282-301). The output is `DIRECTION  ;` and `USE  ;` (two spaces). The VDD/GND Tcl fallback (1083-1101) runs only when the use is 0.
- Options: replicate | fix (write `USE RESET`/`USE SCAN`/`USE TIEOFF`; LEF 5.8 defines none of these, and SIGNAL is a reasonable fallback) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto writes the malformed `    DIRECTION  ;` and `    USE  ;` lines, as Magic does. With it OFF, it writes `USE SIGNAL ;` for reset, scan and tie-off, because LEF 5.8 has no such uses and all three are digital signals, and it omits the DIRECTION line for the undefined class codes. The OFF output is always legal LEF. Resolve together with Q-LEFDEF-14. (corrected 2026-11-13, was fix (write SIGNAL), with an opt-in compatibility mode)

### Q-LEFDEF-37: -pinonly larger than -hide setback has no effect
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: With `-hide d` and `-pinonly p` where p > d, the second removal takes a rectangle (B shrunk by p) that lies inside the area already removed (B shrunk by d). It therefore does nothing.
- Evidence: lef/lefWrite.c:1612-1619
- Verified 2026-11-13: Confirmed at lef/lefWrite.c:1608-1619. With setback d > 0, the second removal runs only when pinonly > setback, and B shrunk by p is then inside B shrunk by d, so it removes nothing. For p <= d it does not run at all. So -pinonly has no effect for any p under -hide d > 0. Without -hide (lefWrite.c:1628-1641), -pinonly p removes the net beyond p and repaints the label, which is the documented meaning (doc/html/lef.html:60-65). The comparison looks inverted (should be p < d).
- Options: replicate | fix (intended semantics unclear) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the extra removal runs only when p > d, so it is a no-op, as Magic does. With it OFF, the test is inverted: when p < d (including `-pinonly` with no distance, p = 0), the material inside B shrunk by p is also removed from the pin selection before the label rectangle is painted back. So -pinonly means under -hide d what it means without -hide: pins are the label plus net within p of the boundary. When p >= d the setback already limits the pin, which is the same in both modes. (corrected 2026-11-13, was replicate)

### Q-LEFDEF-38: Setback strips copy whole tiles
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: With `-hide d` and d > 0, obstruction geometry is collected by copying whole tiles that overlap the four setback strips, without clipping. Tiles that cross into the hidden interior therefore appear in full, and the result depends on the tile decomposition.
- Evidence: lef/lefWrite.c:1921-1956, lef/lefWrite.c:655-745
- Verified 2026-11-13: Confirmed. lef/lefWrite.c:1919-1954 searches four strips around the boundary with DBSrPaintArea and lefYankGeometry. lefYankGeometry (655-731) copies the whole tile (TiToRect, 703) unclipped, so tiles that cross into the hidden interior come out in full. The result follows the tile decomposition, which Hefesto reproduces (X-1).
- Options: replicate | fix (clip to the strips) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, whole overlapping tiles are copied, as Magic does. This is byte-identical because Hefesto reproduces Magic's tile decomposition. With it OFF, each copied tile is clipped to the strip it overlaps, so the obstruction ring is exactly the area within d of the boundary, independent of tile structure. Interior detail that Magic leaks through crossing tiles is what -hide is meant to remove. (corrected 2026-11-13, was replicate, because golden comparison requires it)

### Q-LEFDEF-39: Split tile with both halves writable
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: If both triangles of one diagonal tile are writable LEF types, only the left half is written, because the tile is marked visited after the first half. The source comment also claims that port mode ignores split tiles, but the code writes POLYGONs in all modes except the contact rounds.
- Evidence: lef/lefWrite.c:840-849, lef/lefWrite.c:436-449 (corrected 2026-11-13: Behaviour confirmed at lef/lefWrite.c:828-849: the tile is marked visited before its side is examined, so the second half enumerated is skipped. The comment ref is wrong: the 'LEF_MODE_PORT ignores split tiles' claim is in the lefClient struct comment at lefWrite.c:484-492, not 436-449. Only rmask types are searched in the yank buffer (1989-2031), so both halves must be routing types for the bug to occur.)
- Options: replicate | fix (write both halves) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, only the first-enumerated (left) half of a split tile with two writable halves is written, as Magic does. With it OFF, both halves are written as separate POLYGONs. Losing output geometry from a non-Manhattan tile follows the M5 precedent (Q-MASK-7, compat). This replaces the previous 'fix'. The case is rare, so goldens are almost always identical. (corrected 2026-11-13, was fix)

### Q-LEFDEF-40: Antenna type masks without extraction style
- Kind: bug
- Magic behavior: With no extraction style loaded, the gate and diffusion masks are left uninitialized, so the antenna areas are undefined.
- Evidence: extract/ExtTech.c:519, extract/ExtTech.c:556, lef/lefWrite.c:1286-1287
- Options: fix (treat the masks as empty)
- Recommendation: Treat them as empty. The spec already states this.
- Resolution: fix — with no extraction style the gate and diffusion masks are empty (uninitialised masks) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-41: writeall macro order depends on subcell enumeration; doc says only cells with ports are written
- Kind: doc-mismatch
- Magic behavior: The macro order is the reverse of a pre-order traversal over the database's subcell enumeration, which is a spatial index order. The documentation says that cells containing port labels are treated as standard cells, but every child is written, with or without ports.
- Evidence: lef/lefWrite.c:2246-2291, lef/lefWrite.c:2321-2339, doc/html/lef.html
- Options: replicate the enumeration order | fix (sort by name)
- Recommendation: Replicate the documented traversal, and pin down the enumeration order in the database/tile-plane spec. Golden case LEFDEF-140 captures it.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-LEFDEF-42: LEFproperties format differs from documentation
- Kind: doc-mismatch
- Magic behavior: The writer parses space-separated `key value` pairs, where the value may be double-quoted. The documentation describes `key:value` pairs. A `key:value` token with no following value is dropped.
- Evidence: lef/lefWrite.c:2040-2084, lef/lefWrite.c:2139-2200, doc/html/lef.html
- Options: replicate the code
- Recommendation: Replicate the code, and correct the Hefesto documentation.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-LEFDEF-43: Output file location
- Kind: platform
- Magic behavior: Output files are opened relative to the current working directory, or the cell's .mag directory for writeall. A dotted cell name with no `.lef` extension (for example `my.cell`) is written without the `.lef` suffix.
- Evidence: lef/lefWrite.c:150-235
- Options: replicate | platform alternative (on iPadOS, use the document container or a user-chosen location)
- Recommendation: Replicate the naming rules on desktop. On iPadOS, resolve relative paths against the project's document folder.
- Resolution: platform — Magic's naming rules on desktop; on iPadOS relative paths resolve against the project's document folder (X-4, hefesto-files-and-pdks, 2026-11-07).

### Q-LEFDEF-44: Power/ground names come from Tcl variables
- Kind: platform
- Magic behavior: The `$VDD` and `$GND` Tcl globals decide USE POWER/GROUND for pins with default use. This happens only in the Tcl build.
- Evidence: lef/lefWrite.c:1092-1112
- Options: platform alternative (session settings named VDD and GND, settable from scripts and preferences)
- Recommendation: Provide the settings. Their defaults are unset.
- Resolution: platform — decided by D-14 (2026-11-07). The power and ground names are the `vdd` and `gnd` fields of the project setting "global nets and power names" (`hefesto-settings`), seeded from `$VDD` and `$GND`.

### Q-LEFDEF-45: float32 arithmetic in coordinate output
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Coordinates are computed as float32 `oscale*n` and rounded with float32 `truncf`. Values near half-grid can round differently than exact decimal arithmetic would, and values in (-1/(2U),0) print as `-0.000`.
- Evidence: lef/lefWrite.c:90-127, lef/lefWrite.c:943-947
- Verified 2026-11-13: Confirmed at lef/lefWrite.c:89-127. lefPrint computes truncf(invalue*U + r)/U in float, with r = ±0.5. The default case computes `invalue*100000. + r` in double, narrows it to float for truncf, divides by 100000. in double and casts back to float (lef/lefWrite.c:120-122). Callers pass float `scale * (float)n` (for example 943-947). For invalue in (-1/(2U), 0), the expression truncates to -0.0f and prints `-0.000`.
- Options: replicate float32 | fix (exact rational arithmetic)
- Recommendation: replicate. In both modes, Hefesto does the float32 computation exactly as lefPrint does: a float32 product, float32 truncf of value ± 0.5, and float division by U; in the default case the product `invalue*100000. + r` is computed in double, narrowed to float32 before `truncf`, then divided by 100000. in double and cast to float. It also keeps `-0.000`, which every LEF parser reads as 0. This is an internal arithmetic choice that byte identity needs, and Magic defines no other intended rounding (M5 precedent Q-MASK-15). Golden LEFDEF-105 covers half-grid values.

### Q-LEFDEF-46: Pins without LEF geometry are dropped silently
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: A port whose net has no material on a LEF-mapped layer produces no PIN record and no warning.
- Evidence: lef/lefWrite.c:1680-1709, lef/lefWrite.c:1785-1787
- Verified 2026-11-13: Confirmed. The PIN header is written only when lefWriteGeometry returns 1 in needHeader mode (lef/lefWrite.c:1689-1707), and `END pin` only when needHeader was cleared (1786-1787). A port with no material on a LEF-mapped layer produces nothing and no message.
- Options: replicate | fix (warn)
- Recommendation: replicate with a warning. In both modes, the output is the same as Magic's: no PIN record. Hefesto adds a warning naming the port and its index, because dropping a declared pin silently is a common source of LVS and P&R surprises. The output file does not change, so magic-compat is not needed.

### Q-LEFDEF-47: Too-large -hide distance silently disables hiding
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: If 2*setback exceeds the boundary width or height, the macro is written with full detail and no message.
- Evidence: lef/lefWrite.c:1355-1362
- Verified 2026-11-13: Confirmed at lef/lefWrite.c:1355-1361. For setback > 0, if the width or height < 2*setback, setback becomes -1 (no hiding at all, full detail). The comment says this is intended ('there is no effect of the -hide option').
- Options: replicate | fix (fall back to setback 0, or warn)
- Recommendation: replicate with a warning. In both modes, the macro is written with full detail, as Magic does on purpose (lefWrite.c:1355-1356 comment). Hefesto adds a warning naming the macro, the setback and the boundary size. The output does not change, so magic-compat is not needed. Falling back to full hiding (setback 0) would be a different product choice that Magic's intent does not support.

### Q-LEFDEF-50: Ports sharing an index with different names are merged
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: All labels with the same port index go into one PIN, named after the first label. An error message is printed for each differently named label, and its geometry becomes extra PORTs of the first PIN.
- Evidence: lef/lefWrite.c:1436-1440, lef/lefWrite.c:1776-1782, lef/lefWrite.c:1795-1808 (corrected 2026-11-13: Behaviour confirmed, with refs corrected. Per index, the first unvisited label is reflab (lef/lefWrite.c:1418-1460). The geometry loop then continues through every unvisited label with the same index, whatever its text (1772-1778), and writes it as further PORTs of the same PIN. The duplicate check (1789-1808) prints 'Index %d is used for ports "a" and "b"' and decrements idx once per differently named label. The re-scan finds only visited labels, so it has no effect. The cited 1436-1440 is only the visited check.)
- Options: replicate | fix (separate PINs)
- Recommendation: replicate. In both modes, all labels with one port index form a single PIN named after the first label, with one error message per differently named label, as Magic does. In Magic and in Hefesto's DEF and netlist writers, a shared index means one electrical port, so separate PINs would disagree with the netlist. Magic defines no other intended behaviour.

### Q-LEFDEF-51: Pin removal under -hide -pinonly p is overridden
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: With setback 0 and p > 0, interior material of the selected chunk is removed from the selection. The full chunk rectangle is then painted back with the label's type, so the removal only affects the chunk when it lies on a different, connected type.
- Evidence: lef/lefWrite.c:1561-1600
- Verified 2026-11-13: Confirmed at lef/lefWrite.c:1539-1600 (setback == 0 branch). SelectChunk gives carea, and with p > 0 SelRemoveArea removes B shrunk by p from the selection (1575-1580). Then DBPaintPlane paints the whole carea back with the label's type (1586-1590), so the removal survives only where the chunk lies on a different connected type. Without -hide, -pinonly p keeps only the net within p of the boundary plus the label (1628-1641), which is the documented meaning (doc/html/lef.html:60-65).
- Options: replicate | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the full chunk C is painted back with the label's type, as Magic does. With it OFF, the pin is C minus the area inside B shrunk by p, plus the label rectangle, painted with the label's type. So `-hide -pinonly p` limits pins to within p of the boundary, as -pinonly p does without -hide. With p = 0, or without -pinonly, both modes are identical. (corrected 2026-11-13, was replicate)

### Q-LEFDEF-52: Cut layers are written as whole contact areas
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Via cuts in PORT and OBS records are the magic contact-type area, not the individual cut squares produced by GDS generation.
- Evidence: lef/lefWrite.c:747-814
- Verified 2026-11-13: Confirmed. lefYankContacts and lefEraseGeometry (lef/lefWrite.c:747-814) yank and erase contact tiles whole, and the contact round writes each contact tile's full area on the cut layer. This is deliberate, as the comment at 735-741 says, not an accident.
- Options: replicate | fix (emit cut arrays as in the cifoutput rules)
- Recommendation: replicate. In both modes, cut-layer PORT and OBS records are whole magic contact areas, as Magic does on purpose. An abstract does not need individual cut squares, downstream tools accept the area, and golden tests depend on it. Magic defines no other intended behaviour, so there is nothing for an OFF mode to do.

### Q-LEFDEF-53: Boundary layer declared but unpainted
- Kind: bug
- Magic behavior: If the tech declares a `bound` layer but the cell has no such paint, only the x-extent of the boundary is initialized (to 0..0). The y-extent is uninitialized memory.
- Evidence: lef/lefWrite.c:1321-1330
- Options: fix (fall back to the cell bounding box)
- Recommendation: Fall back to the cell bbox, or FIXED_BBOX if present.
- Resolution: fix — a declared but unpainted boundary layer falls back to FIXED_BBOX if present, otherwise the cell bounding box (uninitialised y-extent) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-60: DEF "generated via" flag is never reset between vias
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Once any via in a VIAS section uses CUTSIZE/LAYERS/CUTSPACING/ENCLOSURE/ROWCOL, every later via (including RECT-only vias) also gets generated geometry from the stale parameters at its `;`.
- Evidence: lef/defRead.c:1925, lef/defRead.c:1999-2008 (`generated` initialised once per section, never cleared) (corrected 2026-11-13: Behaviour confirmed, with refs corrected. `generated` is initialised once per VIAS section at lef/defRead.c:1916 (not 1925). Only rows and cols are reset per via (1958). At each via's `;`, LefGenViaGeometry runs whenever generated is TRUE (1993-2003). So once a CUTSIZE has been seen, every later via, including RECT-only ones, gets extra generated geometry from the stale parameters. Mixed sections, as OpenROAD writes them, are realistic.)
- Options: replicate | fix (reset per via definition) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the flag persists for the rest of the VIAS section, as Magic does, and later RECT-only vias get extra geometry from the last generated via's parameters (kept as a golden). With it OFF, the flag and all generated-via parameters are reset at each `- name`. A via is then generated only from its own CUTSIZE, LAYERS, CUTSPACING, ENCLOSURE and ROWCOL, with omitted parameters handled as in Q-LEFDEF-61. The parameters are stale, not uninitialised (Q-LEFDEF-61 covers that case), so this is tier 2 under the standing rule. (corrected 2026-11-13, was fix)

### Q-LEFDEF-61: Generated-via parameters undefined when omitted
- Kind: bug
- Magic behavior: CUTSIZE, CUTSPACING, ENCLOSURE and LAYERS have no defaults; omitted ones use uninitialised or previous values. The VIARULE is ignored, not consulted.
- Evidence: lef/defRead.c:1920-1923, lef/defRead.c:2027-2031
- Options: replicate (previous value) | fix (take defaults from the LEF VIARULE GENERATE or DRC rules; 0 spacing/enclosure)
- Recommendation: fix: use the VIARULE when known, otherwise 0, and warn.
- Resolution: fix — omitted generated-via parameters take the LEF VIARULE values when known, otherwise 0, with a warning (uninitialised values) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-62: VIAS ORIGIN/OFFSET/PATTERN/POLYGON/MASK ignored
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: These properties produce an "Unknown via property" message and are dropped; generated vias are always centred and fully populated.
- Evidence: lef/defRead.c:2011-2016 (corrected 2026-11-13: Confirmed, with lines shifted. via_property_keys (lef/defRead.c:1928-1937) has no ORIGIN, OFFSET, PATTERN, POLYGON or MASK. The unknown-property message is at 2005-2012, and the arguments are then skipped by `if (*token != '+') continue` (2004). Generated vias are always centred and fully populated by LefGenViaGeometry.)
- Options: replicate | fix (implement per DEF 5.8) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, these properties report 'Unknown via property ...; ignoring.' and are dropped, as Magic does. With it OFF, they are implemented per DEF 5.8. ORIGIN shifts the whole via. OFFSET shifts the bottom and top enclosure rectangles. PATTERN drops cuts from the ROWCOL array. `+ POLYGON layer pts` adds that polygon. `+ MASK n` is accepted and ignored, because Hefesto has no mask colours. Ignoring OFFSET and ORIGIN moves geometry, but per the standing rule and the M5 precedent (Q-MASK-7) that is tier 2. Share the generator with the Q-LEFDEF-7 OFF path. (corrected 2026-11-13, was fix, since ignoring OFFSET/ORIGIN moves the geometry)

### Q-LEFDEF-63: Generated via primary type is the top metal; layer switch fails when routing on the top layer
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: A generated via's primary type is the top layer (not the cut). After the via in a route, the "other residue" search over a non-contact type finds only the top layer, so a route arriving on the top layer stays on the top layer instead of switching to the bottom layer.
- Evidence: lef/lefRead.c:2575-2576, lef/defRead.c:479-525 (corrected 2026-11-13: Confirmed, with refs corrected. LefGenViaGeometry sets lefl->type = tlayer (lef/lefRead.c:2578-2580, not 2575-2576). In a route, paintLayer = lefl->type and iscontact = TRUE (defRead.c:447-459). The switch loop (483-525) takes the residues of the non-contact tlayer, which is {tlayer} (DBTechFinalContact, database/DBtcontact.c:754-762). So a route arriving on the bottom layer switches to the top correctly, but one arriving on the top stays on the top, and the following segments are painted on the wrong layer. RECT-defined vias are unaffected, because LefAddViaGeometry makes a contact type primary (2675-2682).)
- Options: replicate | fix (choose the via's other routing layer: bottom if current == top, else top) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the primary type stays T and the layer switch uses the residues of T, so a route on T stays on T, as Magic does. With it OFF, the primary type is still T (geometry unchanged, and def-write and lef-write depend on it), but after a generated via the route continues on the via's other routing layer: the bottom layer B if the current layer is T, otherwise T. RECT-defined vias behave the same in both modes. Wrong-layer painting changes output values but is not a crash or undefined behaviour, so this is tier 2. (corrected 2026-11-13, was fix)

### Q-LEFDEF-64: Pin DIRECTION/USE persist across pins
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: pinDir/pinUse are initialised once per PINS section; a pin without DIRECTION/USE inherits the previous pin's values.
- Evidence: lef/defRead.c:1462-1463, lef/defRead.c:1590-1610
- Verified 2026-11-13: Confirmed. pinDir/pinUse are initialised once per section (lef/defRead.c:1463-1464). The '-' handler resets only pending/curlayer (lef/defRead.c:1552-1577), not pinDir/pinUse. They are set at lef/defRead.c:1600-1614 and applied to labels when painting at LAYER (pending) or PLACED (1629-1740). Line refs adjusted from 1462-1463/1590-1610.
- Options: replicate | fix (reset to DEFAULT per pin) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, DIRECTION and USE carry over from the previous pin until changed, as Magic does. With it OFF, each pin starts with DEFAULT class and DEFAULT use, so a pin without DIRECTION/USE gets default flags, as DEF intends. The values are applied to the labels Hefesto writes to .mag, so this is an output-value quirk (tier 2), not tier 1. (corrected 2026-11-13, was fix)

### Q-LEFDEF-65: Pin rectangle list and layer state leak between pins
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The pending rectangle list and "layer seen" flag are not cleared at a new pin. With `+ PLACED ... + LAYER a ... + LAYER b ...`, only `a` is painted; `b` remains pending and is painted with the next pin's placement and name.
- Evidence: lef/defRead.c:1600-1608, lef/defRead.c:1630-1730 (corrected 2026-11-13: Partly wrong. At each '- <pin>' Magic does reset pending=FALSE and curlayer=-1 (lef/defRead.c:1576-1577), so the 'layer seen' state does not leak. Only rectList is never cleared. With '+ PLACED ... + LAYER a + LAYER b', a is painted at LAYER a (pending branch, 1629-1683); b is pushed with pending FALSE and stays in rectList. It is then painted with the next pin's transform and name at that pin's next paint (pending LAYER or PLACED, 1685-1740). Rects left after the last pin are never painted (the section ends at 1747-1754 without flushing). The same leak happens within a pin for '+ LAYER a + PLACED + LAYER b'. Lines: pin start 1552-1583, LAYER 1629-1683, PLACED 1685-1740.)
- Options: replicate | fix (paint all LAYER rects of a port with that port's placement; clear per pin/port) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto keeps Magic's rectangle list as described, including painting leftover rectangles with the next pin's placement and name, and dropping rectangles still pending at the end of the section. With it OFF, rectangles are collected per port (a pin, or each '+ PORT' of a pin); at the end of the port they are all painted with that port's placement and labelled with that pin's name; a port that has rectangles but no placement is reported with a 'was not placed' message and dropped. Geometry is misplaced, not lost through undefined behaviour, so tier 2 applies. (corrected 2026-11-13, was fix)

### Q-LEFDEF-66: Pin POLYGON and VIA ignored
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `+ POLYGON` and `+ VIA` in PINS give an "Unknown pin property" message and no geometry.
- Evidence: lef/defRead.c:1615-1621
- Verified 2026-11-13: Confirmed at lef/defRead.c:1588-1594 (not 1615-1621): an unknown '+' keyword prints 'Unknown pin property ... ignoring.' and the loop skips tokens until the next '+' (1585). A pin whose only geometry is POLYGON/VIA keeps curlayer=-1, so PLACED sets pending and the next pin prints 'Pin specified without layer, was not placed.'
- Options: replicate | fix (paint polygon/via geometry and label it) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, POLYGON and VIA in PINS give Magic's message and no geometry. With it OFF, '+ POLYGON layer pts' is painted through lef-read's polygon decomposition and '+ VIA name ( x y )' paints the via's geometry; both are transformed by the port placement and labelled with the pin name using the same label rule as LAYER rectangles (polygons follow lef-read's polygon port-label rule, Q-LEFDEF-23). Magic's output exists and the loss is a missing feature, so the standing rule (compat) applies rather than fix. (corrected 2026-11-13, was fix)

### Q-LEFDEF-67: Unknown pin layer paints with an invalid type
- Kind: bug
- Magic behavior: The rectangle is stored with type -1 before the code substitutes "space"; the paint and label calls receive type -1 on a zero rectangle at (0,0).
- Evidence: lef/defRead.c:1632-1650
- Options: replicate | fix (report and skip the rectangle)
- Recommendation: fix.
- Resolution: fix — an unknown pin layer is reported and its rectangle skipped (paint and label with invalid type -1) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-68: Rectangle parse failure dereferences NULL
- Kind: bug
- Magic behavior: A malformed RECT in DIEAREA, PINS LAYER or BLOCKAGES returns no rectangle and Magic dereferences it (crash).
- Evidence: lef/lefRead.c:928-930, lef/defRead.c:1631-1635, lef/defRead.c:1850-1851, lef/defRead.c:2638-2644
- Options: replicate (n/a) | fix (report error, skip statement)
- Recommendation: fix.
- Resolution: fix — a malformed RECT in DIEAREA, PINS LAYER or BLOCKAGES is reported and its statement skipped (NULL dereference) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-69: Unknown special-net "+" keyword swallows the next net
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Inside special routing, an unknown `+` keyword, an unknown SHAPE, or a non-numeric STYLE skips to `;` and then keeps parsing route tokens, consuming the following net statement until its `;`.
- Evidence: lef/defRead.c:224-247
- Verified 2026-11-13: Confirmed at lef/defRead.c:222-256 (entry: 224-247). LefEndStatement consumes through ';' and 'continue' keeps the DefAddRoutes loop running. The next net's '- name' gives a via-name error, '( comp pin )' gives 'Cannot parse X coordinate' (skipped to ')'), and its '+ ROUTED ...' wiring is painted under the previous net's name/label state; the next net is never created or labelled. Special-wiring shapes '+ RECT', '+ POLYGON', '+ VIA', '+ MASK' are not in specnet_keys (lef/defRead.c:110-126), so they all take this path (the bare POLYGON branch of Q-LEFDEF-70 is not used for them).
- Options: replicate | fix (skip to the next `+` or end the net at `;`) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto keeps Magic's recovery, swallowing the following net statement as route tokens. With it OFF, an unknown '+' keyword, unknown SHAPE or non-numeric STYLE reports the same message and skips only to the next '+' or ';'. A ';' ends the current net, so the next net is read normally. Unknown special-wiring shapes ('+ RECT', '+ POLYGON', '+ VIA', '+ MASK') are skipped this way; painting them is out of scope here. This is a misparse of the input file, not a crash, undefined behaviour or loss of data already in the database. The M5 precedent (not-square drops geometry: compat) keeps it in tier 2. (corrected 2026-11-13, was fix, e.g. for the `+ MASK`, `+ RECT`, `+ POLYGON` and `+ VIA` special-net shapes)

### Q-LEFDEF-70: POLYGON in routes partially parsed as points
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Only tokens up to the first `)` are skipped; remaining vertices become wire points.
- Evidence: lef/defRead.c:349-355
- Verified 2026-11-13: Confirmed at lef/defRead.c:350-355 with endCoord at 679-682: Magic reads the '(' then skips through the first vertex's ')'; later vertices become ordinary route points and Manhattan legs are painted as wire of the current width (non-Manhattan legs are reported and reset the reference). This bare-POLYGON branch is reached only by a non-standard POLYGON token inside wiring; DEF's '+ POLYGON' special shape goes through Q-LEFDEF-69 instead.
- Options: replicate | fix (skip or paint the polygon) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto gives Magic's message, skips the first vertex and paints the rest as wire points. With it OFF, the same error message is reported, then the whole vertex list up to the next non-point token is consumed and painted as a polygon on the current route layer, using lef-read's polygon decomposition. The reference point is not changed. Magic's output is deterministic and nothing crashes, so this is tier 2. (corrected 2026-11-13, was fix (paint polygon via the polygon decomposition of `lef-read`))

### Q-LEFDEF-71: VIRTUAL route points are connected with wire
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `VIRTUAL` is ignored and the following point is joined to the previous one by painted wire.
- Evidence: lef/defRead.c:356-361
- Verified 2026-11-13: Confirmed at lef/defRead.c:357-362 (entry 356-361): VIRTUAL is skipped with 'continue', and the following point is joined to the previous one by painted wire if the leg is Manhattan (a non-Manhattan leg is reported and only resets the reference, lef/defRead.c:597-606).
- Options: replicate | fix (treat VIRTUAL point as a new reference without painting) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, VIRTUAL is ignored and a wire is painted to the next point, as Magic does. With it OFF, per DEF 5.8 the point after VIRTUAL becomes the new reference point without painting a wire, and no non-Manhattan message is given for that leg. This is an output-value quirk. (corrected 2026-11-13, was fix per DEF 5.8)

### Q-LEFDEF-72: Via orientation and MASK tokens in routes
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `MASK n`, via `MASK` bits, and via orientation tokens are treated as via names, producing errors and no effect.
- Evidence: lef/defRead.c:398-411
- Verified 2026-11-13: Confirmed. Any token that is not '(', ';', '+', NEW or a keyword enters the via-name branch (lef/defRead.c:397-531; entry 398-411). Orientation tokens and 'MASK'/mask numbers fail the LefInfo lookup and print 'Via name ".." unknown in route.' (530). Before any point they print 'Route has via name ... but no points!' (403-406) instead. The via is painted unrotated.
- Options: replicate | fix (parse and apply orientation; ignore MASK silently) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto gives Magic's per-token messages, with orientation ignored. With it OFF, 'MASK n' before a point or RECT, and the mask-bits token before a via name, are consumed silently. A DEF orientation after a via name (N, S, E, W, FN, FS, FE, FW) rotates or flips the via's rectangles about the via point before painting. The route-layer switch still follows the via's residues. Magic produces output, so this is the standing compat rule. (corrected 2026-11-13, was fix)

### Q-LEFDEF-73: Route RECT uses truncating division and undefined reference
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: RECT offsets use truncation toward zero for the reference point (other conversions floor); a RECT before any point uses an undefined reference; a parse error links an uninitialised rectangle for painting.
- Evidence: lef/defRead.c:285-347, lef/defRead.c:658-675
- Verified 2026-11-13: Confirmed at lef/defRead.c:287-348. Corners use refp/2 (C truncation), while wire points use >>=1 (floor, 663-668). refp is declared uninitialised (lef/defRead.c:95) and RECT does not check 'valid', so a RECT before any point reads an undefined reference. On a parse error, newRoute (malloc'd at 297, partly set) goes to endCoord and is linked and painted (684-697), which reads uninitialised fields.
- Options: replicate | fix (floor; require a reference; drop on parse error) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat for the rounding, fix for the undefined parts. Rounding: with magic-compat ON, trunc(P/2), as Magic does; with it OFF, floor(P/2), consistent with wire points. They differ only for negative odd doubled coordinates. Fix in both modes (X-2 tier 1, uninitialised reads): a RECT before any reference point reports 'No reference point for RECT' and is skipped; a RECT whose parse fails is reported with Magic's message and not painted. (corrected 2026-11-13, was fix)

### Q-LEFDEF-74: Net NONDEFAULTRULE applies only to the next routing statement
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The rule is cleared after each ROUTED/FIXED/COVER/NOSHIELD statement and is not applied if it appears after the routing.
- Evidence: lef/defRead.c:1207-1229
- Verified 2026-11-13: Confirmed: ruleset is cleared at net start (lef/defRead.c:1119) and after every ROUTED/FIXED/COVER/NOSHIELD (1207-1215), and is set at 1218-1229. A rule that appears after the wiring, or a second wiring statement, gets no rule. Magic's own def write never emits net-level NONDEFAULTRULE (it uses TAPERRULE), so round trips are unaffected.
- Options: replicate | fix (apply rule to the entire net regardless of position) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the rule applies only to the next routing statement, as Magic does. With it OFF, a net-level NONDEFAULTRULE applies to all regular wiring of the net wherever it appears in the statement. Hefesto parses the whole net statement before painting the net's routes, so a rule written after the wiring still applies; TAPER and TAPERRULE still override it locally. This is an output-value quirk (widths). (corrected 2026-11-13, was fix)

### Q-LEFDEF-75: NONDEFAULTRULES state bugs and table pollution
- Kind: bug
- Magic behavior: The `inlayer` flag is uninitialised on the first rule; looking up layer/via names inserts empty entries into the LEF/DEF layer table, after which a route `NEW <name>` on such a name keeps the previous route layer (or an undefined one on the first leg).
- Evidence: lef/defRead.c:800, lef/defRead.c:880-886, lef/defRead.c:163-169
- Options: replicate | fix (initialise; look up without inserting; report unknown layer)
- Recommendation: fix.
- Resolution: fix — the `inlayer` flag is initialised, names are looked up without inserting entries, and an unknown layer is reported (uninitialised flag) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-76: Placement and other non-layer blockages painted on stale layer
- Kind: bug
- Magic behavior: A blockage with no LAYER (e.g. `- PLACEMENT RECT ...`) paints its RECT on the last layer of a previous blockage (or an uninitialised type for the first).
- Evidence: lef/defRead.c:1790-1855
- Options: replicate | fix (ignore placement blockages or map to a dedicated type)
- Recommendation: fix (ignore placement blockages, with a message).
- Resolution: fix — blockages without a LAYER (placement blockages) are ignored with a message instead of painting a stale or uninitialised type (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-77: Polygonal DIEAREA uses only the first two points
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The FIXED_BBOX is formed from the first two vertices only.
- Evidence: lef/defRead.c:2637-2650
- Verified 2026-11-13: Confirmed at lef/defRead.c:2637-2650: LefReadRect reads two points, FIXED_BBOX is set from them, and LefEndStatement skips the remaining vertices.
- Options: replicate | fix (bounding box of all vertices) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, FIXED_BBOX comes from the first two points, as Magic does. With it OFF, FIXED_BBOX is the bounding box of all DIEAREA vertices, which is what a rectilinear die means. This changes a stored property value, so tier 2 applies. (corrected 2026-11-13, was fix)

### Q-LEFDEF-78: Fallback design cell name is the raw command argument
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Without a preceding DESIGN, the cell is named exactly as the `def read` argument (may include directory and `.def`); a later DESIGN renames it.
- Evidence: lef/defRead.c:2645, lef/defRead.c:2664
- Verified 2026-11-13: Confirmed: DefNewCell(inName) at lef/defRead.c:2646, 2665, 2680, 2690, 2698, 2706, 2733 (entry cited 2645/2664); inName is the raw `def read` argument (lef/lefCmd.c:257-265). A later DESIGN renames the cell (2589-2601).
- Options: replicate | fix (use basename without extension) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the cell is named exactly as the argument, as Magic does. With it OFF, it is named with the argument's basename without the '.def' extension, so the cell name cannot contain a directory or extension, and a later DESIGN still renames it. Only DEF files without a leading DESIGN statement are affected. Cell contents are intact, so this is not tier 1. (corrected 2026-11-13, was fix (basename without extension))

### Q-LEFDEF-79: UNITS multiplies cumulatively and accepts integers only
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Each UNITS statement multiplies the scale; non-integer values fall back to 100.
- Evidence: lef/defRead.c:2604-2618 (corrected 2026-11-13: Partly wrong. Multiplication is confirmed (oscale *= dscale, lef/defRead.c:2616; oscale is reset per read at 2555). But sscanf("%d") parses an integer prefix, so '1000.5' is read as 1000 without a message. Only a token with no leading integer gives 'Invalid syntax for UNITS statement.' and falls back to 100 (2607-2613).)
- Options: replicate | fix (set rather than multiply) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, each UNITS multiplies the scale, the integer prefix is taken, and a token with no leading integer falls back to 100 with Magic's messages. With it OFF, UNITS sets the scale, a repeated UNITS warns and replaces the previous one, and a value that is not a positive integer is reported and falls back to 100. This is a parse quirk that changes output values (tier 2). (corrected 2026-11-13, was fix: set, and warn on repeat)

### Q-LEFDEF-80: Annotation mode still paints pin geometry
- Kind: doc-mismatch
- Magic behavior: Documentation says `-annotate` adds no geometry, but PINS geometry is painted.
- Evidence: lef/defRead.c:1703, doc/html/def.html
- Options: replicate | fix (labels only)
- Recommendation: decide; replicate for byte compatibility unless users object.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-LEFDEF-81: Annotation terminal lookup uses the edit cell
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Terminal `<comp>/<pin>` is searched in the current edit cell, not the DEF design cell; tile search does not expand the area.
- Evidence: lef/defRead.c:1150-1170
- Verified 2026-11-13: Confirmed at lef/defRead.c:1155-1170: CmdFindNetProc(termname, EditCellUse, ...) and DBSrPaintArea over the unexpanded area r, although the comment at 1161 says 'Search the area (+1)'.
- Options: replicate | fix (search in the design cell) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the terminal is looked up in the current edit cell and the paint search uses the terminal area unexpanded, as Magic does. With it OFF, the terminal is looked up in the DEF design cell (rootDef), and the search area is the terminal area grown by 1 internal unit, as the code comment intends. Both differ from Magic only when the edit cell is not the design cell, or when paint touches the terminal without overlapping it. This changes which labels are placed (tier 2). (corrected 2026-11-13, was fix: use the design cell)

### Q-LEFDEF-82: Layer record after a via depends on hash iteration order
- Kind: ambiguity
- Magic behavior: After a via, the route layer's LEF record is the first record (in hash-table iteration order) of class route/masterslice whose type or obstruction type matches; widths may differ if several names map to the same magic type.
- Evidence: lef/defWrite.c:2041-2075, lef/defRead.c:500-515
- Options: replicate (unspecified order) | fix (deterministic: tech-file declaration order)
- Recommendation: fix with declaration order.
- Resolution: compat — the LEF layer table is name-keyed, so the record found after a via is the first match in Magic's string hash order, reproduced exactly (X-1 as amended by D-1, 2026-11-07).

### Q-LEFDEF-83: Duplicate component instance names
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: The link-uniqueness failure is unchecked; a second use with the same name is placed anyway, leaving two uses with the same id.
- Evidence: lef/defRead.c:2322, database/DBcellname.c:2409-2414
- Verified 2026-11-13: Confirmed: DBLinkCell's FALSE return (database/DBcellname.c:2407-2414, DBFindUse hit) is ignored at lef/defRead.c:2322, and DBPlaceCell runs anyway (2365). Two uses share an id and the id hash points to the first. Magic's own .mag reader renames duplicates on load ('Duplicate instance-id ... will be renamed', cell-file-format 'Instance identifiers on read') (corrected 2026-11-13: the rename happens only on the next load, through DBGenerateUniqueIds (database/DBio.c:1013) and dbFindNamesFunc (database/DBcellname.c:2690-2711). Magic's saved .mag and an in-session `def write` both still contain the duplicate id, and which use gets renamed follows DBCellEnum order, not necessarily the later DEF component). Hefesto's layout-hierarchy requires ids unique in the parent (layout-hierarchy spec line 36).
- Options: replicate | fix (rename with suffix and warn, or reject)
- Recommendation: fix (both modes). A component whose name equals an existing use in the design cell is placed and renamed with the cell-file-format rule: message 'Duplicate instance-id for cell <child> (<id>) will be renamed', then '<child>_<k>'. The database invariant (unique instance ids) cannot hold Magic's state. This is a deviation: Hefesto's `.mag` and `def write` output differ from Magic's at those instance names, since Magic writes the duplicate id and renames only on a later load (and then not necessarily the later component). The `tests/golden/deviations.toml` entry for Q-LEFDEF-83 names that difference.

### Q-LEFDEF-84: Unsupported FILLS/SLOTS/STYLES sections recovered noisily
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Not in the section table; each statement produces "Unknown keyword" messages and `END FILLS` produces "END statement out of context."
- Evidence: lef/defRead.c:2496-2526, lef/defRead.c:2738-2745
- Verified 2026-11-13: Confirmed: FILLS, SLOTS and STYLES are not in the sections table (lef/defRead.c:2470-2503). The section header and each '- ...' statement print 'Unknown keyword ... in DEF file; ignoring.' and are skipped to ';' (2562-2567). 'END FILLS' hits DEF_END, prints 'END statement out of context.' and the read continues (2737-2742). These count as errors in the DEF_SUMMARY.
- Options: replicate | fix (skip known-but-unsupported sections silently or paint FILLS) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto gives Magic's messages, error counts and recovery. With it OFF, FILLS, SLOTS and STYLES are known sections and are skipped to their END with one info message ('<SECTION> section not supported; skipped.'), adding no errors to the summary. Painting FILLS is out of scope. Only messages differ; the layout is the same in both modes. (corrected 2026-11-13, was fix: skip silently with one message)

### Q-LEFDEF-85: Placement of unplaced/no-placement components
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: UNPLACED puts the cell lower-left at the origin; a component with no placement keyword keeps the identity transform (master origin at 0,0).
- Evidence: lef/defRead.c:1324-1331, lef/defRead.c:2344-2370
- Verified 2026-11-13: Confirmed: UNPLACED calls DefReadLocation with noplace (lef/defRead.c:1324-1331), which warns and puts the bbox lower-left at (0,0) N. A component with no placement keeps the identity transform from DBCellNewUse (2316-2366). Any '( x y ) orient' after UNPLACED is skipped by the '+' scan.
- Options: replicate | fix
- Recommendation: replicate (both modes). Hefesto has no unplaced-instance concept and DEF gives no location to use, so Magic's choice (lower-left at origin with a warning for UNPLACED; identity transform when no placement keyword) has no corrected alternative.

### Q-LEFDEF-86: def read requires a window/box context [platform]
- Kind: platform
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Without a selected cell, a layout window, or box, all non-help subcommands fail with "Point to a window first", even reading.
- Evidence: lef/lefCmd.c:200-219
- Verified 2026-11-13: Confirmed at lef/lefCmd.c:203-218: every non-help subcommand needs a selected cell, a window, or a box cell, or it prints 'Point to a window first'. DefRead itself does not use selectedUse.
- Options: replicate | platform alternative (allow headless/iPadOS read without a window)
- Recommendation: platform. Hefesto allows `def read` (and the other non-write subcommands that do not need a target cell) headless and on iPadOS without a window or box; the read targets the document model. Subcommands that need a target cell (write) use the active document's root/selected cell, or report an error when none exists. This does not change file output.

### Q-LEFDEF-88: Event restriction and timer-driven progress [platform]
- Kind: platform
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: Tk event restriction during the read and SIGALRM-style 5 s progress timer.
- Evidence: lef/defRead.c:2537-2547, lef/lefRead.c:87-140
- Verified 2026-11-13: Confirmed: Tk_RestrictEvents around the read (lef/defRead.c:2541-2543, 2769-2772) and LefEstimate's SIGALRM-driven progress (lef/lefRead.c:87-133, PRINT_INTERVAL timer).
- Options: platform alternative (async progress callback, cancellable task)
- Recommendation: platform. The read runs as a task that blocks edits to the target document until it finishes (the equivalent of Magic's event restriction), and it reports progress through the platform progress service with optional cancellation. The text form '  Processed N of M <items> (P%).  Est. time remaining: Ts' is kept for the scripting console. Timing and the progress medium are not part of compatibility.

### Q-LEFDEF-89: Pin label sizing loops forever with no default width
- Kind: bug
- Magic behavior: If the pin layer has no DRC default width (0) and the rectangle height is > 0, the doubling loop never terminates.
- Evidence: lef/defRead.c:1715-1717, drc/DRCtech.c:4620-4665
- Options: replicate (n/a) | fix (use minimum size 1 or the rectangle height)
- Recommendation: fix.
- Resolution: fix — the pin label sizing loop terminates; with no default width the rectangle height is used (infinite loop) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-90: DEF write file mechanics (temporary _part file, search paths)
- Kind: platform
- Magic behavior: writes NETS/SPECIALNETS to a sibling temporary file `<base>_part.def`, then appends it to the main file and deletes it. If the name has a non-.def extension, the name is first opened as given. Paths are resolved by Magic's path-open helper.
- Evidence: lef/defWrite.c:3015-3043, lef/defWrite.c:3093-3203; lef/lefWrite.c:150-235
- Options: replicate the temp file | buffer in memory (same bytes) | on iPadOS resolve within the sandboxed document container
- Recommendation: buffer in memory and keep the naming rules for the final file. On iPadOS, write through the document picker or container.
- Resolution: platform — NETS and SPECIALNETS are buffered in memory instead of a `_part.def` temporary file (same bytes) and the final file keeps Magic's naming rules; on iPadOS it is written through the document model (X-4, hefesto-files-and-pdks, 2026-11-07).

### Q-LEFDEF-91: VIAS section writes uncounted via records
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the VIAS writer prints every class-via layer-table record whose name has at least two underscores. This includes via definitions read from LEF/DEF (e.g. `via_1_2`), not just vias generated by this write, but the `VIAS n ;` count includes only the generated ones. The count can therefore be wrong.
- Evidence: lef/defWrite.c:2130-2140, lef/defWrite.c:1990-1996
- Verified 2026-11-13: Confirmed. The writer filter (lef/defWrite.c:2128-2136) takes any CLASS_VIA record whose canonName has two distinct underscores. The count is the number of records newly created by defCountViaFunc (lef/defWrite.c:1944-1950), printed at 3035-3036 (entry cited 1990-1996 for the count).
- Options: replicate | fix (write only records generated by this write) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto uses Magic's filter and count, so via definitions from LEF/DEF with two or more underscores are written but not counted. With it OFF, the VIAS section contains exactly the via records generated by this write, so the count matches the records. A count mismatch is an output-value defect, not data loss, so the standing rule applies. (corrected 2026-11-13, was fix)

### Q-LEFDEF-92: Cut-type layer map can resolve to a VIA definition name
- Kind: bug
- Magic behavior: the magic-to-LEF map for contact types takes the first via-class record, in hash order, whose type matches. A VIA definition read from LEF (e.g. `M1M2_PR`) can win over the cut layer, so the VIAS RECT lines and blockage names use a via name as the layer name.
- Evidence: lef/defWrite.c:2041-2081, lef/defWrite.c:2910-2928
- Options: replicate | fix (prefer cut-layer records, i.e. records without via geometry)
- Recommendation: fix. Prefer true cut layers.
- Resolution: compat — the magic-to-LEF map is name-keyed, so the first via-class record in Magic's string hash order wins, reproduced exactly; a VIA definition such as `M1M2_PR` can win over the cut layer (X-1 as amended by D-1, 2026-11-07).

### Q-LEFDEF-93: COMPONENTS placement subtracts the untransformed bbox origin; arrays do not
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: a non-arrayed use is placed at `T(R).ll − R.ll`, where R.ll is not transformed. Array elements are placed at `T(R).ll`. When R.ll ≠ (0,0) and the orientation is not N, the two disagree, and neither may round-trip through `def read`.
- Evidence: lef/defWrite.c:2804-2815, lef/defWrite.c:2860-2897 (corrected 2026-11-13: Formula confirmed, round-trip claim wrong. The non-array path writes cu_bbox.ll - R.ll + off (lef/defWrite.c:2864-2888); the array path writes T(R).ll (2790-2819). The array path is the consistent one: def read places T(R).ll at the given point (lef/defRead.c:1408-1410), and lef write puts ORIGIN at -boundary.ll (lef/lefWrite.c:1367-1369), so the DEF placement point of a Magic-written macro is T(R).ll. Only the non-array path is wrong when R.ll is not (0,0), for any orientation (including N).)
- Options: replicate | fix (always write T(R).ll − (orientation-transformed origin offset) consistent with def-read) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto uses Magic's two formulas. With it OFF, every instance, arrayed or not, is written at T(R).ll (R = FIXED_BBOX if present, else the cell bbox). This round-trips through def read and matches the ORIGIN convention of Magic's own LEF. The bug only shows for cells whose bbox lower-left is not at the origin. It changes output values (tier 2), so the standing rule replaces the original 'replicate'. (corrected 2026-11-13, was replicate for compatibility, and confirm round-trip behaviour with def-read golden cases first)

### Q-LEFDEF-94: Component count wrong for arrays with reversed index ranges
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the count adds (xhi−xlo+1)(yhi−ylo+1) without normalizing, so an array declared 3:0 contributes −2·… while 4 elements are written.
- Evidence: lef/defWrite.c:2300-2306
- Verified 2026-11-13: Confirmed at lef/defWrite.c:2300-2306. sx = xhi-xlo+1 can be negative; the product is computed in unsigned long and added to a pointertype, which in effect adds the signed product (e.g. 3:0 x 0:0 adds -2; 3:0 x 3:0 adds 4 for 16 elements). The ASSERT(sx >= 0 && sy >= 0) is compiled out in default builds (NDEBUG, scripts/configure.in:296) and aborts only with --enable-assertions.
- Options: replicate | fix (use |xhi−xlo|+1) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the count uses Magic's signed product (xhi-xlo+1)(yhi-ylo+1), as in a default (NDEBUG) build. With it OFF, (|xhi-xlo|+1)(|yhi-ylo|+1), so the count matches the number of COMPONENTS records written. A wrong header count changes an output value and is not data loss. (corrected 2026-11-13, was fix)

### Q-LEFDEF-95: Regular-net endpoint inset uses integer half width
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: regular wire endpoints are inset by ⌊W/2⌋·s, not W·s/2, so odd widths produce endpoints half a unit off the true wire extent (DEF readers extend by W/2).
- Evidence: lef/defWrite.c:1240-1290
- Verified 2026-11-13: Confirmed at lef/defWrite.c:1245-1281: routeWidth and rule->width are int, so W/2 is integer division before multiplication by oscale.
- Options: replicate | fix (inset by W·s/2) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the inset is floor(q/2)*s, as Magic does. With it OFF, the inset is q*s/2, so a DEF reader that extends by half the width rebuilds the wire's true extent. Like the centreline coordinates Magic already writes, the value may end in .5 DBU when q*s is odd. This changes output values (tier 2), so compat replaces the original 'replicate'. (corrected 2026-11-13, was replicate (exact output), and document it)

### Q-LEFDEF-96: Dogleg test compares the previous y with x2
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the dogleg condition is `prev.x != x1 && prev.y != x2`. It was presumably meant to be `prev.y != y1`.
- Evidence: lef/defWrite.c:1513 (corrected 2026-11-13: Behaviour confirmed, line wrong: the test is at lef/defWrite.c:1534 (not 1513): (orient == defdata->orient) && (defdata->x != x1) && (defdata->y != x2).)
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the test compares the previous y with x2, as Magic does. With it OFF, it compares the previous y with y1, the evident intent. Output differs only for continuations in the same orientation where y == y1 but y != x2 (or the reverse). This changes route records (tier 2), so compat replaces the original 'replicate'. (corrected 2026-11-13, was replicate for byte compatibility)

### Q-LEFDEF-97: Column-count inaccuracies in net output
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `TAPERRULE <name> ` text is not added to the line-break column. In the vertical restart branch the column is reset after the width is written, so the width is not counted. The `( PIN name )` text is not counted either.
- Evidence: lef/defWrite.c:1478-1500, lef/defWrite.c:1560-1575, lef/defWrite.c:716-717 (corrected 2026-11-13: Behaviour confirmed, lines adjusted. 'TAPERRULE <name> ' is printed without column accounting at lef/defWrite.c:1490 and 1518. The vertical restart resets outcolumn after the width at 1589-1595 (the horizontal branch at 1559-1567 counts correctly; entry cited 1560-1575). '( PIN name )' is uncounted at 720-721 (entry: 716-717).)
- Options: replicate | fix
- Recommendation: replicate (both modes). The column counter only decides where Magic inserts line breaks. Every placement is valid DEF and the geometry is unchanged, and Magic defines no corrected layout to match. A second counting mode would double the golden surface for a cosmetic difference (as for M5 rows where the quirk is a formatting choice that byte identity needs).

### Q-LEFDEF-98: NONDEFAULTRULES writes the first layer entry repeatedly; WIREEXT unscaled
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the per-layer loop always prints `nrules->rule` (the first entry) instead of the loop variable, so a multi-layer rule read from LEF prints its first layer N times. WIREEXT prints `extend/2.0` without multiplying by the output scale.
- Evidence: lef/defWrite.c:3125-3132 (corrected 2026-11-13: Behaviour confirmed, lines wrong: the loop is at lef/defWrite.c:3162-3170 (not 3125-3132) and prints nrules->rule (the first entry) on every pass. extend is stored in doubled internal units (lef/lefRead.c:2031, lef/defRead.c:953: roundf(2*v/oscale)), so the correct WIREEXT is extend/2*s, while Magic prints extend/2 (internal units). Rules created by def write have one entry and extend 0 (lef/defWrite.c:1197-1208), so only rules from lef/def read show the defect.)
- Options: replicate | fix (print each entry, scale the extension) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the first entry is written N times and WIREEXT is extend/2, as Magic does. With it OFF, each entry's own layer, width and extension are written, with WIREEXT = extend/2*s in DEF units. This is an export-value defect, not loss of database data, so tier 2. With Q-LEFDEF-99 OFF, LEF-read rules are written only when referenced, so the OFF fix rarely shows. (corrected 2026-11-13, was fix both, after confirming the units in which lef-read stores `extend`)

### Q-LEFDEF-99: Non-default rules persist across writes and include LEF/DEF-read rules
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the non-default rule table is global. Rules created by previous `def write` runs and rules read from LEF/DEF files are written into every subsequent DEF until the technology is reloaded.
- Evidence: lef/defWrite.c:1170-1195, lef/defWrite.c:3063, lef/defWrite.c:3205 ("To do: Clean up") (corrected 2026-11-13: Behaviour confirmed, lines wrong. Rules are created in the global LefNonDefaultRules table at lef/defWrite.c:1189-1209 (not 1170-1195). The count is ht_nEntries at 3101 (not 3063), and the 'To do: Clean up nondefault rules tables' comment is at 3218 (not 3205). Also, a write-created name '<layer>_width_<n>' that collides with a LEF-read rule reuses that rule (HashFind at 1193-1195).)
- Options: replicate | fix (write only rules referenced by this file's nets) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, all entries of the global rule table are written, as Magic does: rules from earlier writes and from lef/def read. With it OFF, only rules referenced by a TAPERRULE in this file's nets are written, and rules created by a write are discarded after it. LEF/DEF-read rules stay in the table for lef write and later reads, and a name collision with a LEF-read rule creates a write-local rule rather than reusing it. Extra rules change the output but lose no data, so tier 2 (the original 'fix' becomes compat). (corrected 2026-11-13, was fix)

### Q-LEFDEF-100: -anal dereferences cleared layer-table slots
- Kind: bug
- Magic behavior: the `-anal` loop reads `lefl2->lefClass` without a NULL check. Generated-via slots cleared by a previous write are NULL, so this crashes.
- Evidence: lef/defWrite.c:3090-3100, lef/lefTech.c:151-180
- Options: replicate (crash) | fix (skip empty slots)
- Recommendation: fix. The spec already requires skipping.
- Resolution: fix — `-anal` skips empty layer-table slots (NULL dereference) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-101: Blockage collector called with mismatched arguments
- Kind: bug
- Magic behavior: the blockage collector for non-port, non-special nets is declared (tile, plane, data), but the connectivity search calls it with (tile, dinfo, plane, data). The data pointer receives a plane number, which is undefined behaviour and in practice crashes whenever such a net has geometry.
- Evidence: lef/defWrite.c:2614, lef/defWrite.c:2642-2646, database/DBconnect.c:456
- Options: replicate (impossible: crash) | fix (implement intended collection)
- Recommendation: fix, as specified. Golden cases must avoid floating nets until this is confirmed on a real Magic build.
- Resolution: fix — the blockage collector receives its intended arguments and collects the net's geometry (crash from mismatched arguments) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-102: Special-net classification depends on Tcl variables
- Kind: platform
- Magic behavior: top-level nets are special if they are in the Tcl array `globals`, equal `$VDD` or `$GND`, or (Tcl builds only) are labeled, not ports and not `#`-terminated. Non-Tcl builds use only the `!` suffix.
- Evidence: lef/defWrite.c:328-372
- Options: replicate the Tcl build with Hefesto settings (global-net list, VDD/GND names) | expose as preferences
- Recommendation: provide a project setting for the global nets and VDD/GND, and apply the Tcl-build rules.
- Resolution: platform — decided by D-14 (2026-11-07). DEF applies the Tcl-build rules with `globalNets`, `vdd` and `gnd` from the project setting "global nets and power names" (`hefesto-settings`); the trailing-`!` rule stays as an explicit additional rule.

### Q-LEFDEF-103: PINS formatting defects
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the `+ LAYER ...` fragment has no trailing newline, so `+ PLACED` follows on the same line. Port classes 0x60/0x70 and uses RESET/SCAN/TIEOFF print `+ DIRECTION ` / `+ USE ` with an empty keyword. RESET/SCAN/TIEOFF are not valid DEF pin USE values, but a blank is invalid too.
- Evidence: lef/defWrite.c:2395-2455 (corrected 2026-11-13: Partly wrong. The missing newline is confirmed: the '+ LAYER' fprintf has no '\n' (lef/defWrite.c:2436-2440), so '        + PLACED' follows on the same line (2441). Empty keywords are confirmed (2376-2420). But RESET, SCAN and TIEOFF ARE valid DEF PINS USE values (DEF 5.8: SIGNAL | POWER | GROUND | CLOCK | TIEOFF | ANALOG | SCAN | RESET), and Magic's own def reader accepts them (lef/defRead.c:1503-1534). Classes 0x60/0x70 are undefined in Magic (database/database.h.in:282-289) and cannot be set through commands. Lines are 2376-2441, not 2395-2455.)
- Options: replicate | fix (newline; omit unknown keywords or map to SIGNAL) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto writes Magic's bytes: no newline before '+ PLACED', and '+ USE ' / '+ DIRECTION ' with an empty keyword. With it OFF, '+ USE RESET', '+ USE SCAN' and '+ USE TIEOFF' are written (valid DEF, no data dropped), a DIRECTION line for an undefined class is omitted, and '+ LAYER ...' ends with a newline. Dropping the use on export is an output-value defect of an export format, not native-save data loss, so compat rather than fix. (corrected 2026-11-13, was replicate the missing newline (it is valid DEF))

### Q-LEFDEF-104: -units argument handling
- Kind: bug
- Magic behavior: `-units` as the last argument checks `argc < i` (off by one) and then reads `argv[argc]` (NULL), which can crash. Values ≤ 0 are not rejected and produce a zero or negative scale.
- Evidence: lef/lefCmd.c:396-414
- Options: replicate | fix (report the missing argument; reject n ≤ 0)
- Recommendation: fix.
- Resolution: fix — a missing `-units` argument is reported and values ≤ 0 are rejected (NULL `argv` read) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-105: Asymmetric directional-surround probe
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: to decide via orientation, the probe area is x ∈ [xl − d + s, xh + d + s], so it is shifted right by 2s instead of [xl − d − s, xh + d + s].
- Evidence: lef/defWrite.c:1938-1955 (corrected 2026-11-13: Behaviour confirmed, lines wrong: the probes are at lef/defWrite.c:1910-1931 (r2.r_xbot = rorig.r_xbot - ldist + sldist at 1915, - hdist + shdist at 1926), not 1938-1955. The y extent is symmetric (+-s); only x is shifted right by 2s.)
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the probe spans x from xl - d + s to xh + d + s, as Magic does. This affects via names (the _hv suffixes) and via geometry, which sky130-style directional surrounds depend on. With it OFF, the probe is symmetric, from xl - d - s to xh + d + s. A sign slip that changes output values is tier 2, so compat replaces the original 'replicate'. (corrected 2026-11-13, was replicate (it affects via names))

### Q-LEFDEF-106: Uninitialized surround for single-residue contacts
- Kind: bug
- Magic behavior: if a contact type has only one residue, the high-layer surround variable is used uninitialized when residue rectangles are created.
- Evidence: lef/defWrite.c:1920-1937, lef/defWrite.c:2002
- Options: fix (treat as 0)
- Recommendation: treat as 0.
- Resolution: fix — the high-layer surround of a single-residue contact is 0 (uninitialised) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-107: Inverted y warning in via cut generation
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the "via size ... area height" warning fires when the centred cut fits (bottom ≥ Y1) instead of when it does not.
- Evidence: lef/defWrite.c:2208-2216
- Verified 2026-11-13: Confirmed at lef/defWrite.c:2208-2213: the y warning fires when square.r_ybot >= r->r_ybot (cut fits), whereas the x warning (2196-2200) fires when left < r_xbot.
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the warning fires when bottom >= Y1, as Magic does. With it OFF, it fires when bottom < Y1, the same as the x test. Only a message changes; the DEF bytes are identical in both modes. (corrected 2026-11-13, was fix (the message is informative only))

### Q-LEFDEF-108: Byte-exact route ordering depends on tile structure and traversal
- Kind: ambiguity
- Magic behavior: the order of route segments, NEW records and jogs follows the corner-stitched tile decomposition and the LIFO connectivity traversal. Different but equivalent decompositions or traversals give different (but equally valid) DEF.
- Evidence: database/DBconnect.c:395-640, lef/defWrite.c:765-768
- Options: replicate the traversal exactly | compare nets semantically (geometry union per net) in golden tests
- Recommendation: replicate if Hefesto uses maximal-horizontal-strip tiles. Otherwise compare NETS semantically and everything else byte-exactly.
- Resolution: compat — tile order: route segments follow Magic's maximal-horizontal-strip decomposition and traversal and are byte-identical (X-1 tile order, 2026-11-07).

### Q-LEFDEF-109: `def write` help shows `[cell]`, but the argument is the output file name
- Kind: doc-mismatch
- Magic behavior: the help line says `write [cell] [-allspecial]`, but the argument is used as the output file name. The written cell is always the selected or root cell. The help also omits `-units` and `-anal`.
- Evidence: lef/lefCmd.c:177, lef/lefCmd.c:432-440
- Options: replicate | fix help text
- Recommendation: fix the help text.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-LEFDEF-110: Stale .ext used without check
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: nets come from existing `.ext` files. Nothing checks that they match the current layout, so stale files produce wrong nets or "floating label" errors.
- Evidence: lef/defWrite.c:273-283
- Verified 2026-11-13: Confirmed at lef/defWrite.c:265-283 (EFReadFile with no staleness check). Magic intends one: .ext starts with 'timestamp <cd_timestamp>' (extract/ExtCell.c:573), and extflat leaves 'To-do: compare timestamp against the cell' (extflat/EFread.c:647).
- Options: replicate | warn when the cell is newer than the .ext | auto-extract | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. Both modes read nets from the existing .ext without re-extracting, so the DEF is identical. With magic-compat ON, there is no check, as in Magic. With it OFF, Hefesto compares the .ext timestamp with the cell's timestamp (and modified flag) and warns '<cell>.ext is out of date; nets may be wrong. Run extract.' when they differ. This is the check Magic's own to-do describes. (corrected 2026-11-13, was replicate, and add a staleness warning)

### Q-LEFDEF-111: Naive contact-region merging
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: a contact region is the starting tile plus the tiles chained straight up from its top-left neighbour and right from its right-top neighbour. Non-rectangular or L-shaped contact areas are split or merged inconsistently.
- Evidence: lef/defWrite.c:1782-1858
- Verified 2026-11-13: Confirmed at lef/defWrite.c:1786-1856: the region is the seed tile plus the chain upward from RT(tile) and rightward from TR(tile), merged with GeoInclude. For L-shaped areas the bounding box overshoots the contact, so the written via covers area that has no contact.
- Options: replicate | fix (proper maximal-rectangle decomposition) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto uses Magic's chain-and-bounding-box rule, needed for byte-identical via names and positions. With it OFF, each contact region is split into rectangles that lie wholly inside the contact area. These are Magic's maximal horizontal strips, merged vertically only when their x extents are equal, so a rectangular contact still gives one via, as in Magic. The overshoot writes cut and metal that are not in the layout, an output-value defect, so compat replaces the original 'replicate'. (corrected 2026-11-13, was replicate)

### Q-LEFDEF-112: Repeated geometry for connected seeds
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: each location seed of a node starts a fresh connectivity search. Seeds connected through top-level paint emit the same geometry again.
- Evidence: lef/defWrite.c:752-777
- Verified 2026-11-13: Confirmed at lef/defWrite.c:754-790: every efnode_disjoint seed starts a new DBSrConnect, so seeds that connect through top-level paint emit the same route geometry again.
- Options: replicate | fix (skip tiles already emitted for the net) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, each seed's search emits its geometry, duplicates included, as Magic does. With it OFF, tiles already emitted for the net are not emitted again, so a seed whose region was covered by an earlier search adds nothing. Duplicate wires are valid DEF but are redundant output, so this is the standing compat rule (the original was 'replicate'). (corrected 2026-11-13, was replicate)

### Q-LEFDEF-113: Fixed-size name buffers
- Kind: bug
- Magic behavior: net names are built in 256-byte static buffers with no bounds check. The fallback via name is truncated to 23 characters.
- Evidence: lef/defWrite.c:477, lef/defWrite.c:668-669, lef/defWrite.c:1448-1450
- Options: fix (unbounded strings)
- Recommendation: fix.
- Resolution: fix — net and via names are unbounded strings (unchecked 256-byte buffers) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-114: Pin layer error message arguments swapped
- Kind: bug
- Magic behavior: `No LEF layer corresponding to layer %s of pin "%s".` is printed with the label text and the layer name swapped.
- Evidence: lef/defWrite.c:2432-2435
- Options: replicate | fix
- Recommendation: fix (informative message).

### Q-LEFDEF-115: `(null)` layer names in VIAS
- Kind: bug
- Magic behavior: a residue type of a generated via with no LEF mapping is printed through `%s` with a NULL pointer, giving `(null)` on glibc and macOS.
- Evidence: lef/defWrite.c:2154-2160
- Options: replicate | fix (skip the RECT)
- Recommendation: fix (skip it, and warn).
- Resolution: fix — a generated-via residue with no LEF mapping is skipped with a warning instead of printing a NULL string as `(null)` (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-116: Hash ordering depends on char signedness
- Kind: platform
- Magic behavior: the string hash adds key bytes as C `char`. For non-ASCII names, the enumeration order (VIAS, NONDEFAULTRULES, BLOCKAGES, layer map) differs between platforms where `char` is signed and platforms where it is unsigned.
- Evidence: utils/hash.c:229-232
- Options: signed | unsigned
- Recommendation: signed, which matches Magic on x86-64 Linux and on macOS.
- Resolution: compat — the string hash adds key bytes as signed values on every platform, matching Magic on x86-64 Linux and macOS (X-1 as amended by D-1, 2026-11-07).
