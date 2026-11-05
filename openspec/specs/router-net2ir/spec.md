# Router: net2ir Converter

## Purpose

Defines `net2ir`, a standalone converter shipped with Magic that turns a saved feedback file (label locations produced by the netlist `find` command followed by `feedback save`) plus a netlist file into a script of `iroute route` commands that route each two-terminal net. Hefesto may provide this as a separate tool or as an import function; the textual output format is the compatibility surface.

## Requirements

### Requirement: net2ir invocation
The tool SHALL be invoked with exactly two arguments, a feedback file and a netlist file, and write the generated commands to standard output and diagnostics to standard error. With a wrong argument count it SHALL print `Usage: net2ir feedfile netfile` and exit with status 1. If either file cannot be opened it SHALL print the operating-system error for that path and exit with status 1. On success it SHALL exit with status 0.

Compatibility: behavioral
(Magic ref: net2ir/net2ir.c:60-80, net2ir/net2ir.c:120-130, net2ir/net2ir.c:178)

#### Scenario: Wrong argument count
- **WHEN** `net2ir only-one-file` is run
- **THEN** `Usage: net2ir feedfile netfile` is written to standard error and the exit status is 1

### Requirement: Feedback file parsing
The feedback file SHALL be read line by line (lines up to 1023 characters). A record is a line matching `box %d %d %d %d` immediately followed by a line matching `feedback add "<layer>;<label>` (layer = text up to the first `;`, label = text up to the next `"`). If the second line does not match it is re-examined as a potential `box` line. Lines matching neither are ignored. Each label maps to its first record: area = box shrunk by 1 on every side, and the layer text. A repeated label prints `Warning: multiple locs for label %s; 2nd loc ignored.` and keeps the first.

```ebnf
feed_file   = { line } ;
record      = "box " , int , " " , int , " " , int , " " , int , NL ,
              "feedback add \"" , layer , ";" , label , "\"" , [ " " , style ] , NL ;
layer       = { char - ";" }- ;
label       = { char - "\"" }- ;
```

Example (as written by `find` + `feedback save` in Magic):

```
box 9 19 13 23
feedback add "m1;in" pale
box 99 19 103 23
feedback add "m1;out" pale
```

Compatibility: exact
(Magic ref: net2ir/net2ir.c:82-118, commands/CmdFI.c:416-447, netmenu/NMlabel.c:635-653)

#### Scenario: Label area restored
- **GIVEN** the record `box 9 19 13 23` / `feedback add "m1;in" pale`
- **WHEN** the feedback file is parsed
- **THEN** label `in` maps to layer `m1` and area (10,20)-(12,22)

### Requirement: Netlist conversion output
The netlist file SHALL be processed line by line: a line whose first character is whitespace (including an empty line or the header line ` Netlist File`) starts a new net (terminal count reset to 0); any other line (trailing newline removed) is a terminal name. For each terminal:
- if the current net already has 2 or more terminals: print `Net with >2 terms ignored: %s` to standard error and skip it;
- else if the name has no feedback location: print `No location for terminal %s` to standard error and skip it (the terminal count is not incremented);
- else if it is the net's first located terminal: write `:iroute route -slayers %s -sPoint %d %d ` (layer, area xbot, area ybot; note trailing space and no newline);
- else write `-dlayers %s -dRect %d %d %d %d\n` (layer, area xbot ybot xtop ytop) and increment the count.

Comment lines are not recognised and are treated as terminal names (see Q-LEGACY-25). A net whose second terminal is missing leaves an unterminated `:iroute` fragment that runs into the next output (see Q-LEGACY-24).

Compatibility: exact
(Magic ref: net2ir/net2ir.c:120-177)

#### Scenario: Two-terminal net
- **GIVEN** the feedback example above and the netlist ` Netlist File` / (blank) / `in` / `out`
- **WHEN** net2ir runs
- **THEN** standard output is exactly `:iroute route -slayers m1 -sPoint 10 20 -dlayers m1 -dRect 100 20 102 22` followed by a newline

#### Scenario: Three-terminal net
- **GIVEN** a net with located terminals `a`, `b`, `c` in that order
- **WHEN** net2ir runs
- **THEN** the command for `a`→`b` is written and `Net with >2 terms ignored: c` is written to standard error
