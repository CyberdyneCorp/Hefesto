# netlist-extcheck

## Purpose

This capability defines `extcheck`, a standalone checker. It reads and flattens a `.ext` tree using capabilities `ext-flattening-read` and `ext-flattening-hierarchy`, reports global nets that are not fully connected, and prints counts of devices, nodes, couplings and resistors.

## Requirements

### Requirement: Invocation
`extcheck` SHALL accept the common arguments of capability `ext-flattening-hierarchy` (`-c -C -r -R -p -s -S -t -T -v`) and a root cell name. A trailing `.ext` on the name is removed. Missing arguments or a bad flag SHALL print the usage text and exit with status 1. The tree SHALL be read without `.res.ext` overlays and without subcircuit marking. It SHALL be flattened with node, coupling and resistor flattening, including global name merging. That merging prints the `*** Global name … not fully connected` reports. The default thresholds SHALL be 2 fF and 10 Ω; the manual page states 10 fF (Q-NETLIST-36).
Compatibility: behavioral
(Magic ref: extcheck/extcheck.c:60-100, extflat/EFargs.c:46-48, doc/man/extcheck.1)

#### Scenario: Missing root
- **GIVEN** the invocation `extcheck` with no arguments
- **WHEN** it runs
- **THEN** it prints the usage text and exits with status 1

### Requirement: Counts and report format
`extcheck` SHALL count the following:
- every visited device, which excludes devices on killed nodes and inside subcircuit-flagged cells
- every visited node, and how many of those have a global canonical name
- nodes where `(C + 500)/1000 > cthresh` (C in aF, floating-point)
- nodes where `(R + 500)/1000 > rthresh` (R the lumped resistance in mΩ, integer division)
- coupling pairs and how many have C/1000 > cthresh, only if cthresh is finite
- explicit resistors and how many have R/1000 > rthresh, only if rthresh is finite

It SHALL then print, in this order, and exit with status 0:

```
Memory used: <text>
<D> devices
<N> nodes (<G> global, <N-G> local)
<NC> nodes above capacitance threshold
<NR> nodes above resistance threshold
<K> internodal capacitors (<KT> above threshold)
<RR> explicit resistors (<RT> above threshold)
```

The `Memory used` line is informative.
Compatibility: exact
(Magic ref: extcheck/extcheck.c:80-190)

#### Scenario: Inverter summary
- **GIVEN** an inverter with 2 MOSFETs, nodes `in`, `out`, `vdd!` and `gnd!`, where only `out` has more than 2 fF, no couplings and no resistors
- **WHEN** `extcheck inv` runs
- **THEN** the counting lines are `2 devices`, `4 nodes (2 global, 2 local)`, `1 nodes above capacitance threshold`, `0 nodes above resistance threshold`, `0 internodal capacitors (0 above threshold)`, `0 explicit resistors (0 above threshold)`

#### Scenario: Rounding at threshold
- **GIVEN** a node with 2400 aF and cthresh 2
- **WHEN** it is counted
- **THEN** (2400 + 500)/1000 = 2.9 > 2, so it counts as above the threshold
