# Netlist Window and Netlist Files

## Purpose

Defines Magic's netlist facility used by the routers and by interactive netlist editing: the `.net` file format (exact grammar, reading and writing), the in-memory netlist model (named terminals grouped into nets), the netlist window (`specialopen netlist`) and its command set (`add`, `cleanup`, `cull`, `dnet`, `dterm`, `extract`, `find`, `flush`, `joinnets`, `measure`, `netlist`, `print`, `pushbutton`, `ripup`, `savenetlist`, `shownet`, `showterms`, `trace`, `verify`, `writeall`), the netlist tool in layout windows (`netlist select|join|terminal`), and the conversion of a netlist into terminal locations for the routers (capabilities `router-channel`, `router-gate-array`). Terminal names are hierarchical label names (`useid/useid/label`). This capability is desktop-only in the first iPadOS release (decision P-13; capability `hefesto-ipad-feature-scope`).

## Requirements

### Requirement: Netlist file grammar
A netlist file SHALL have the extension `.net` and this grammar (lines terminated by newline; a line is truncated to 255 characters by the reader):

```ebnf
net_file   = header , { line } ;
header     = " Netlist File" , NL | " Net List File" , NL ;      (* compared case-insensitively, exact otherwise *)
line       = separator | comment | terminal ;
separator  = NL | " " , { char } , NL ;      (* empty line, or any line whose first character is a space *)
comment    = "#" , { char } , NL ;
terminal   = non_space_char , { char } , NL ;  (* the whole line (without newline) is the terminal name *)
```

Semantics: consecutive terminal lines form one net; a separator ends the current net; comments are ignored and do not end a net. A terminal appearing again is moved to the later net with the warning `Warning: terminal "%s" appears in more than one net.` / `    Only the last appearance will be used.`. Terminal names are case-sensitive and may contain any characters other than newline (including spaces after the first character). A file without a valid header reports `%s isn't a legal netlist file.` and `Creating new netlist.`, yielding an empty netlist associated with that file. Reading is not undoable and leaves the netlist unmodified.

Example:

```
 Netlist File

# clock net
clkgen_0/clk
reg_0/clk
reg_1/clk

reg_0/q
alu_0/a
```

Compatibility: exact
(Magic ref: netmenu/NMnetlist.c:395-470)

#### Scenario: Two nets read
- **GIVEN** the example file above
- **WHEN** it is read
- **THEN** the netlist has two nets: {clkgen_0/clk, reg_0/clk, reg_1/clk} and {reg_0/q, alu_0/a}

#### Scenario: Bad header
- **GIVEN** a file `x.net` whose first line is `Netlist File` (no leading space)
- **WHEN** `netlist x` is executed
- **THEN** `x.net isn't a legal netlist file.` and `Creating new netlist.` are reported and the netlist is empty

### Requirement: Netlist selection and file lookup
Selecting a netlist named N SHALL: make N current if a netlist with exactly that name is already loaded; otherwise search for `N.net` along the cell search path (current directory, cell path, library path); if not found report `Netlist file %s.net couldn't be found.` / `Creating new netlist.` and associate the new empty netlist with file `N.net`. Selecting clears the current net selection. An empty name deselects (no current netlist). The netlist name is shown in the netlist window. `netlist` with no argument uses the edit cell's name.

Compatibility: behavioral
(Magic ref: netmenu/NMnetlist.c:345-395, netmenu/NMcmdLZ.c:412-428)

#### Scenario: Missing file creates netlist
- **GIVEN** no file `foo.net` exists on the search path
- **WHEN** `netlist foo` is executed
- **THEN** the two messages are printed and a new empty netlist bound to `foo.net` becomes current

### Requirement: Netlist file writing
`savenetlist [file]` SHALL write the current netlist (`There isn't a current net list to write.` if none): without an argument to the file it was read from (or would have been created as); with an argument to `file.net` in the current directory, asking `Net list file %s already exists.  Should I overwrite it? [no] ` if it exists (only `y` or `yes` overwrite). Output: the line ` Netlist File`, then for each net a blank line followed by one terminal name per line. Net order and terminal order are the netlist's enumeration order: entries are visited in Magic's string hash-table order (see next requirement), and each not-yet-visited entry starts a net written beginning at that terminal and continuing around the net's circular order (the order terminals were added, rotated to start at the first visited terminal). Writing to the original file clears the modified flag. Write failure: `Couldn't write file %s.`.

Compatibility: exact
(Magic ref: netmenu/NMnetlist.c:405-470, netmenu/NMnetlist.c:480-540)

#### Scenario: Round trip of a single net
- **GIVEN** a netlist with one net read from lines `a`, `b`, `c`
- **WHEN** `savenetlist` writes it back
- **THEN** the file is ` Netlist File`, an empty line, then the three names in the circular order starting at whichever of `a`, `b`, `c` the hash order visits first (e.g. `b`, `c`, `a`)

### Requirement: Netlist enumeration order
The netlist's terminal table SHALL be a hash table with Magic's ordering so that written files and router net order are reproducible: initial bucket count 32 (`downShift = 25`, `mask = 31`); string hash `h = 0; for each byte c (as a sign-extended char): h = c + (h << 6) + (h << 16) - h` in unsigned 64-bit arithmetic; bucket = `((h * 1103515245 + 12345) >> downShift) & mask` (64-bit unsigned); new entries are prepended to their bucket; when the entry count reaches 3 × bucket count before an insertion, the table grows 4× (downShift decreases by 2, mask = 4×size−1) by re-inserting entries in old-bucket order (each prepended). Iteration visits buckets in increasing index, entries in bucket-list order. Deleted terminals leave empty entries that are skipped. See Q-LEGACY-70 regarding platform dependence.

Compatibility: exact
(Magic ref: utils/hash.c:146-262, utils/hash.c:353-520, netmenu/NMnetlist.c:520-575)

#### Scenario: Deterministic order
- **GIVEN** the same set of terminal names inserted in the same order
- **WHEN** the netlist is written twice
- **THEN** both files are byte-identical

### Requirement: Netlist editing operations
The netlist window commands SHALL edit the current netlist (`Select a netlist first.` when none) as follows; every edit marks the netlist modified and is undoable:
- `add term1 term2`: remove term1 from its net (if any) and add it to the net of term2 (term2 becomes a one-terminal net first if unknown). Usage error: `Usage: add term1 term2`.
- `dterm name...`: remove each named terminal from its net (`"%s" isn't in the current netlist.  Do you have the right netlist?.` if absent).
- `dnet [name...]`: delete the whole net containing each name, or the current (selected) net if no names.
- `joinnets term1 term2`: merge the two nets (no-op if already the same net; `"%s" isn't in a net, so can't join it.` if either is unknown).
- `print [name]`: print `Nodes in net:` followed by `    %s` for each terminal of the named net or the current net (`Can't print current net:  there's nothing selected!`, `There's nothing in the current net!`).
- `flush [netlist]`: discard changes by reloading the netlist from disk; if modified asks `Really throw away all changes made to netlist "%s"? [no] ` (`Netlist "%s" isn't currently loaded.`, `There's no current netlist to flush.`); confirming also flushes the undo history.
- `writeall`: for each modified netlist ask `%s: write, skip, or abort command? [write] `.
- `cleanup`: for each net with fewer than two terminals (`Net "%s" has less than two terminals.`) and each terminal with no matching label in the edit cell hierarchy (`"%s" doesn't exist in the circuit.`), ask `Delete terminal (dterm), delete net (dnet), skip, or abort command? [default] ` and act; `No problems found.` otherwise.
- When quitting with modified netlists the user is asked `Net-list "%s" has been modified.  Do you want to lose the changes? [no] ` (or the count form).

Compatibility: behavioral
(Magic ref: netmenu/NMcmdAK.c:40-360, netmenu/NMcmdLZ.c:428-470, netmenu/NMcmdLZ.c:640-660, netmenu/NMnetlist.c:60-300, netmenu/NMnetlist.c:575-700)

#### Scenario: Add moves a terminal
- **GIVEN** nets {a, b} and {c, d}
- **WHEN** `add a c` is executed
- **THEN** the nets are {b} and {c, d, a}

### Requirement: Verify
`verify` SHALL check the edit cell's wiring against the current netlist. For each net (in enumeration order): starting from the first terminal (in net order) that has at least one label location (`Terminal "%s" not found` for each terminal skipped), trace all paint electrically connected to every location of that terminal (anything touching the label area expanded by 1, through the edit cell's connectivity), collecting every label (in any cell) attached to that paint whose hierarchical name is a terminal of the netlist, and recursively tracing from all other locations of each collected name (feedthroughs). Then:
- every terminal of the net not collected reports `Terminal "%s" not connected.` and adds feedback `Net of "%.100s" isn't fully connected.` over its first label location expanded by 1;
- the first collected terminal not in this net reports `Net "%s" shorted to net "%s".` and adds feedback `Net "%.80s" shorted to net "%.80s".\n` (the feedback text includes a trailing newline; see Q-LEGACY-72) over that label expanded by 1;
- if the net has errors and labels not in the netlist were found, print `Error found on net of %s:` / `Additional electrically connected labels:` and each name preceded by a tab.
Finally print `No wiring errors found.`, `One feedback area generated (you're getting close!).` or `%d feedback areas generated.`.

Compatibility: behavioral
(Magic ref: netmenu/NMwiring.c:560-1060)

#### Scenario: Open net
- **GIVEN** net {A/x, B/y} where only A/x's label is wired
- **WHEN** `verify` is executed
- **THEN** `Terminal "B/y" not connected.` is printed, one feedback area is added at B/y, and `One feedback area generated (you're getting close!).` is printed

### Requirement: Cull
`cull` SHALL remove from the current netlist every net that verifies with no missing terminals and no shorts (shorts still produce the short message and feedback, and the net is kept), then print `No fully-wired nets found.`, `One fully-wired net deleted from the netlist.` or `%d fully-wired nets deleted from the netlist.`. Unlike verify, cull starts tracing only from the net's first terminal in enumeration order.

Compatibility: behavioral
(Magic ref: netmenu/NMwiring.c:1062-1180)

#### Scenario: Cull a routed net
- **GIVEN** two nets, one fully wired and one unwired
- **WHEN** `cull` is executed
- **THEN** the wired net is deleted and `One fully-wired net deleted from the netlist.` is printed

### Requirement: Extract, ripup, trace and show
The commands SHALL behave as:
- `extract`: trace all edit-cell paint connected to paint under the box; collect labels in subcells under that paint (labels in the edit cell itself are ignored); subcells touched by the paint without any label report `Cell id %s touches net but has no terminals.`; terminals already in another net are left there (`Warning: %s was already in a net (I left it there).`); the others form a new net which becomes the current net. None found: `There aren't any terminals connected to paint under the box` / `(except those, if any, already in other nets).`.
- `ripup`: erase all edit-cell paint connected to paint under the box; `ripup netlist`: erase paint connected to every terminal location of every net (`The only permissible argument to "ripup" is "netlist".`).
- `trace [name]`: highlight paint connected to the terminals of the named or current net.
- `shownet [erase]`: highlight (not as feedback) the paint connected to paint under the box; `erase` removes the highlight.
- `showterms`: add feedback `"Showterms" result` over every terminal location of the netlist.
- `find pattern [layers]`: add feedback `<shorttype>;<hierarchical name>` over each label under the box matching the pattern (on the given layers), with the label area expanded by 1 (this is the input format of `net2ir`).
- `measure [all [file]]`: report wire length statistics `Total: %d;  Metal: %d;  Poly: %d;  Vias: %d` for the selection or, with `all`, for every net (optionally logging to a file).

Compatibility: behavioral
(Magic ref: netmenu/NMwiring.c:40-560, netmenu/NMwiring.c:1180-1390, netmenu/NMcmdAK.c:263-306, netmenu/NMcmdLZ.c:369-412, netmenu/NMcmdLZ.c:504-640, netmenu/NMlabel.c:600-654, netmenu/NMshowcell.c, netmenu/NMshowpt.c)

#### Scenario: Find labels
- **GIVEN** a label `in` on metal1 at (10,20)-(12,22) inside use `u0` under the box
- **WHEN** `find in` is executed in the netlist window
- **THEN** a feedback area (9,19)-(13,23) with text `m1;u0/in` (short type name first) is created

### Requirement: Netlist window and tool
`specialopen netlist` SHALL open at most one netlist window (`Sorry, can't have more than one netlist menu at a time.`) captioned `NETLIST MENU`, containing: a label-name entry with next/previous controls, label placement and orientation controls, two numeric fields for label name generation, `Find`, the current netlist name (click to type a new name at `New net list name: `), and buttons `Verify`, `Print`, `Terms`, `Cleanup`, `No Net` (delete current net), `Show` (show net under box). `pushbutton left|middle|right` invokes the button action under the cursor. In layout windows, the netlist tool binds: left = `netlist select` (select the net of the terminal nearest the cursor, printing `Selected net is now "%s".`), right = `netlist terminal` (toggle the nearest terminal into/out of the current net: `Adding "%s" to net.` / `Removing "%s" from net.`, moving it from another net with `"%s" was already in a net;  I'm removing it from the old net.`), middle = `netlist join` (merge the nearest terminal's net into the current net: `Merging net "%s" into current net.`). The nearest terminal is the nearest label within 20 screen pixels of the cursor in a window editing the edit cell's root, and it must be inside a subcell (`You can't route to a terminal in the Edit cell!  Please select one in a subcell.`; none: `There's no terminal near the cursor.`). Selected-net terminals are highlighted. The window layout and highlight styles are behavioral; Hefesto MAY present an equivalent UI.

Compatibility: behavioral
(Magic ref: netmenu/NMmain.c:55-200, netmenu/NMmain.c:360-573, netmenu/NMbutton.c:60-509, tcltk/tools.tcl:560-570, doc/html/netlist.html)

#### Scenario: Terminal in edit cell rejected
- **GIVEN** the netlist tool is active and the nearest label is in the edit cell itself
- **WHEN** the right button is pressed
- **THEN** `You can't route to a terminal in the Edit cell!  Please select one in a subcell.` is printed and the netlist is unchanged

### Requirement: Netlist resolution for routing
Building the routing netlist SHALL enumerate the current netlist (enumeration order), creating one routing net per net and one terminal per name; each new net and each new terminal is PREPENDED to its list (so the routers see nets and terminals in reverse enumeration order). Each terminal's locations are all label occurrences matching its hierarchical name in the route cell hierarchy (each new location prepended), and each net's area is the bounding box of its locations. Diagnostics: `Warning: terminal %s appears in more than one net`, `Terminal %s couldn't be found`, and feedback `Net %s has only one terminal` over the single terminal's first location expanded by 1. The result is the number of nets.

Compatibility: exact
(Magic ref: utils/netlist.c:60-290)

#### Scenario: Single-terminal net warning
- **GIVEN** a netlist with a net containing only `u0/a`, whose label is at (0,0)-(2,2)
- **WHEN** a router builds the netlist
- **THEN** feedback `Net u0/a has only one terminal` is added over (-1,-1)-(3,3)
