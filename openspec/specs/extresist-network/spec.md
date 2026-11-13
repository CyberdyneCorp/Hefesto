# extresist-network

## Purpose

This capability defines how the `extresist` command builds a resistor network for one net from layout geometry, and how it reduces that network. It covers:
- tile fracturing;
- breakpoints and junctions;
- per-tile resistors and contact resistors;
- node merging and construction-time reductions;
- path resistance and capacitance distribution;
- Elmore delay, pruning and simplification.

Other capabilities own the related rules:
- which net is selected: capability `extresist-command`;
- node naming and file output: capability `extresist-output`;
- tile geometry, CIF contact sizes and connectivity: the tech and extract capabilities.

Units: resistances are single-precision floats in mΩ. Node path resistances are 32-bit integers in mΩ. Sheet resistances come from the extraction style (integer mΩ/square), and via resistances are integer mΩ per via. Coordinates are internal units of the copied net geometry.

Notation used below, for a tile T:
- `L`, `R`, `B` and `Tp` are its left, right, bottom and top.
- `TR(T)` is the right neighbour at T's top, `RT(T)` the top neighbour at T's right end, `LB(T)` the bottom neighbour at T's left end, and `BL(T)` the left neighbour at T's bottom (corner-stitch neighbours).
- `a >> 1` is an arithmetic shift (floor division by 2). `a / 2` is C division (truncation toward zero).
- Lists are singly linked. "Prepend" means insertion at the head.
- `INF_T = 2^30 − 4` is the tile-plane infinity.

## Requirements

### Requirement: Net geometry collection
For a selected net, the extractor SHALL build a private copy of the geometry as follows:
1. Collect all paint electrically connected to the net. Start from the net's recorded location and layer, or from the first drive point if the location is undefined. Connectivity is per layer, using the per-type copy masks, which the `skip` option can reduce.
2. Add the full area of each device that has a terminal on the net.
3. Erase the upper layers of stacked contacts so that each contact is processed once. Then record each contact tile as a contact point: center `((L+R)>>1, (B+Tp)>>1)`, width `R−L`, height `Tp−B`, its rectangle, and its type.
4. Dissolve the contacts into their residue layers.
5. Fracture every plane, in ascending plane order over the cell bounding box, as defined in Requirement "Tile fracturing".
6. Attach device records and terminal regions as defined in Requirement "Device annotation". Then attach ports as defined in capability `extresist-command`.
7. Find each contact's tiles. For each contact point, check each plane in ascending order on which the contact type exists, plus the global substrate plane. The tile containing the contact center is a contact tile if either:
   - its type (either side of a split tile) is in the contact's full residue mask, extended with the substrate-plane types that connect to the contact; or
   - it is itself a contact type whose residues intersect that mask.

Compatibility: behavioral

(Magic ref: resis/ResMain.c:1243-1580, resis/ResMain.c:589-700, resis/ResUtils.c:48-100)

#### Scenario: Skipped layer not followed
- **GIVEN** `extresist skip poly` and a net that continues from metal1 through a contact onto poly
- **WHEN** the geometry is collected
- **THEN** the poly portion is not part of the copied net

### Requirement: Tile fracturing
After contacts are dissolved, each plane SHALL be re-tiled by the concave-corner fracture defined below. At each concave corner, the split runs in the direction with less same-type material.

Compatibility: exact

**Enumeration.** The walk below visits the tiles of the live plane over rectangle r (the cell bounding box). Tiles created by splits are visited when the walk reaches them. The tile currently being visited always survives any join.

```
t = tile containing (r.x1, r.y2 - 1)
while Tp(t) > r.y1:
  repeat:
    process(t)
    n = TR(t)
    if L(n) < r.x2:
      while B(n) >= r.y2: n = LB(n)
      if B(n) >= B(t) or B(t) <= r.y1: t = n; continue repeat
    while L(t) > r.x1:
      if B(t) <= r.y1: return
      n = LB(t); t = BL(t)
      if B(n) >= B(t) or B(t) <= r.y1: t = n; continue repeat
    break
  t = LB(t); while R(t) <= r.x1: t = TR(t)

process(S):                       # S non-space, type k
  u = RT(S)
  while R(u) > L(S):
    if type(u) != k: u = BL(u); continue
    concavity(S, u, k)            # may split/join; sets u = none if u was joined away
    if u = none: break
    u = (B(u) != Tp(S)) ? RT(S) : BL(u)

concavity(S, U, k):               # U is a top neighbour of S of the same type; y0 = B(U)
  1. if R(U) > R(S) and type(TR(S)) != k:            corner x0 = R(S)
       xl = x0 - walkLeft(U, x0, y0); yl = walkUp(U, x0, y0) - y0
       if xl > yl: walkUp(U, x0, y0, split)
  if u = none: return
  2. if R(U) < R(S) and type(right neighbour of U containing U's bottom) != k:   x0 = R(U)
       xl = x0 - walkLeft(U, x0, y0); yl = y0 - walkDown(S, x0, y0)
       if xl > yl: walkDown(S, x0, y0, split)
  if u = none: return
  3. if L(U) < L(S) and type(left neighbour of S containing S's top) != k:         x0 = L(S)
       xl = walkRight(U, x0, y0) - x0; yl = walkUp(U, x0, y0) - y0
       if xl > yl: walkUp(U, x0, y0, split)
  if u = none: return
  4. if L(U) > L(S) and type(BL(U)) != k:            x0 = L(U)
       xl = walkRight(U, x0, y0) - x0; yl = y0 - walkDown(S, x0, y0)
       if xl > yl: walkDown(S, x0, y0, split)
```

**Walks.** Each walk starts at the given tile t. `at(x, y)` is the tile containing the point; a point on a left or bottom edge belongs to the tile to the right or above.

```
walkUp(t, x0, y0, split?):    while type(t) = k:
    if x0 = L(t): for left neighbours p of t from the one with Tp(p) > y0 upward while B(p) < Tp(t):
                    if type(p) != k: return B(p)
    elif split?: t = splitX(t, x0)
    t = at(x0, Tp(t))
  return B(t)
walkDown(t, x0, y0, split?):  while type(t) = k:
    if x0 = L(t): e = the LAST left neighbour p (bottom to top) with type(p) != k and B(p) < y0
                  if e: return Tp(e)
    elif split?: t = splitX(t, x0)
    t = at(x0, B(t) - 1)
  return Tp(t)
walkRight(t, x0, y0): while type(t) = k:
    if y0 = B(t): for bottom neighbours p of t, starting at the first with L(p) >= x0, while L(p) < R(t):
                    if type(p) != k: return L(p)
    t = at(R(t), y0)
  return L(t)
walkLeft(t, x0, y0):  while type(t) = k:
    if y0 = B(t): e = the LAST bottom neighbour p (left to right) with type(p) != k and L(p) < x0
                  if e: return R(e)
    t = at(L(t) - 1, y0)
  return R(t)
splitX(t, x): split t at x into t=[L,x] and n=[x,R] (same type);
  join t with the tile above, then with the tile below, when that tile has the same type and the same L and R;
  join n with the tile above, then below, under the same condition; return the surviving left tile.
  If a join removes the tile u of process(), u becomes none.
```

The resulting tiling is generally not the maximal-horizontal-strip form. Resistor extraction uses it as is.

(Magic ref: resis/ResFract.c:43-160, resis/ResFract.c:164-269, resis/ResFract.c:271-421, resis/ResFract.c:423-493, resis/ResMain.c:1531-1537)

#### Scenario: Wide vertical branch is cut vertically
- **GIVEN** a metal1 net `[0,100]×[0,10] ∪ [0,30]×[10,100]` (strips `[0,100]×[0,10]` and `[0,30]×[10,100]`)
- **WHEN** the plane is fractured
- **THEN** at the corner (30,10) (case 2) xl = 30 and yl = 10, so the bottom strip is split at x = 30 and its left part joins the strip above
- **AND** the tiles are `[0,30]×[0,100]` and `[30,100]×[0,10]`

#### Scenario: Equal runs keep the horizontal strips
- **GIVEN** a metal1 net `[0,100]×[0,10] ∪ [0,10]×[10,100]`
- **WHEN** the plane is fractured
- **THEN** at the corner (10,10), xl = 10 and yl = 10
- **AND** since xl is not greater than yl, nothing is split

### Requirement: Device annotation
Each device of the net SHALL be attached to the fractured geometry as follows:
1. The device record is created at the tile that contains the device's lower-left reference point.
2. All tiles of the device region (4-connected through the device's connect mask) are flagged as device tiles of that record.
3. The first source/drain region found next to the device is the *source* region, and it is flood-marked. Device tiles are visited in depth-first order. For each device tile, neighbours are scanned top, then bottom, then right, then left. The terminal index i is the first S/D class whose type mask contains the neighbour's type.
4. Each device tile records its *source edges*: the sides (top, bottom, right, left) on which it touches tiles of the source region.

A device has `n_SD + 2` terminal slots: gate, substrate, then one slot per S/D terminal class (index 0 = source, 1 = drain). Terminals on other planes are recorded per tile, for use in Requirement "Breakpoints".

Compatibility: behavioral

(Magic ref: resis/ResUtils.c:186-433, resis/ResUtils.c:434-850, resis/ResUtils.c:887-1086)

#### Scenario: Source edge
- **GIVEN** an nfet tile `[10,20]×[0,10]` with space above and below and ndiff tiles on its left and right sides
- **WHEN** devices are annotated
- **THEN** the right ndiff region is the source region, because the right side is scanned before the left side
- **AND** the nfet tile's source edges are {right}

### Requirement: Start tile and origin
The extraction SHALL start from one tile and one point, chosen by the rules below. At that point it SHALL create an origin node with kind origin, path resistance 0 and the ready flag set, plus a breakpoint on the start tile.

Compatibility: exact

- **Contact driver type.** Try each residue type of the contact, in ascending type order and excluding the contact itself, until one gives a start tile.
- **Drive point only.** Use the tile that contains the drive point on the driver type's plane. If its type is not the driver type:
  - if the point lies on the tile's left edge, use the first left neighbour (bottom to top) whose right type is the driver type;
  - if the point lies on the tile's bottom edge, use the first bottom neighbour (left to right) whose top type is the driver type;
  - otherwise report `Couldn't find wire at <x> <y>` and fail.
- **Device driver.** The tile that contains the device location must be a device type: for a split tile, the left side if it is a device type, else the right side. Otherwise report `Couldn't find device at <x> <y>`.
  - For each device record of that type and each S/D terminal class i, scan the neighbours left (bottom to top), right (top to bottom), top (right to left) and bottom (left to right).
  - The first neighbour whose type is non-space and in S/D class i is the start tile.
  - The start point is the midpoint of the shared edge: left edge `(L, (min(Tp,Tp')+max(B,B'))>>1)`, right edge `(R, …)`, top edge `((min(R,R')+max(L,L'))>>1, Tp)`, bottom edge `(…, B)`.
  - If no start tile is found but a neighbour is another tile of the same device, the device tiles are searched depth-first with the same neighbour scan.

(Magic ref: resis/ResMain.c:1698-2110, resis/ResBasic.c:103-120)

#### Scenario: Origin at the driver's terminal edge
- **GIVEN** an nfet tile `[10,20]×[0,10]` with an ndiff tile `[0,10]×[0,10]` on its left whose type is in S/D class 0
- **WHEN** the start tile is found
- **THEN** the start tile is the ndiff tile, and the origin node is at (10, 5) with path resistance 0

### Requirement: Breakpoints
When a tile is processed, it SHALL receive breakpoints in the order below. Each breakpoint is prepended to the tile's breakpoint list, so the list is newest-first.
1. **Device gate node.** This step applies when all of the following hold:
   - the tile's type is a device type (for a split tile: the non-space side, or the left side when neither side is space);
   - the tile is a device tile;
   - that device has no gate node yet.

   A junction-kind node at `((L+R)>>1, (B+Tp)>>1)` then becomes the device's gate node.
2. **Contacts.** For each contact point in the tile's contact list whose nodes do not exist yet, create the contact nodes and resistors defined in Requirement "Contact resistors".
3. **Sides.** Scan the sides left (neighbours bottom to top), right (top to bottom), top (right to left) and bottom (left to right). A split tile skips its two hypotenuse-adjacent sides:
   - if the left side is processed, skip the right side, plus the top side for direction 1 or the bottom side otherwise;
   - if the right side is processed, skip the left side, plus the bottom side for direction 1 or the top side otherwise.

   For each neighbour P of type t2 (the type facing the tile):
   - **Device terminal.** If t2 is a device type and some device record of t2 has an S/D class i that contains the tile's type, add a device-terminal breakpoint. Its position and direction depend on the side; the direction is the side of the device that faces the tile:

     | Side | Position | Direction |
     |---|---|---|
     | left | `(L, (Tp(P)+B(P))>>1)` | right edge |
     | right | `(R, (Tp(P)+B(P))>>1)` | left edge |
     | top | `((L(P)+R(P))>>1, Tp)` | bottom edge |
     | bottom | `((L(P)+R(P))>>1, B)` | top edge |
   - **Junction.** If t2 connects to the tile's type and P is not processed yet, create a junction. The junction node lies at the midpoint of the shared edge segment, for example `(L, (max(B,B(P)) + min(Tp,Tp(P)))>>1)` on the left side. It gets a breakpoint on both tiles.
4. **Other-plane terminals.** For each device terminal recorded for this tile on another plane, the position is the device tile's center `((R+L)/2, (Tp+B)/2)` (C division). Substrate terminals use the device's substrate node. Other terminals use the terminal rule below with direction other-plane.
5. **Ports.** The tile is marked processed. Then each port on the tile creates an origin-kind node with path resistance unset, the ready flag set and name = port name, and a breakpoint at the port location.

Device terminal rule, for device record D found through the neighbour tile, class n and direction d:

```
if D.term[n] unset:                         create node -> D.term[n]
elif (sourceEdges(neighbour tile) ∩ d) ≠ ∅ or D has < 4 slots:   use D.term[n]
elif n = 0 and D.term[1] unset:             create node -> D.term[1]     # source/drain permuted
elif n = 1 and D.term[0] unset:             create node -> D.term[0]
else:                                       no breakpoint
```

A created node is device-kind and sits at the breakpoint location. Every new node is prepended to the pending queue.

Compatibility: exact

(Magic ref: resis/ResBasic.c:143-409, resis/ResBasic.c:47-100, resis/ResJunct.c:51-234, resis/ResRex.c:2244-2334, resis/ResUtils.c:1088-1110)

#### Scenario: Junction location
- **GIVEN** net tiles A = `[0,10]×[0,10]` and B = `[10,30]×[5,20]`, with A processed first
- **WHEN** A's right side is scanned
- **THEN** one junction node is created at (10, 7), with a breakpoint on A and one on B
- **AND** when B is processed later, no second junction is made for A

### Requirement: Tile processing order
Tiles SHALL be processed in the order produced by the queue algorithm below. `current` is the node on whose behalf a tile is processed.

Compatibility: exact

```
pending = [origin]; done = []; current = origin
processTile(startTile)
while pending not empty:
  N = head(pending); merged = false
  for each junction J in N's junction list (list order) not yet handled:
    for tile in (J.tile0, J.tile1): if tile not processed: current = N; merged |= processTile(tile)
    J.handled = true
  for each contact point C in N's contact list (list order) not yet handled:
    ok = true
    for i over C's tiles: if tile i not processed:
      if C.node[i] = N: current = N; merged |= processTile(tile i) else ok = false
    C.handled = ok
  if not merged:
    move N from pending to the head of done; flag N finished and marked
    if N.pathres = 0: originNode = N
    discard N's junction and contact lists
    finishNode(N)                          # Requirement "Construction-time reductions"
```

- `processTile` adds breakpoints (Requirement "Breakpoints") and then turns them into resistors (Requirement "Tile resistors"). It returns true if `current` was merged with another node.
- Merged nodes stay in the queue and are processed again later.
- Each iteration takes the queue head, which may be a newer node.

(Magic ref: resis/ResMain.c:700-842)

#### Scenario: Node finished after its tiles
- **GIVEN** a net of two abutting tiles: A (the start tile) and B
- **WHEN** the network is built
- **THEN** A is processed first and the junction node J is queued
- **AND** handling J processes B, and J is finished before the origin

### Requirement: Tile resistors
For each processed tile with breakpoints, the breakpoints SHALL be turned into resistors as follows.

**Mode selection.** If any breakpoint belongs to a device-kind node, use Requirement "Near-device cutting". Otherwise, compare the breakpoints' extents `MaxX−MinX` and `MaxY−MinY`. If the y-extent is strictly greater, cut north-south. Otherwise cut east-west.

**East-west cut.** Let H = Tp − B.
1. With one breakpoint, add `H × (R − L)` to its node's area and stop.
2. The type is the tile type. For a split tile it is the left type, or the right type when the left is space.
3. Sort the breakpoints by x with a stable sort.
4. Add `H × (x_first − L)` to the first node's area.
5. Walk the consecutive pairs (p1, p2). If `x2 = x1`, merge the two nodes:
   - same node: drop p2;
   - p2's node is `current`: merge p1's node into it;
   - p1's node is `current`: merge p2's node into it, and continue with p1;
   - otherwise: merge p1's node into p2's node.

   In each case, replace the removed node by the survivor in all later breakpoints of the list.
6. If `x2 ≠ x1`, create a resistor p1→p2 with these fields:
   - value `(float)sheet[type] × (float)(x2−x1) / (float)H`, computed in single precision from left to right;
   - `((x2−x1) × H) / 2` (integer division) added to the area of both nodes;
   - centerline `(Tp+B)>>1`, width H, area 0;
   - direction east-west. A split tile instead gets the diagonal flag, plus north-south for direction 1 or east-west otherwise.

   The resistor is prepended to the network's resistor list, then to p1's and to p2's resistor lists.
7. Add `H × (R − x_last)` to the last node's area.

**North-south cut.** As east-west, with x and y exchanged, the width W = R − L in place of H, and centerline `(L+R)>>1`.

Merging follows Requirement "Node merging". A merge involving `current` is reported to the queue algorithm.

Compatibility: exact

(Magic ref: resis/ResMakeRes.c:55-120, resis/ResMakeRes.c:122-545, resis/ResMakeRes.c:1013-1230)

#### Scenario: Straight wire segment
- **GIVEN** a metal1 tile 100 wide and 4 high with sheet resistance 80 mΩ/sq, and breakpoints at x = 0 and x = 100 on its left and right edges
- **WHEN** it is cut
- **THEN** one resistor of 2000 mΩ is made
- **AND** each end node gains an area of 200

#### Scenario: Truncated area halves
- **GIVEN** a tile `[0,7]×[0,3]` with sheet resistance 100, a breakpoint of node A at (0,1) and one of node B at (7,2)
- **WHEN** it is cut
- **THEN** the cut is east-west (x-extent 7 > y-extent 1), and the resistor is the single-precision value of 700/3 (≈ 233.33333 mΩ)
- **AND** A and B each gain area (7×3)/2 = 10, so 1 unit of the tile's area of 21 is not assigned

#### Scenario: North-south cut
- **GIVEN** a tile `[0,4]×[0,100]` with sheet resistance 50, and breakpoints at (2,0) and (2,100)
- **WHEN** it is cut
- **THEN** one north-south resistor of 50 × 100 / 4 = 1250 mΩ is made, with centerline 2 and width 4

### Requirement: Near-device cutting
A tile with a device-terminal breakpoint SHALL be cut by the rule below. A tile with exactly one breakpoint depends on the `magic-compat` setting (Q-NETLIST-43, resolution: compat): with it ON (the default) the breakpoint is discarded and its node gains no area, as in Magic; with it OFF the tile's width × height is first added to the breakpoint's node, as the other cutting routines do, and then the breakpoint is discarded.

Compatibility: exact

```
if exactly one breakpoint:
  if magic-compat OFF: node(b).area += (R-L)·(Tp-B)
  discard it; stop
edges = ∅; count = 0
for each device-kind breakpoint b: count++; edges |= first of {left if b.x=L, right if b.x=R, top if b.y=Tp, bottom if b.y=B}
if count = 1 or edges ⊆ one single edge (including ∅):
  sort by x (stable); dx = gap(x)
  sort by y (stable); dy = gap(y)
  cut north-south if dy > dx, else east-west
else if (top ∈ edges and bottom ∈ edges and left ∉ edges and right ∉ edges)
     or ((top or bottom ∈ edges) and (left or right ∈ edges) and (R-L) > (Tp-B)):
  sort by y; for each device breakpoint p on the top or bottom edge:
     delete every other breakpoint of the same node whose y is not on the top or bottom edge
  cut north-south
else:
  for each device breakpoint p on the left or right edge:
     delete every other breakpoint of the same node whose x is not on the left or right edge
  cut east-west

gap(axis):   # in the current sort order
  p = first device-kind breakpoint
  q = last non-device breakpoint before p whose successor is at a different (x,y) location
  r = first breakpoint after p at a different (x,y) location
  g = INF_T
  if r: g = dist(p, r)
  if q: g = min(g, dist(p, q))
dist(p, b) along axis: if b has a contact rectangle C: (C.lo > p ? C.lo - p : C.hi < p ? p - C.hi : 0) else |p - b|
```

(Magic ref: resis/ResMakeRes.c:547-855)

#### Scenario: Device on the right edge
- **GIVEN** an ndiff tile `[0,20]×[0,10]` with a junction breakpoint at (0,5) and a device-terminal breakpoint at (20,5)
- **WHEN** it is cut
- **THEN** dx = 20 and dy = 0, so the cut is east-west
- **AND** one resistor of `sheet × 20 / 10` joins the two nodes

#### Scenario: Lone device breakpoint loses the tile area
- **GIVEN** an ndiff tile whose only breakpoint is a device terminal
- **WHEN** it is cut
- **THEN** with `magic-compat` ON no resistor is made, and the tile's area is not added to any node, as in Magic

#### Scenario: Lone device breakpoint keeps the tile area with magic-compat OFF
- **GIVEN** an ndiff tile `[0,20]×[0,10]` whose only breakpoint is a device terminal of node D
- **WHEN** it is cut with `magic-compat` OFF
- **THEN** no resistor is made, and 200 is added to D's area

### Requirement: Contact resistors
When the first of a contact point's tiles is processed, the contact point SHALL be turned into nodes and resistors as follows, for contact type t.

**No resistance.** If the via resistance of t is 0 or the CIF via size is 0, create one contact-kind node at the contact center. It joins all of the contact's plane tiles, with one breakpoint per tile that carries the contact rectangle. No resistor is made.

**Via count.** Otherwise, let `c` be the CIF scale factor, `minside` the CIF cut size plus twice the border, `via` the cut size and `spacing` the cut separation.
- If `width·c < minside` or `height·c < minside`, then `nx = ny = 1`. A once-per-session warning `Warning: <type> at <x> <y> smaller than extract section allows` is printed.
- Otherwise `nx = (int)((float)(width·c − minside) / (float)(via + spacing)) + 1`, and ny is computed the same way from the height.

**Resistors.**
- One contact-kind node SHALL be created per contact tile, at the contact center and in tile order. Each node is prepended to the pending queue and gets a breakpoint on its tile.
- Consecutive tiles SHALL be joined by a resistor from node i−1 to node i, of value `(float)viaResist[t] / (float)(nx·ny)`.
- Each such resistor has centerline field = ny, width field = nx, type t, area 0 and no direction flags.

Compatibility: exact

(Magic ref: resis/ResMakeRes.c:856-1011, cif/CIFtech.c:2306-2365)

#### Scenario: Two-via contact
- **GIVEN** a via resistance of 10000 mΩ, CIF scale factor 10, cut size 20, border 10 and separation 20 (so minside = 40), and an 8×4 contact spanning two planes
- **WHEN** it is processed
- **THEN** nx = (int)(40/40) + 1 = 2 and ny = (int)(0/40) + 1 = 1
- **AND** one resistor of 5000 mΩ joins the two plane nodes

### Requirement: Node merging
Merging node D into node K SHALL have these effects, in order:
1. If K = D, nothing happens.
2. If D is origin-kind, K's kind becomes origin. Then, if D is sink-kind, K's kind becomes sink. The kind is assigned, not combined.
3. K's path resistance becomes min(K, D). If it decreased and K is not finished, K moves to the head of the pending queue.
4. K's area increases by D's area. K inherits D's maximum-delay flag.
5. Every device terminal that pointed to D now points to K. D's device links are prepended one by one to K's list.
6. D's junctions move to K. For each tile of each junction that is not processed yet, and for each such contact tile whose node was D, D's breakpoints on that tile are re-pointed to K. If K already has a breakpoint on that tile, D's breakpoints are deleted instead, and K's first breakpoint inherits D's contact rectangle if it has none.
7. If K has no name, it takes D's name. If both have different names, D's name replaces K's only when D's name is a port of the input.
8. Every resistor of D is re-pointed to K, and its link is prepended to K's list.
9. D is removed from its list: the done list if D is finished, otherwise the pending queue. If D was the origin node, K becomes the origin node.

Compatibility: exact

(Magic ref: resis/ResMerge.c:878-1048, resis/ResMerge.c:1222-1267)

#### Scenario: Port name survives a merge
- **GIVEN** an unnamed junction node K and a port node D named `out`
- **WHEN** D is merged into K
- **THEN** K becomes origin-kind and is named `out`

### Requirement: Construction-time reductions
Each time a node N is finished, the network SHALL be reduced by the steps below. The steps also run again for a neighbour that was already finished when a reduction changes it. They use no tolerance, and they are skipped entirely in FastHenry geometry mode.

Compatibility: exact

```
finishNode(N):
  N.ready = true                      # origin and port nodes start with ready = true
  if N has no resistors: return
  for r in N's resistors (list order):
    if r joins N to itself: unlink r from N twice; N.area += r.area; delete r; finishNode(N); return
    if r.value = 0: M = other end; unlink r; merge N into M; M.area += r.area; delete r
                    if M.ready: M.ready = false; finishNode(M)
                    return
  if N has no devices and N is not origin/sink: s = series(N)
  if s untouched and N not origin/sink: s = parallel(N)
  if s untouched and N not origin/sink: triangle(N)
```

**series(N).** Let r1 be the first resistor in N's list.
- **Only r1.** Let M be the other end. Unlink r1, add `r1.area + N.area` to M's area, and delete r1 and N. If M is ready, clear the flag and call finishNode(M).
- **Exactly r1 and r2.** Let a be the far end of r1 and b the far end of r2.
  - If a = b, delete both resistors, add `r1.area + r2.area + N.area` to a's area, and delete N.
  - Otherwise:
    - r1's N-end becomes b;
    - `b.area += r1.value·N.area/(r1.value+r2.value)` and `a.area += r2.value·N.area/(r1.value+r2.value)` (float);
    - `r1.value += r2.value` and `r1.area += r2.area`;
    - b's link to r2 now refers to r1;
    - r2 and N are deleted.
  - Then, if a is ready, clear the flag and call finishNode(a).
- **Three or more resistors.** N is left untouched.

**parallel(N).** Look for two resistors with the same pair of end nodes.
- With fewer than 10 resistors: for each r1 in list order, take the first later r2 with the same ends.
- With 10 or more: for each r2 in list order, take the first earlier r1 whose other end equals r2's other end.
- On a match:
  - `r2.value = (r1.value·r2.value)/(r1.value+r2.value)` (float), or 0 if the sum is 0;
  - `r2.area += r1.area`, and r1 is deleted;
  - let M be the other end. If M is ready, clear the flag. Call finishNode(N), then finishNode(M) if M was ready.

**triangle(N).** Look for neighbours n1 (reached via r1) and n2 (via r2) that are joined by a resistor r3.
- With fewer than 10 resistors: r1 in list order, r2 after r1, r3 in n1's list order.
- With 10 or more: n1 is taken in the hash-table iteration order of N's neighbours (Q-NETLIST-45), and r3 in n1's list order, skipping r1 and loops.
- On a match, with S = R1+R2+R3, replace the triangle by a star around a new node X at N's location. X is flagged finished, ready and marked, and is prepended to the done list.
  - `r1 = R1·R2/S + h` now joins N and X.
  - `r2 = R2·R3/S + h` now joins n2 and X.
  - `r3 = R1·R3/S + h` now joins n1 and X.
  - The offset h depends on the `magic-compat` setting (Q-NETLIST-44, resolution: compat): with it ON (the default) h = 0.5 mΩ, as in Magic; with it OFF h = 0.
  - The products are computed in float as `(Ra·Rb)·(1/S)`. All three values are 0 if S = 0.
  - X's resistor list is r3, r2, r1.
  - Then clear the ready flag of n1 and n2 where it was set. Call finishNode(N), then finishNode(n1) and finishNode(n2) for those that were ready.

A no-merge type mask exists but is never populated, so no pair of resistor types is excluded from these reductions.

(Magic ref: resis/ResMerge.c:45-138, resis/ResMerge.c:139-220, resis/ResMerge.c:221-416, resis/ResMerge.c:418-534, resis/ResMerge.c:536-876, resis/ResMerge.c:630-641, resis/ResMerge.c:765-776, resis/ResMerge.c:1050-1220)

#### Scenario: L-shaped wire reduced to one resistor (verify against Magic)
- **GIVEN** metal1 tiles S = `[0,100]×[0,10]` and T = `[90,100]×[10,100]` with sheet resistance 100, an origin O at (0,5) on S and a port P at (95,100) on T
- **WHEN** the network is built
- **THEN** the junction J at (95,10) gets R1 = 950 (O–J; areas O 475, J 525) and R2 = 900 (J–P; areas J +450, P 450)
- **AND** when J is finished it is removed as a series node: one resistor O–P of 1850 mΩ remains
- **AND** O's area becomes 475 + 900·975/1850 ≈ 949.3243 and P's area becomes 450 + 950·975/1850 ≈ 950.6757

#### Scenario: Parallel pair
- **GIVEN** a node N with two resistors of 1000 and 3000 mΩ to the same node M
- **WHEN** N is finished
- **THEN** one resistor of 750 mΩ remains between N and M

#### Scenario: Delta-to-star
- **GIVEN** three nodes pairwise joined by 300 mΩ resistors, where the node finished last allows no series or parallel reduction
- **WHEN** the triangle is reduced
- **THEN** with `magic-compat` ON each star arm is 300·300/900 + 0.5 = 100.5 mΩ
- **AND** with `magic-compat` OFF each star arm is 300·300/900 = 100 mΩ

### Requirement: Path resistance and capacitance distribution
After the network is built, the path resistance of every node SHALL be computed from the origin by the Dijkstra variant below. The variant depends on the `magic-compat` setting (Q-NETLIST-25 and Q-NETLIST-46, resolution: compat): with it ON (the default) path resistances and heap keys are truncated to int mΩ and the heap below, with its session-wide state, is reproduced exactly, as in Magic; with it OFF path resistances and keys are floats (double), and each net uses a fresh priority queue that breaks ties between equal keys by insertion order. Only the upstream end chosen and the loop resistor marked as ignored for delay can differ, and only with fractional or equal path resistances.

Compatibility: exact

```
for N in done list: if N.pathres = 0: origin = N, N.finished = true else N.pathres = 2^30-1, N.finished = false
if no origin: origin = source terminal of the driver device; its kind becomes origin; its pathres = 0
            (if the device is not found: "Error:  No device type <t> found at location <x> <y>"; stop)
expand(origin)
while heap not empty: r = removeMin(heap); relax(r)
expand(N): N.finished = true
  for r in N's resistors (list order): if r.inHeap: continue
    if other end not finished: heap.insert(key = (int)((float)N.pathres + r.value), r)
relax(r): r.inHeap = true; clear r.marked
  if both ends finished: r.ignoreForDelay = true
  elif end0 finished: end1.pathres = (int)(end0.pathres + r.value); expand(end1)
  else: swap r's ends so end0 is the finished one; then as above
```

The heap is a binary min-heap stored in a 1-based array. It works as follows:
- **Insert into a built heap.** Place the entry at position i = n+1, then repeat:
  1. Let c = i. If i is odd and not 1, and key[i] ≥ key[i−1], let c = i−1.
  2. Move to the parent: i = i/2.
  3. Stop at the root, or if key[c] ≥ key[i].
  4. Otherwise swap c with the parent, and sift down from c.
- **Insert into an unbuilt heap.** Append the entry.
- **removeMin.** If the heap is not built, sift down every position from n down to 1. Then take position 1, move position n to position 1, and sift down.
- **Sift-down.** Pick the left child, or the right child if the left child's key is greater. Stop when that child's key is ≥ the root key.
- **Persistence** (`magic-compat` ON only). The heap is created once per session, and its "built" state persists. The first net of a session therefore inserts the origin's resistors unbuilt (Q-NETLIST-46). Golden NL-RESNET captures therefore run each `extresist` in a fresh session.

`maxres` SHALL be the largest path resistance, as a float.

The net capacitance C SHALL then be distributed: each node's area is multiplied by `C / Σarea` (float).
- If Σarea = 0, the message `Error: Node with no area.` SHALL be printed, and the capacitance is left undistributed.
- A resistor with non-zero area prints `Nonnull resistor area`.

(Magic ref: resis/ResSimple.c:942-960, resis/ResSimple.c:1083-1210, resis/ResSimple.c:716-757, utils/heap.c:114-260, utils/heap.c:314-460)

#### Scenario: Two-segment path
- **GIVEN** an origin connected to A by 2000 mΩ, and A connected to B by 3000 mΩ
- **WHEN** path resistance is computed
- **THEN** A has 2000, B has 5000, and maxres = 5000

#### Scenario: Loop resistor ignored for delay
- **GIVEN** an origin O, nodes A and B, and resistors O–A 100, O–B 200 and A–B 50
- **WHEN** path resistance is computed
- **THEN** A = 100 and B = 150 (through A)
- **AND** the resistor O–B is marked as ignored for delay

### Requirement: Elmore delay and pruning
The delay computation below SHALL run when simplification or lumped output is enabled. With neither enabled, the network is left as it is after capacitance distribution. If no origin node exists, `Error:  Network simplification:  Failed to to get origin node.` SHALL be printed and Tdi = 0.

**Downstream capacitance.** When mindelay > 0 and the net capacitance is known, the downstream capacitance SHALL be computed recursively from the origin:
- `Cdown(N) = N.cap + Σ_{devices whose gate is N} (L·W·gateCap + 2·W·sdCap) + Σ Cdown(child)`, in both `magic-compat` settings (Q-NETLIST-24, resolution: replicate). sdCap is the per-length gate–diffusion capacitance (gscap), and a gate has two diffusion edges. Children are reached through non-ignored resistors whose upstream end is N.
- The device type of a split device tile is its left type if that is a device type, otherwise its right type.
- Reaching a node a second time is a loop and makes the delay undefined (Tdi = −1).

**Delay.** If the delay is defined, the net capacitance becomes Cdown(origin), and:
- `Tdi(origin) = 0`;
- `Tdi(child) = Cdown(child) × R + Tdi(parent)` (float);
- the net Tdi is the maximum over all nodes;
- the first node in done-list order that attains the maximum (the list head when it ties) is flagged as the maximum-delay node.

**Pruning.** When simplification is on, every node with path resistance 0 is selected again as the origin (the last one in list order wins), and all nodes are flagged finished. If Tdi ≠ −1 and mindelay > 0, the tree SHALL be pruned in post-order from the origin:
- For each resistor in N's list that is not ignored and whose upstream end is N, prune its downstream end first. The next link is read before recursing.
- Then, if all of the following hold, delete that resistor and merge N into its upstream node:
  - N has exactly one resistor, and N is its downstream end;
  - N is not origin or sink;
  - `Tdi(N) < mindelay`.

Compatibility: exact

(Magic ref: resis/ResSimple.c:759-940, resis/ResSimple.c:798-802, resis/ResSimple.c:960-1081)

#### Scenario: Single RC delay
- **GIVEN** origin → A through 1000 mΩ, A with 2000 aF, and no devices
- **WHEN** Tdi is computed
- **THEN** Tdi(A) = 2e6 zs

#### Scenario: Chain pruned bottom-up
- **GIVEN** origin → A (1000 mΩ) → B (1000 mΩ), A and B each with 1 aF, neither a port, and mindelay 5000 zs
- **WHEN** pruning runs
- **THEN** Cdown(B) = 1, Cdown(A) = 2, Tdi(A) = 2000 and Tdi(B) = 1000 + 2000 = 3000
- **AND** B is merged into A, and then A, now a leaf with Tdi 2000 < 5000, is merged into the origin

### Requirement: Network simplification
When simplification is on, the origin SHALL be moved from the done list to an empty pending queue. The step below SHALL then be repeated, with tolerance `tol = minres` (float), until the queue is empty. Origin-kind and sink-kind nodes (the driver, ports and drive points) are *protected*.

Compatibility: exact

```
step:
  N = pop head of pending; N.marked = N.finished = true; prepend N to done
  r1 = r2 = none; drive = recv = 0
  for r in N's resistors (list order):
    if r.marked and r.end1 = N: drive++                               # oriented into N
    else: if not r.marked: (if r.end1 = N: swap ends so r.end0 = N); r.marked = true
          recv++
    if r1 = none: r1 = r else r2 = r
  if recv = 0 and drive = 1 and N not protected:                      # dead end
    r1.deadend = true
    if r1.value < tol:
      M = other end; marked = unmarked = drivers = pendingRecv = 0; big = r1
      for r in M's resistors:
        if r.end0 = M:
          if not r.end1.marked: pendingRecv++
          if r.deadend or r.value > tol: marked++; if r.value >= big.value: big = r
          else unmarked++
        else drivers++
      if marked+unmarked+drivers = 2 or (unmarked = 0 and marked > 1 and big = r1 and pendingRecv = 0):
        if M.marked: M.marked = false; move M from done to head of pending
        return
      if unmarked = 0 and ((marked = 1 and drivers > 1) or big ≠ r1):
        M.area += r1.area; M.doneOnce = false; delete r1; merge N into M
        if M.marked: M.marked = false; move M to head of pending
        finishNode(M)
  elif drive+recv = 2 and r1.value < tol and r2.value < tol and N not protected:    # series
    orient r1 and r2 away from N if unmarked; mark both
    n1 = far end of r1; n2 = far end of r2
    K = ((r1.deadend and r1.value < tol/2) or (not r2.deadend and r1.value < r2.value)) ? n1 : n2
    move N's devices to K (gate, else substrate, else source if ≥3 slots, else drain if ≥4 slots;
        otherwise print "Missing Device connection in squish routines at <x>, <y>"); links prepended
    K.pathres = min(N.pathres, K.pathres)
    n2.area += r1.value·N.area/(r1.value+r2.value); n1.area += r2.value·N.area/(r1.value+r2.value)
    r1.value += r2.value; r1.area += r2.area
    re-insert r1 into the resistor list in ascending order (before the first r with r.value >= r1.value)
    unlink r1, r2 from N and r2 from n2; r1's N-end := n2; prepend r1 to n2's list; delete r2
    K inherits N's maximum-delay flag; delete N
    for n in (n1, n2): n.doneOnce = false; if n.marked: n.marked = false; move n to head of pending
    finishNode(n1)
  elif recv > 0 and not N.doneOnce:                                   # propagate
    N.doneOnce = true
    for r in N's resistors with r.end0 = N and not r.doneOnce:
      if r.end1.marked:
        if r.value > tol: r.doneOnce = true
        r.end1.marked = false; move r.end1 to head of pending
```

(Magic ref: resis/ResSimple.c:66-376, resis/ResSimple.c:378-416, resis/ResSimple.c:660-714, resis/ResSimple.c:1040-1072)

#### Scenario: Series reduction
- **GIVEN** minres 1000 and a chain origin → X → port with 400 mΩ and 300 mΩ, where X has a gate load (so it survived construction) and is not protected
- **WHEN** the network is simplified
- **THEN** X is removed, and one 700 mΩ resistor joins the origin to the port
- **AND** X's device moves to the end selected by the dead-end and smaller-resistance rule

#### Scenario: Large resistors kept
- **GIVEN** minres 1000 and a chain origin → X → port with 4000 mΩ and 300 mΩ, where X has a gate load
- **WHEN** the network is simplified
- **THEN** X is not removed in the series step, because 4000 ≥ tol

### Requirement: Final scrunch pass
After the simplification queue empties, the remaining under-tolerance resistors SHALL be eliminated as follows.

Compatibility: exact

**Sorting.** The resistor list is first sorted by ascending value.
- With 10 or more resistors, an unstable array sort is used (Q-NETLIST-26).
- Otherwise, the list is rebuilt by inserting each resistor, from head to tail, before the first resistor whose value is ≥ its own. Equal values therefore end up in the reverse of their previous order (Q-NETLIST-47).

**Elimination loop.**

```
cur = head
while cur and cur.value < tol:
  if head.next = none: break                            # one resistor left
  if both ends of cur are protected: cur = cur.next; continue
  work = none; c1 = c2 = 0
  for r in cur.end0's resistors, r ≠ cur: c1++; if work = none or work.value > r.value: work = r; n1 = cur.end0
  for r in cur.end1's resistors, r ≠ cur: c2++; if work = none or work.value > r.value: work = r; n1 = cur.end1
  if c1 > 0 and c2 > 0: work.value += cur.value; work.area += cur.area
  else: n1.area += cur.area
  n2 = the other end of cur
  delete cur; unlink work and re-insert it in ascending order
  if n2 protected: merge n1 into n2; n1 = n2 else merge n2 into n1
  move n1 from done to head of pending; clear n1.doneOnce and n1.finished
  finishNode(n1)
  while pending not empty: simplification step
  cur = head
```

(Magic ref: resis/ResSimple.c:419-446, resis/ResSimple.c:448-658, resis/ResSimple.c:660-714)

#### Scenario: Port-to-port resistor kept
- **GIVEN** a 10 mΩ resistor whose ends are two port nodes
- **WHEN** the final pass runs
- **THEN** the resistor remains in the network

#### Scenario: Short-list ordering of equal values
- **GIVEN** a resistor list (head first) a = 5, b = 5, c = 3
- **WHEN** it is sorted for the final pass
- **THEN** the order is c, b, a

#### Scenario: Smallest resistor folded into its smallest neighbour
- **GIVEN** tol 1000 and resistors A–B 100, B–C 500 and A–D 2000, where A, C and D are protected and B is not
- **WHEN** the final pass runs on A–B
- **THEN** both ends have other resistors, so B–C becomes 600 and A–B is deleted
- **AND** B (unprotected) is merged into A, so C is now joined to A by 600 mΩ
