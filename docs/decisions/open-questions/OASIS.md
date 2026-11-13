# Open questions: OASIS (SEMI P39 read and write)

OASIS support is a Hefesto product decision (docs/decisions/product.md#P-6). Magic has no OASIS code, so the entries below are not about Magic quirks. They record:
- details of SEMI P39 that the specs state but that were confirmed only against independent open implementations (KLayout for reading, gdstk for END and validation), not against the standard's text;
- places where the GDS-equivalence rule inherits a questionable gds-read or gds-write behaviour;
- native design choices that the team may want to revisit.

"Magic behavior" describes the GDS behaviour that Hefesto mirrors, or says "n/a". ID ranges: 1–29 oasis-format, 30–59 oasis-read, 60–89 oasis-write, 90–99 oasis-command. Gaps in the numbering are intentional.

### Q-OASIS-1: Non-minimal integer encodings
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. The spec makes the reader accept non-minimal unsigned encodings (for example `80 00` for 0), and the writer always emits minimal ones. KLayout itself writes the END padding length as a run of `0x80` bytes ending in `0x00` (an overlong zero) and reads overlong values.
- Evidence: KLayout src/plugins/streamers/oasis/db_plugin/dbOASISWriter.cc (END record padding loop); SEMI P39 integer clause (not consulted)
- Verified 2026-11-13: Magic 8.3.694 (fc5daa9e) has no OASIS code (`git grep -il oasis fc5daa9e` returns nothing), so there is no Magic behaviour to match. KLayout master dbOASISWriter.cc (END record, 'write a b-string to pad up to 255 bytes') pads the END record with a b-string whose length is written as 0x80 bytes followed by 0x00, an overlong zero, as the entry says. Any reader that rejects non-minimal encodings would therefore reject every KLayout-written file. SEMI P39 itself is paywalled and was not consulted.
- Options: accept (current spec) | reject as malformed
- Recommendation: fix (no Magic output). Both modes behave the same: the reader accepts non-minimal unsigned and signed encodings and decodes them to the same value, and the writer always emits minimal ones. Rejecting them would break on KLayout output, the most common open-source OASIS producer. Nothing depends on what P39 says here: even if P39 forbade the form, KLayout files would still have to be read. Keep the spec as it is.

### Q-OASIS-2: Validation signature coverage and byte order
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. The spec computes CRC32 (zlib) or a byte-sum CHECKSUM32 over bytes from the first magic byte through the validation-scheme byte, and stores it least significant byte first. This matches gdstk's writer and validator. KLayout neither writes nor checks signatures.
- Evidence: gdstk src/library.cpp (END record writer; `oas_validate`)
- Verified 2026-11-13: Not in Magic. gdstk main library.cpp:2865-2945 (oas_validate) sums or CRCs from byte 0 for size = (EOF-5)+1 bytes, which is through the validation-scheme byte, and compares against the 4 bytes that follow after little_endian_swap32, so least significant byte first. The writer at library.cpp:872-901 is consistent with this. KLayout writes validation-scheme 0 (dbOASISWriter.cc END record) and does not check signatures. The P39 text is paywalled, so only one independent implementation confirms the coverage.
- Options: keep | change after checking the P39 text
- Recommendation: fix (no Magic output). Keep the spec in both modes: CRC32 (zlib) or CHECKSUM32 over bytes from the first magic byte through the validation-scheme byte, stored least significant byte first, as gdstk does. Golden case OASIS-012 (a file signed by a commercial writer) stays the release gate. A mismatch on read is reported and does not stop the read. Separate scope decision for the user (judgement item): this entry and Q-OASIS-1, 3, 4, 5, 6, 7, 8, 9, 11 and 66 all rest on KLayout or gdstk rather than P39 (Q-OASIS-6, which changes the spec's PROPERTY association to KLayout's, is the most exposed). Buying SEMI P39 would close them all at once; otherwise they rest on KLayout and gdstk parity plus commercial-reader goldens.

### Q-OASIS-3: RECTANGLE square flag and modal geometry-h
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. With S=1, the spec sets the modal `geometry-h` to the width, as KLayout does (its source marks this with "TODO: really?").
- Evidence: KLayout dbOASISReader.cc do_read_rectangle
- Verified 2026-11-13: Not in Magic. Confirmed at KLayout dbOASISReader.cc do_read_rectangle: `if (m & 0x80) { mm_geometry_h = mm_geometry_w; // TODO: really? } else if (m & 0x20) ...`. The KLayout writer is consistent with it: its square path sets S and then sets mm_geometry_h = height (= width). Difference from the Hefesto spec: with S=1, KLayout never reads an H field even when the H bit is set. The spec instead reads the field and ignores it, with a report.
- Options: set geometry-h (current) | leave geometry-h unchanged
- Recommendation: fix (no Magic output). Keep the spec in both modes: after S=1, the modal geometry-h equals the width. This is what KLayout reads and what its writer assumes, so KLayout-written files decode correctly only under this rule. The Hefesto writer never sets S, so output is not affected. Also keep reading and ignoring an H field when H=1 with S=1, and report it as malformed. KLayout would desync on such a file, so a cross-check golden for that case must not compare against KLayout.

### Q-OASIS-4: CTRAPEZOID table, trapezoid formulas and modal w/h after CTRAPEZOID
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. The 26-entry CTRAPEZOID vertex table and the TRAPEZOID delta-a/delta-b formulas were transcribed from KLayout's reader, not from the P39 figure. KLayout also overwrites the modal `geometry-w`/`geometry-h` with the bounding box of the decoded CTRAPEZOID, which matters for types 20–23 (only one dimension given).
- Evidence: KLayout dbOASISReader.cc do_read_trapezoid, do_read_ctrapezoid (ctraps_table)
- Verified 2026-11-13: Not in Magic. The spec table was compared with KLayout's ctraps_table (dbOASISReader.cc) by script: all 26 types match, with w>h for types 0-7 and h>w for types 8-15, 22 and 23. gdstk, an independent implementation (library.cpp:2124-2285), gives the same vertex sets for types 0-23 and 25 (24 is the default rectangle). gdstk also overwrites the unused modal dimension with the bounding box for types 16-23, which supports rule 2. KLayout sets both modal values to the bounding box after decoding.
- Options: keep | correct after checking P39
- Recommendation: fix (no Magic output). Keep the table and rule 2 (modal w/h = bounding box of the decoded vertices) in both modes. Two independent implementations agree with the spec on every type, and both update the modal dimensions. The remaining risk is that both copied the same error, which is small. Golden case OASIS-030 (all 26 types, ideally from a commercial writer) stays the release gate.

### Q-OASIS-5: Modal reset at name records
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. The spec resets all modal variables at every name record (CELLNAME, TEXTSTRING, PROPNAME, PROPSTRING, LAYERNAME, XNAME), following KLayout. P39 may only require resets at START and CELL.
- Evidence: KLayout dbOASISReader.cc do_read (reset_modal_variables after each name record)
- Verified 2026-11-13: Not in Magic. Confirmed in KLayout dbOASISReader.cc: reset_modal_variables() is called after CELLNAME, TEXTSTRING, PROPNAME, PROPSTRING, LAYERNAME, XNAME and CELL. The P39 text was not available.
- Options: reset at name records (current) | reset only at START and CELL
- Recommendation: fix (no Magic output). Keep the reset at every name record in both modes. It is what KLayout does, so files decode the same way in both readers, and the Hefesto writer never carries modal state across a name record, so output is unaffected. A file that relied on modal state surviving a name record would also fail in KLayout, the de facto reference reader.

### Q-OASIS-6: PROPERTY attachment after XYABSOLUTE/XYRELATIVE
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. The spec attaches such a PROPERTY to the preceding element.
- Evidence: SEMI P39 property-association clause (not consulted) (corrected 2026-11-13: Not in Magic. The entry cites only P39, which was not consulted, and the spec does not match KLayout. In KLayout each element reader collects its trailing properties through read_element_properties (dbOASISReader.cc), which skips only PAD (0), CBLOCK (34) and PROPERTY (28/29) and stops at any other record. XYABSOLUTE and XYRELATIVE therefore end element attachment. A PROPERTY that follows them is read by the cell loop and stored with store_last_properties(cell_properties...), so KLayout makes it a cell property. The spec attaches it to the preceding element instead.)
- Options: attach to preceding element | treat as cell property | error
- Recommendation: fix (no Magic output). In both modes, change the spec to match KLayout: XYABSOLUTE and XYRELATIVE end element attachment, and a PROPERTY after them belongs to the cell. oasis-read discards cell properties, so in practice the property is ignored. The main effect is that an S_GDS_PROPERTY placed there no longer sets a placement's instance name or array index, which is how KLayout reads the same file too. Neither writer emits this form, so the only effect is parity with KLayout, which golden cross-checks use as the reference reader. The P39 property-association clause was not consulted, so this rests on KLayout parity and is covered by the P39 judgement item (Q-OASIS-2). (corrected 2026-11-13, was confirm in P39 (spec: attach to the preceding element))

### Q-OASIS-7: Exact definition of strict mode
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. The spec treats strict tables as contiguous, exclusive and refnum-only, including a ban on inline PROPSTRING values. KLayout warns about the same set of violations when strict mode is expected.
- Evidence: KLayout dbOASISReader.cc (`m_expect_strict_mode` warnings)
- Verified 2026-11-13: Not in Magic. Confirmed in KLayout dbOASISReader.cc. It warns on name records outside their table, on table offsets that do not match the first occurrence, on inline CELL names, on inline PROPERTY names, on inline PROPERTY strings (the PROPSTRING rule) and on inline TEXT strings. This matches the spec's set. KLayout checks only when its expect-strict option is set.
- Options: keep | narrow to the P39 wording
- Recommendation: fix (no Magic output). Keep the spec's definition in both modes. Violations are warnings only, so decoded content never changes, and the set matches KLayout's, so the warnings agree with the reference reader. The Hefesto writer in strict mode must satisfy every rule, including PROPSTRING refnums only, so that KLayout reads its files without warnings. Add that as a golden check.

### Q-OASIS-8: Table offsets pointing at a CBLOCK
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. The writer compresses each name table and sets the table offset to the CBLOCK record, as KLayout does.
- Evidence: KLayout dbOASISWriter.cc begin_table/begin_cblock
- Verified 2026-11-13: Not in Magic. Confirmed in KLayout dbOASISWriter.cc: begin_table records pos = stream position and then calls begin_cblock(), so the table offset is the offset of the CBLOCK record. KLayout's reader accepts its own output.
- Options: keep | never compress name tables
- Recommendation: fix (no Magic output). Keep it in both modes: a table offset may point at the CBLOCK that contains the table, and the writer compresses name tables. Files that KLayout writes with CBLOCKs already rely on this, so a reader has to accept it whatever P39 says. Golden case OASIS-061 (a third-party strict-mode reader on a Hefesto file) stays the check on the writer side. If that reader rejects the file, add a `oasis compress-tables no` fallback rather than changing the default.

### Q-OASIS-9: Records spanning a CBLOCK boundary
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. The writer only puts whole records into a CBLOCK. The reader tolerates a record that continues past the end of a CBLOCK, as KLayout's stream-switching reader does.
- Evidence: KLayout dbOASISReader.cc CBLOCK handling (`m_stream.inflate ()`)
- Verified 2026-11-13: Not in Magic. Partly confirmed. KLayout tl::InputStream::get (tlStream.cc) deletes the inflater at the end of the inflated block and continues reading raw file bytes, so byte-wise reads continue past the CBLOCK. A multi-byte get() that spans the boundary would, however, hit tl_assert(r != 0). KLayout reads integers byte by byte but strings with get(n), so it tolerates only spans inside integer and byte fields, not inside a string.
- Options: tolerate (current) | reject
- Recommendation: fix (no Magic output). Keep it in both modes: the writer puts only whole records in a CBLOCK, and the reader accepts any record that continues past the end of the inflated data, including a split string, by reading on into the raw bytes. This is more tolerant than KLayout, costs nothing in a byte-stream reader, and never changes how a conforming file decodes.

### Q-OASIS-10: Zero spacing in repetitions
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: gds-read reads an AREF whose column vector is zero as an array with xsep 0. gds-write emits such arrays when a Magic array has separation 0.
- Evidence: calma/CalmaRdcl.c:1243-1505; KLayout dbOASISReader.cc read_repetition (collapses a zero-spacing dimension to 1 element) (corrected 2026-11-13: Magic behaviour confirmed. In calma/CalmaRdcl.c:1476-1500, xsep = p.p_x/cols, so a zero column vector gives xsep 0, which goes to DBMakeArray at :1511. gds-write emits zero-separation AREFs with COLROW at calma/CalmaWrite.c:1770-1822 (xsep*cols = 0). KLayout's read_repetition collapses a zero dimension to 1 element (dbOASISReader.cc, `dx == 0 ? 1 : nx + 2`). Correction: the per-displacement path also matters. Coincident single placements from types 4-7, 10 and 11, or from rule 3, become identical uses, and gds-read's duplicate-use rule (CalmaRdcl.c:1513 DBCellFindDup; gds-read spec 'Use placement rules') discards each one with 'placed on top of itself'. So they are not all kept. This applies only to cells that are not flattened: for CDFLATGDS cells (`flatten` or `flatglob`), coincident placements are painted into the parent, not discarded (calma/CalmaRdcl.c:1508-1525).)
- Options: keep coincident elements and zero-separation arrays (current, GDS-consistent) | collapse like KLayout
- Recommendation: replicate, in both modes, the gds-read behaviour. A zero-spacing regular repetition of a PLACEMENT becomes a Magic array with separation 0, so an oasis-write/oasis-read round trip gives the same array that GDS does. Coincident single placements of a cell that is not flattened are discarded by the duplicate-use rule, as for GDS SREFs; placements of a flattened (CDFLATGDS) cell are painted, each one, as gds-read does. Coincident shapes are painted again, which changes nothing. Golden case OASIS-024 records the KLayout difference, where KLayout keeps one element.

### Q-OASIS-11: Path extension modal values for flush and half-width schemes
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. The spec sets the extension modal variables to 0 or to the half-width when scheme 1 or 2 is used, as KLayout does (its source marks this with TODO).
- Evidence: KLayout dbOASISReader.cc do_read_path
- Verified 2026-11-13: Not in Magic. Confirmed in KLayout dbOASISReader.cc do_read_path: scheme 1 sets the start and end extension modal values to 0, and scheme 2 sets them to mm_path_halfwidth, each line marked TODO. The Hefesto writer always emits an explicit scheme when the value would differ (oasis-write).
- Options: keep | store the scheme symbolically (a later half-width change would then change the extension)
- Recommendation: fix (no Magic output). Keep it in both modes: schemes 1, 2 and 3 store the resulting number in the modal variable, as KLayout does. Only files whose next PATH reuses the extension (scheme 0) after changing the half-width are affected, and KLayout reads those the same way. Storing the scheme symbolically would make Hefesto the only reader that differs.

### Q-OASIS-12: Non-quarter-turn placement angles
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: gds-read truncates ANGLE to an integer and rounds it to a quarter turn with an error, because Magic uses only Manhattan transforms.
- Evidence: calma/CalmaRdio.c:81-205 (corrected 2026-11-13: Confirmed, with corrected lines. calmaReadTransform is at calma/CalmaRdio.c:81-193, not 81-205. At :146-165 it computes angle = (int)dangle (truncating), normalises it into 0..360, maps 360 to 0, and otherwise reports 'Non-Manhattan angle (%d) in transform' and rounds to the nearest quarter turn using the thresholds <45, <135, <225 and <315. A slightly-off real angle such as 179.9999 therefore gives a spurious error and the correct 180. A true 30 degrees becomes 0 with an error. KLayout supports arbitrary angles, so cross-checks will differ.)
- Options: replicate (current) | represent arbitrary angles by a flattened rotated copy
- Recommendation: replicate, in both modes, the gds-read angle handling for record 18 placements: truncate, normalise, round to a quarter turn and report the error. Magic's database has only Manhattan transforms, so Magic defines no corrected output and the change is never silent. The GDS-equivalence rule then gives identical results for the same placement in GDS and in OASIS. Representing arbitrary angles as a flattened rotated copy would be a new feature, not a fix. Record it as a possible later extension, to be considered if real mask data needs it.

### Q-OASIS-30: Cell timestamps without dates in OASIS
- Kind: platform
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: gds-read takes cell timestamps from BGNSTR dates. OASIS has no dates, so the spec gives cells timestamp 0 (they get the current time when next saved) unless `datestamp <N>` is set.
- Evidence: calma/CalmaRdio.c:308-372 (corrected 2026-11-13: Confirmed, range corrected. calmaReadStampRecord (calma/CalmaRdio.c:308-358, not 308-372) converts the BGNSTR creation date. When the date is not in the 24-byte I2 form it sets *stampptr = 0 and reports 'Unknown timestamp format; setting timestamp to zero' (:326-332). calma/CalmaRdcl.c:500-508 applies it, or CalmaDateStamp, to cd_timestamp.)
- Options: 0 (current) | file modification time | a private `HEFESTO_TIMESTAMP` property written by `oasis write`
- Recommendation: replicate, in both modes, Magic's gds-read rule for a missing or unknown date. A cell read from OASIS gets timestamp 0 and so receives the current time when it is next saved. `datestamp N` overrides this as in gds-read, and read-only cells keep the content-hash stamp. OASIS has no dates, which is the same case as Magic's unknown-format date, so 0 is the Magic-consistent value. A private HEFESTO_TIMESTAMP property is optional and belongs to the P-8 diff/round-trip work, not M6. Using the file modification time would make reads depend on the file system.

### Q-OASIS-31: Circle approximation
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a (GDS has no circles).
- Evidence: oasis-read "CIRCLE elements"
- Verified 2026-11-13: Not in Magic: GDS and Magic have no circles. Other readers differ. KLayout reads a CIRCLE as a single-point round-ended path of width 2r (dbOASISReader.cc), approximated later with its own setting. gdstk uses ellipse(..., tolerance), a maximum-chord-error rule (library.cpp CIRCLE case). The spec also has a gap: vertices are round(r*cos/sin) in binary64, but cos and sin are not correctly rounded and differ between libm implementations (glibc, Apple, MSVC). Results can then differ across platforms at .5 boundaries, which breaks X-1 determinism. Strictly, rounded vertices are only approximately inscribed.
- Options: inscribed N-gon with N = 64 (current) | circumscribed | N chosen from a maximum chord error
- Recommendation: fix (no Magic output). In both modes, keep the regular N-gon with N = circlepoints (default 64) and vertex k at angle 2*pi*k/N, but make it bit-for-bit deterministic on every platform. Either compute cos and sin with a bundled correctly-rounded implementation (for example the CORE-MATH routines, built with P-1 CMake), or compute only the first octant and mirror it, so that every platform rounds the same values. Keep the inscribed-polygon intent. Because KLayout and gdstk each approximate differently, CIRCLE goldens compare against Hefesto's own expected vertices, not against those tools. Revisit N or a chord-error rule only if foundry data uses circles on mask layers.

### Q-OASIS-32: Label presentation lost through OASIS
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: gds-write writes PRESENTATION, STRANS, MAG and ANGLE for font labels. OASIS TEXT has no counterpart, so font, size, rotation and justification are dropped, and reading yields plain labels.
- Evidence: calma/CalmaWrite.c:2829-2984 (corrected 2026-11-13: Confirmed, range narrowed. calmaWriteLabelFunc (calma/CalmaWrite.c:2829-2930) writes PRESENTATION (:2900), STRANS (:2903), MAG (:2906, lab_size/800 scaled) and ANGLE (:2913, only for a non-zero rotation) when the label has a font. OASIS TEXT carries only textlayer, texttype, position and string, so none of these can be represented without a property. Magic has no OASIS output, so nothing is lost relative to Magic.)
- Options: accept loss (current) | write a private PROPERTY (for example `HEFESTO_TEXT_PRESENTATION`) and read it back
- Recommendation: other (a Hefesto format choice; Magic writes no OASIS). In both modes, `oasis write` drops font, size, rotation and justification, as the spec says, and `oasis read` yields plain labels. Hefesto adds one warning per write that gives the number of font labels written without presentation, so the loss is visible. No private property in M6: it would help only Hefesto-to-Hefesto round trips, which keep labels in .mag files anyway, and foundry tools would ignore it. Revisit with P-8 if OASIS becomes a round-trip format for Hefesto designs.

### Q-OASIS-33: Negative path extensions ignored
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: gds-read ignores negative BGNEXTN/ENDEXTN (Q-MASK-114). OASIS writers produce negative explicit extensions more often than GDS writers do.
- Evidence: calma/CalmaRdpt.c:534-750 (corrected 2026-11-13: Confirmed, lines corrected: gds-read reads BGNEXTN and ENDEXTN at calma/CalmaRdpt.c:593-614 and applies them only when > 0 (:641 and :654), not 534-750. This entry is settled by the approved Q-MASK-114 resolution (compat): oasis-read hands OASIS paths to the gds-read path rules, and oasis-read spec line ~264 already states the ON/OFF split.)
- Options: replicate (current) | honour negative extensions | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, settled by Q-MASK-114 (approved 2026-11-13). oasis-read applies the gds-read path rules unchanged (GDS-equivalence). With magic-compat ON, negative extensions are ignored, as in Magic's gds-read. With it OFF, the end is extended along the end segment's direction and a negative extension shortens the path. No separate decision is needed.

### Q-OASIS-34: Name for a cell refnum with no CELLNAME
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. The spec names such a cell `#<n>`, as KLayout does.
- Evidence: KLayout dbOASISReader.cc (CELL record handling)
- Verified 2026-11-13: Not in Magic. Confirmed in KLayout dbOASISReader.cc (CELL record): `m_cellname = name_for_id (id); if (m_cellname.empty ()) m_cellname = std::string ("#") + tl::to_string (id);`. `#` is safe in Hefesto: cmd-hierarchy rejects only `[`, `]`, `,` and non-printable characters in cell names, `#` is legal in file names on macOS, iPadOS, Linux and Windows, and Tcl treats `#` as a comment only at the start of a command, so `load #3` works.
- Options: `#<n>` (current) | `OASIS_CELL_<n>` | abort
- Recommendation: fix (no Magic output). In both modes, a CELL refnum n with no CELLNAME is reported and the cell is named `#<n>`, as KLayout names it, so cross-checks against KLayout compare the same cell names. `OASIS_CELL_<n>` would be equally safe but would differ from the reference reader, and aborting would lose the whole file for one missing name record.

### Q-OASIS-35: GDS output of read-only OASIS cells
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: gds-write copies `GDS_FILE` cells verbatim and has no notion of `OASIS_FILE`.
- Evidence: calma/CalmaWrite.c:1085-1335 (corrected 2026-11-13: Confirmed, lines corrected. calmaProcessDef (calma/CalmaWrite.c:972-1361) reads GDS_FILE at :1028 and copies the structure verbatim from the vendor file; a cell without GDS_FILE is generated from the database (calmaOutFunc at :1349-1361). Magic has no OASIS_FILE, so in Magic the only path for such a cell would be generation from the database. That content is representational: it holds only the layers the `cifinput` style maps, so generating GDS from it silently drops every unmapped vendor layer, and replaces vendor polygons with Magic's tile-derived ones.)
- Options: generate from the representational layout (what gds-write's rules imply today) | transcode OASIS to GDS (requires a change to capability gds-write) | refuse
- Recommendation: fix (both modes), and a scope choice for the user (judgement item). Generating GDS from the representational layout loses the vendor data that the read-only flag exists to protect, which is the tier-1 data-loss case. Recommended: transcode in M6. gds-write gains a requirement mirroring oasis-write's 'Read-only GDS vendor cells': the cell's records are decoded from the OASIS file and written as GDS elements (RECTANGLE, POLYGON, TRAPEZOID, CTRAPEZOID and CIRCLE as BOUNDARY, CIRCLE with the Q-OASIS-31 vertices; PATH as PATH with explicit extensions; repetitions expanded; TEXT as TEXT; PLACEMENT as SREF/AREF). An element GDS cannot express (a layer or datatype above 32767, since GDS LAYER and DATATYPE are signed I2 values; a polygon above the 8191-point XY limit after expansion) makes the write fail with an error naming the cell, rather than writing altered data. The alternative is to refuse `gds write` of a hierarchy that contains OASIS_FILE cells in M6, with an error naming them, and add transcoding later. Golden case for the transcoder: a layer of 32767 is written, 32768 fails. Magic itself writes only 0..255 (CalmaIsValidLayer, calma/calmaInt.h:119) and reads I2 values unsigned (READI2 uses unsigned short, calma/calmaInt.h:171-181), so 32768-65535 would read back positive in Magic; the 32767 limit follows the GDS signed I2 type for readers that honour it.

### Q-OASIS-37: User properties discarded on read
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: gds-read discards element properties other than 61, 98 and 99 on references.
- Evidence: calma/CalmaRdcl.c:1385-1445 (corrected 2026-11-13: Confirmed, range corrected. gds-read parses PROPATTR/PROPVALUE only for SREF/AREF (calma/CalmaRdcl.c:1384-1444): attributes 61 and 98 set the use name, 99 the array limits, and every other attribute is skipped. Properties on BOUNDARY, PATH and TEXT are skipped by the element readers. The cited 1385-1445 is off by one line.)
- Options: discard (current, GDS-consistent) | map string cell properties to Hefesto cell properties
- Recommendation: replicate (both modes): discard user properties, as gds-read does, keeping only the standard properties the spec already uses and S_GDS_PROPERTY 61/98/99 on placements. A design read from GDS and from OASIS then gives the same cells (GDS-equivalence). Mapping string cell properties to Hefesto cell properties would be a new feature with no consumer in M6. Revisit with P-8 if diff or round-trip flows need them.

### Q-OASIS-38: LAYERNAME not used for mapping
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `cifinput` maps by GDS numbers only.
- Evidence: cif/CIFrdtech.c:822-854 (corrected 2026-11-13: Confirmed, refs refined. The `calma` statement parse is at cif/CIFrdtech.c:822-854 (cifParseCalmaNums at :833-834, defined at :1388-1432), and the read-time lookup CIFCalmaLayerToCifLayer (:200-232) keys only on (layer, datatype) numbers, with wildcard fallbacks. No Magic structure holds a layer name for input mapping.)
- Options: numbers only (current) | allow `cifinput` statements to name OASIS layers
- Recommendation: replicate (both modes): map by layer and datatype numbers only, as `cifinput` does for GDS. LAYERNAME records are kept only for re-encoding read-only cells. Name-based mapping would be a tech-file extension that Magic cannot load, and no reference PDK needs it.

### Q-OASIS-39: Layer or datatype numbers above 255
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `calma` statements accept only 0..255. OASIS allows 32-bit numbers, which can only match `*` wildcards.
- Evidence: cif/CIFrdtech.c:822-854 (corrected 2026-11-13: Confirmed, refs refined. The limit is CALMA_LAYER_MAX = 255 (calma/calmaInt.h:117), enforced by cifParseCalmaNums (cif/CIFrdtech.c:1388-1432, 'Calma layer and type numbers must be 0 to 255.'). CIFCalmaLayerToCifLayer (:200-232) tries the exact pair, then the datatype wildcard, the layer wildcard and both. The entry misses that GDS has the same limit: a GDS LAYER above 255, legal in the I2 record, also matches only wildcards in Magic. OASIS therefore behaves exactly as GDS does.)
- Options: keep | extend `calma` statements to 32-bit values (tech-file extension)
- Recommendation: replicate (both modes). OASIS numbers above 255 match only `*` wildcards, exactly as GDS numbers above 255 do in Magic. Extending `calma` statements to 32-bit values would make tech files Magic cannot load, and no reference PDK uses such numbers. oasis-read inherits gds-read's report of unknown pairs: calmaLayerError (calma/CalmaRdpt.c:1251-1276) reports each unmapped (layer, datatype) pair once per read, for any number, and stays silent in read-only (CDVENDORGDS) cells, whose unmapped layers are expected. No separate report for numbers above 255 is added. (corrected 2026-11-13, dropped a new above-255 report that duplicated calmaLayerError)

### Q-OASIS-40: Extension records
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. XELEMENT and XGEOMETRY are ignored on read, and are copied with their payloads when read-only cells are re-encoded. XNAME records are not copied, so payloads that refer to XNAME refnums may dangle.
- Evidence: oasis-write "Read-only OASIS vendor cells"
- Verified 2026-11-13: Not in Magic. The spec behaviour is as described (oasis-write 'Read-only OASIS vendor cells' item 5). XELEMENT and XGEOMETRY payloads are opaque to every reader, so whether they refer to XNAME refnums cannot be decided from the file.
- Options: keep | copy XNAME tables too (refnum collisions between sources must then be handled)
- Recommendation: fix (no Magic output). In both modes, keep the spec: XELEMENT and XGEOMETRY are ignored on read and copied unchanged when read-only cells are re-encoded, and XNAME records are not copied. Copying XNAME tables would need renumbering refnums inside opaque payloads when several sources are combined, which cannot be done safely. Add a warning when a copied cell carries XELEMENT or XGEOMETRY records and its source file had XNAME records, because the payload may then refer to names that are not written.

### Q-OASIS-60: Character mapping applied to OASIS strings
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: gds-write maps label and name characters through its strict or permissive table, and writes control characters as `0x00`.
- Evidence: calma/CalmaWrite.c:3158-3244
- Verified 2026-11-13: calmaOutStringRecord (calma/CalmaWrite.c:3158-3244) selects the permissive or strict table, and the permissive table is per magic-compat setting since the approved Q-MASK-200 resolution. (corrected 2026-11-13: the message was misattributed. In the loop at :3205-3231, bytes 0x01-0x1F and 0x7F map to 0 through the table and are written as 0x00 with no per-character message, only the 'characters changed in string' summary. 'Unprintable character changed to 'X'' applies to bytes above 127, which are written as `X`. With `gds lower` off (CalmaDoLower false), lower-case output is upper-cased. The mapped character is also written back into the source string (`locstrprv[0] = c`), which the approved Q-MASK-208 resolution does not replicate.) oasis-write applies the same mapping and turns 0x00 into `_` because a-strings cannot contain it.
- Options: same mapping, with control characters as `_` (current) | write the a-string range unchanged
- Recommendation: replicate (both modes) the gds-write mapping, including its magic-compat dependence from Q-MASK-200: the strict or permissive table, bytes above 127 written as `X` with the 'Unprintable character' warning, upper-casing when `oasis lower` is off, and the 'characters changed' summary warning; layout data is not mutated (Q-MASK-208). Then a TEXT string or property value in OASIS carries the same bytes as the GDS STRING or PROPVALUE for the same design. The only OASIS change stays: a byte gds-write maps to 0x00 is written as `_`. Writing the a-string range unchanged would make GDS and OASIS output of one design disagree, which the GDS-equivalence rule exists to prevent.

### Q-OASIS-61: Duplicate cell names after mapping
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: gds-write does not check uniqueness after mapping (Q-MASK-209), and can emit two structures with the same name.
- Evidence: calma/CalmaWrite.c:1877-1934
- Verified 2026-11-13: Confirmed: calmaOutStructName (calma/CalmaWrite.c:1877-1934) does not check uniqueness after mapping. Settled by the approved Q-MASK-209 resolution (fix in both modes, `XXXXX<n>` with `_k`), and oasis-write line ~156 already states the rule.
- Options: rename the later one to `XXXXX<n>` (current) | fail the write
- Recommendation: fix (both modes), settled by Q-MASK-209 (approved 2026-11-13): the later cell gets `XXXXX<n>` (with `_k` if taken), with a warning. OASIS forbids duplicate cell names, and GDS now uses the same rule, so both outputs name cells identically. No separate decision is needed.

### Q-OASIS-62: 200-point polygon limit kept
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the merged polygon algorithm splits polygons at 200 points, because of the GDS limit.
- Evidence: calma/CalmaWrite.c:2394-2709 (corrected 2026-11-13: Confirmed, lines refined. The limit is in calmaMergeSegments (calma/CalmaWrite.c:2190-2260, `bounds->bt_points + num_points > 201` at :2214), called from calmaMergePaintFunc (:2394-2709) at :2684. The merge passes are ported exactly in both modes (Q-MASK-217, approved replicate).)
- Options: keep (current, element-for-element equal to GDS) | lift the limit for OASIS
- Recommendation: replicate (both modes): keep the 200-point limit for OASIS. oasis-write is verified by comparing its shapes with gds-write's BOUNDARY set, element for element. Lifting the limit would only reduce element counts, and OASIS repetitions and CBLOCK compression already make the size gain small. GDS output is unaffected either way.

### Q-OASIS-63: Transcoding vendor GDS paths that OASIS cannot express
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a (Magic copies such paths verbatim to GDS).
- Evidence: oasis-write "Read-only GDS vendor cells"
- Verified 2026-11-13: Not in Magic: Magic copies vendor GDS paths verbatim. KLayout master dbOASISWriter.cc: a path with an odd width is an error ('Paths with odd width cannot be written to OASIS files'), or in permissive mode is rounded DOWN to 2*(w/2) with a warning; a round-ended path is written as a flush path plus two CIRCLE records at the ends ('write two circles at the path ends to mimic the round path ends'). The current spec rounds odd widths UP and makes round ends flush, so both choices change the vendor geometry.
- Options: odd width rounded up, round ends made flush, negative width made absolute, with warnings (current) | convert these paths to polygons | refuse the cell
- Recommendation: fix (no Magic output; both modes). A transcoded vendor cell should carry the vendor geometry exactly, and OASIS cannot express an odd width: the outline edges of a path of odd integer width w lie at ±w/2, which are half database units, and a POLYGON has integer coordinates too, so converting to a polygon does not help either. Only doubling the file's unit for the whole file would represent it, which changes every coordinate in the output and is out of M6 scope. So an odd-width vendor path (any PATHTYPE) makes the write fail with an error naming the cell and the path's layer/datatype, as KLayout's default does ('Paths with odd width cannot be written to OASIS files'), rather than writing altered data. KLayout's permissive round-down is not offered, because it changes vendor geometry. Round ends (PATHTYPE 1) with an even width: the path is written flush and each end gets a CIRCLE of radius w/2 centred on the endpoint, as KLayout does, so the cap is represented rather than cut off. Negative (absolute) width: |w| with a warning, as now, since the transcoder writes the cell unmagnified. This keeps vendor data rather than Magic's gds-read interpretation, which only affects how Hefesto displays the cell. (corrected 2026-11-13, was prefer polygon conversion for odd widths if a PDK needs it (spec: round odd widths up, round ends flush); polygon conversion is not exact for odd widths, so the write fails instead)

### Q-OASIS-64: Pinned DEFLATE implementation
- Kind: platform
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a.
- Evidence: oasis-write "CBLOCK compression"
- Verified 2026-11-13: Not in Magic. Different DEFLATE implementations, and different zlib versions or settings, produce different compressed bytes for the same input, so byte-identical CBLOCKs need one implementation. Apple's libcompression is not byte-compatible with zlib's deflate output.
- Options: bundle zlib at a fixed version (current) | zlib-ng (faster, different bytes) | accept platform differences
- Recommendation: platform. Bundle one zlib version, fixed in the P-1 CMake build, with a fixed compression level, window and strategy, and use it for CBLOCK compression on every platform, iPadOS included. Never use the system zlib or Apple libcompression for writing CBLOCKs. zlib-ng is not used for writing, because its output bytes differ. Golden OASIS files are then byte-identical across platforms (X-1). Decompression accepts any valid DEFLATE stream.

### Q-OASIS-65: Default validation scheme
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. KLayout writes no signature by default; gdstk optionally writes CRC32.
- Evidence: KLayout dbOASISWriter.cc; gdstk library.cpp
- Verified 2026-11-13: Not in Magic. Confirmed: KLayout master dbOASISWriter.cc writes validation-scheme 0 at the end of the END record ('// validation-scheme', write_byte (0)); gdstk writes CRC32 only when asked. Readers that do not check signatures ignore them.
- Options: `crc32` (current) | `none`
- Recommendation: other (a Hefesto default). Keep `crc32` as the default in both modes. It costs one pass over the written file, lets readers detect truncated or corrupted files, and changes nothing for readers that ignore signatures. `validation none` stays available, for example to compare bytes with KLayout output.

### Q-OASIS-66: Integer-width standard properties
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. The writer always declares 4-byte maximum widths, excluding file offsets, which can exceed 32 bits in files over 4 GiB.
- Evidence: oasis-write "Standard properties"
- Verified 2026-11-13: Not in Magic. KLayout master dbOASISWriter.cc declares S_MAX_SIGNED_INTEGER_WIDTH = sizeof (db::Coord) = 4 and S_MAX_UNSIGNED_INTEGER_WIDTH = sizeof (distance_type) = 4 for every file, including files over 4 GiB whose table offsets exceed 32 bits. So the spec's 4/4 matches KLayout. Whether offsets count toward the declared width is a P39 question (judgement item 1).
- Options: keep | compute the true widths (requires a second pass or buffering)
- Recommendation: other (a Hefesto default). Keep declaring 4 and 4 in both modes, as KLayout does. The properties are advisory, and computing true widths would need a second pass or buffering the whole file. If the P39 text is bought (Q-OASIS-2 judgement item) and says offsets count, revisit only for files over 4 GiB.

### Q-OASIS-67: Shape repetition detection changes element order
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. GDS element order follows tile enumeration. With `repetitions yes`, elements within a mask layer are sorted by origin.
- Evidence: oasis-write "Shape repetition detection"
- Verified 2026-11-13: Not in Magic. The spec behaviour is as described: with `repetitions yes`, shapes in a mask layer are grouped into repetitions and sorted by origin, so element order differs from GDS's tile order while the expanded set of shapes is equal (oasis-write line ~355).
- Options: default `yes` (current) | default `no` for element-level equality with GDS
- Recommendation: other (a Hefesto default). Keep `repetitions yes` as the default in both modes. OASIS has no Magic reference output, X-1 compares mask data per layer as geometry, not element order, and compact repetitions are the reason to use OASIS. `repetitions no` stays available for element-level comparison with GDS when debugging.

### Q-OASIS-68: Full-dump prefix randomness
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: gds-write chooses vendor library prefixes from the C library `random()` sequence (Q-MASK-211).
- Evidence: calma/CalmaWrite.c:400-660 (corrected 2026-11-13: Confirmed, lines corrected: the prefix comes from two random() calls in calmaFullDump (calma/CalmaWrite.c:679-ff, `random() % 26` at :756 and `random() % 36` at :759), not 400-660. Settled by the approved Q-MASK-211 resolution, which names Q-OASIS-68; oasis-write line ~460 already states it.)
- Options: follow the Q-MASK-211 decision | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, settled by Q-MASK-211 (approved 2026-11-13). With magic-compat ON, the full-dump prefix comes from Hefesto's own glibc TYPE_3 random() (seed 1), shared with `random`. With it OFF, it is derived from a hash of the resolved library file name, with deterministic collision resolution. OASIS and GDS full dumps use the same prefixes. No separate decision is needed.

### Q-OASIS-69: Value type of S_GDS_PROPERTY
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. The writer stores the GDS PROPVALUE as a b-string (type 11 or 14). Some tools may expect an a-string or n-string.
- Evidence: KLayout dbOASISReader.cc (reads any string type for S_GDS_PROPERTY) (corrected 2026-11-13: Partly wrong. KLayout's reader accepts any string type for S_GDS_PROPERTY, as the entry says, but KLayout's writer does not use b-strings for it: every property string goes through string_type() (dbOASISWriter.cc), which picks an n-string for printable text without spaces, an a-string when it contains a space, and a b-string only for non-printable bytes. So the narrowest type, not b-string, is what the reference writer emits.)
- Options: b-string (current) | a-string
- Recommendation: other (a Hefesto format choice). Keep the b-string in both modes. Every reader we know accepts any string type here, a b-string can hold any byte a GDS PROPVALUE holds, and the value bytes are the same either way. Golden OASIS-061 (a third-party strict reader) checks that no reader rejects it. If it does, switch to KLayout's narrowest-type rule.

### Q-OASIS-90: Shared or independent settings for gds and oasis
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: n/a. PDK startup scripts set `gds flatglob`, `gds readonly` and similar options. With independent settings, these have no effect on `oasis read`.
- Evidence: oasis-command "Settings independent of the gds command"
- Verified 2026-11-13: Not in Magic. The spec keeps `oasis` settings separate (oasis-command line ~138). PDK and flow scripts were not available in this workspace to count their `gds` option use, so the PDK survey the entry asks for was not done. The trade-off does not depend on it: any script that sets read-side `gds` options before reading vendor data (readonly, flatglob, rescale and similar) gets a different import if it switches to `oasis read` while settings are independent.
- Options: independent (current) | shared read-side settings | independent, with `oasis` defaults copied from `gds` at first use
- Recommendation: other (a product choice; judgement item). Recommended: share the read-side settings and keep write-side settings independent. Every `gds` option (commands/CmdCD.c:151-190) is assigned explicitly. Shared (one setting; the `oasis` option is an alias of the `gds` one): `drccheck`, `flatten`, `flatglob`, `maskhints`, `noduplicates`, `ordering`, `polygon subcells`, `path subcells`, `readonly`, `rescale`, `savepaths`, `unique` and `warning`. These act only on reading, so a flow behaves the same whichever format it reads. Independent: `abstract`, `addendum`, `arrays`, `compress`, `contacts`, `labels`, `library`, `lower`, `merge` and `undefined` (write-side), `datestamp` and `nodatestamp` (used on both sides by `gds`; a `gds datestamp` set for reproducible GDS output must not change OASIS cell timestamps), and `magscale` (no effect on OASIS). The OASIS-only options (`strict`, `validation`, `repetitions`, `layernames`, `circlepoints`) are independent by definition, and `help`, `read` and `write` are actions, not settings. The alternatives are fully independent settings (the current spec), or independent settings that copy the `gds` values at first use. (corrected 2026-11-13, was gather PDK script usage before deciding (spec: independent settings))

### Q-OASIS-91: No gzip-wrapped OASIS output
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: gds-write supports `compress` for gzip-wrapped GDS.
- Evidence: oasis-command "Options shared with the gds command"
- Verified 2026-11-13: Not in Magic: gds-write's `compress` option gzip-wraps GDS. A gzip-wrapped OASIS file is not standard, and CBLOCK compression already compresses the content inside the file.
- Options: CBLOCK only (current) | also allow gzip wrapping
- Recommendation: other (a Hefesto format choice). In both modes, OASIS output uses CBLOCK compression only and is never gzip-wrapped. Reading gzip-wrapped OASIS files stays supported, because other tools produce them.
