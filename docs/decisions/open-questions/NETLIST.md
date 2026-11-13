# Open Questions: NETLIST area

These entries cover the capabilities `ext-flattening-read`, `ext-flattening-hierarchy`, `ext-flattening-names`, `netlist-spice-options`, `netlist-spice-devices`, `netlist-spice-hierarchy`, `netlist-sim`, `netlist-extcheck`, `extresist-command`, `extresist-network` and `extresist-output`. The Magic reference is commit fc5daa9e (8.3.694).

### Q-NETLIST-1: Coupling-capacitor order depends on memory addresses
- Kind: platform
- Magic behavior: flat coupling capacitances are accumulated in a hash table whose key is the pair of node addresses. The lower address is stored first. Both the order of the `C` lines and which node is printed first therefore depend on the allocator, and are not reproducible even between Magic builds.
- Evidence: extflat/EFflat.c:1140-1150, extflat/EFvisit.c:629-650, extflat/EFhier.c:681-700
- Options: replicate (impossible) | fix (deterministic order: by first-creation order of the node pair, with terminals in canonical-name order)
- Recommendation: fix with a documented deterministic order. Golden comparison of `C` coupling lines must be order-insensitive and symmetric in the two terminals. The capacitor counter values in the names then differ, so compare after renumbering.
- Resolution: fix — address-keyed order: coupling capacitors are written in the documented deterministic order and golden comparison is order-insensitive and symmetric in the terminals (X-1 memory-address order, as amended by D-1, 2026-11-07).

### Q-NETLIST-2: Output order inherits Magic's hash-table internals
- Kind: quirk
- Magic behavior: device order, subcircuit and instance order, flatten order, and implicit port numbering come from iterating chained hash tables. These use a specific string hash, power-of-two bucket counts, prepend-on-insert, and growth by a factor of 4.
- Evidence: utils/hash.c:218-262, utils/hash.c:405-470, utils/hash.c:492-530, extflat/EFdef.c:256-272
- Options: replicate (emulate the table exactly) | fix (sort by key)
- Recommendation: replicate. It is required for line-by-line identical netlists. Encapsulate it as a `MagicOrderedTable` utility. Bytes ≥ 0x80 in keys hash differently on signed-char and unsigned-char platforms in Magic; Hefesto should use signed char, as on x86-64 Linux, and record this.
- Resolution: decided by X-1 as amended (2026-11-06, D-1): replicate. The table is defined once in capability `hefesto-output-determinism` (requirement "Magic string hash table"), with signed key bytes.

### Q-NETLIST-3: `ext2spice rthresh` has no effect
- Kind: doc-mismatch
- Magic behavior: the value is stored in the resistance threshold but is never consulted by the SPICE writer. All explicit resistors are written.
- Evidence: ext2spice/ext2spice.c:840, ext2spice/ext2spice.c:4041-4060
- Options: replicate | fix (filter resistors below the threshold)
- Recommendation: replicate, and document the option as inert.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-NETLIST-4: Flat CDL/value writer prints resistance for an unnamed `c` parameter
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: in flat mode, an unnamed `c` parameter writes the device resistance instead of the capacitance. Hierarchical mode writes the capacitance.
- Evidence: ext2spice/ext2spice.c:2128-2134 vs ext2spice/ext2hier.c:158-163
- Verified 2026-11-13: Confirmed. In ext2spice/ext2spice.c:2131-2136 (spcWriteValue, flat), case 'c' writes dev->dev_res. The hierarchical writer at ext2spice/ext2hier.c:158-163 writes dev->dev_cap.
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: the flat writer writes the device resistance for an unnamed `c`, as Magic does. OFF: it writes the capacitance, matching hierarchical mode. This is a wrong output value, so the compat rule applies. Hierarchical mode writes the capacitance in both modes. (corrected 2026-11-13, was fix)
- Resolution: compat — with `magic-compat` ON the flat SPICE writer writes the device resistance for an unnamed `c` parameter, as Magic does; with it OFF the capacitance, as hierarchical mode does in both modes. Approved 2026-11-13.

### Q-NETLIST-5: Flat-mode tee for `rsubckt` never writes its second half
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the code that writes the `X<n>B` line comes after a `break`, so it is unreachable. Flat output has only `X<n>A` with half the length. Hierarchical output writes both halves.
- Evidence: ext2spice/ext2spice.c:3056-3100
- Verified 2026-11-13: Confirmed. In ext2spice/ext2spice.c:3051-3056 the subckt case ends with `break;`, so the X<n>B block at :3058-3100 is unreachable. In flat mode the first half (X<n>A, :2890/:2914) is written with l/2 (:2988-2995) between source and body, and the drain side is never connected. ext2hier.c:1000 writes the B half. Resistor tee is off by default (esDoResistorTee = FALSE, :54).
- Options: replicate | fix (write the B half as in hierarchical mode) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: flat tee output writes only X<n>A, as Magic does. OFF: write the X<n>B half as hierarchical mode does, including zeroing the body node's cap when a substrate pin is written. This bug changes the netlist, so the compat rule applies, and it only shows with `ext2spice resistor tee` in flat mode. Release notes: with ON, the flat tee leaves the drain open.
- Resolution: compat — with `magic-compat` ON flat `resistor tee` output writes only X<n>A, as Magic does; with it OFF it also writes X<n>B as hierarchical mode does, zeroing the body node's cap when a substrate pin is written. Approved 2026-11-13.

### Q-NETLIST-6: Flat merge multipliers desynchronize on 1-terminal devices
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the merge pre-pass skips devices with fewer than 2 terminals without reserving a multiplier slot. The output pass consumes a slot for every device with at least one terminal. Later devices then read the wrong multiplier, so a device can be dropped or receive a wrong `M=`.
- Evidence: ext2spice/ext2spice.c:4730-4734, ext2spice/ext2spice.c:2725-2727
- Verified 2026-11-13: Confirmed, and it is worse than the entry says. devMergeVisit (ext2spice/ext2spice.c:4726-4730) returns before addDevMult (:4534) for dev_nterm < 2. spcdevVisit (:2731) does devIsKilled(esFMIndex++) for every device with nterm ≥ 1. devIsKilled (:2583-2587) and getCurDevMult (:2626-2628) do not check bounds. Every 1-terminal device moves later devices one slot off. Reads past the last written slot are uninitialised, or out of bounds once the index passes esFMSize: undefined behaviour. This only happens with `ext2spice merge` on (default off, :96-97).
- Options: replicate | fix (index by device identity)
- Recommendation: fix. Both modes: key each device's merge multiplier and killed flag to the device itself, so devices with exactly one terminal take part in no merge, are never killed and are written with M = 1 (devices with no terminals are never written, ext2spice.c:2727-2728). Magic's misindexing drops devices or assigns the wrong M, and can read uninitialised or out-of-bounds memory, so it is X-2 tier 1 and cannot be reproduced reliably. Netlists with no 1-terminal devices are identical to Magic's.
- Resolution: fix — in both modes each device's merge multiplier and killed flag are keyed to the device, so devices with exactly one terminal take part in no merge and are written with M = 1 (X-2 tier 1; Magic's misindexing is undefined behaviour). Netlists without 1-terminal devices match Magic. Approved 2026-11-13.

### Q-NETLIST-7: Terminal area/perimeter divided by M only in scale mode
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: with `.option scale`, `ad/as/pd/ps` are divided by the merge multiplier. Without it they are not.
- Evidence: ext2spice/ext2spice.c:3760-3788, ext2spice/ext2spice.c:3870-3883
- Verified 2026-11-13: Confirmed. In ext2spice/ext2spice.c:3762-3786 (spcnAP) and :3849-3862 (spcnAPHier), when esScale < 0 (the `.option scale` mode, :1055-1062, default on) the code writes 1e-12·A·s²/m and 1e-6·P·s/m. Otherwise it writes the values times esScale with no /m. m differs from 1 only when merging is on (default off).
- Options: replicate | fix (always divide) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: divide by m only in scale mode, as Magic does. OFF: divide by m in both modes, so that M·ad equals the merged junction area in either output style. The missing division is a bug that changes simulation and LVS numbers. The compat rule settles it, so no PDK consultation is needed: the default ON keeps Magic's numbers. It only matters with device merging on and scale off. (corrected 2026-11-13, was replicate, decided with PDK users)
- Resolution: compat — with `magic-compat` ON terminal area and perimeter are divided by m only in scale mode, as Magic does; with it OFF in both output styles, so M·ad equals the merged junction area. Approved 2026-11-13.

### Q-NETLIST-8: Parameter scale and offset handling
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the `+N`/`-N` offset is applied only in the SI (scale-off, no `*k`) branch. It is ignored in scale mode and when `*k` is present. When `*k` is present, the offset is never initialized; it is unused in that case.
- Evidence: extflat/EFbuild.c:808-834, ext2spice/ext2spice.c:2222-2236
- Verified 2026-11-13: Confirmed at extflat/EFbuild.c:809-835. With `*`, parm_offset is never set; mallocMagic does not zero it. In ext2spice.c (:2225-2233, :2356-2363, :2401-2408, :2457-2474), and the same in ext2hier.c, the offset is applied only when esScale ≥ 0 (scale off) and parm_scale == 1.0. This is worse than the entry says: a declaration with an explicit `*1` has parm_scale == 1.0, so the uninitialised offset is read. That is undefined behaviour, not 'unused'.
- Options: replicate | fix (apply the offset in all branches) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. Both modes (tier 1): a parameter declared with `*S` has offset 0, so `*1` never reads garbage. ON: otherwise apply the offset only in the scale-off, scale==1 branch, as Magic does. OFF: add the offset to the raw value before scaling in every branch (scale mode and `*S` included). Ignoring a declared offset changes output values, so the compat rule applies; the uninitialised read is fixed in both modes. (corrected 2026-11-13, was replicate)
- Resolution: compat — in both modes a `*S` parameter has offset 0 (tier 1); otherwise with `magic-compat` ON the offset is applied only in the scale-off, scale-1 branch, as Magic does, and with it OFF it is added before scaling in every branch. Approved 2026-11-13.

### Q-NETLIST-9: Declared parameter order is reversed on output
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: `parameters` tokens are prepended to a list, so output order is the reverse of declaration order. Verbatim device parameters are also reversed.
- Evidence: extflat/EFbuild.c:858-866, extflat/EFbuild.c:955-962 (corrected 2026-11-13: The behaviour is described correctly, but the lines have moved. `parameters` tokens are prepended at extflat/EFbuild.c:849 (efBuildDeviceParams, :777-852), not :858-866. Verbatim device parameters are prepended at :982-985 (also :1042 and :1067), not :955-962. Context the entry leaves out: the extractor keeps the tech-file order in the .ext `parameters` line, because extract/ExtTech.c:2458-2524 parses right to left and prepends, and extract/ExtBasic.c:1784-1810 writes the list in order. The only reversal is therefore the reader's, so SPICE output lists parameters in the reverse of the tech-file order. The `:`-prefixed cell instance parameters are reversed too.)
- Options: replicate | fix
- Recommendation: replicate. Both modes keep the declared list and the verbatim list in reverse order of their tokens, as Magic does. SPICE reads these as name=value pairs, so their order has no meaning to a simulator or to netgen. The choice only matters for byte identity, and Magic defines no other order: this is the 'internal order choice that byte identity needs' case from the M5 table. The PDK goldens (sky130, gf180mcuD, ihp-sg13g2) capture the reversed order.
- Resolution: replicate — in both modes declared and verbatim parameter lists are written in reverse token order, as Magic does; the PDK goldens capture the reversed order. Approved 2026-11-13.

### Q-NETLIST-10: Name preference favors the lexically greater name
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: on a full tie (same component count and length), the name with the greater `strcmp` wins. For example `b` beats `a`.
- Evidence: extflat/EFname.c:575-592 (corrected 2026-11-13: The behaviour is described correctly, but the lines have moved. The full tie is decided at extflat/EFname.c:568 (`return efHNLexOrder(h1,h2) > 0`) in EFHNBest (:510-568). efHNLexOrder (:590-607) compares the root component first. The block that would prefer global (`!`) names is disabled with `#if 0` at :531-541, which matches the spec's 'Global names receive no extra preference'.)
- Options: replicate | fix
- Recommendation: replicate. On a full tie (same number of components, same length) both modes prefer the name with the greater root-first strcmp, as Magic does. The tie-break is arbitrary rather than wrong, and Magic gives no other intended order. The canonical node names in every SPICE, .sim and LVS netlist depend on it, so any other rule would rename nets against the goldens.
- Resolution: replicate — in both modes, on a full tie (same component count and length) the name with the greater root-first strcmp wins, as Magic does. Approved 2026-11-13.

### Q-NETLIST-11: Global names in the Tcl build use the `globals` Tcl array
- Kind: platform
- Magic behavior: the Tcl build decides whether a name is global by looking up `$globals(<leaf>)`, not by a trailing `!`. Global merging, implicit substrate ports and alias filtering all change with it.
- Evidence: extflat/EFname.c:101-118
- Options: replicate (configuration map of global names) | platform alternative (trailing-`!` rule)
- Recommendation: use the trailing-`!` rule by default, and offer an optional configured global-name set that emulates the Tcl array.
- Resolution: platform — decided by D-14 (2026-11-07). A name is global if its leaf is in `globalNets` of the project setting "global nets and power names" (`hefesto-settings`), seeded from the PDK's `globals` array; this follows Magic's Tcl build, and a trailing `!` alone does not make a name global in flattening.

### Q-NETLIST-12: `resist` values are offset by +0.5 mΩ
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: on reading, `resist` values become `(float)(0.5 + rscale·R)`. The intended rounding is never truncated, so SPICE shows `2.0005` for 2000 mΩ.
- Evidence: extflat/EFread.c:600-603 (corrected 2026-11-13: The behaviour is described correctly, but the lines have moved: the RESISTOR case is at extflat/EFread.c:626-635 (value at :634), not :600-603. History confirms it is a bug. Commit fc62a63d (2023) changed `(int)(0.5 + rscale*atof(R))` to `(float)(0.5 + ...)` and kept the term that used to round to an integer. The SPICE writer prints `%g` of R/1000 (ext2spice/ext2spice.c:4050-4051), so 2000 mΩ prints as 2.0005. The 0-Ω orphan-node patch resistors (resis/ResPrint.c:321) come out as 0.0005.)
- Options: replicate | fix (round to integer) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, a `resist` record becomes `(float)(0.5 + rscale × atof(R))`, as Magic does. This is the `.res.ext` +0.5 mΩ offset that X-2 names as a tier-2 example. With it OFF, the value is `(float)(rscale × atof(R))`. It is not rounded to an integer: values have been floating-point since 2019, and the entry's 'round to integer' option would undo that. Resolve together with Q-NETLIST-44. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON a `resist` record becomes `(float)(0.5 + rscale × R)`, as Magic does; with it OFF `(float)(rscale × R)`, still a float, not rounded. Resolved together with Q-NETLIST-44. Approved 2026-11-13.

### Q-NETLIST-13: `ext2spice run` and unknown option words become the cell name
- Kind: doc-mismatch
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the option table's first entry is `[run]`, which cannot match `run`. Any unmatched or ambiguous first word (for example `re`) is passed to the run as a cell name.
- Evidence: ext2spice/ext2spice.c:290-292, ext2spice/ext2spice.c:375-380, utils/lookup.c:86-120
- Verified 2026-11-13: Confirmed. The first entry of the option table is the help string `[run] [options]` (ext2spice/ext2spice.c:290), and Lookup (utils/lookup.c:86-120) compares characters up to the first space, so `run` never matches `[run]`. Any value below 0 (-1 ambiguous, -2 unknown) selects RUN without consuming the word (ext2spice.c:376-380), and EFArgs (extflat/EFargs.c) then takes the first non-dash word as the cell name. So `ext2spice run` netlists a cell named `run`, and `ext2spice re` (ambiguous between resistor and renumber) netlists a cell named `re`. The Tcl build is the same: the Tcl Lookup only strips the `::magic::` prefix. The documentation (doc/html/ext2spice.html:32, :206) lists `run` as a keyword.
- Options: replicate | fix (accept `run`, and reject ambiguous words) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto copies Magic: `run` and any ambiguous or unknown first word are run arguments, so the word becomes the cell name. With it OFF, `run` is accepted as the documented keyword and the remaining words are run arguments, so `ext2spice run foo` netlists foo. An ambiguous prefix prints `Ambiguous option: <w>` and does nothing, and `run <cell>` is the way to netlist a cell whose name is an option prefix. In both modes, an unknown word that is not an option prefix is still taken as the cell name: that is Magic's documented `ext2spice <cell>` form. (corrected 2026-11-13, was fix)
- Resolution: compat — with `magic-compat` ON `run` and any ambiguous first word are run arguments, so the word becomes the cell name, as Magic does; with it OFF `run` is a keyword and an ambiguous prefix prints `Ambiguous option: <w>` and does nothing. In both modes a word that is not an option prefix is the cell name. Approved 2026-11-13.

### Q-NETLIST-14: ext2sim merge-type table lacks a terminator
- Kind: bug
- Magic behavior: the `merge` value table has no NULL sentinel, so lookup reads past the array.
- Evidence: ext2sim/ext2sim.c:325-329
- Options: fix
- Recommendation: fix. Accept `none`, `conservative` and `aggressive`.
- Resolution: fix — the merge value table is terminated, so lookup accepts `none`, `conservative` and `aggressive` without reading past the array (X-2 tier 1, 2026-11-07).

### Q-NETLIST-15: ext2sim node-capacitance line lost its GND field
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: since commit a3f5f4db, node capacitance is written as `C <name>0 <cap>` instead of `C <name> GND <cap>`.
- Evidence: ext2sim/ext2sim.c:1660-1666, git show a3f5f4db (corrected 2026-11-13: The behaviour is described correctly, but the lines have moved: simnodeVisit writes `C `, the name, then `esSpiceCapNode` immediately followed by a space and `%.1f`, at ext2sim/ext2sim.c:1644-1652, not :1660-1666. esSpiceCapNode is set to esSpiceDefaultGnd = "0" (ext2sim.c:698, ext2spice/ext2spice.c:62). `git show a3f5f4db` confirms that the old format was ` GND %.<acc>lf`. The current line `C out0 5.0` has only two fields, so irsim reads it as a malformed capacitor.)
- Options: replicate | fix (`C <name> GND <cap>`) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the line is `C <name>0 <%.1f>`, byte-identical to Magic 8.3.694. With it OFF, it is `C <name> GND <%.1f>`, the pre-a3f5f4db format that irsim parses. Coupling (`C n1 n2`) and resistor lines are the same in both modes. (corrected 2026-11-13, was fix with a compatibility flag)
- Resolution: compat — with `magic-compat` ON ext2sim writes node capacitance as `C <name>0 <cap>`, byte-identical to Magic 8.3.694; with it OFF as `C <name> GND <cap>`, which irsim parses. Coupling and resistor lines are the same in both modes. Approved 2026-11-13.

### Q-NETLIST-16: Name-conversion flags leak between commands
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: `ext2spice` permanently ORs trim-global, convert-equal and convert-comma into the global flags. A later `ext2sim` in the same session inherits them, while a fresh `ext2sim` does not trim.
- Evidence: ext2spice/ext2spice.c:1081-1084, ext2sim/ext2sim.c:430-445 (corrected 2026-11-13: The behaviour is correct, but the cited lines are wrong. ext2spice ORs EF_TRIMGLOB|EF_CONVERTEQUAL|EF_CONVERTCOMMA into the global EFOutputFlags at ext2spice/ext2spice.c:1071 (not :1081-1084), plus EF_TRIMLOCAL for HSPICE at :1073-1074. ext2sim.c:430-445 is the `format` option. The real point is that ext2sim never clears EFOutputFlags at run start: only `ext2sim default` does, at ext2sim/ext2sim.c:495. EFHNOut (extflat/EFvisit.c:878-920) then applies the leftover flags. The `-t` characters also accumulate across runs of either tool (extflat/EFargs.c:215-220), and `ext2spice default` resets to COMMA|EQUAL (ext2spice.c:794-796).)
- Options: replicate | fix (per-command flags) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto keeps one session-wide flag set with Magic's OR-only updates, so `ext2sim` after `ext2spice` trims `!` while a fresh `ext2sim` does not. With it OFF, each run starts from its command's own base flags: ext2spice forces the set from :1071-1075, and ext2sim starts with none. The run's `-t` characters are added on top, and nothing carries over to later runs or to the other command. Golden captures run each tool in a fresh session, where both modes agree. (corrected 2026-11-13, was fix)
- Resolution: compat — with `magic-compat` ON name-conversion flags are one session-wide set with Magic's OR-only updates; with it OFF each run starts from its own command's base flags plus its `-t` characters, and nothing carries over. Goldens run each tool in a fresh session. Approved 2026-11-13.

### Q-NETLIST-17: ext2sim merges devices of different class or type
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: ext2sim's parallel test checks only nodes and L/W, so an nfet and a pfet with the same nodes can merge.
- Evidence: ext2sim/ext2sim.c:1786-1795 (corrected 2026-11-13: The behaviour is described correctly, but the lines have moved. The parallelDevs macro is at ext2sim/ext2sim.c:1770-1778 and is used by simmergeVisit (:1794-1858). It compares only g, b, truncated l (and w in conservative mode), and s/d or swapped s/d. ext2spice's parallelDevs does check dev_class and dev_type (ext2spice/ext2spice.c:4558-4570). Merging also covers 2-terminal devices such as resistors, so devices of different types on the same nodes and with the same L/W can fold into one line, and a device disappears from the .sim.)
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, ext2sim merging ignores class and type, as Magic does. With it OFF, two devices merge only when they also have the same dev_class and dev_type, which is the test ext2spice already applies. Merging is off by default (`ext2sim merge none`), so default .sim output is the same in both modes. (corrected 2026-11-13, was fix)
- Resolution: compat — with `magic-compat` ON ext2sim merging ignores device class and type, as Magic does; with it OFF devices also need equal class and type, as in ext2spice. Merging is off by default. Approved 2026-11-13.

### Q-NETLIST-18: ext2sim `-c` does not enable coupling output
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: whether couplings are flattened depends on the option-level threshold, not on a `-c` given for the run.
- Evidence: ext2sim/ext2sim.c:672-674 (corrected 2026-11-13: The behaviour is described correctly, but the lines have moved. EF_FLATCAPS depends on LocCapThreshold, the option-level `cthresh`, at ext2sim/ext2sim.c:676, and EF_FLATRESISTS likewise depends on LocResistThreshold at :677. `-c` and `-r` set only EFCapThreshold and EFResistThreshold (extflat/EFargs.c `case 'c'`/`'r'`). ext2spice tests the run-level EFCapThreshold instead (ext2spice/ext2spice.c:1072). So `ext2sim cthresh inf` followed by `ext2sim -c 5` writes no coupling lines, while node lines do use 5.)
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, coupling and resistor flattening follow the option-level thresholds, as Magic does. With it OFF, they follow the thresholds in force for the run (after `-c` and `-r`), as ext2spice does. The default thresholds are finite, so the defaults give the same output in both modes. (corrected 2026-11-13, was fix)
- Resolution: compat — with `magic-compat` ON ext2sim flattens coupling caps and resistors by the option-level thresholds, as Magic does; with it OFF by the run's thresholds after `-c` and `-r`. The defaults give the same output in both modes. Approved 2026-11-13.

### Q-NETLIST-19: Hierarchical and flat SPICE formatting disagree
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the two modes differ in these ways:
  - Flat mode uses signed comparisons for capacitance thresholds; hierarchical mode uses |C|.
  - The flat perimeter parameter in SI mode uses 1e-12 instead of 1e-6.
  - The hierarchical modeled resistor in scale mode prints integer `w=%d l=%d`.
  - The "connected" test differs between the modes.
- Evidence: ext2spice/ext2spice.c:2283-2285, ext2spice/ext2hier.c:575-578, ext2spice/ext2hier.c:1497-1499, ext2spice/ext2hier.c:1655-1660 (corrected 2026-11-13: All four differences are confirmed, but most of the lines are wrong. (1) Flat perimeter `p`/`p0` in SI mode uses `1.0E-12` at ext2spice/ext2spice.c:2297-2299 (case 'p' :2286); the hierarchical writer uses 1.0E-6 (ext2hier.c:305-307). Area correctly uses 1e-12 in both. (2) The flat cap thresholds are signed: `cap <= thr` at ext2spice.c:4002 and `cap > thr` at :4180; the hierarchical ones use fabs (ext2hier.c:1494, :1673). (3) The hierarchical modeled resistor in scale mode prints `w=%d l=%d` at ext2hier.c:567-570 (the entry cites :575-578); flat prints `%g` at ext2spice.c:2566. (4) Connected test: flat checks the visitMask bit efNumResistClasses (ext2spice.c:4157-4161), while hierarchical checks for a non-zero mask (ext2hier.c:1655-1660).)
- Options: replicate | fix (unify) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat for the two output bugs, replicate for the two per-mode design choices. (a) Flat SI perimeter factor: with magic-compat ON, 1e-12 as Magic; with it OFF, 1e-6, as the hierarchical writer and the unit (µm) require. (b) Hierarchical scale-mode modeled resistor w/l: with ON, truncated `%d`; with OFF, `%g` as in flat mode, so fractional lambda values are not lost. (c) Signed vs |C| thresholds: replicate per mode in both settings. In hierarchical output, negative caps are legitimate parent-level corrections; after full flattening they are round-off. (d) The 'connected' test: replicate per mode. It only drives the `**FLOATING` comment and `-F`, and Magic defines no single intended rule. (corrected 2026-11-13, was replicate per mode, perimeter factor fixed behind a flag)
- Resolution: compat for the flat SI perimeter factor (ON 1e-12, OFF 1e-6) and the hierarchical scale-mode modeled resistor w/l (ON truncated `%d`, OFF `%g`); replicate per output mode, in both settings, for the signed vs |C| thresholds and the 'connected' test. Approved 2026-11-13.

### Q-NETLIST-20: Hierarchical output omits node capacitances without a `substrate` record
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: in compatibility mode (old `.ext` files with no `substrate` line), hierarchical output writes no node capacitance lines at all.
- Evidence: ext2spice/ext2hier.c:2425-2437
- Verified 2026-11-13: Confirmed at ext2spice/ext2hier.c:2431-2441: the whole substrate-node search and spcnodeHierVisit are inside `if (EFCompat == FALSE)`. EFCompat is cleared only when a `substrate` record or global substrate node is read (extflat/EFbuild.c:237, :315). Flat mode writes node caps in both cases (ext2spice/ext2spice.c:1180-1186), to the substrate it finds or else to esSpiceDefaultGnd "0". Hierarchical output of an old-style .ext therefore loses every node-to-ground capacitance and lumped node resistance.
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, hierarchical output writes no node lines when no `substrate` record was read, as Magic does. With it OFF, node lines are written as in flat mode, to the found substrate node or else to `0`. Only legacy .ext files (no `substrate` record) are affected. Current extractions, and every reference PDK, produce the same output in both modes. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON hierarchical output writes no node capacitance lines when no `substrate` record was read, as Magic does; with it OFF they are written as in flat mode, to the substrate node or `0`. Only legacy .ext files are affected. Approved 2026-11-13.

### Q-NETLIST-21: extresist FastHenry frequency is never initialized
- Kind: bug
- Magic behavior: an unterminated comment swallows `frequency = 10e6`. `.freq` prints garbage unless the user supplies a frequency.
- Evidence: resis/ResRex.c:240-241
- Options: fix (default 10 MHz)
- Recommendation: fix.
- Resolution: fix — the FastHenry frequency defaults to 10 MHz instead of being uninitialised (X-2 tier 1, 2026-11-07).

### Q-NETLIST-22: Drive/sink renaming stubs
- Kind: doc-mismatch
- Magic behavior: comments promise `.uX` and `.dX` names for drivers and sinks, but the routines are empty. The sink loop also passes the wrong variable.
- Evidence: resis/ResRex.c:1469-1514, resis/ResRex.c:2196-2203
- Options: replicate (no renaming) | fix
- Recommendation: replicate (no-op).
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-NETLIST-23: `extresist skip` takes a single type
- Kind: doc-mismatch
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the documentation says it takes a comma-separated mask. The code accepts a single type name, and with no argument resets the masks.
- Evidence: resis/ResRex.c:497-516, doc/html/extresist.html (corrected 2026-11-13: The behaviour is described correctly, but the lines have moved: RES_SKIP is at resis/ResRex.c:572-588, not :497-516. The documentation (doc/html/extresist.html:71-72) describes a comma-separated mask. The entry misses a tier-1 defect: when DBTechNoisyNameType fails (-1 ambiguous, -2 unknown, which includes any comma list), the code still runs `TTMaskZero(&ResCopyMask[j])` with j < 0 (:578). That is an out-of-bounds write. Only the clearing loop is guarded by `j >= 0`.)
- Options: replicate | fix (accept a list)
- Recommendation: fix (both modes). `skip` accepts a comma-separated list of types, as documented. A single type keeps working exactly as in Magic: it is removed from every net-copy connectivity mask and its own mask is cleared. An ambiguous or unknown name reports the error and changes nothing. With no argument, the masks are reset to technology connectivity, as in Magic. Any input Magic handles without undefined behaviour gives the same result. The inputs that differ (lists and bad names) are an out-of-bounds write in Magic (X-2 tier 1), so there is no Magic output to match.
- Resolution: fix — in both modes `extresist skip` takes a comma-separated list of types; a single type behaves as in Magic, an ambiguous or unknown name reports an error and changes no mask, and no argument resets the masks (Magic writes out of bounds for lists and bad names, X-2 tier 1). Approved 2026-11-13.

### Q-NETLIST-24: Gate load uses (W+W) for source/drain capacitance
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the downstream capacitance adds `(W+W)·sdCap`, probably intended as `(L+W)` or `2W`.
- Evidence: resis/ResSimple.c:790-796 (corrected 2026-11-13: The lines have moved and the entry's premise is wrong. The term is at resis/ResSimple.c:798-802, not :790-796. exts_deviceSDCap is the tech file's `gscap`, documented as 'capacitance between the transistor's gate and its diffusion terminals, in attofarads per lambda' (doc/latexfiles/maint2.tex:3044-3045). A gate has two diffusion edges of length W, so (W+W)·gscap = 2W·gscap is the intended formula, not a typo for (L+W). (A separate oddity, not in scope: ExtTech.c:3884 and :4051 scale gscap quadratically, as if it were per area; that is Q-EXTRACT-3.))
- Options: replicate | fix
- Recommendation: replicate. Both modes compute Cdown with `L·W·gateCap + 2W·sdCap`, as Magic does. This is not a bug: gscap is per unit length of gate–diffusion edge and a MOSFET has two such edges. The value only feeds the Elmore-delay cutoff.
- Resolution: replicate — in both modes the gate load is `L·W·gateCap + 2·W·sdCap`, as Magic does: gscap is per edge length and a MOSFET has two gate–diffusion edges. Approved 2026-11-13.

### Q-NETLIST-25: Path-resistance heap uses integer keys
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: Dijkstra keys are truncated to int mΩ, so fractional ties and their order depend on the heap.
- Evidence: resis/ResSimple.c:1160-1165 (corrected 2026-11-13: The behaviour is described correctly, but the lines have moved. The key is `node->rn_noderes + res->rr_value` passed to HeapAddInt at resis/ResSimple.c:1156-1157 (resPathNode). rn_noderes is an int (resis/resis.h:191) and rr_value a float (:38), so the key is truncated. Node path resistances are truncated as well (ResSimple.c:1200, :1208). The heap is utils/heap.c with HeapInit(&ResistorHeap,128,FALSE,FALSE) (min-heap, ResSimple.c:1092), and its tie order is deterministic given the insertion order. The spec already gives the exact algorithm (extresist-network:505-516).)
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. Resolve together with Q-NETLIST-46. With magic-compat ON, path resistances and heap keys are truncated to int mΩ and ties follow Magic's heap exactly, as the spec now describes. With it OFF, path resistances and keys are floats (double), and equal keys are broken by insertion order (a stable priority queue). Only the choice of upstream end and which loop resistor is marked delay-ignored can change, and only on fractional or equal path resistances. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON path resistances and heap keys are int mΩ and ties follow Magic's heap; with it OFF they are doubles and ties break by insertion order. Resolved together with Q-NETLIST-46. Approved 2026-11-13.

### Q-NETLIST-26: Unstable sort in final simplification
- Kind: platform
- Magic behavior: with 10 or more resistors, `qsort` is used, so equal values may be reordered differently by different C libraries.
- Evidence: resis/ResSimple.c:470-500
- Options: replicate (glibc order) | fix (stable sort)
- Recommendation: fix (stable sort). Golden networks may then differ only in equal-value cases.
- Resolution: fix — stable sort, one order on every platform (X-1 platform-dependent sorts, as amended by D-1, 2026-11-07).

### Q-NETLIST-27: FastHenry z for contact-only nodes is uninitialized
- Kind: bug
- Magic behavior: if a node touches only contact resistors, `height` is never assigned before it is used.
- Evidence: resis/ResPrint.c:445-460
- Options: fix (use the contact's upper-layer height)
- Recommendation: fix.
- Resolution: fix — a contact-only node takes the height of the contact's upper layer instead of an uninitialised value (X-2 tier 1, 2026-11-07).

### Q-NETLIST-28: `mindelay` query reports raw zeptoseconds in the standalone build
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the text says picoseconds, but the stored zs value is printed. The Tcl result converts correctly.
- Evidence: resis/ResRex.c:335-345 (corrected 2026-11-13: The behaviour is described correctly, but the lines have moved: RES_MINDELAY is at resis/ResRex.c:371-390, not :335-345. The value is stored as `MagAtof(v) * P_TO_Z`. The Tcl build returns `mindelay * Z_TO_P` as a double (correct ps), and only the non-Tcl TxPrintf prints the raw zs value with 'picoseconds'. Under D-7 the Tcl build is normative and non-Tcl behaviour is informative.)
- Options: replicate (the normative Tcl build reports ps) | fix
- Recommendation: replicate (the Tcl build, D-7). The query returns the delay in ps, as Magic's normative Tcl build does. Wherever Hefesto also prints the value as text, it prints ps. The raw-zs print in the non-Tcl build is not reproduced in either mode. No magic-compat switch is needed, because the normative Magic behaviour is already correct. (corrected 2026-11-13, was fix)
- Resolution: replicate — the `extresist mindelay` query returns ps in both modes, as Magic's normative Tcl build (D-7) does; the non-Tcl build's raw-zs print is not reproduced. Approved 2026-11-13.

### Q-NETLIST-29: `attr` records apply to the previous node in extresist
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: `res:` attributes attach to the last `node` record read, not to the node named by the `attr` line.
- Evidence: resis/ResReadExt.c:209-212 (corrected 2026-11-13: Confirmed but incomplete. ATTR passes `curnode`, the last NODE/SUBSTRATE record, at resis/ResReadExt.c:204-210, not :209-212. ResReadAttribute (:1000-1060) never looks at the name. The extractor writes `attr` lines directly after their node (extract/ExtBasic.c:757-771), so extractor output gives the same result either way. Two tier-1 defects the entry misses: `curnode` is an uninitialised local (:139), so an `attr` before any `node` record dereferences garbage; and `res:min=<v>` copies into `char digit[MAXDIGIT=20]` (:105, :1033-1037) with no bound, a stack overflow for long values.)
- Options: replicate | fix (look up by name) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, with two tier-1 fixes in both modes. With magic-compat ON, an `attr` applies to the most recent node record, as in Magic. With it OFF, it applies to the node named in the record, and an unknown name prints a warning and is ignored. In both modes an `attr` with no preceding node is ignored with a warning, and the `res:min=` value is parsed without a fixed buffer. For extractor-written .ext files, ON and OFF are identical. (corrected 2026-11-13, was fix)
- Resolution: compat — with `magic-compat` ON an `attr` record applies to the most recent node record, as Magic does; with it OFF to the named node (unknown names warn and are ignored). In both modes an `attr` before any node is ignored with a warning and `res:min=` has no length limit (tier 1). Approved 2026-11-13.

### Q-NETLIST-30: Leaf-name conversion deletes characters anywhere
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: SPICE leaf conversion deletes every `!` (trim-global) and every `#` (trim-local), not only a trailing one. `=` and `,` are deleted when their conversions are off. `.`→`@` (HSPICE) applies to the leaf only. In non-leaf components a comma is deleted, but `=` is kept.
- Evidence: ext2spice/ext2spice.c:4325-4390 (corrected 2026-11-13: The behaviour is confirmed, but the lines are slightly off: the leaf is in EFHNSprintf at ext2spice/ext2spice.c:4307-4343 and the prefix in efHNSprintfPrefix at :4345-4375. extflat/EFtypes.h:41-42 defines EF_TRIMGLOB and EF_TRIMLOCAL as 'Delete trailing !/#', and the .sim writer EFHNOut trims only the trailing character (extflat/EFvisit.c:895-917). The SPICE leaf conversion therefore departs from the flags' definition. `=` is never deleted in practice, because ext2spice always sets CONVERTEQUAL (:1071). Edge-case UB the entry misses: a root prefix component that starts with `,` (comma conversion off) runs `str--` before the output buffer and can write `/` there (:4362-4369).)
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat for trim position, replicate for the rest. With magic-compat ON, every `!` (trim-global) and every `#` (trim-local) in the SPICE leaf is deleted, as Magic does. With it OFF, only a trailing `!`/`#` is deleted, matching the flag definitions and the .sim writer, so that `a!b` and `ab` stay distinct nets. These stay the same in both modes: `=` and `,` deletion when their conversion is off; `.`→`@` on the leaf only in HSPICE; comma deletion in prefixes. In both modes, a leading comma in a root prefix component is simply dropped, with no write before the buffer (tier 1). (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON every `!` and `#` in the SPICE leaf is deleted, as Magic does; with it OFF only a trailing one. The other conversions are the same in both modes, and a leading comma in a root prefix component is dropped without writing before the buffer (tier 1). Approved 2026-11-13.

### Q-NETLIST-31: Shorted-port devices create a `dummy` node
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the 0 Ω or 0 V device inserted for shorted ports has a first terminal named `dummy`. This creates a real node `dummy` in the cell, which receives pi capacitance redistribution.
- Evidence: extflat/EFbuild.c:640-660, extflat/EFbuild.c:1825-1840 (corrected 2026-11-13: The device is built at extflat/EFbuild.c:638-648 (equiv) and :1817-1836 (merge). argv[1]="dummy" is terminal 0 of a 3-terminal `None` resistor or voltage source, and efBuildDevNode creates or reuses one node named `dummy` per def, so every short device in a cell shares it. The node does reach the netlist: with `ext2spice resistor tee on`, ext2spice/ext2spice.c:3105-3125 writes a shorted-port resistor as two halves, gate→source and gate→drain, so `dummy` is the middle node of each. Two separate shorted-port pairs A/B and C/D in one cell are then all joined through `<path>/dummy`, shorting two distinct nets, and a user net named `dummy` is merged in as well. In the default pi mode, spcdevResPi (ext2spice.c:3887-3913) moves the `dummy` node's capacitance onto the two ports and zeroes it, so the entry's "receives pi capacitance redistribution" is right; a user net named `dummy` loses its capacitance this way. A 0 V short (`short voltage`) writes only terminals 1 and 2.)
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, every short device in a cell takes the shared terminal-0 node `dummy`, as Magic does, including the tee-mode joining and the pi-mode capacitance move. With it OFF, each short device gets its own anonymous terminal-0 node, which can never collide with a user net or another short device; the device keeps three terminals and its visit order, so only tee-mode midpoints and a user `dummy` net differ. Golden: `short resistor`, `resistor tee on`, one cell with two shorted-port pairs, in both modes. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON every short device in a cell takes the shared terminal-0 node `dummy`, as Magic does; with it OFF each gets its own anonymous terminal-0 node. Golden: `short resistor`, `resistor tee`, one cell with two shorted-port pairs, in both modes. Approved 2026-11-13.

### Q-NETLIST-32: Consumed `l<n>`/`w<n>` parameters leave a stray space
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: after an `l1=`/`w1=` verbatim entry is used by a declared parameter, it is blanked, but still printed as ` ` (a single space).
- Evidence: ext2spice/ext2spice.c:2393-2398, ext2spice/ext2spice.c:2510-2512 (corrected 2026-11-13: The behaviour is described correctly, but the lines have moved. Blanking is `dparam->parm_name[0] = '\0'` at ext2spice/ext2spice.c:2388 and :2433 (hierarchical: ext2hier.c:379, :424), not :2393-2398. The verbatim loop `fprintf(" %s")` is at ext2spice.c:2495-2497, not :2510-2512. The blanking changes dev_params in place, so a device written twice in one run, such as the two halves of a hierarchical tee, finds no `l<n>`/`w<n>` the second time.)
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, each consumed verbatim entry still prints a single space, as Magic does (byte identity). With it OFF, consumed entries are skipped, so lines have no doubled space. SPICE parsers ignore the extra whitespace, so simulation and LVS results are the same in both modes. Consumption is per write in both modes, matching Magic's in-place blanking within a run. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON each consumed `l<n>`/`w<n>` verbatim entry still writes a single space, as Magic does; with it OFF it is skipped. Approved 2026-11-13.

### Q-NETLIST-33: Tcl-variable substitutions
- Kind: platform
- Magic behavior: the following values come from Tcl variables:
  - default substrate names of the form `$var`
  - CDL globals from `$SUB`, `$VDD` and `$GND` when they end in `!`
  - the compatibility ground name from `$GND`
  - extresist substrate names
- Evidence: ext2spice/ext2spice.c:943-956, ext2spice/ext2spice.c:1005-1050, resis/ResPrint.c:126-133
- Options: platform alternative (project variables map)
- Recommendation: provide a per-project variable map with the same names. iPadOS has no Tcl.
- Resolution: platform — decided by D-14 (2026-11-07). `$SUB`, `$VDD` and `$GND` come from the `sub`, `vdd` and `gnd` fields of the project setting "global nets and power names" (`hefesto-settings`), seeded from the startup scripts; the Tcl variables mirror the setting.

### Q-NETLIST-34: Flat device names from gate attributes lack a separator
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: a device named by its gate attribute prints the raw instance path immediately followed by the attribute, for example `Mu1foo` for path `u1`.
- Evidence: ext2spice/ext2spice.c:2850-2853 (corrected 2026-11-13: The behaviour is described correctly, but the lines have moved: the code is at ext2spice/ext2spice.c:2876-2881 (`fprintf("%s%s", EFHNToStr(hierName), gate->dterm_attrs)`), not :2850-2853. The hierarchical writer prints the attribute alone (ext2hier.c:815-816). The missing separator can make names collide: path `u1` with attribute `1foo` and path `u11` with attribute `foo` both give `Mu11foo`, a duplicate SPICE instance name.)
- Options: replicate | fix (insert `/`) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, flat mode writes the instance path immediately followed by the attribute (`Mu1foo`), as Magic does. With it OFF, it inserts `/` between them (`Mu1/foo`), as in flat node names, so names cannot collide. Top-level devices have an empty path and are the same in both modes, and hierarchical mode is unchanged. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON flat device names from gate attributes join path and attribute with no separator (`Mu1foo`), as Magic does; with it OFF with `/` (`Mu1/foo`). Hierarchical mode and top-level devices are the same in both modes. Approved 2026-11-13.

### Q-NETLIST-35: Hierarchical voltage-source counter not reset per cell
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: all other counters restart per subcircuit, but `V` numbering continues across cells and across runs.
- Evidence: ext2spice/ext2hier.c:2455-2462, ext2spice/ext2spice.c:826-835 (corrected 2026-11-13: Partly wrong. The per-cell reset at ext2spice/ext2hier.c:2453-2459 indeed omits esVoltNum. However, `esVoltNum = 0` is reset at the start of every run (ext2spice/ext2spice.c:828), so the entry's 'and across runs' is incorrect: numbering continues across subcircuits within one run only. V devices come only from `short voltage` (extflat/EFbuild.c).)
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. In both modes, numbering restarts at 0 for each run. With magic-compat ON, `V` numbering continues across subcircuits within a run, as Magic does. With it OFF, it restarts per subcircuit, like the C, R, D, X and device counters. Instance names stay unique within each subcircuit either way, so simulation and LVS are the same in both modes. Only the names differ, and only with `short voltage`. (corrected 2026-11-13, was replicate)
- Resolution: compat — numbering restarts per run in both modes; with `magic-compat` ON the voltage-source counter continues across subcircuits within a run, as Magic does; with it OFF it restarts per subcircuit like the other counters. Approved 2026-11-13.

### Q-NETLIST-36: extcheck default capacitance threshold
- Kind: doc-mismatch
- Magic behavior: the man page says the default is 10 fF. The code uses 2 fF.
- Evidence: doc/man/extcheck.1, extflat/EFargs.c:47
- Options: replicate code | follow doc
- Recommendation: replicate the code (2 fF), and fix the doc.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-NETLIST-37: Flat SPICE ignores ports for subcircuit marking
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: with `hierarchy off`, files are read without subcircuit marking. `subcircuits top auto` therefore never wraps the top cell, and `subcircuits descend on` never emits subcircuit calls in flat mode. Only `top on` writes a wrapper.
- Evidence: ext2spice/ext2spice.c:880-886, extflat/EFread.c:466-472 (corrected 2026-11-13: Confirmed, but the lines have moved: the read is `EFReadFile(inName, esDoHierarchy, ...)` at ext2spice/ext2spice.c:888, not :880-886, and port marking is at extflat/EFread.c:469-472. Commit 9800d98d (2024-04) introduced this on purpose, to stop flat mode emitting subcircuit calls for child cells with ports. Its message admits that `subcircuit descend off` no longer works. As a side effect, the default `subcircuits top auto` never wraps the top cell in flat mode (ext2spice.c:1133-1137), although the documentation (doc/html/ext2spice.html:101-112) says auto wraps when ports exist. EFVisitSubcircuits needs DEF_SUBCIRCUIT (extflat/EFvisit.c:104), so `descend` has no effect in flat mode.)
- Options: replicate | fix (mark subcircuits in flat mode as documented) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, flat mode reads without subcircuit marking, as in Magic 8.3.694: `top auto` never wraps, only `top on` does, and `descend` is inert. With it OFF, `top auto` wraps the top cell when the root .ext has `port` records, as documented. Child cells are still fully flattened (the intent of 9800d98d), and `descend` remains inert in flat mode, since Magic defines no working flat `descend off`. With OFF this changes default flat netlists of cells with ports, which gain a .subckt wrapper. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON flat `top auto` never wraps, only `top on` does, as Magic 8.3.694 does; with it OFF `top auto` wraps the top cell when the root .ext has `port` records. `descend` is inert in flat mode in both. User choice (a) of the closest calls. Approved 2026-11-13.

### Q-NETLIST-38: Tile fracture and near-device resistor rules not fully specified
- Kind: ambiguity
- Magic behavior: the concave-corner fracture (ResFracture/ResCheckConcavity) and the near-device cutting (ResCalcNearDevice) define the exact resistor topology. They are only summarized in the spec.
- Evidence: resis/ResFract.c:64-490, resis/ResMakeRes.c:547-855
- Options: specify from golden captures | port the algorithm literally
- Recommendation: capture golden networks (NL-RESNET cases), then refine `extresist-network` with exact pseudo-code before implementation.
- Status: resolved. The fracture walk, concavity test, near-device cutting and the rest of the network construction are now specified as exact pseudo-code in `extresist-network` (requirements "Tile fracturing" and "Near-device cutting"). Golden captures NL-RESNET-07..14 confirm them.

### Q-NETLIST-39: `.res.ext` device records omit value and L/W for non-MOS classes
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: re-emitted `devres`, `devcap` and similar records have no value or L/W fields. On reading back, the reader expects them, so terminals are misparsed or rejected.
- Evidence: resis/ResPrint.c:136-160, extflat/EFbuild.c:930-948 (corrected 2026-11-13: Partly wrong. ResPrintExtDev is at resis/ResPrint.c:109-185, with the class switch at :150-163, which writes area/perimeter for fet and L W for mosfet/asymmetric/bjt only. The reader needs a value or L W only for resistor, capacitor and caprev records (extflat/EFbuild.c:939-948, :1082-1110, :1207-1260). Diodes and the subcircuit classes have none and parse correctly, so 'non-MOS classes' overstates the problem. For a resistor, the substrate token is taken as the value. Three terminals are then read (gate, source, drain) against the original's two, so the record is rejected with 'incompatible number of terminals' (:1160-1167), or with 'Incomplete terminal description' with a model. The overlay's terminal rewiring for that device is lost, while its original nodes have been killed.)
- Options: replicate | fix (emit the same fields as the extractor) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, Hefesto writes Magic's record (byte-identical .res.ext) and reads it back with Magic's reader, as Magic does. With it OFF, resistor, capacitor and caprev records carry the same leading fields the extractor writes: the value, or L W when the device has a model. The terminals of the 2-terminal classes are written as in extOutputDevices, so the overlay replaces the original device's terminals. The reader is the same in both modes. Other classes are unchanged. (corrected 2026-11-13, was fix)
- Resolution: compat — with `magic-compat` ON the .res.ext device record is Magic's; with it OFF resistor, capacitor and caprev records carry the value (or L W when modelled) and the extractor's terminal layout. The reader is the same in both modes. Approved 2026-11-13.

### Q-NETLIST-40: FastHenry unnamed-node ids restart for every net
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the counters for unnamed nodes (`N<id>`) and segments (`E<k>`) restart at 1 for each net, so several nets in one `.fh` file reuse the same names.
- Evidence: resis/ResRex.c:1076-1078
- Verified 2026-11-13: Confirmed. `int nidx = 1, eidx = 1` are locals of ResProcessNode (resis/ResRex.c:1077), called once per net (:1396). There is a single .fh file per run (:1360). Unnamed nodes print as `N<id>` (resis/ResPrint.c:393-394, ids assigned at :430-434) and segments as `E<k>` (:630, :647), so nets in one file reuse `N1`, `E1` and so on, which FastHenry treats as the same node or segment.
- Options: replicate | fix (per-file counters) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, the counters restart at 1 for each net, as Magic does. With it OFF, `N` and `E` counters run across the whole .fh file, so every node and segment name is unique and multi-net files are valid FastHenry input. Single-net files (the usual `extresist fasthenry` on one net) are the same in both modes. The geometry output (ResCreateCenterlines, :2219) uses the same counters. (corrected 2026-11-13, was fix)
- Resolution: compat — with `magic-compat` ON FastHenry unnamed-node and segment counters restart at 1 per net, as Magic does; with it OFF they run across the whole .fh file. Approved 2026-11-13.

### Q-NETLIST-41: `subcircuits descend auto` changes the top setting
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the value `auto` under `descend` sets the *top* wrapper mode to auto.
- Evidence: ext2spice/ext2spice.c:604-606 (corrected 2026-11-13: The behaviour is described correctly, but the lines have moved: the second-word switch is at ext2spice/ext2spice.c:629-646, and `case IDX_AUTO: esDoSubckt = AUTO` (:641-643) does not check whether the first word was `descend`. The entry cites :604-606.)
- Options: replicate | fix (reject) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. This is a parse-argument quirk that changes results (X-2 tier 2). With magic-compat ON, `subcircuits descend auto` sets the top wrapper mode to auto, as Magic does. With it OFF, it prints the usage message and changes nothing, since `descend` accepts only on/off. (corrected 2026-11-13, was fix)
- Resolution: compat — with `magic-compat` ON `subcircuits descend auto` sets the top wrapper mode to auto, as Magic does; with it OFF it prints the usage message and changes nothing. Approved 2026-11-13.

### Q-NETLIST-42: `-o` output name persists across runs
- Kind: bug
- Magic behavior: the output name pointer keeps the `-o` argument after the run, which may also be a dangling pointer. Later runs without `-o` write to the old file name.
- Evidence: ext2spice/ext2spice.c:1452-1455, ext2spice/ext2spice.c:872-878
- Options: fix (per-run)
- Recommendation: fix.
- Resolution: fix — the `-o` output name applies to one run only (dangling pointer) (X-2 tier 1, 2026-11-07).

### Q-NETLIST-43: Near-device tile with a single breakpoint loses its area
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: a tile whose breakpoints include a device terminal, and which has only that one breakpoint, is dropped without adding its area to the node. Tiles cut east-west or north-south add the whole tile area in the same situation. The node's share of the net capacitance is therefore too small.
- Evidence: resis/ResMakeRes.c:565-570, resis/ResMakeRes.c:148-154
- Verified 2026-11-13: Confirmed at resis/ResMakeRes.c:565-570: ResCalcNearDevice frees the only breakpoint without adding area. The east-west and north-south cases add `height × width` to the breakpoint's node at :148-154. Capacitance is distributed in proportion to node areas (`area × C/Σarea`), so the net total is kept but shifts away from the node next to the device.
- Options: replicate | fix (add `width × height` to the node) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, a near-device tile with one breakpoint adds no area, as Magic does. With it OFF, it adds `width × height` to that breakpoint's node, as the other cutting routines do. Only how the net capacitance is shared between nodes changes; the topology and resistor values stay the same. NL-RESNET goldens are captured in ON mode. (corrected 2026-11-13, was fix)
- Resolution: compat — with `magic-compat` ON a near-device tile with a single breakpoint adds no area, as Magic does; with it OFF it adds width × height to that breakpoint's node. NL-RESNET goldens are captured with ON. Approved 2026-11-13.

### Q-NETLIST-44: Delta-to-star conversion adds 0.5 mΩ instead of rounding
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the star resistor values are computed as `Ra·Rb/S + 0.5` and stored in a float. The `+ 0.5` was meant to round to an integer, but it adds half a milliohm to every arm.
- Evidence: resis/ResMerge.c:630-641, resis/ResMerge.c:765-776
- Verified 2026-11-13: Confirmed at resis/ResMerge.c:638-640 and :773-775: `rr_value = r + 0.5`, with rr_value a float. This is the same leftover integer-rounding term as Q-NETLIST-12, which persisted after resistor values became float. Each star arm gains 0.5 mΩ. The spec text already shows the +0.5 (extresist-network:459-461).
- Options: replicate | fix (drop the + 0.5) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. With magic-compat ON, each star arm is `Ra·Rb/S + 0.5`, as Magic does. With it OFF, it is `Ra·Rb/S` with no offset. This is the same kind of quirk as X-2's `.res.ext` +0.5 mΩ example. Resolve together with Q-NETLIST-12. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON each delta-to-star arm is `Ra·Rb/S + 0.5`, as Magic does; with it OFF `Ra·Rb/S`. Resolved together with Q-NETLIST-12. Approved 2026-11-13.

### Q-NETLIST-45: Triangle search order depends on hash-table iteration for high-degree nodes
- Kind: quirk
- Magic behavior: for a node with 10 or more resistors, neighbours are visited in hash-table iteration order. The table is keyed by node addresses, so when several triangles exist, the one that is reduced first can differ between runs and platforms.
- Evidence: resis/ResMerge.c:560-585
- Options: replicate (impossible to reproduce exactly) | fix (iterate neighbours in resistor-list order, as for small nodes)
- Recommendation: fix with list order. Golden networks with high-fanout triangles may differ.
- Resolution: fix — address-keyed order: neighbours of high-degree nodes are visited in resistor-list order, as for small nodes (X-1 memory-address order, as amended by D-1, 2026-11-07).

### Q-NETLIST-46: Path-resistance heap behaves differently for the first net of a session
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the heap is created once per session, and its "built" state is never reset. For the first net, the origin's resistors are appended unbuilt and heapified on the first removal. For later nets, every insert sifts up. Ties between equal integer keys can therefore be broken differently for the same net, depending on whether it is the first net extracted.
- Evidence: resis/ResSimple.c:1088-1125, utils/heap.c:242-260, utils/heap.c:386-452
- Verified 2026-11-13: Confirmed. A static `init` creates the heap once (resis/ResSimple.c:1086-1093) and it is never killed. HeapRemoveTop sets `he_built = he_used` before decrementing (utils/heap.c:251-258), so after the first removal he_built is never 0 again. Every later insert then sifts up (heap.c:414-452), while the first net's initial inserts are appended unbuilt and heapified on the first removal. With equal integer keys (Q-NETLIST-25), the same net can therefore come out differently depending on whether it is the first net of the session.
- Options: replicate | fix (deterministic tie-break, e.g. insertion order) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat, resolved with Q-NETLIST-25. With magic-compat ON, Hefesto keeps Magic's session-wide heap state, unbuilt for the first net of the session and built afterwards. The golden harness pins this with one extresist run per fresh session, as for M5's Q-MASK-204. With it OFF, each net uses a fresh priority queue that breaks ties by insertion order, so results do not depend on session history. (corrected 2026-11-13, was fix)
- Resolution: compat — with `magic-compat` ON the path-resistance heap keeps Magic's session-wide state (golden captures run in a fresh session); with it OFF each net uses a fresh queue with insertion-order ties. Resolved together with Q-NETLIST-25. Approved 2026-11-13.

### Q-NETLIST-47: Final-pass sort and neighbour search edge cases
- Kind: bug
- Magic behavior:
  - With fewer than 10 resistors, the final pass sorts by repeated insertion before the first value that is greater or equal. Equal values come out in reverse order, so this is not a stable sort.
  - When the smallest adjacent resistor `work` is the head of the resistor list, which can happen after protected resistors are skipped, the code dereferences a null predecessor while unlinking it.
  - If neither end of the current resistor has another resistor, `work` stays null.
- Evidence: resis/ResSimple.c:512-527, resis/ResSimple.c:612-620
- Options: replicate the order and guard the crashes | fix (stable sort)
- Recommendation: replicate the order (it decides which equal resistor is folded first). Guard both null cases by skipping the resistor.
- Resolution: fix — both null cases are guarded by skipping the resistor (NULL dereference); the final-pass insertion order stays Magic's, as the entry recommends (X-2 tier 1, 2026-11-07).

### Q-NETLIST-48: sim2simp is shipped as source only and is not built
- Kind: platform
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: `ext2sim/sim2simp.c` is a standalone stdin/stdout filter that merges parallel MOSFETs in a `.sim` file. The makefile no longer builds or installs it ("deprecated/removed"). It also has quirks:
  - every non-device line is echoed with an extra blank line;
  - the last field keeps its newline;
  - attribute parsing assumes the `A_<n>,P_<n>` form.
- Evidence: ext2sim/Makefile.in:18-20, ext2sim/sim2simp.c:125-170
- Verified 2026-11-13: Confirmed. ext2sim/Makefile.in:20 reads `main: finds # sim2simp and ext2sim_main are deprecated/removed`. In ext2sim/sim2simp.c, non-device lines are echoed with puts() after fgets(), which doubles the newline (:152), and device lines are rebuilt with `%g` and `A_%d,P_%d` fields (:158-166).
- Options: omit | reimplement as an optional tool (`merge` option of the `.sim` writer covers the use case)
- Recommendation: other: omit. Hefesto does not provide sim2simp in either mode. Magic itself neither builds nor installs it, so there is no Magic-provided tool to match. Its function, merging parallel MOSFETs in .sim output, is covered by `ext2sim merge conservative|aggressive`. The informative description in netlist-sim may stay as a record.
- Resolution: other — Hefesto does not provide sim2simp in either mode; Magic does not build it, and `ext2sim merge` covers its function. Approved 2026-11-13.

