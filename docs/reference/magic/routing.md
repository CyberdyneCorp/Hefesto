# Routing

## Purpose
Magic includes several routers:
- **Netlist channel router (`route`):** decomposes the routing area into channels, does global routing (grouter), routes the channels (gcr), and paints the result back (garouter).
- **Gate-array router (`garoute`):** routes through user-defined channels.
- **Maze router (mzrouter):** used interactively through `iroute` and for stems.

All of them are built unless `--disable-route` is given. Tech sections `router` and `mzrouter` configure them.

## Requirements

### Requirement: Channel routing from a netlist
`route` SHALL route the area under the box, as follows:
1. Use the selected netlist. If none is selected, use the netlist named after the edit cell and report "No netlist selected yet;  using \"%s\".".
2. Build channels from free space.
3. Run global and channel routing.
4. Paint the wires.

At the end it SHALL report "No routing errors." or the number of errors as feedback. (src: router/rtrMain.c:85-262, garouter/gaMain.c:331-410)

#### Scenario: Empty netlist
- **WHEN** the netlist has no nets
- **THEN** the system SHALL report "No nets to route."

#### Scenario: Tiny area
- **WHEN** the box is too small
- **THEN** the system SHALL report "Routing area (box) is too small to be of any use."

### Requirement: Router parameters
`route` SHALL accept these subcommands: `end`, `jog`, `metal`, `netlist <file>`, `obstacle`, `origin`, `stats`, `settings`, `steady`, `tech`, `vias`, `viamin` and `mazestems`. Wizard options SHALL start with `*`.

Defaults SHALL be:

| Parameter | Default |
|---|---|
| end constant | 1.0 |
| via limit | 2 |
| min jog | 1 |
| steady net | 1 |
| obstacle distance | 0.7 |
| origin | (0,0) |
| maze stems | off |

(src: router/rtrCmd.c:438-761)

#### Scenario: Invalid option
- **WHEN** `route bogus` is run
- **THEN** the system SHALL report "\"bogus\" isn't a valid router option."

### Requirement: Gate-array routing
`garoute` SHALL support:
- `channel [xl yl xh yh] [h|v]`, which defines normal or river channels
- `generate h|v [file]`
- `route [netlist]`
- `reset`
- `warn` and `nowarn`

Overlapping channels SHALL be rejected. All channel definitions SHALL be cleared after routing. The command SHALL require an mzrouter style named `garouter`. (src: router/rtrCmd.c:222-418, garouter/gaMain.c, garouter/gaChannel.c:191)

#### Scenario: Route without channels
- **WHEN** `garoute route` runs with no channels defined
- **THEN** the system SHALL report "Must define channels before routing."

### Requirement: Interactive maze routing
`iroute` SHALL require an mzrouter style named `irouter`. With no arguments it SHALL route from the cursor to the box.

`iroute route` SHALL accept these options (`-timeout` is Tcl only):
- `-slabel`, `-spoint`, `-scursor`, `-slayers`
- `-dbox`, `-dlabel`, `-drect`, `-dselection`, `-dlayers`
- `-timeout`

In Tcl the result SHALL be one of: `Route success`, `Route best before interrupt`, `Route failure`, `Route unroutable`, `Route interrupted`, `Route already routed`.

Parameters SHALL be inspectable and settable with `contacts`, `layers`, `search`, `spacings`, `verbosity` and `wizard`. `saveParameters <file>` SHALL write them as a sourceable script. (src: irouter/irCommand.c, irouter/irMain.c:148)

#### Scenario: Missing style
- **WHEN** the tech file has no `irouter` style
- **THEN** `iroute` SHALL report "Need irouter style in mzrouter section of technology file to use irouter."

### Requirement: Router tech sections
The `router` section SHALL accept `layer1`, `layer2`, `contacts` and `gridspacing` lines.

The `mzrouter` section SHALL accept `style`, `layer`, `contact`, `notactive`, `spacing`, `search` and `width` lines. The optional costs on a `layer` line default to 1. Width and spacing SHALL be refilled from the DRC rules after the technology loads. (src: router/rtrTech.c, mzrouter/mzTech.c:235-1010)

#### Scenario: Missing style line
- **WHEN** an mzrouter section has rules before any `style`
- **THEN** the loader SHALL report "Missing style line."

### Requirement: net2ir conversion
The standalone `net2ir feedfile netfile` program SHALL convert feedback output and two-terminal nets into `iroute route` commands. Nets with more than 2 terminals SHALL be ignored with a warning. (src: net2ir/net2ir.c)

#### Scenario: Multi-terminal net
- **WHEN** a net has 3 terminals
- **THEN** net2ir SHALL print "Net with >2 terms ignored"
