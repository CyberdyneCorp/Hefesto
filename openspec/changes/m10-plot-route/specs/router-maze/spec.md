## MODIFIED Requirements

### Requirement: mzrouter section grammar
The `mzrouter` technology section SHALL be parsed with this grammar (keywords exact, case-sensitive; one statement per line):

```ebnf
mzrouter_section = "mzrouter" , NL , { stmt , NL } , "end" ;
stmt        = style | layer | contact | notactive | spacing | search | width ;
style       = "style" , name ;
layer       = "layer" , type , hCost , vCost , [ jogCost , [ hintCost , [ overCost ] ] ] ;
contact     = "contact" , type , layerType1 , layerType2 , cost ;
notactive   = "notactive" , type , { type } ;
spacing     = "spacing" , routeType , ( type | "SUBCELL" ) , ( int | "NIL" ) , { ( type | "SUBCELL" ) , ( int | "NIL" ) } ;
search      = "search" , rate , width , penalty ;
width       = "width" , routeType , int , [ int ] ;
hCost, vCost, jogCost, hintCost, overCost, cost = positive_int ;
rate, width_int = positive_int ;  penalty = nonnegative_decimal ;
```

Rules:
- Every statement other than `style` requires a preceding `style` line; otherwise `Missing style line.` is reported. Each `style` line starts a new style with default parameters (next requirement); the previous style is closed. A `style` line without a name reports `"style" lines need a style name` and the statements up to the next `style` line are skipped (Magic dereferences a missing argument; see Q-LEGACY-39). Unknown keywords report `Unrecognized keyword: "%s"`.
- Every statement SHALL be validated in full before any of it takes effect: a statement that reports an error defines nothing, in both `magic-compat` settings (Magic leaves a half-registered route layer or contact; see Q-LEGACY-31, Q-LEGACY-39).
- `layer`: 4 to 7 fields; the type must be valid and on a plane not already used by another route layer of the style (`Attempt to define two route layers on same plane:  %s and %s`). Optional costs default to 1. Any non-numeric or non-positive cost aborts the line (the layer is not defined).
- `contact`: exactly 5 fields; both layer types must already be route layers of the style (`route layer must be declared before used in contact.`); cost must be a positive integer.
- `notactive`: marks listed route layers/contacts inactive by default (unknown ones report `Unrecognized route type: "%.20s"`).
- `spacing`: even field count >= 4; the second word of each pair may be any type, or a case-insensitive prefix of `subcell`; values are non-negative integers or the literal `NIL` (= no restriction, stored as -1). Explicit spacings are applied after the DRC-derived values (see derivation).
- `search`: exactly 4 fields; sets window rate and window width (positive integers) and the penalty factor (stored as the integer `floor(p * 2)` interpreted as `/2`, i.e. 0.5 resolution; see Q-LEGACY-32).
- `width`: sets route-type width and optional minimum length (default = width). With `magic-compat` ON it is overridden later by DRC derivation, as in Magic; with it OFF it overrides the derived values (requirement "Maze router behaviour with magic-compat off"; see Q-LEGACY-33).
- Route layers, contacts and types are kept in the order they appear in the file.

Example (scmos):

```
mzrouter
  style irouter
    layer       m2      32       64     256     1
    layer       m1      64       32     256     1
    layer       poly    128     128     512     1
    contact     m2contact       metal1  metal2  1024
    contact     pcontact        metal1  poly    2056
    notactive poly pcontact
  style garouter
    layer       m2      32       64     256     1
    layer       m1      64       32     256     1
    contact     m2contact       metal1  metal2  1024
end
```

Compatibility: exact
(Magic ref: mzrouter/mzTech.c:180-260, mzrouter/mzTech.c:300-800, mzrouter/mzTech.c:1136-1215, scmos/scmos.tech.in mzrouter section) (See Q-LEGACY-31, Q-LEGACY-33, Q-LEGACY-39.)

#### Scenario: Layer defaults
- **GIVEN** `layer m2 32 64` inside a style
- **WHEN** the line is parsed
- **THEN** metal2 is a route layer with hCost 32, vCost 64, jogCost 1, hintCost 1, overCost 1, active

#### Scenario: Statement before style
- **GIVEN** an mzrouter section whose first line is `layer m1 64 32`
- **WHEN** the section is read
- **THEN** `Missing style line.` is reported and nothing is defined

#### Scenario: Contact with an undeclared layer defines nothing
- **GIVEN** a style with route layer metal1 only and the line `contact m2contact metal1 metal2 1024`
- **WHEN** the section is read and `iroute contacts` is run
- **THEN** `route layer must be declared before used in contact.` is reported, and m2contact is neither a route contact nor attached to metal1

## ADDED Requirements

### Requirement: Maze router behaviour with magic-compat off
With `magic-compat` OFF, the maze router SHALL differ from Magic in exactly these points; with it ON, each is as the other requirements of this capability state. Each point has one branch point in the engine.
- **Explicit width (Q-LEGACY-33).** A `width` statement's width and length replace the DRC-derived width and length of that route type (as explicit `spacing` statements replace derived spacings).
- **Initial left-right contact (Q-LEGACY-85).** An initial path through a contact at the start point in the left-right direction is offered when `tile.xtop − p.x > length(C) − width(C)`, the test the up-down direction and the in-search contact extension use.
- **Upward extension (Q-LEGACY-87).** In the upward extension, a free next blockage tile does not override a closer stop point; the end point is the nearest of the pruned candidates, as in the other three directions.
- **Contact same-node exemption (Q-LEGACY-89).** When blocked material is painted into a contact route type over same-node area, the exemption tests the layer-plane tile, so it applies only where the layer plane itself is same-node.

Routes found with `magic-compat` OFF are still deterministic and identical on every platform.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2) (See Q-LEGACY-33, Q-LEGACY-85, Q-LEGACY-87, Q-LEGACY-89.)

#### Scenario: Width statement honoured
- **GIVEN** `magic-compat` OFF, a DRC width of 3 for metal2 and the statement `width m2 4` in style `irouter`
- **WHEN** the technology is loaded and `iroute layers metal2 width` is run
- **THEN** the width printed is 4; with `magic-compat` ON it is 3

### Requirement: Maze router engine interface
The engine SHALL be callable without global state, so that several editor sessions (including an agent's scratch sessions) can route at the same time. A route request SHALL carry the style parameters (a value copy), the start terms, the destination areas, the expansion mask and reference area of the layout it sees, and these run controls: a cancellation token (set by an interrupt or by `-timeout`), an optional budget in blooms, and the search memory limit. The result SHALL carry:
- the result code: success, best before interrupt, failure, unroutable, interrupted or already routed;
- the path as a list of points with their route types, and the paint it converts to (requirement "Path to paint conversion");
- the path's cost, the initial cost estimate, and the counts of blooms and points processed;
- the reason the search ended (complete, bloom limit, budget, cancellation, memory limit).

The engine SHALL NOT modify the layout: painting the result into the edit cell, the undo record, the selection and the redisplay belong to the caller. A budget ends the search exactly as an interrupt does at the same bloom, so a route with a budget gives the same result on every machine. For the same request the engine SHALL return the same result whatever other sessions are routing.

Compatibility: native

(Hefesto decisions: docs/decisions/product.md#P-13, docs/decisions/cross-cutting.md#X-1) (See Q-LEGACY-41, Q-AGENT-1.)

#### Scenario: Two sessions route at once
- **GIVEN** two editor sessions with the same layout and the same route request, one of them already routing
- **WHEN** the second session routes
- **THEN** both return the same result code, path and cost as a route run alone

#### Scenario: Budget ends the search deterministically
- **GIVEN** a route request whose search needs more than 50 blooms, with a budget of 50 blooms
- **WHEN** it is run on Linux x86_64 and on macOS arm64
- **THEN** both runs end at bloom 50 with the same result code and, if a complete path existed, the same path

### Requirement: Maze search safety and limits
In both `magic-compat` settings the maze router SHALL NOT crash, hang or leave the editor in a changed state after a route:
- **Undo (Q-LEGACY-38).** Undo recording is restored on every path out of start registration and of the route, including the start-points-on-both-sides-of-a-fence case, errors and cancellation.
- **Search memory (Q-LEGACY-48).** When the memory of path records, hash entries and blockage planes of one route exceeds the setting `router.max-search-memory` (default 1 GiB on desktop, 256 MiB on iPadOS), the search ends as an interrupt does ("Search Interrupted!" and "Using best path found prior to interrupt." with a complete path, otherwise "Route Interrupted!") and the limit is reported.
- **Arithmetic (Q-LEGACY-90).** Cost, estimate and adjusted-cost arithmetic saturates instead of overflowing.
- **Penalty (Q-LEGACY-47).** A penalty whose stored mantissa floor(p × 2) does not fit in 32 bits is rejected with `Penalty value out of range: %s` and the penalty is unchanged.

Compatibility: native

(Hefesto decisions: docs/decisions/cross-cutting.md#X-2, docs/decisions/cross-cutting.md#X-4) (See Q-LEGACY-38, Q-LEGACY-47, Q-LEGACY-48, Q-LEGACY-90.)

#### Scenario: Undo after a fence route
- **GIVEN** start points on both sides of fence paint
- **WHEN** the route runs, printing `Start points on both sides of fence. Arbitrarily choosing those inside fence.`, and the user then paints a box and runs `undo`
- **THEN** the painted box is removed

#### Scenario: Memory limit reached
- **GIVEN** `router.max-search-memory` set to 1 MiB and a route over a large congested area
- **WHEN** the route runs
- **THEN** the search ends with "Search Interrupted!" or "Route Interrupted!" and a message naming the limit, and the editor stays responsive
