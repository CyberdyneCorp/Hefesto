## MODIFIED Requirements

### Requirement: Netlist maze-routing script
`mazeroute.tcl` is loaded at startup when present. It defines `mazeroute <netfile>`. If `<netfile>` cannot be opened, the command SHALL try `<netfile>.net`. If that also fails, it SHALL print `Can't read netlist file <netfile>` and return `1`. Otherwise it SHALL:
1. Discard the first line of the file.
2. Treat blank lines as net separators.
3. Within each net, route consecutive terminal pairs as a chain: for each terminal after the first, run `iroute route -dlabel <previous> -slabel <current> -timeout 3`.

The script also defines an interactive "maze helper" window, with lists of unrouted and routed nets, route/rip-up/verify actions, and saving and loading of failed nets. It is reachable from the wrapper's Options menu as `Maze Router`. Hefesto provides the helper as a desktop panel (not on iPadOS) driving the same commands, with two differences from Magic's script, in both `magic-compat` settings:
- **Nets with fewer than two pins (Q-LEGACY-49).** Routing such a net leaves it in the unrouted list with a console note and goes on with the next selected net (Magic's helper stops with an unset-variable Tcl error, or reuses the previous net's result).
- **No parameter window (Q-LEGACY-54).** The helper has no Params button; Magic's `genmazeparams` window fails before it opens. The route parameters are set with `iroute layers`, `iroute contacts`, `iroute spacings` and `iroute wizard`.

The `.net` fallback and the chain order are reproduced in both `magic-compat` settings (Q-SCRIPT-30, resolution: replicate). The script drives `iroute` (capability `router-maze-iroute`), so it is provided and tested together with that capability.
Compatibility: behavioral

(Magic ref: tcltk/mazeroute.tcl:1-675) (See Q-SCRIPT-30, Q-LEGACY-49, Q-LEGACY-54.)

#### Scenario: Chain routing order
- **GIVEN** a `.net` file whose first line is a header, followed by the lines `a`, `b`, `c`, an empty line, and `d`, `e`
- **WHEN** `mazeroute f.net` runs
- **THEN** the routes SHALL be attempted in this order: a←b, b←c, d←e (destination←source)

#### Scenario: Helper net with one pin
- **GIVEN** the maze helper with the nets `n1` (pins `a`, `b`) and `n2` (pin `c` only) selected in the unrouted list
- **WHEN** the user runs Route
- **THEN** `n2` stays in the unrouted list with a console note, and `n1` is routed as a chain
