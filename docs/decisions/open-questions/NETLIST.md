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
- Magic behavior: in flat mode, an unnamed `c` parameter writes the device resistance instead of the capacitance. Hierarchical mode writes the capacitance.
- Evidence: ext2spice/ext2spice.c:2128-2134 vs ext2spice/ext2hier.c:158-163
- Options: replicate | fix
- Recommendation: fix, since this is plainly wrong in output. Keep a compatibility flag if golden tests require it.

### Q-NETLIST-5: Flat-mode tee for `rsubckt` never writes its second half
- Kind: bug
- Magic behavior: the code that writes the `X<n>B` line comes after a `break`, so it is unreachable. Flat output has only `X<n>A` with half the length. Hierarchical output writes both halves.
- Evidence: ext2spice/ext2spice.c:3056-3100
- Options: replicate | fix (write the B half as in hierarchical mode)
- Recommendation: fix, and gate the behavior behind a compatibility switch for golden comparison.

### Q-NETLIST-6: Flat merge multipliers desynchronize on 1-terminal devices
- Kind: bug
- Magic behavior: the merge pre-pass skips devices with fewer than 2 terminals without reserving a multiplier slot. The output pass consumes a slot for every device with at least one terminal. Later devices then read the wrong multiplier, so a device can be dropped or receive a wrong `M=`.
- Evidence: ext2spice/ext2spice.c:4730-4734, ext2spice/ext2spice.c:2725-2727
- Options: replicate | fix (index by device identity)
- Recommendation: fix.

### Q-NETLIST-7: Terminal area/perimeter divided by M only in scale mode
- Kind: quirk
- Magic behavior: with `.option scale`, `ad/as/pd/ps` are divided by the merge multiplier. Without it they are not.
- Evidence: ext2spice/ext2spice.c:3760-3788, ext2spice/ext2spice.c:3870-3883
- Options: replicate | fix (always divide)
- Recommendation: replicate. It affects LVS and simulation numbers, so decide together with PDK users.

### Q-NETLIST-8: Parameter scale and offset handling
- Kind: quirk
- Magic behavior: the `+N`/`-N` offset is applied only in the SI (scale-off, no `*k`) branch. It is ignored in scale mode and when `*k` is present. When `*k` is present, the offset is never initialized; it is unused in that case.
- Evidence: extflat/EFbuild.c:808-834, ext2spice/ext2spice.c:2222-2236
- Options: replicate | fix (apply the offset in all branches)
- Recommendation: replicate.

### Q-NETLIST-9: Declared parameter order is reversed on output
- Kind: quirk
- Magic behavior: `parameters` tokens are prepended to a list, so output order is the reverse of declaration order. Verbatim device parameters are also reversed.
- Evidence: extflat/EFbuild.c:858-866, extflat/EFbuild.c:955-962
- Options: replicate | fix
- Recommendation: replicate. PDK netlists depend on it.

### Q-NETLIST-10: Name preference favors the lexically greater name
- Kind: quirk
- Magic behavior: on a full tie (same component count and length), the name with the greater `strcmp` wins. For example `b` beats `a`.
- Evidence: extflat/EFname.c:575-592
- Options: replicate | fix
- Recommendation: replicate. Node names in netlists depend on it.

### Q-NETLIST-11: Global names in the Tcl build use the `globals` Tcl array
- Kind: platform
- Magic behavior: the Tcl build decides whether a name is global by looking up `$globals(<leaf>)`, not by a trailing `!`. Global merging, implicit substrate ports and alias filtering all change with it.
- Evidence: extflat/EFname.c:101-118
- Options: replicate (configuration map of global names) | platform alternative (trailing-`!` rule)
- Recommendation: use the trailing-`!` rule by default, and offer an optional configured global-name set that emulates the Tcl array.
- Resolution: platform — decided by D-14 (2026-11-07). A name is global if its leaf is in `globalNets` of the project setting "global nets and power names" (`hefesto-settings`), seeded from the PDK's `globals` array; this follows Magic's Tcl build, and a trailing `!` alone does not make a name global in flattening.

### Q-NETLIST-12: `resist` values are offset by +0.5 mΩ
- Kind: quirk
- Magic behavior: on reading, `resist` values become `(float)(0.5 + rscale·R)`. The intended rounding is never truncated, so SPICE shows `2.0005` for 2000 mΩ.
- Evidence: extflat/EFread.c:600-603
- Options: replicate | fix (round to integer)
- Recommendation: replicate. It is visible in every `extresist-command` netlist.

### Q-NETLIST-13: `ext2spice run` and unknown option words become the cell name
- Kind: doc-mismatch
- Magic behavior: the option table's first entry is `[run]`, which cannot match `run`. Any unmatched or ambiguous first word (for example `re`) is passed to the run as a cell name.
- Evidence: ext2spice/ext2spice.c:290-292, ext2spice/ext2spice.c:375-380, utils/lookup.c:86-120
- Options: replicate | fix (accept `run`, and reject ambiguous words)
- Recommendation: fix. Accept `run` and report ambiguous prefixes. (verify against Magic Tcl wrapper behavior)

### Q-NETLIST-14: ext2sim merge-type table lacks a terminator
- Kind: bug
- Magic behavior: the `merge` value table has no NULL sentinel, so lookup reads past the array.
- Evidence: ext2sim/ext2sim.c:325-329
- Options: fix
- Recommendation: fix. Accept `none`, `conservative` and `aggressive`.
- Resolution: fix — the merge value table is terminated, so lookup accepts `none`, `conservative` and `aggressive` without reading past the array (X-2 tier 1, 2026-11-07).

### Q-NETLIST-15: ext2sim node-capacitance line lost its GND field
- Kind: bug
- Magic behavior: since commit a3f5f4db, node capacitance is written as `C <name>0 <cap>` instead of `C <name> GND <cap>`.
- Evidence: ext2sim/ext2sim.c:1660-1666, git show a3f5f4db
- Options: replicate | fix (`C <name> GND <cap>`)
- Recommendation: fix. irsim cannot parse the current form. Offer a compatibility flag.

### Q-NETLIST-16: Name-conversion flags leak between commands
- Kind: quirk
- Magic behavior: `ext2spice` permanently ORs trim-global, convert-equal and convert-comma into the global flags. A later `ext2sim` in the same session inherits them, while a fresh `ext2sim` does not trim.
- Evidence: ext2spice/ext2spice.c:1081-1084, ext2sim/ext2sim.c:430-445
- Options: replicate | fix (per-command flags)
- Recommendation: fix (per-command state), and document that golden captures must run each tool in a fresh session.

### Q-NETLIST-17: ext2sim merges devices of different class or type
- Kind: bug
- Magic behavior: ext2sim's parallel test checks only nodes and L/W, so an nfet and a pfet with the same nodes can merge.
- Evidence: ext2sim/ext2sim.c:1786-1795
- Options: replicate | fix
- Recommendation: fix (also require the same class and type).

### Q-NETLIST-18: ext2sim `-c` does not enable coupling output
- Kind: quirk
- Magic behavior: whether couplings are flattened depends on the option-level threshold, not on a `-c` given for the run.
- Evidence: ext2sim/ext2sim.c:672-674
- Options: replicate | fix
- Recommendation: fix.

### Q-NETLIST-19: Hierarchical and flat SPICE formatting disagree
- Kind: quirk
- Magic behavior: the two modes differ in these ways:
  - Flat mode uses signed comparisons for capacitance thresholds; hierarchical mode uses |C|.
  - The flat perimeter parameter in SI mode uses 1e-12 instead of 1e-6.
  - The hierarchical modeled resistor in scale mode prints integer `w=%d l=%d`.
  - The "connected" test differs between the modes.
- Evidence: ext2spice/ext2spice.c:2283-2285, ext2spice/ext2hier.c:575-578, ext2spice/ext2hier.c:1497-1499, ext2spice/ext2hier.c:1655-1660
- Options: replicate | fix (unify)
- Recommendation: replicate per mode. The flat perimeter factor is a clear bug and should be fixed behind a compatibility flag.

### Q-NETLIST-20: Hierarchical output omits node capacitances without a `substrate` record
- Kind: quirk
- Magic behavior: in compatibility mode (old `.ext` files with no `substrate` line), hierarchical output writes no node capacitance lines at all.
- Evidence: ext2spice/ext2hier.c:2425-2437
- Options: replicate | fix
- Recommendation: replicate.

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
- Magic behavior: the documentation says it takes a comma-separated mask. The code accepts a single type name, and with no argument resets the masks.
- Evidence: resis/ResRex.c:497-516, doc/html/extresist.html
- Options: replicate | fix (accept a list)
- Recommendation: fix (accept a list; a single type remains valid).

### Q-NETLIST-24: Gate load uses (W+W) for source/drain capacitance
- Kind: quirk
- Magic behavior: the downstream capacitance adds `(W+W)·sdCap`, probably intended as `(L+W)` or `2W`.
- Evidence: resis/ResSimple.c:790-796
- Options: replicate | fix
- Recommendation: replicate. It affects only the delay cutoff.

### Q-NETLIST-25: Path-resistance heap uses integer keys
- Kind: quirk
- Magic behavior: Dijkstra keys are truncated to int mΩ, so fractional ties and their order depend on the heap.
- Evidence: resis/ResSimple.c:1160-1165
- Options: replicate | fix
- Recommendation: replicate truncation, and reproduce the heap's tie order (verify against Magic).

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
- Magic behavior: the text says picoseconds, but the stored zs value is printed. The Tcl result converts correctly.
- Evidence: resis/ResRex.c:335-345
- Options: fix
- Recommendation: fix (always report ps).

### Q-NETLIST-29: `attr` records apply to the previous node in extresist
- Kind: quirk
- Magic behavior: `res:` attributes attach to the last `node` record read, not to the node named by the `attr` line.
- Evidence: resis/ResReadExt.c:209-212
- Options: replicate | fix (look up by name)
- Recommendation: fix. The extractor writes `attr` immediately after its node, so results normally match.

### Q-NETLIST-30: Leaf-name conversion deletes characters anywhere
- Kind: quirk
- Magic behavior: SPICE leaf conversion deletes every `!` (trim-global) and every `#` (trim-local), not only a trailing one. `=` and `,` are deleted when their conversions are off. `.`→`@` (HSPICE) applies to the leaf only. In non-leaf components a comma is deleted, but `=` is kept.
- Evidence: ext2spice/ext2spice.c:4325-4390
- Options: replicate | fix
- Recommendation: replicate.

### Q-NETLIST-31: Shorted-port devices create a `dummy` node
- Kind: quirk
- Magic behavior: the 0 Ω or 0 V device inserted for shorted ports has a first terminal named `dummy`. This creates a real node `dummy` in the cell, which receives pi capacitance redistribution.
- Evidence: extflat/EFbuild.c:640-660, extflat/EFbuild.c:1825-1840
- Options: replicate | fix
- Recommendation: replicate. It is invisible unless the node has capacitance.

### Q-NETLIST-32: Consumed `l<n>`/`w<n>` parameters leave a stray space
- Kind: quirk
- Magic behavior: after an `l1=`/`w1=` verbatim entry is used by a declared parameter, it is blanked, but still printed as ` ` (a single space).
- Evidence: ext2spice/ext2spice.c:2393-2398, ext2spice/ext2spice.c:2510-2512
- Options: replicate | fix
- Recommendation: replicate. It affects byte identity only.

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
- Magic behavior: a device named by its gate attribute prints the raw instance path immediately followed by the attribute, for example `Mu1foo` for path `u1`.
- Evidence: ext2spice/ext2spice.c:2850-2853
- Options: replicate | fix (insert `/`)
- Recommendation: replicate.

### Q-NETLIST-35: Hierarchical voltage-source counter not reset per cell
- Kind: quirk
- Magic behavior: all other counters restart per subcircuit, but `V` numbering continues across cells and across runs.
- Evidence: ext2spice/ext2hier.c:2455-2462, ext2spice/ext2spice.c:826-835
- Options: replicate | fix
- Recommendation: replicate within a run. `V` numbering also restarts at 0 per run.

### Q-NETLIST-36: extcheck default capacitance threshold
- Kind: doc-mismatch
- Magic behavior: the man page says the default is 10 fF. The code uses 2 fF.
- Evidence: doc/man/extcheck.1, extflat/EFargs.c:47
- Options: replicate code | follow doc
- Recommendation: replicate the code (2 fF), and fix the doc.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (D-5, 2026-11-07).

### Q-NETLIST-37: Flat SPICE ignores ports for subcircuit marking
- Kind: quirk
- Magic behavior: with `hierarchy off`, files are read without subcircuit marking. `subcircuits top auto` therefore never wraps the top cell, and `subcircuits descend on` never emits subcircuit calls in flat mode. Only `top on` writes a wrapper.
- Evidence: ext2spice/ext2spice.c:880-886, extflat/EFread.c:466-472
- Options: replicate | fix (mark subcircuits in flat mode as documented)
- Recommendation: replicate. Confirm with golden output first (verify against Magic).

### Q-NETLIST-38: Tile fracture and near-device resistor rules not fully specified
- Kind: ambiguity
- Magic behavior: the concave-corner fracture (ResFracture/ResCheckConcavity) and the near-device cutting (ResCalcNearDevice) define the exact resistor topology. They are only summarized in the spec.
- Evidence: resis/ResFract.c:64-490, resis/ResMakeRes.c:547-855
- Options: specify from golden captures | port the algorithm literally
- Recommendation: capture golden networks (NL-RESNET cases), then refine `extresist-network` with exact pseudo-code before implementation.
- Status: resolved. The fracture walk, concavity test, near-device cutting and the rest of the network construction are now specified as exact pseudo-code in `extresist-network` (requirements "Tile fracturing" and "Near-device cutting"). Golden captures NL-RESNET-07..14 confirm them.

### Q-NETLIST-39: `.res.ext` device records omit value and L/W for non-MOS classes
- Kind: bug
- Magic behavior: re-emitted `devres`, `devcap` and similar records have no value or L/W fields. On reading back, the reader expects them, so terminals are misparsed or rejected.
- Evidence: resis/ResPrint.c:136-160, extflat/EFbuild.c:930-948
- Options: replicate | fix (emit the same fields as the extractor)
- Recommendation: fix (verify against Magic).

### Q-NETLIST-40: FastHenry unnamed-node ids restart for every net
- Kind: bug
- Magic behavior: the counters for unnamed nodes (`N<id>`) and segments (`E<k>`) restart at 1 for each net, so several nets in one `.fh` file reuse the same names.
- Evidence: resis/ResRex.c:1076-1078
- Options: replicate | fix (per-file counters)
- Recommendation: fix.

### Q-NETLIST-41: `subcircuits descend auto` changes the top setting
- Kind: quirk
- Magic behavior: the value `auto` under `descend` sets the *top* wrapper mode to auto.
- Evidence: ext2spice/ext2spice.c:604-606
- Options: replicate | fix (reject)
- Recommendation: fix (reject with usage).

### Q-NETLIST-42: `-o` output name persists across runs
- Kind: bug
- Magic behavior: the output name pointer keeps the `-o` argument after the run, which may also be a dangling pointer. Later runs without `-o` write to the old file name.
- Evidence: ext2spice/ext2spice.c:1452-1455, ext2spice/ext2spice.c:872-878
- Options: fix (per-run)
- Recommendation: fix.
- Resolution: fix — the `-o` output name applies to one run only (dangling pointer) (X-2 tier 1, 2026-11-07).

### Q-NETLIST-43: Near-device tile with a single breakpoint loses its area
- Kind: bug
- Magic behavior: a tile whose breakpoints include a device terminal, and which has only that one breakpoint, is dropped without adding its area to the node. Tiles cut east-west or north-south add the whole tile area in the same situation. The node's share of the net capacitance is therefore too small.
- Evidence: resis/ResMakeRes.c:565-570, resis/ResMakeRes.c:148-154
- Options: replicate | fix (add `width × height` to the node)
- Recommendation: fix after golden comparison (verify against Magic). The change alters only capacitance distribution.

### Q-NETLIST-44: Delta-to-star conversion adds 0.5 mΩ instead of rounding
- Kind: bug
- Magic behavior: the star resistor values are computed as `Ra·Rb/S + 0.5` and stored in a float. The `+ 0.5` was meant to round to an integer, but it adds half a milliohm to every arm.
- Evidence: resis/ResMerge.c:630-641, resis/ResMerge.c:765-776
- Options: replicate | fix (drop the + 0.5)
- Recommendation: replicate. Output values are printed in Ω with limited precision, so the effect is usually invisible.

### Q-NETLIST-45: Triangle search order depends on hash-table iteration for high-degree nodes
- Kind: quirk
- Magic behavior: for a node with 10 or more resistors, neighbours are visited in hash-table iteration order. The table is keyed by node addresses, so when several triangles exist, the one that is reduced first can differ between runs and platforms.
- Evidence: resis/ResMerge.c:560-585
- Options: replicate (impossible to reproduce exactly) | fix (iterate neighbours in resistor-list order, as for small nodes)
- Recommendation: fix with list order. Golden networks with high-fanout triangles may differ.
- Resolution: fix — address-keyed order: neighbours of high-degree nodes are visited in resistor-list order, as for small nodes (X-1 memory-address order, as amended by D-1, 2026-11-07).

### Q-NETLIST-46: Path-resistance heap behaves differently for the first net of a session
- Kind: quirk
- Magic behavior: the heap is created once per session, and its "built" state is never reset. For the first net, the origin's resistors are appended unbuilt and heapified on the first removal. For later nets, every insert sifts up. Ties between equal integer keys can therefore be broken differently for the same net, depending on whether it is the first net extracted.
- Evidence: resis/ResSimple.c:1088-1125, utils/heap.c:242-260, utils/heap.c:386-452
- Options: replicate | fix (deterministic tie-break, e.g. insertion order)
- Recommendation: fix together with Q-NETLIST-25.

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
- Magic behavior: `ext2sim/sim2simp.c` is a standalone stdin/stdout filter that merges parallel MOSFETs in a `.sim` file. The makefile no longer builds or installs it ("deprecated/removed"). It also has quirks:
  - every non-device line is echoed with an extra blank line;
  - the last field keeps its newline;
  - attribute parsing assumes the `A_<n>,P_<n>` form.
- Evidence: ext2sim/Makefile.in:18-20, ext2sim/sim2simp.c:125-170
- Options: omit | reimplement as an optional tool (`merge` option of the `.sim` writer covers the use case)
- Recommendation: omit. Point users to the `merge` option of `ext2sim`. See the informative note in capability `netlist-sim`.

