# Open Questions: TECH area

These entries cover the capabilities `tech-file-format`, `tech-layers-and-planes`, `tech-contacts-and-stacking`, `tech-compose-and-connect`, `tech-styles-section`, `tech-loading-and-scaling` and `tech-command`. The Magic reference is commit fc5daa9e (8.3.694).

### Q-TECH-1: include appends the technology suffix to the include name
- Kind: quirk
- Magic behavior: `include foo` opens `foo.tech`. `include foo.inc` opens `foo.inc.tech`, because the file-open routine always appends the suffix. If the top-level file was found as `X.tech27`, includes get `.tech27` instead.
- Evidence: utils/tech.c:461, utils/tech.c:524, utils/tech.c:639, utils/tech.c:659, utils/path.c:689-710
- Options: replicate | fix (try the name verbatim first, then with the suffix)
- Recommendation: replicate the suffix behavior, and also accept the verbatim name as a fallback that warns. Existing PDKs rely only on the suffixed form.

### Q-TECH-2: Missing include file falls through as an ordinary line
- Kind: bug
- Magic behavior: after `Warning: Couldn't find include file X`, the `include X` line is processed as a normal line. Outside a section it is a bad header and the next section is skipped. Inside a section it goes to the section handler, which usually reports an error.
- Evidence: utils/tech.c:674-677
- Options: replicate | fix (discard the line after the warning)
- Recommendation: fix (discard the line). Keep the warning, and make it fail the load, so that error reporting stays visible.

### Q-TECH-3: Over-long physical lines and final line without newline
- Kind: bug
- Magic behavior: the tokenizer looks for `\n` without a bound. A physical line longer than the 1024-byte buffer, or a final line without LF, causes a read past the buffer. This is undefined behavior.
- Evidence: utils/tech.c:1007-1041
- Options: replicate (impossible to do safely) | fix (truncate at 1023 characters with a warning, and treat EOF as end of line)
- Recommendation: fix. Accept EOF as end of line. Report `long line truncated` and keep going.

### Q-TECH-4: More than 30 tokens per line
- Kind: bug
- Magic behavior: the token vector has 30 slots and is never bounds-checked. More tokens overflow the stack.
- Evidence: utils/tech.c:120, utils/tech.c:1053
- Options: fix (error) | fix (accept any number of tokens)
- Recommendation: accept any number of tokens. Long `variant`, `layer` and `mask` lists in real PDKs may exceed 30 tokens without this ever having been noticed.

### Q-TECH-5: repeat/endrepeat limitations
- Kind: quirk
- Magic behavior: there is one counter and one saved file offset. Nested blocks clobber each other. `repeat 0` and negative counts read the body once. A block that crosses an include boundary seeks in the wrong file.
- Evidence: utils/tech.c:607-632
- Options: replicate | fix (support nesting)
- Recommendation: replicate the single-level semantics and the count rules, and make cross-file blocks an error.

### Q-TECH-6: Stray `end` swallows the next section
- Kind: quirk
- Magic behavior: an `end` outside a section is reported as `Unrecognized section name: end`, and skip mode then ignores everything up to the next `end`, including a whole valid section.
- Evidence: utils/tech.c:677-690
- Options: replicate | fix (ignore a stray `end` with a warning)
- Recommendation: replicate. The error is visible, and the load result (failure because of the missing section) should match.

### Q-TECH-7: A repeated section re-runs init
- Kind: quirk
- Magic behavior: a second `types`, `planes`, `contact`, `compose` or `connect` section runs its init again and discards what the first one defined. Sections whose init does nothing (such as `version`, `aliases` and `styles`) accumulate instead.
- Evidence: utils/tech.c:713-719
- Options: replicate | fix (error on duplicate)
- Recommendation: replicate, and add a warning.

### Q-TECH-8: Technology name, version and description survive a reload
- Kind: bug
- Magic behavior: these strings are never reset. A new file without a name line or without a `version` section reports the previous technology's values.
- Evidence: database/DBtech.c:70-82, database/DBtech.c:147-151
- Options: replicate | fix (reset to empty at the start of a full load)
- Recommendation: fix for version and description. Make a missing technology name an error.

### Q-TECH-9: Format line edge cases
- Kind: quirk
- Magic behavior: any first token beginning with `format` or `version`, such as `formatting`, is accepted. A non-integer value prints `assuming 27`, but the format version is left unchanged, so it may not be 27.
- Evidence: database/DBtech.c:113-126
- Options: replicate | fix (exact keywords, and actually set 27)
- Recommendation: replicate the acceptance. Set 27 on a bad value, which is what the message promises.

### Q-TECH-10: Loads are not atomic; partial-reload failure uses an uninitialized plane count
- Kind: bug
- Magic behavior: a failed full load leaves the tables partly replaced, restores only the plane count, and clears the stored file name. A failed partial reload sets the plane count from an uninitialized variable and clears the file name, after which partial reloads cannot work.
- Evidence: utils/tech.c:444, utils/tech.c:577-595, utils/tech.c:861-867
- Options: replicate | fix (atomic load: build the new technology aside and swap on success)
- Recommendation: fix (atomic), with the same diagnostics.

### Q-TECH-11: Dangling names after a rejected type or plane line
- Kind: bug
- Magic behavior: names are added to the name table before the line is validated (unknown plane, a later duplicate in the same list, or too many types). Names already added stay bound to the next free number, which the next valid line then takes. As a result, extra names silently alias a different type.
- Evidence: database/DBtechtype.c:462-475, database/DBtechtype.c:290-293, database/DBtechtype.c:755-768
- Options: replicate | fix (validate first, then add names)
- Recommendation: fix. The load fails in any case.

### Q-TECH-12: `alias` with two tokens in the types section crashes
- Kind: bug
- Magic behavior: `alias name` with no type list dereferences a null third token.
- Evidence: database/DBtechtype.c:437-458
- Options: fix (report `Line must contain at least 2 fields`)
- Recommendation: fix.

### Q-TECH-13: Alias precedence differs between single names and type lists
- Kind: quirk
- Magic behavior: in type lists, an alias-table key wins over type names. In single-name resolution, a unique type-name prefix wins, and the alias table is consulted only when the type lookup fails. Example: alias `met` (multi-type) and type `metal1`. As a list, `met` is the alias set. As a single name, `met` is metal1.
- Evidence: database/DBtechname.c:74-103, database/DBtechname.c:689-696
- Options: replicate | fix (aliases always win)
- Recommendation: replicate, because commands and rules that use these names must resolve the same way.

### Q-TECH-14: Prefix ambiguous among names of the same type
- Kind: quirk
- Magic behavior: `ndif` is ambiguous when one type has both `ndiff` and `ndiffusion` as names, even though both mean the same type.
- Evidence: database/DBtechtype.c:674-693
- Options: replicate | fix (resolve when all candidates denote the same value)
- Recommendation: fix as a strict superset. It only accepts inputs that Magic rejects. Log a note for compatibility testing.

### Q-TECH-15: Null list `0` only at the end of the string
- Kind: quirk
- Magic behavior: `0` means the empty list only when the rest of the string is exactly `0`. `0,x` and `0/plane` report `Unrecognized layer (type) name "0"`.
- Evidence: database/DBtechname.c:682
- Options: replicate | fix (treat a `0` atom as empty anywhere)
- Recommendation: replicate the error, and treat it as non-fatal, as Magic does. The load still fails only where the owning section rejects the line.

### Q-TECH-16: Plane qualifier quirks in type lists
- Kind: bug
- Magic behavior: `/router`, an unknown plane or an ambiguous plane skips the restriction, so all of the types remain selected. A plane-qualified segment replaces the accumulated plane set instead of adding to it. So `a,b/m1` reports only plane m1 even if `a` lives elsewhere.
- Evidence: database/DBtechname.c:745-764
- Options: replicate | fix (empty selection on a bad plane; add planes instead of replacing)
- Recommendation: replicate the plane-set behavior, because explicit paint rules depend on it (`paint ... R` uses the plane set of R). Treat a bad plane as an error.

### Q-TECH-17: Contact-section keyword handling and redeclaration
- Kind: quirk
- Magic behavior: the first token is resolved as a type by unique prefix before the keywords `contact`, `device` and `stackable` are checked. A type named `contactcut` would make `contact ...` lines resolve to that type. Declaring the same contact twice ORs the residue sets and registers the contact twice in the contact list, which duplicates the default-rule work.
- Evidence: database/DBtcontact.c:177-189, database/DBtcontact.c:267, database/DBtcontact.c:617
- Options: replicate | fix (check keywords first; reject redeclaration)
- Recommendation: check keywords first only when the token is an exact keyword and no type has that exact name. Reject redeclaration.

### Q-TECH-18: Contact compose rule kept after a failing pair
- Kind: quirk
- Magic behavior: a contact compose or decompose line that fails validation on pair k still applies pairs 1..k−1 at finalization. The load is marked failed.
- Evidence: database/DBtpaint.c:471-521
- Options: replicate | fix (discard the whole rule)
- Recommendation: replicate. It is only observable on files that fail to load.

### Q-TECH-19: Saved contact rules are never cleared
- Kind: bug
- Magic behavior: the saved contact compose/decompose rule list is never reset. Every later technology load re-applies the earlier technology's rules, using that technology's type numbers. The array holds 256 entries for the whole session, and overflowing it corrupts memory.
- Evidence: database/DBtpaint.c:58-59, database/DBtpaint.c:471
- Options: replicate | fix (clear at compose init)
- Recommendation: fix. The specs already require clearing it. Behavior on the first load of a session is identical.

### Q-TECH-20: Explicit `erase` rules mark the paint entry as user-defined
- Kind: bug
- Magic behavior: both `paint h t R` and `erase h t R` set the user-defined flag of the paint entry (h, t). The erase entry is never protected, so contact default steps (contact erase, locking) can overwrite an explicit erase rule for a contact. They also protect a paint default that the user never set.
- Evidence: database/DBtpaint.c:690
- Options: replicate | fix (mark the erase flag for erase rules)
- Recommendation: replicate. It changes tables for real PDKs. Golden tests (TECH-G21) must show the actual result.

### Q-TECH-21: Non-contact compose with components on different planes
- Kind: quirk
- Magic behavior: for `compose T r s`, every entry is written on the home plane of r only, even when s lives on another plane.
- Evidence: database/DBtpaint.c:419-437
- Options: replicate | fix (error)
- Recommendation: replicate.

### Q-TECH-22: Contacts with identical residues
- Kind: quirk
- Magic behavior: identical residue sets only produce an informational message. Painting a residue over the higher-numbered contact gives the lowest-numbered contact with those residues (step 3a, case A1b), so the contact silently changes type.
- Evidence: database/DBtcontact.c:594-610, database/DBtpaint2.c:479-494
- Options: replicate | fix (keep the image type)
- Recommendation: replicate, and log a warning at load time.

### Q-TECH-23: Painting a non-residue over a contact differs from the manual
- Kind: doc-mismatch
- Magic behavior: at table level, painting ndiff over a poly contact gives ndiff on active (the same-plane default) and metal1 on metal1. The manual says ntransistor on active. Whether the paint operation's image resolution changes the final geometry is up to the editing capabilities.
- Evidence: database/DBtpaint.c:199-211, database/DBtpaint2.c:496-507, doc/latexfiles/maint2.tex ("Compose section")
- Options: replicate tables | fix (per-plane residue composition)
- Recommendation: replicate the tables. Confirm the final geometry with golden case TECH-G19.

### Q-TECH-24: styles section without styletype, and headless loading
- Kind: quirk / platform
- Magic behavior: the type-to-style map is cleared only by `styletype`. A styles section without one adds to the previous technology's map. A `styletype` line whose display files cannot be found rejects the line and fails the whole load, unless the display driver needs no color map. Even then the `.dstyle` file must be found.
- Evidence: dbwind/DBWdisplay.c:1739-1775, graphics/grCMap.c:129-146
- Options: replicate | fix (clear the map at styles init; never fail a load for missing display resources in headless mode)
- Recommendation: fix both. Display resources are a platform concern, and data compatibility does not depend on them.

### Q-TECH-25: `tech revert *` does not re-lock
- Kind: bug
- Magic behavior: with `*`, the mask used is the tech-file active set, so layers that were locked in the technology file and later unlocked stay unlocked. `tech revert` with no argument is correct.
- Evidence: commands/CmdTZ.c:389-397
- Options: replicate | fix (`*` means all user layers)
- Recommendation: fix.

### Q-TECH-26: `tech lambda` label is inverted, and setting it does not rescale
- Kind: quirk
- Magic behavior: the non-Tcl build prints `Internal units per Lambda = L / I`, which is lambda per internal unit. `tech lambda N [D]` overwrites the scale fraction without rescaling any table or geometry.
- Evidence: commands/CmdTZ.c:227-243, commands/CmdTZ.c:259-271
- Options: replicate | fix (correct the label; make setting either rescale or be refused)
- Recommendation: the label is informative, so fix it. Keep the raw-set capability, but only behind an explicit expert option.

### Q-TECH-27: set-minimum-grid with grid limit 0
- Kind: bug
- Magic behavior: the post-load minimum-grid step divides the CIF scale factor by the grid limit, which may be 0. That is division by zero.
- Evidence: utils/tech.c:808-812, cif/CIFtech.c:775-786
- Options: fix (treat 0 as 1, as the grid-limit check does)
- Recommendation: fix.

### Q-TECH-28: Host mechanisms for finding and loading technologies
- Kind: platform
- Magic behavior: the search path is built from `$CAD_ROOT`. PDK discovery uses the environment variables `PDK_PATH`, `PDKPATH`, `PDK_ROOT` and `PDKROOT`, plus `/usr/local/share/pdk`. Tcl `.magicrc` scripts are run next to the technology file. The `-T` command-line option selects the technology, and Tcl tag callbacks run after `tech load`. The `requires` check compares against the program version.
- Evidence: utils/paths.h:40, database/DBio.c:577-735, utils/main.c:303-307, commands/CmdTZ.c:544-622, database/DBtech.c:231-232
- Options: platform alternative
- Recommendation: provide an ordered, user-configurable list of technology roots (on iPadOS: inside the sandbox, plus user-granted folders). Provide a project-level default technology instead of startup scripts. Compare `requires` against the Magic compatibility level (8.3.694), not Hefesto's own version.

### Q-TECH-29: Manual and code disagree on file and command syntax
- Kind: doc-mismatch
- Magic behavior:
  - `#` starts a comment only as the first non-blank character. The manual says it starts one anywhere.
  - A backslash continuation joins lines with no blank. The manual says it is replaced by a blank.
  - Sections may appear in any order that satisfies the prerequisites. The manual says a fixed order is required.
  - The lock options are `tech locked|unlocked|revert`. The manual says `tech layers lock|unlock|revert`.
- Evidence: utils/tech.c:1024-1035, utils/tech.c:696, commands/CmdTZ.c:194-196, doc/latexfiles/maint2.tex, doc/html/tech.html
- Options: replicate code
- Recommendation: replicate the code, and document the differences in the user docs.

### Q-TECH-30: `tech layers` list form returns the longest name
- Kind: doc-mismatch
- Magic behavior: the list form returns the longest non-alias name of each type. The manual says it returns the primary (short) name. Stacked types are never listed.
- Evidence: database/DBtechname.c:459-464
- Options: replicate | fix
- Recommendation: replicate, because scripts depend on it.

### Q-TECH-31: Existing layout is not remapped when the technology changes
- Kind: ambiguity
- Magic behavior: cells keep raw type numbers across a technology reload, so the same number may mean a different layer afterwards. Only a warning is given.
- Evidence: utils/tech.c:852-859, commands/CmdTZ.c:571-574
- Options: replicate | extension (remap by long name, or refuse the reload while layout exists)
- Recommendation: replicate by default. Offer an opt-in remap-by-name.

### Q-TECH-32: Fringe-halo default depends on section order
- Kind: quirk
- Magic behavior: the format version is read when the `extract` section is finalized. A `tech` section placed after `extract` leaves the default fringe-halo setting based on the previous technology's format, or on 27 if an earlier `tech` section init ran.
- Evidence: extract/ExtTech.c:3580-3587, database/DBtech.c:78
- Options: replicate | fix (apply the default after the whole file is read)
- Recommendation: fix. Every known PDK puts `tech` first, so the outcome there is the same.
