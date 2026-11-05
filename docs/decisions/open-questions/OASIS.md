# Open questions: OASIS (SEMI P39 read and write)

OASIS support is a Hefesto product decision (docs/decisions/product.md#P-6). Magic has no OASIS code, so the entries below are not about Magic quirks. They record:
- details of SEMI P39 that the specs state but that were confirmed only against independent open implementations (KLayout for reading, gdstk for END and validation), not against the standard's text;
- places where the GDS-equivalence rule inherits a questionable gds-read or gds-write behaviour;
- native design choices that the team may want to revisit.

"Magic behavior" describes the GDS behaviour that Hefesto mirrors, or says "n/a". ID ranges: 1–29 oasis-format, 30–59 oasis-read, 60–89 oasis-write, 90–99 oasis-command. Gaps in the numbering are intentional.

### Q-OASIS-1: Non-minimal integer encodings
- Kind: ambiguity
- Magic behavior: n/a. The spec makes the reader accept non-minimal unsigned encodings (for example `80 00` for 0), and the writer always emits minimal ones. KLayout itself writes the END padding length as a run of `0x80` bytes ending in `0x00` (an overlong zero) and reads overlong values.
- Evidence: KLayout src/plugins/streamers/oasis/db_plugin/dbOASISWriter.cc (END record padding loop); SEMI P39 integer clause (not consulted)
- Options: accept (current spec) | reject as malformed
- Recommendation: Keep accepting, since rejecting would break KLayout-written files. Confirm against the P39 text whether non-minimal forms are legal.

### Q-OASIS-2: Validation signature coverage and byte order
- Kind: ambiguity
- Magic behavior: n/a. The spec computes CRC32 (zlib) or a byte-sum CHECKSUM32 over bytes from the first magic byte through the validation-scheme byte, and stores it least significant byte first. This matches gdstk's writer and validator. KLayout neither writes nor checks signatures.
- Evidence: gdstk src/library.cpp (END record writer; `oas_validate`)
- Options: keep | change after checking the P39 text
- Recommendation: Verify against SEMI P39 and against a commercial writer (golden case OASIS-012) before release.

### Q-OASIS-3: RECTANGLE square flag and modal geometry-h
- Kind: ambiguity
- Magic behavior: n/a. With S=1, the spec sets the modal `geometry-h` to the width, as KLayout does (its source marks this with "TODO: really?").
- Evidence: KLayout dbOASISReader.cc do_read_rectangle
- Options: set geometry-h (current) | leave geometry-h unchanged
- Recommendation: Check P39. The Hefesto writer never sets S, so only reading is affected.

### Q-OASIS-4: CTRAPEZOID table, trapezoid formulas and modal w/h after CTRAPEZOID
- Kind: ambiguity
- Magic behavior: n/a. The 26-entry CTRAPEZOID vertex table and the TRAPEZOID delta-a/delta-b formulas were transcribed from KLayout's reader, not from the P39 figure. KLayout also overwrites the modal `geometry-w`/`geometry-h` with the bounding box of the decoded CTRAPEZOID, which matters for types 20–23 (only one dimension given).
- Evidence: KLayout dbOASISReader.cc do_read_trapezoid, do_read_ctrapezoid (ctraps_table)
- Options: keep | correct after checking P39
- Recommendation: Verify every type against P39 (golden case OASIS-030 renders all 26 types).

### Q-OASIS-5: Modal reset at name records
- Kind: ambiguity
- Magic behavior: n/a. The spec resets all modal variables at every name record (CELLNAME, TEXTSTRING, PROPNAME, PROPSTRING, LAYERNAME, XNAME), following KLayout. P39 may only require resets at START and CELL.
- Evidence: KLayout dbOASISReader.cc do_read (reset_modal_variables after each name record)
- Options: reset at name records (current) | reset only at START and CELL
- Recommendation: The writer never depends on either. Keep the reader permissive. A file that relies on modal state across a name record would decode differently, so confirm with P39.

### Q-OASIS-6: PROPERTY attachment after XYABSOLUTE/XYRELATIVE
- Kind: ambiguity
- Magic behavior: n/a. The spec attaches such a PROPERTY to the preceding element.
- Evidence: SEMI P39 property-association clause (not consulted)
- Options: attach to preceding element | treat as cell property | error
- Recommendation: Confirm in P39. Hefesto discards element properties except `S_GDS_PROPERTY` on placements, so the impact is small.

### Q-OASIS-7: Exact definition of strict mode
- Kind: ambiguity
- Magic behavior: n/a. The spec treats strict tables as contiguous, exclusive and refnum-only, including a ban on inline PROPSTRING values. KLayout warns about the same set of violations when strict mode is expected.
- Evidence: KLayout dbOASISReader.cc (`m_expect_strict_mode` warnings)
- Options: keep | narrow to the P39 wording
- Recommendation: Verify with P39. Violations are warnings only, so reading is not affected.

### Q-OASIS-8: Table offsets pointing at a CBLOCK
- Kind: ambiguity
- Magic behavior: n/a. The writer compresses each name table and sets the table offset to the CBLOCK record, as KLayout does.
- Evidence: KLayout dbOASISWriter.cc begin_table/begin_cblock
- Options: keep | never compress name tables
- Recommendation: Check that P39 allows it, and cross-check with a third-party strict-mode reader (golden case OASIS-061).

### Q-OASIS-9: Records spanning a CBLOCK boundary
- Kind: ambiguity
- Magic behavior: n/a. The writer only puts whole records into a CBLOCK. The reader tolerates a record that continues past the end of a CBLOCK, as KLayout's stream-switching reader does.
- Evidence: KLayout dbOASISReader.cc CBLOCK handling (`m_stream.inflate ()`)
- Options: tolerate (current) | reject
- Recommendation: Tolerate.

### Q-OASIS-10: Zero spacing in repetitions
- Kind: quirk
- Magic behavior: gds-read reads an AREF whose column vector is zero as an array with xsep 0. gds-write emits such arrays when a Magic array has separation 0.
- Evidence: calma/CalmaRdcl.c:1243-1505; KLayout dbOASISReader.cc read_repetition (collapses a zero-spacing dimension to 1 element)
- Options: keep coincident elements and zero-separation arrays (current, GDS-consistent) | collapse like KLayout
- Recommendation: Keep for GDS consistency. Golden case OASIS-024 documents the KLayout difference.

### Q-OASIS-11: Path extension modal values for flush and half-width schemes
- Kind: ambiguity
- Magic behavior: n/a. The spec sets the extension modal variables to 0 or to the half-width when scheme 1 or 2 is used, as KLayout does (its source marks this with TODO).
- Evidence: KLayout dbOASISReader.cc do_read_path
- Options: keep | store the scheme symbolically (a later half-width change would then change the extension)
- Recommendation: Check P39. The writer always emits explicit schemes when needed.

### Q-OASIS-12: Non-quarter-turn placement angles
- Kind: quirk
- Magic behavior: gds-read truncates ANGLE to an integer and rounds it to a quarter turn with an error, because Magic uses only Manhattan transforms.
- Evidence: calma/CalmaRdio.c:81-205
- Options: replicate (current) | represent arbitrary angles by a flattened rotated copy
- Recommendation: Replicate. Arbitrary-angle placements are rare in mask data.

### Q-OASIS-30: Cell timestamps without dates in OASIS
- Kind: platform
- Magic behavior: gds-read takes cell timestamps from BGNSTR dates. OASIS has no dates, so the spec gives cells timestamp 0 (they get the current time when next saved) unless `datestamp <N>` is set.
- Evidence: calma/CalmaRdio.c:308-372
- Options: 0 (current) | file modification time | a private `HEFESTO_TIMESTAMP` property written by `oasis write`
- Recommendation: Keep 0. A private property would make Hefesto-to-Hefesto round trips preserve timestamps; decide together with P-8 (diff workflow).

### Q-OASIS-31: Circle approximation
- Kind: ambiguity
- Magic behavior: n/a (GDS has no circles).
- Evidence: oasis-read "CIRCLE elements"
- Options: inscribed N-gon with N = 64 (current) | circumscribed | N chosen from a maximum chord error
- Recommendation: Keep the inscribed 64-gon. Revisit if foundry data uses circles on mask layers.

### Q-OASIS-32: Label presentation lost through OASIS
- Kind: quirk
- Magic behavior: gds-write writes PRESENTATION, STRANS, MAG and ANGLE for font labels. OASIS TEXT has no counterpart, so font, size, rotation and justification are dropped, and reading yields plain labels.
- Evidence: calma/CalmaWrite.c:2829-2984
- Options: accept loss (current) | write a private PROPERTY (for example `HEFESTO_TEXT_PRESENTATION`) and read it back
- Recommendation: Accept the loss for foundry exchange. Consider a private property, off by default, for Hefesto round trips.

### Q-OASIS-33: Negative path extensions ignored
- Kind: quirk
- Magic behavior: gds-read ignores negative BGNEXTN/ENDEXTN (Q-MASK-114). OASIS writers produce negative explicit extensions more often than GDS writers do.
- Evidence: calma/CalmaRdpt.c:534-750
- Options: replicate (current) | honour negative extensions
- Recommendation: Decide together with Q-MASK-114.

### Q-OASIS-34: Name for a cell refnum with no CELLNAME
- Kind: ambiguity
- Magic behavior: n/a. The spec names such a cell `#<n>`, as KLayout does.
- Evidence: KLayout dbOASISReader.cc (CELL record handling)
- Options: `#<n>` (current) | `OASIS_CELL_<n>` | abort
- Recommendation: Keep `#<n>` for cross-check parity with KLayout. Check that `#` is safe in Hefesto file names on every platform.

### Q-OASIS-35: GDS output of read-only OASIS cells
- Kind: ambiguity
- Magic behavior: gds-write copies `GDS_FILE` cells verbatim and has no notion of `OASIS_FILE`.
- Evidence: calma/CalmaWrite.c:1085-1335
- Options: generate from the representational layout (what gds-write's rules imply today) | transcode OASIS to GDS (requires a change to capability gds-write) | refuse
- Recommendation: Transcode, and add a requirement to gds-write in a follow-up change.

### Q-OASIS-37: User properties discarded on read
- Kind: quirk
- Magic behavior: gds-read discards element properties other than 61, 98 and 99 on references.
- Evidence: calma/CalmaRdcl.c:1385-1445
- Options: discard (current, GDS-consistent) | map string cell properties to Hefesto cell properties
- Recommendation: Discard for now. Revisit when properties matter for flows such as P-8.

### Q-OASIS-38: LAYERNAME not used for mapping
- Kind: ambiguity
- Magic behavior: `cifinput` maps by GDS numbers only.
- Evidence: cif/CIFrdtech.c:822-854
- Options: numbers only (current) | allow `cifinput` statements to name OASIS layers
- Recommendation: Numbers only. A name-based mapping would be a tech-file extension.

### Q-OASIS-39: Layer or datatype numbers above 255
- Kind: quirk
- Magic behavior: `calma` statements accept only 0..255. OASIS allows 32-bit numbers, which can only match `*` wildcards.
- Evidence: cif/CIFrdtech.c:822-854
- Options: keep | extend `calma` statements to 32-bit values (tech-file extension)
- Recommendation: Keep until a real PDK needs more.

### Q-OASIS-40: Extension records
- Kind: ambiguity
- Magic behavior: n/a. XELEMENT and XGEOMETRY are ignored on read, and are copied with their payloads when read-only cells are re-encoded. XNAME records are not copied, so payloads that refer to XNAME refnums may dangle.
- Evidence: oasis-write "Read-only OASIS vendor cells"
- Options: keep | copy XNAME tables too (refnum collisions between sources must then be handled)
- Recommendation: Keep. No known PDK relies on XNAME.

### Q-OASIS-60: Character mapping applied to OASIS strings
- Kind: ambiguity
- Magic behavior: gds-write maps label and name characters through its strict or permissive table, and writes control characters as `0x00`.
- Evidence: calma/CalmaWrite.c:3158-3244
- Options: same mapping, with control characters as `_` (current) | write the a-string range unchanged
- Recommendation: Keep the GDS mapping, so that GDS and OASIS output agree. Revisit if users want punctuation such as `<0>` preserved in OASIS.

### Q-OASIS-61: Duplicate cell names after mapping
- Kind: bug
- Magic behavior: gds-write does not check uniqueness after mapping (Q-MASK-209), and can emit two structures with the same name.
- Evidence: calma/CalmaWrite.c:1877-1934
- Options: rename the later one to `XXXXX<n>` (current) | fail the write
- Recommendation: Rename. OASIS forbids duplicates.

### Q-OASIS-62: 200-point polygon limit kept
- Kind: quirk
- Magic behavior: the merged polygon algorithm splits polygons at 200 points, because of the GDS limit.
- Evidence: calma/CalmaWrite.c:2394-2709
- Options: keep (current, element-for-element equal to GDS) | lift the limit for OASIS
- Recommendation: Keep for consistency. Lifting it would reduce element counts but break element-level equivalence.

### Q-OASIS-63: Transcoding vendor GDS paths that OASIS cannot express
- Kind: ambiguity
- Magic behavior: n/a (Magic copies such paths verbatim to GDS).
- Evidence: oasis-write "Read-only GDS vendor cells"
- Options: odd width rounded up, round ends made flush, negative width made absolute, with warnings (current) | convert these paths to polygons | refuse the cell
- Recommendation: Prefer polygon conversion for odd widths if any PDK needs it. Measure on sky130 and gf180 libraries.

### Q-OASIS-64: Pinned DEFLATE implementation
- Kind: platform
- Magic behavior: n/a.
- Evidence: oasis-write "CBLOCK compression"
- Options: bundle zlib at a fixed version (current) | zlib-ng (faster, different bytes) | accept platform differences
- Recommendation: Bundle one zlib version in the build (P-1, CMake). iPadOS must not use the system libcompression for CBLOCKs.

### Q-OASIS-65: Default validation scheme
- Kind: ambiguity
- Magic behavior: n/a. KLayout writes no signature by default; gdstk optionally writes CRC32.
- Evidence: KLayout dbOASISWriter.cc; gdstk library.cpp
- Options: `crc32` (current) | `none`
- Recommendation: Keep `crc32`. It costs one pass and catches truncation.

### Q-OASIS-66: Integer-width standard properties
- Kind: ambiguity
- Magic behavior: n/a. The writer always declares 4-byte maximum widths, excluding file offsets, which can exceed 32 bits in files over 4 GiB.
- Evidence: oasis-write "Standard properties"
- Options: keep | compute the true widths (requires a second pass or buffering)
- Recommendation: Keep. Confirm in P39 whether offsets count.

### Q-OASIS-67: Shape repetition detection changes element order
- Kind: ambiguity
- Magic behavior: n/a. GDS element order follows tile enumeration. With `repetitions yes`, elements within a mask layer are sorted by origin.
- Evidence: oasis-write "Shape repetition detection"
- Options: default `yes` (current) | default `no` for element-level equality with GDS
- Recommendation: Keep `yes`. Comparisons use per-layer geometry (X-1).

### Q-OASIS-68: Full-dump prefix randomness
- Kind: quirk
- Magic behavior: gds-write chooses vendor library prefixes from the C library `random()` sequence (Q-MASK-211).
- Evidence: calma/CalmaWrite.c:400-660
- Options: follow the Q-MASK-211 decision
- Recommendation: Decide together with Q-MASK-211, preferring a deterministic prefix.

### Q-OASIS-69: Value type of S_GDS_PROPERTY
- Kind: ambiguity
- Magic behavior: n/a. The writer stores the GDS PROPVALUE as a b-string (type 11 or 14). Some tools may expect an a-string or n-string.
- Evidence: KLayout dbOASISReader.cc (reads any string type for S_GDS_PROPERTY)
- Options: b-string (current) | a-string
- Recommendation: Keep b-string, and verify against P39's definition of S_GDS_PROPERTY.

### Q-OASIS-90: Shared or independent settings for gds and oasis
- Kind: ambiguity
- Magic behavior: n/a. PDK startup scripts set `gds flatglob`, `gds readonly` and similar options. With independent settings, these have no effect on `oasis read`.
- Evidence: oasis-command "Settings independent of the gds command"
- Options: independent (current) | shared read-side settings | independent, with `oasis` defaults copied from `gds` at first use
- Recommendation: Gather PDK script usage (sky130, gf180, ihp-sg13g2) before deciding.

### Q-OASIS-91: No gzip-wrapped OASIS output
- Kind: ambiguity
- Magic behavior: gds-write supports `compress` for gzip-wrapped GDS.
- Evidence: oasis-command "Options shared with the gds command"
- Options: CBLOCK only (current) | also allow gzip wrapping
- Recommendation: CBLOCK only, because gzip-wrapped OASIS is non-standard. Reading gzip-wrapped files stays supported.
