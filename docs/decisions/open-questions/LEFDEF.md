# Open questions: LEFDEF (LEF/DEF input and output)

Each entry records Magic behavior (commit fc5daa9e) that looks accidental, ambiguous or platform-specific. The specs describe Magic's actual behavior and point to these IDs. ID ranges: 1-29 tech section and LEF read, 30-59 LEF write, 60-89 DEF read, 90-119 DEF write. Gaps in the numbering are intentional.

### Q-LEFDEF-1: Route width and spacing defaults are effectively never filled
- Kind: bug
- Magic behavior: Route width and spacing are set to the sentinel -1 when DRC rules exist. Contact areas start at (-1,-1,-1,-1). The fill step runs only when the internal grid is not 1:1, and it runs after the sentinel has been scaled by n/d. A scaled sentinel generally no longer equals -1: it becomes -n/d when n > d and 0 when d > n, so it is not filled. With a 1:1 grid the fill step never runs. In practice the route width stays negative or zero unless a LEF LAYER section sets WIDTH. `def read` then falls back to that value for default wire widths, and `lef write -tech` omits WIDTH. Contact areas stay null, so the `contact` keyword behaves like `cut`, except on shrinking grids where the fill step does run.
- Evidence: lef/lefTech.c:350-368, 477-516, utils/tech.c:817-830, lef/defRead.c:198-216
- Options: replicate | fix (fill width, spacing and contact area from the DRC defaults after every tech load and grid change)
- Recommendation: Fix, and record it in the compatibility notes. Capture LEFDEF-002/003 with Magic to confirm the observed width values before deciding.

### Q-LEFDEF-2: Default spacing written into the width field (read-side consequence)
- Kind: bug
- Magic behavior: This is the same defect as Q-LEFDEF-31, seen from the reader. When the fill step runs and the spacing is still -1, the DRC default spacing overwrites the route width and the spacing stays -1. `def read` then uses the spacing value as the default wire width.
- Evidence: lef/lefTech.c:510-512
- Options: replicate | fix (assign to spacing)
- Recommendation: Fix together with Q-LEFDEF-1 and Q-LEFDEF-31.

### Q-LEFDEF-3: Secondary obstruction type and contact area carry over between LAYER statements
- Kind: bug
- Magic behavior: In a PORT or OBS block, the secondary obstruction type is updated only when a LAYER names a VIA-class entry that has an obstruction type. A later LAYER that names a routing layer keeps the previous secondary type, so its OBS rectangles also paint that via-obstruction type. Likewise, a LAYER that is resolved through the magic-name fallback does not reset the contact area, so a cut-size override from an earlier LAYER can be applied to it.
- Evidence: lef/lefRead.c:718-748, 1176-1224
- Options: replicate | fix (reset both at every LAYER statement)
- Recommendation: Fix. The current behavior paints spurious obstruction material.

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
- Magic behavior: Every `SPACING` statement overwrites the stored spacing with its first number, ignoring RANGE, ENDOFLINE, PARALLELRUNLENGTH and similar qualifiers. The last statement wins, even when it describes a wide-wire rule.
- Evidence: lef/lefRead.c:2829-2840
- Options: replicate | fix (keep the first plain SPACING, or the minimum)
- Recommendation: Replicate. The value affects `lef write -tech` output.

### Q-LEFDEF-7: Unsupported LEF geometry constructs
- Kind: quirk
- Magic behavior: LEF 5.6+ generated via definitions (`VIA name ... VIARULE ... CUTSIZE ... ROWCOL ... END`) produce no geometry. `RECT MASK n`, `RECT ITERATE`, `POLYGON MASK`, and POLYGON in VIA definitions fail or are skipped. Via definitions are read only from LAYER/RECT pairs.
- Evidence: lef/lefRead.c:2749-2786, 868-936
- Options: replicate | fix (support LEF 5.8 via rules and MASK)
- Recommendation: Replicate for compatibility, and track as a future extension.

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
- Magic behavior: `HARDSPACING ;`, `USEVIA n ;`, `USEVIARULE n ;`, `MINCUTS a b ;` and `PROPERTY a b ;` leave the `;` unread. The `;` is then treated as an unknown keyword and the statement after it is skipped. A `VIA` whose name already exists has its body parsed as rule statements instead of being skipped.
- Evidence: lef/lefRead.c:1928-1990
- Options: replicate | fix (consume through `;`; skip the bodies of known VIAs)
- Recommendation: Fix. Replication silently loses rules, and golden case LEFDEF-020 records Magic's behavior.

### Q-LEFDEF-11: `lef read -annotate` still creates new cells
- Kind: doc-mismatch
- Magic behavior: The documentation says `-annotate` ignores macros that are not already cells. In the code, the option only stops renamed duplicate macros from being created. Every other unknown macro is created normally. Annotation mode is chosen only when the cell already exists, whether or not `-annotate` is given.
- Evidence: lef/lefRead.c:2127-2158, doc/html/lef.html
- Options: replicate | fix (with -annotate, skip every macro whose cell does not exist)
- Recommendation: Fix to match the documentation. It is a low-risk change because users rely on the documented meaning.

### Q-LEFDEF-12: ORIGIN shifts geometry but not FIXED_BBOX
- Kind: ambiguity
- Magic behavior: Macro geometry is offset by −ORIGIN (and −FOREIGN point). The stored FIXED_BBOX is (FOREIGN pt) to (FOREIGN pt + SIZE), independent of ORIGIN. With a nonzero ORIGIN, the box and the geometry therefore disagree by ORIGIN. LEF says ORIGIN gives the offset from the macro geometry to the placement origin, so this may be a sign error.
- Evidence: lef/lefRead.c:2214-2228, 2364-2368
- Options: replicate | fix (shift the box by the same offset)
- Recommendation: Replicate until LEFDEF-012 confirms Magic's output, then decide together with how `def read` places components using FIXED_BBOX.

### Q-LEFDEF-13: `DIRECTION OUTPUT TRISTATE` reads as OUTPUT
- Kind: bug
- Magic behavior: Only the first token after DIRECTION is matched, so `OUTPUT TRISTATE` becomes output and the tristate port class is never produced by LEF read.
- Evidence: lef/lefRead.c:1546-1563, 1699-1707
- Options: replicate | fix (map OUTPUT TRISTATE to tristate)
- Recommendation: Fix. The tristate class exists in the database and `lef write` emits `OUTPUT TRISTATE`, so the round trip currently loses it.

### Q-LEFDEF-14: USE values not recognized on read
- Kind: quirk
- Magic behavior: The database has port uses reset, scan and tie-off, but LEF read accepts only DEFAULT, SIGNAL, ANALOG, POWER, GROUND and CLOCK. The other uses report `Improper USE statement`.
- Evidence: lef/lefRead.c:1565-1582, database/database.h.in:292-301
- Options: replicate | fix (accept SCAN, TIEOFF and RESET)
- Recommendation: Fix. The change is additive.

### Q-LEFDEF-15: PATH and VIA pin/obstruction geometry ignored
- Kind: quirk
- Magic behavior: `WIDTH`/`PATH` and `VIA` statements in PORT and OBS produce no paint, so pins drawn as paths or via instances vanish.
- Evidence: lef/lefRead.c:1219-1224, 1318-1323
- Options: replicate | fix (paint paths with the current WIDTH, and instantiate via definitions)
- Recommendation: Replicate for now, and track as an extension.

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
- Magic behavior: A VIA definition RECT on a layer declared `ignore` reports `No layer defined for RECT.` once per rectangle. The same layer in a macro OBS is skipped silently.
- Evidence: lef/lefRead.c:2652-2660, 920-925
- Options: replicate | fix (silent skip)
- Recommendation: Fix. This changes messages only.

### Q-LEFDEF-21: LEF annotation alters existing layout and non-port labels
- Kind: quirk
- Magic behavior: In annotation mode, accepted PORT rectangles are painted into the existing cell, which changes the layout. When a pin has both port and non-port labels of the same name and the scan stops at a non-port label before annotating anything, that non-port label is converted into a port with the LEF geometry. The code comment says the opposite is intended.
- Evidence: lef/lefRead.c:1236-1258, 1634-1704
- Options: replicate | fix (annotate labels only and never paint; leave non-port labels alone)
- Recommendation: Replicate the label logic, which the golden cases capture. Consider the no-paint fix separately.

### Q-LEFDEF-22: LEF read is not undoable
- Kind: platform
- Magic behavior: Undo is disabled for the whole read and the message `This action cannot be undone.` is printed.
- Evidence: lef/lefRead.c:3018-3020, 3195
- Options: replicate | platform alternative (make the read one undoable transaction)
- Recommendation: Hefesto MAY make it undoable. File content is unaffected.

### Q-LEFDEF-23: Polygon port labels from multi-plane types
- Kind: bug
- Magic behavior: When a polygon layer occupies several planes (contact types), its decomposition is painted on each plane, but only the rectangles from the last plane become port labels. The lists from earlier planes are leaked.
- Evidence: lef/lefRead.c:1045-1065
- Options: replicate | fix
- Recommendation: Replicate. The label set is equivalent because every plane receives the same decomposition.

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
- Magic behavior: Tech defaults are filled in after scaling. When the spacing is still unset (-1), Magic assigns the DRC default spacing to the route *width* and leaves the spacing at -1. As a result, `lef write -tech` prints `WIDTH <default spacing>` and no SPACING line, unless `lef read` has set the values.
- Evidence: lef/lefTech.c:507-511
- Options: replicate | fix (assign the spacing to the spacing field)
- Recommendation: Replicate in the `-tech` writer for compatibility, and flag it to the `lef-tech-section` owner. Fixing it changes the WIDTH and SPACING lines.

### Q-LEFDEF-32: CUT AREA value and keyword
- Kind: quirk
- Magic behavior: The default contact rectangle is (-w,-w)-(w,w), where w is the DRC width, so the printed `CUT AREA` is 4*w^2*oscale^2. `CUT AREA` is also not a standard LEF LAYER keyword; standard LEF uses `AREA` or `WIDTH`.
- Evidence: lef/lefTech.c:495-503, lef/lefWrite.c:410-418
- Options: replicate | fix (emit `WIDTH` from the true cut size)
- Recommendation: Replicate. Other tools ignore or reject the keyword in the same way for Magic and Hefesto output.

### Q-LEFDEF-33: SITE size uses the wrong cell's FIXED_BBOX
- Kind: bug
- Magic behavior: When a site cell has the fixed-bbox flag, the SITE SIZE is read from the `FIXED_BBOX` property of the cell being written (the root), not from the site cell.
- Evidence: lef/lefWrite.c:346-362
- Options: replicate | fix (read the site cell's FIXED_BBOX)
- Recommendation: fix, with an opt-in compatibility mode. Cells created by LEF read for SITEs normally carry FIXED_BBOX equal to their size.

### Q-LEFDEF-34: Header written only for the edit root; LEF database units are stale otherwise
- Kind: quirk
- Magic behavior: `lef write` writes VERSION, UNITS, sites and properties only when the target cell is the edit root. When a subcell instance is selected, the file has no header. In that case the LEF database units used for rounding are left over from the previous header write, or are 1000 if no header has been written.
- Evidence: lef/lefCmd.c:430-433, lef/lefWrite.c:2384-2396, lef/lefWrite.c:66, lef/lefWrite.c:279
- Options: replicate | fix (always write the header, and always recompute the units)
- Recommendation: Always recompute the units, which is harmless. Keep the header-less output for selected subcells for compatibility.

### Q-LEFDEF-35: Name legalization is ineffective
- Kind: bug
- Magic behavior: The routine meant to replace `;# -*$\n` with `_` detects a bad character only if it is the last character of the name. Its result is also never used. Pin and macro names are therefore written verbatim, even when this makes the LEF illegal (for example `a#1` or names containing spaces).
- Evidence: lef/lefWrite.c:976-1018, lef/lefWrite.c:1785-1788
- Options: replicate (verbatim) | fix (sanitize, which breaks name matching with DEF and netlists)
- Recommendation: Replicate verbatim output. Optionally emit a warning.

### Q-LEFDEF-36: Unhandled port class/use values give an empty keyword
- Kind: bug
- Magic behavior: The port uses reset, scan and tieoff produce `    USE  ;`. The undefined class encodings 0x60 and 0x70 produce `    DIRECTION  ;`. Both lines are malformed LEF.
- Evidence: lef/lefWrite.c:1040-1090, database/database.h.in:282-301
- Options: replicate | fix (write `USE RESET`/`USE SCAN`/`USE TIEOFF`; LEF 5.8 defines none of these, and SIGNAL is a reasonable fallback)
- Recommendation: fix (write SIGNAL), with an opt-in compatibility mode.

### Q-LEFDEF-37: -pinonly larger than -hide setback has no effect
- Kind: bug
- Magic behavior: With `-hide d` and `-pinonly p` where p > d, the second removal takes a rectangle (B shrunk by p) that lies inside the area already removed (B shrunk by d). It therefore does nothing.
- Evidence: lef/lefWrite.c:1612-1619
- Options: replicate | fix (intended semantics unclear)
- Recommendation: replicate.

### Q-LEFDEF-38: Setback strips copy whole tiles
- Kind: quirk
- Magic behavior: With `-hide d` and d > 0, obstruction geometry is collected by copying whole tiles that overlap the four setback strips, without clipping. Tiles that cross into the hidden interior therefore appear in full, and the result depends on the tile decomposition.
- Evidence: lef/lefWrite.c:1921-1956, lef/lefWrite.c:655-745
- Options: replicate | fix (clip to the strips)
- Recommendation: Replicate, because golden comparison requires it.

### Q-LEFDEF-39: Split tile with both halves writable
- Kind: quirk
- Magic behavior: If both triangles of one diagonal tile are writable LEF types, only the left half is written, because the tile is marked visited after the first half. The source comment also claims that port mode ignores split tiles, but the code writes POLYGONs in all modes except the contact rounds.
- Evidence: lef/lefWrite.c:840-849, lef/lefWrite.c:436-449
- Options: replicate | fix (write both halves)
- Recommendation: fix. This is a rare case, and losing geometry is a correctness bug.

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
- Magic behavior: Coordinates are computed as float32 `oscale*n` and rounded with float32 `truncf`. Values near half-grid can round differently than exact decimal arithmetic would, and values in (-1/(2U),0) print as `-0.000`.
- Evidence: lef/lefWrite.c:90-127, lef/lefWrite.c:943-947
- Options: replicate float32 | fix (exact rational arithmetic)
- Recommendation: Replicate float32 for exact compatibility. Golden case LEFDEF-105 covers half-grid values.

### Q-LEFDEF-46: Pins without LEF geometry are dropped silently
- Kind: quirk
- Magic behavior: A port whose net has no material on a LEF-mapped layer produces no PIN record and no warning.
- Evidence: lef/lefWrite.c:1680-1709, lef/lefWrite.c:1785-1787
- Options: replicate | fix (warn)
- Recommendation: Replicate the output and add a warning.

### Q-LEFDEF-47: Too-large -hide distance silently disables hiding
- Kind: quirk
- Magic behavior: If 2*setback exceeds the boundary width or height, the macro is written with full detail and no message.
- Evidence: lef/lefWrite.c:1355-1362
- Options: replicate | fix (fall back to setback 0, or warn)
- Recommendation: Replicate and warn.

### Q-LEFDEF-50: Ports sharing an index with different names are merged
- Kind: quirk
- Magic behavior: All labels with the same port index go into one PIN, named after the first label. An error message is printed for each differently named label, and its geometry becomes extra PORTs of the first PIN.
- Evidence: lef/lefWrite.c:1436-1440, lef/lefWrite.c:1776-1782, lef/lefWrite.c:1795-1808
- Options: replicate | fix (separate PINs)
- Recommendation: Replicate, since DEF and netlist output use the same index semantics.

### Q-LEFDEF-51: Pin removal under -hide -pinonly p is overridden
- Kind: quirk
- Magic behavior: With setback 0 and p > 0, interior material of the selected chunk is removed from the selection. The full chunk rectangle is then painted back with the label's type, so the removal only affects the chunk when it lies on a different, connected type.
- Evidence: lef/lefWrite.c:1561-1600
- Options: replicate
- Recommendation: replicate.

### Q-LEFDEF-52: Cut layers are written as whole contact areas
- Kind: quirk
- Magic behavior: Via cuts in PORT and OBS records are the magic contact-type area, not the individual cut squares produced by GDS generation.
- Evidence: lef/lefWrite.c:747-814
- Options: replicate | fix (emit cut arrays as in the cifoutput rules)
- Recommendation: Replicate, because downstream tools accept it and golden tests depend on it.

### Q-LEFDEF-53: Boundary layer declared but unpainted
- Kind: bug
- Magic behavior: If the tech declares a `bound` layer but the cell has no such paint, only the x-extent of the boundary is initialized (to 0..0). The y-extent is uninitialized memory.
- Evidence: lef/lefWrite.c:1321-1330
- Options: fix (fall back to the cell bounding box)
- Recommendation: Fall back to the cell bbox, or FIXED_BBOX if present.
- Resolution: fix — a declared but unpainted boundary layer falls back to FIXED_BBOX if present, otherwise the cell bounding box (uninitialised y-extent) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-60: DEF "generated via" flag is never reset between vias
- Kind: bug
- Magic behavior: Once any via in a VIAS section uses CUTSIZE/LAYERS/CUTSPACING/ENCLOSURE/ROWCOL, every later via (including RECT-only vias) also gets generated geometry from the stale parameters at its `;`.
- Evidence: lef/defRead.c:1925, lef/defRead.c:1999-2008 (`generated` initialised once per section, never cleared)
- Options: replicate | fix (reset per via definition)
- Recommendation: fix; keep a compatibility test documenting Magic's output for mixed sections.

### Q-LEFDEF-61: Generated-via parameters undefined when omitted
- Kind: bug
- Magic behavior: CUTSIZE, CUTSPACING, ENCLOSURE and LAYERS have no defaults; omitted ones use uninitialised or previous values. The VIARULE is ignored, not consulted.
- Evidence: lef/defRead.c:1920-1923, lef/defRead.c:2027-2031
- Options: replicate (previous value) | fix (take defaults from the LEF VIARULE GENERATE or DRC rules; 0 spacing/enclosure)
- Recommendation: fix: use the VIARULE when known, otherwise 0, and warn.
- Resolution: fix — omitted generated-via parameters take the LEF VIARULE values when known, otherwise 0, with a warning (uninitialised values) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-62: VIAS ORIGIN/OFFSET/PATTERN/POLYGON/MASK ignored
- Kind: quirk
- Magic behavior: These properties produce an "Unknown via property" message and are dropped; generated vias are always centred and fully populated.
- Evidence: lef/defRead.c:2011-2016
- Options: replicate | fix (implement per DEF 5.8)
- Recommendation: fix, since ignoring OFFSET/ORIGIN moves the geometry; flag as a deliberate deviation.

### Q-LEFDEF-63: Generated via primary type is the top metal; layer switch fails when routing on the top layer
- Kind: bug
- Magic behavior: A generated via's primary type is the top layer (not the cut). After the via in a route, the "other residue" search over a non-contact type finds only the top layer, so a route arriving on the top layer stays on the top layer instead of switching to the bottom layer.
- Evidence: lef/lefRead.c:2575-2576, lef/defRead.c:479-525
- Options: replicate | fix (choose the via's other routing layer: bottom if current == top, else top)
- Recommendation: fix.

### Q-LEFDEF-64: Pin DIRECTION/USE persist across pins
- Kind: bug
- Magic behavior: pinDir/pinUse are initialised once per PINS section; a pin without DIRECTION/USE inherits the previous pin's values.
- Evidence: lef/defRead.c:1462-1463, lef/defRead.c:1590-1610
- Options: replicate | fix (reset to DEFAULT per pin)
- Recommendation: fix.

### Q-LEFDEF-65: Pin rectangle list and layer state leak between pins
- Kind: bug
- Magic behavior: The pending rectangle list and "layer seen" flag are not cleared at a new pin. With `+ PLACED ... + LAYER a ... + LAYER b ...`, only `a` is painted; `b` remains pending and is painted with the next pin's placement and name.
- Evidence: lef/defRead.c:1600-1608, lef/defRead.c:1630-1730
- Options: replicate | fix (paint all LAYER rects of a port with that port's placement; clear per pin/port)
- Recommendation: fix.

### Q-LEFDEF-66: Pin POLYGON and VIA ignored
- Kind: quirk
- Magic behavior: `+ POLYGON` and `+ VIA` in PINS give an "Unknown pin property" message and no geometry.
- Evidence: lef/defRead.c:1615-1621
- Options: replicate | fix (paint polygon/via geometry and label it)
- Recommendation: fix.

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
- Magic behavior: Inside special routing, an unknown `+` keyword, an unknown SHAPE, or a non-numeric STYLE skips to `;` and then keeps parsing route tokens, consuming the following net statement until its `;`.
- Evidence: lef/defRead.c:224-247
- Options: replicate | fix (skip to the next `+` or end the net at `;`)
- Recommendation: fix (e.g. `+ MASK`, `+ RECT`, `+ POLYGON`, `+ VIA` special-net shapes).

### Q-LEFDEF-70: POLYGON in routes partially parsed as points
- Kind: bug
- Magic behavior: Only tokens up to the first `)` are skipped; remaining vertices become wire points.
- Evidence: lef/defRead.c:349-355
- Options: replicate | fix (skip or paint the polygon)
- Recommendation: fix (paint polygon via the polygon decomposition of `lef-read`).

### Q-LEFDEF-71: VIRTUAL route points are connected with wire
- Kind: quirk
- Magic behavior: `VIRTUAL` is ignored and the following point is joined to the previous one by painted wire.
- Evidence: lef/defRead.c:356-361
- Options: replicate | fix (treat VIRTUAL point as a new reference without painting)
- Recommendation: fix per DEF 5.8.

### Q-LEFDEF-72: Via orientation and MASK tokens in routes
- Kind: quirk
- Magic behavior: `MASK n`, via `MASK` bits, and via orientation tokens are treated as via names, producing errors and no effect.
- Evidence: lef/defRead.c:398-411
- Options: replicate | fix (parse and apply orientation; ignore MASK silently)
- Recommendation: fix.

### Q-LEFDEF-73: Route RECT uses truncating division and undefined reference
- Kind: bug
- Magic behavior: RECT offsets use truncation toward zero for the reference point (other conversions floor); a RECT before any point uses an undefined reference; a parse error links an uninitialised rectangle for painting.
- Evidence: lef/defRead.c:285-347, lef/defRead.c:658-675
- Options: replicate | fix (floor; require a reference; drop on parse error)
- Recommendation: fix; values only differ for negative odd doubled coordinates.

### Q-LEFDEF-74: Net NONDEFAULTRULE applies only to the next routing statement
- Kind: quirk
- Magic behavior: The rule is cleared after each ROUTED/FIXED/COVER/NOSHIELD statement and is not applied if it appears after the routing.
- Evidence: lef/defRead.c:1207-1229
- Options: replicate | fix (apply rule to the entire net regardless of position)
- Recommendation: fix.

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
- Magic behavior: The FIXED_BBOX is formed from the first two vertices only.
- Evidence: lef/defRead.c:2637-2650
- Options: replicate | fix (bounding box of all vertices)
- Recommendation: fix.

### Q-LEFDEF-78: Fallback design cell name is the raw command argument
- Kind: quirk
- Magic behavior: Without a preceding DESIGN, the cell is named exactly as the `def read` argument (may include directory and `.def`); a later DESIGN renames it.
- Evidence: lef/defRead.c:2645, lef/defRead.c:2664
- Options: replicate | fix (use basename without extension)
- Recommendation: fix (basename without extension).

### Q-LEFDEF-79: UNITS multiplies cumulatively and accepts integers only
- Kind: quirk
- Magic behavior: Each UNITS statement multiplies the scale; non-integer values fall back to 100.
- Evidence: lef/defRead.c:2604-2618
- Options: replicate | fix (set rather than multiply)
- Recommendation: fix (set; warn on repeat).

### Q-LEFDEF-80: Annotation mode still paints pin geometry
- Kind: doc-mismatch
- Magic behavior: Documentation says `-annotate` adds no geometry, but PINS geometry is painted.
- Evidence: lef/defRead.c:1703, doc/html/def.html
- Options: replicate | fix (labels only)
- Recommendation: decide; replicate for byte compatibility unless users object.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-LEFDEF-81: Annotation terminal lookup uses the edit cell
- Kind: ambiguity
- Magic behavior: Terminal `<comp>/<pin>` is searched in the current edit cell, not the DEF design cell; tile search does not expand the area.
- Evidence: lef/defRead.c:1150-1170
- Options: replicate | fix (search in the design cell)
- Recommendation: fix: use the design cell.

### Q-LEFDEF-82: Layer record after a via depends on hash iteration order
- Kind: ambiguity
- Magic behavior: After a via, the route layer's LEF record is the first record (in hash-table iteration order) of class route/masterslice whose type or obstruction type matches; widths may differ if several names map to the same magic type.
- Evidence: lef/defWrite.c:2041-2075, lef/defRead.c:500-515
- Options: replicate (unspecified order) | fix (deterministic: tech-file declaration order)
- Recommendation: fix with declaration order.
- Resolution: compat — the LEF layer table is name-keyed, so the record found after a via is the first match in Magic's string hash order, reproduced exactly (X-1 as amended by D-1, 2026-11-07).

### Q-LEFDEF-83: Duplicate component instance names
- Kind: bug
- Magic behavior: The link-uniqueness failure is unchecked; a second use with the same name is placed anyway, leaving two uses with the same id.
- Evidence: lef/defRead.c:2322, database/DBcellname.c:2409-2414
- Options: replicate | fix (rename with suffix and warn, or reject)
- Recommendation: fix (warn and uniquify).

### Q-LEFDEF-84: Unsupported FILLS/SLOTS/STYLES sections recovered noisily
- Kind: quirk
- Magic behavior: Not in the section table; each statement produces "Unknown keyword" messages and `END FILLS` produces "END statement out of context."
- Evidence: lef/defRead.c:2496-2526, lef/defRead.c:2738-2745
- Options: replicate | fix (skip known-but-unsupported sections silently or paint FILLS)
- Recommendation: fix: skip silently with one message.

### Q-LEFDEF-85: Placement of unplaced/no-placement components
- Kind: quirk
- Magic behavior: UNPLACED puts the cell lower-left at the origin; a component with no placement keyword keeps the identity transform (master origin at 0,0).
- Evidence: lef/defRead.c:1324-1331, lef/defRead.c:2344-2370
- Options: replicate | fix
- Recommendation: replicate.

### Q-LEFDEF-86: def read requires a window/box context [platform]
- Kind: platform
- Magic behavior: Without a selected cell, a layout window, or box, all non-help subcommands fail with "Point to a window first", even reading.
- Evidence: lef/lefCmd.c:200-219
- Options: replicate | platform alternative (allow headless/iPadOS read without a window)
- Recommendation: platform alternative.

### Q-LEFDEF-88: Event restriction and timer-driven progress [platform]
- Kind: platform
- Magic behavior: Tk event restriction during the read and SIGALRM-style 5 s progress timer.
- Evidence: lef/defRead.c:2537-2547, lef/lefRead.c:87-140
- Options: platform alternative (async progress callback, cancellable task)
- Recommendation: platform alternative.

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
- Magic behavior: the VIAS writer prints every class-via layer-table record whose name has at least two underscores. This includes via definitions read from LEF/DEF (e.g. `via_1_2`), not just vias generated by this write, but the `VIAS n ;` count includes only the generated ones. The count can therefore be wrong.
- Evidence: lef/defWrite.c:2130-2140, lef/defWrite.c:1990-1996
- Options: replicate | fix (write only records generated by this write)
- Recommendation: fix. Only generated vias belong in the section. Flag it as a deliberate deviation in the golden case.

### Q-LEFDEF-92: Cut-type layer map can resolve to a VIA definition name
- Kind: bug
- Magic behavior: the magic-to-LEF map for contact types takes the first via-class record, in hash order, whose type matches. A VIA definition read from LEF (e.g. `M1M2_PR`) can win over the cut layer, so the VIAS RECT lines and blockage names use a via name as the layer name.
- Evidence: lef/defWrite.c:2041-2081, lef/defWrite.c:2910-2928
- Options: replicate | fix (prefer cut-layer records, i.e. records without via geometry)
- Recommendation: fix. Prefer true cut layers.
- Resolution: compat — the magic-to-LEF map is name-keyed, so the first via-class record in Magic's string hash order wins, reproduced exactly; a VIA definition such as `M1M2_PR` can win over the cut layer (X-1 as amended by D-1, 2026-11-07).

### Q-LEFDEF-93: COMPONENTS placement subtracts the untransformed bbox origin; arrays do not
- Kind: quirk
- Magic behavior: a non-arrayed use is placed at `T(R).ll − R.ll`, where R.ll is not transformed. Array elements are placed at `T(R).ll`. When R.ll ≠ (0,0) and the orientation is not N, the two disagree, and neither may round-trip through `def read`.
- Evidence: lef/defWrite.c:2804-2815, lef/defWrite.c:2860-2897
- Options: replicate | fix (always write T(R).ll − (orientation-transformed origin offset) consistent with def-read)
- Recommendation: replicate for compatibility, and confirm round-trip behaviour with def-read golden cases first.

### Q-LEFDEF-94: Component count wrong for arrays with reversed index ranges
- Kind: bug
- Magic behavior: the count adds (xhi−xlo+1)(yhi−ylo+1) without normalizing, so an array declared 3:0 contributes −2·… while 4 elements are written.
- Evidence: lef/defWrite.c:2300-2306
- Options: replicate | fix (use |xhi−xlo|+1)
- Recommendation: fix.

### Q-LEFDEF-95: Regular-net endpoint inset uses integer half width
- Kind: quirk
- Magic behavior: regular wire endpoints are inset by ⌊W/2⌋·s, not W·s/2, so odd widths produce endpoints half a unit off the true wire extent (DEF readers extend by W/2).
- Evidence: lef/defWrite.c:1240-1290
- Options: replicate | fix (inset by W·s/2)
- Recommendation: replicate (exact output), and document it.

### Q-LEFDEF-96: Dogleg test compares the previous y with x2
- Kind: bug
- Magic behavior: the dogleg condition is `prev.x != x1 && prev.y != x2`. It was presumably meant to be `prev.y != y1`.
- Evidence: lef/defWrite.c:1513
- Options: replicate | fix
- Recommendation: replicate for byte compatibility. A fix changes the output only for collinear same-orientation continuations.

### Q-LEFDEF-97: Column-count inaccuracies in net output
- Kind: quirk
- Magic behavior: `TAPERRULE <name> ` text is not added to the line-break column. In the vertical restart branch the column is reset after the width is written, so the width is not counted. The `( PIN name )` text is not counted either.
- Evidence: lef/defWrite.c:1478-1500, lef/defWrite.c:1560-1575, lef/defWrite.c:716-717
- Options: replicate | fix
- Recommendation: replicate (it only affects where lines break).

### Q-LEFDEF-98: NONDEFAULTRULES writes the first layer entry repeatedly; WIREEXT unscaled
- Kind: bug
- Magic behavior: the per-layer loop always prints `nrules->rule` (the first entry) instead of the loop variable, so a multi-layer rule read from LEF prints its first layer N times. WIREEXT prints `extend/2.0` without multiplying by the output scale.
- Evidence: lef/defWrite.c:3125-3132
- Options: replicate | fix (print each entry, scale the extension)
- Recommendation: fix both, after confirming the units in which lef-read stores `extend`.

### Q-LEFDEF-99: Non-default rules persist across writes and include LEF/DEF-read rules
- Kind: quirk
- Magic behavior: the non-default rule table is global. Rules created by previous `def write` runs and rules read from LEF/DEF files are written into every subsequent DEF until the technology is reloaded.
- Evidence: lef/defWrite.c:1170-1195, lef/defWrite.c:3063, lef/defWrite.c:3205 ("To do: Clean up")
- Options: replicate | fix (write only rules referenced by this file's nets)
- Recommendation: fix. Write only referenced rules, and keep the table persistent for LEF.

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
- Magic behavior: the `+ LAYER ...` fragment has no trailing newline, so `+ PLACED` follows on the same line. Port classes 0x60/0x70 and uses RESET/SCAN/TIEOFF print `+ DIRECTION ` / `+ USE ` with an empty keyword. RESET/SCAN/TIEOFF are not valid DEF pin USE values, but a blank is invalid too.
- Evidence: lef/defWrite.c:2395-2455
- Options: replicate | fix (newline; omit unknown keywords or map to SIGNAL)
- Recommendation: replicate the missing newline (it is valid DEF). Fix empty keywords by omitting the line.

### Q-LEFDEF-104: -units argument handling
- Kind: bug
- Magic behavior: `-units` as the last argument checks `argc < i` (off by one) and then reads `argv[argc]` (NULL), which can crash. Values ≤ 0 are not rejected and produce a zero or negative scale.
- Evidence: lef/lefCmd.c:396-414
- Options: replicate | fix (report the missing argument; reject n ≤ 0)
- Recommendation: fix.
- Resolution: fix — a missing `-units` argument is reported and values ≤ 0 are rejected (NULL `argv` read) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-105: Asymmetric directional-surround probe
- Kind: bug
- Magic behavior: to decide via orientation, the probe area is x ∈ [xl − d + s, xh + d + s], so it is shifted right by 2s instead of [xl − d − s, xh + d + s].
- Evidence: lef/defWrite.c:1938-1955
- Options: replicate | fix
- Recommendation: replicate (it affects via names); revisit with real PDK cases.

### Q-LEFDEF-106: Uninitialized surround for single-residue contacts
- Kind: bug
- Magic behavior: if a contact type has only one residue, the high-layer surround variable is used uninitialized when residue rectangles are created.
- Evidence: lef/defWrite.c:1920-1937, lef/defWrite.c:2002
- Options: fix (treat as 0)
- Recommendation: treat as 0.
- Resolution: fix — the high-layer surround of a single-residue contact is 0 (uninitialised) (X-2 tier 1, 2026-11-07).

### Q-LEFDEF-107: Inverted y warning in via cut generation
- Kind: bug
- Magic behavior: the "via size ... area height" warning fires when the centred cut fits (bottom ≥ Y1) instead of when it does not.
- Evidence: lef/defWrite.c:2208-2216
- Options: replicate | fix
- Recommendation: fix (the message is informative only).

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
- Magic behavior: nets come from existing `.ext` files. Nothing checks that they match the current layout, so stale files produce wrong nets or "floating label" errors.
- Evidence: lef/defWrite.c:273-283
- Options: replicate | warn when the cell is newer than the .ext | auto-extract
- Recommendation: replicate, and add a staleness warning.

### Q-LEFDEF-111: Naive contact-region merging
- Kind: quirk
- Magic behavior: a contact region is the starting tile plus the tiles chained straight up from its top-left neighbour and right from its right-top neighbour. Non-rectangular or L-shaped contact areas are split or merged inconsistently.
- Evidence: lef/defWrite.c:1782-1858
- Options: replicate | fix (proper maximal-rectangle decomposition)
- Recommendation: replicate.

### Q-LEFDEF-112: Repeated geometry for connected seeds
- Kind: quirk
- Magic behavior: each location seed of a node starts a fresh connectivity search. Seeds connected through top-level paint emit the same geometry again.
- Evidence: lef/defWrite.c:752-777
- Options: replicate | fix (skip tiles already emitted for the net)
- Recommendation: replicate. It is rare, because merged duplicate-name nodes are usually disjoint.

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
