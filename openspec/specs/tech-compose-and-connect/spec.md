# Technology Compose and Connect

## Purpose

Defines the `compose` and `connect` sections of a technology file and the tables they produce:
- the per-plane **paint** and **erase** result tables, which define what any type becomes when another type is painted over it or erased from it, on each plane;
- the derived tables of planes affected by each type;
- the electrical **connectivity** tables.

This spec gives the complete algorithm for building the tables: the defaults, the explicit rules, the contact-derived rules and stacked contacts. The geometric paint and erase operations that consult these tables, including how multi-plane contact images are resolved, belong to the editing and database capabilities. Contacts, residues and stacked types are defined in capability `tech-contacts-and-stacking`. Type names and type lists are defined in capability `tech-layers-and-planes`.

Notation used throughout:
- N is the total number of types. U is the number of user layers, so types U..N−1 are the stacked types.
- K is the number of planes.
- For a plane p, an existing type h and an applied type t:
  - `P[p](h,t)` is the paint result.
  - `E[p](h,t)` is the erase result.
  - `W(h,t)` is the write result.
- `udP(h,t)` and `udE(h,t)` are "user-defined" flags. Default-filling steps must not overwrite an entry whose flag is set.
- `setP(h,t,p,r)` sets `P[p](h,t) := r`, but only if `udP(h,t)` is false and h ∈ OnPlane(p). `setE` is the same for erase, using `udE`.
- Unconditional assignments are written `P[p](h,t) := r`.
- Planes(t), OnPlane(p), Res(t) (residue set) and Residue(t,p) are as defined in capabilities `tech-layers-and-planes` and `tech-contacts-and-stacking`.
- "Tech planes" are planes 6..K−1.

## Requirements

### Requirement: Paint, erase and write table model

The technology SHALL define three result functions over all 256 type codes and all 64 plane slots:
- `P[p](h,t)`: the type that results on plane p when type t is painted over a tile of type h.
- `E[p](h,t)`: the type that results on plane p when type t is erased from a tile of type h.
- `W(h,t)`: the type that results when type t is written over h, ignoring composition rules. This is always t.

Compatibility: exact

All three are total functions, and each entry is a type number from 0 to 255. Their values are fixed entirely by the algorithm in this spec, which is applied in this order:
1. Initialization at the `compose` header.
2. The rules in the `compose` section, in file order.
3. Finalization at the `compose` `end`.

The functions are an `exact` compatibility item. The automated test for this capability is a dump of P and E for every (p, h, t) with p in tech planes and h, t below N.

(Magic ref: database/DBtpaint.c:37-44, database/databaseInt.h:225-250, database/database.h.in:1265-1280)

#### Scenario: Write table ignores composition
- **GIVEN** any loaded technology
- **WHEN** W(polysilicon, ndiffusion) is evaluated
- **THEN** the result is ndiffusion

### Requirement: Default paint and erase rules

When the `compose` header is read, the loader SHALL initialize the tables as follows:

Compatibility: exact

1. **Identity:** for every p, h and t: `P[p](h,t) := h` and `E[p](h,t) := h`. Also `W(h,t) := t` for all h and t.
2. **Clear flags:** clear `udP(h,·)` and `udE(h,·)` for every h below N.
3. **Home-plane rules.** For every type s below N whose home plane ps is greater than 0 (this skips space and includes the built-in types 1..8):
   - For every type t below N whose home plane is greater than 0:
     - `E[ps](s,t) := s`.
     - `P[ps](s,t) := t` if t's home plane is ps, and `P[ps](s,t) := s` otherwise.
   - Then `E[ps](s,s) := space` and `P[ps](space,s) := s`.
4. **DRC error types.** On the designRuleError plane, let B = [space, error_p, error_s, error_ps] be indexed by bits 0..3. For all i and j in 0..3: `P(B[i],B[j]) := B[i|j]` and `E(B[i],B[j]) := B[i & ~j]`.
5. **DRC check types.** On the designRuleCheck plane:
   - `P(space,checkpaint) := checkpaint`
   - `P(space,checksubcell) := checksubcell`
   - `P(checkpaint,checksubcell) := checkpaint`
   - `P(checksubcell,checkpaint) := checkpaint`

In short: painting t over s on s's home plane gives t when they share a home plane, and leaves s unchanged otherwise. Erasing s from itself gives space. Erasing anything else leaves s unchanged. Space painted with s gives s on s's home plane.

A contact's home plane is the only plane step 3 touches for it. Its other planes are filled in at finalization.

(Magic ref: database/DBtpaint.c:89-247, database/DBtpaint.c:282-303)

#### Scenario: Same-plane overwrite
- **GIVEN** the example technology of capability `tech-file-format`, with no compose rule for (polysilicon, ndiffusion) yet applied
- **WHEN** the default table is evaluated
- **THEN** P[active](polysilicon, ndiffusion) = ndiffusion after step 3

#### Scenario: Different-plane paint has no effect
- **GIVEN** the example technology
- **WHEN** P[metal1](metal1, polysilicon) is evaluated
- **THEN** the result is metal1

#### Scenario: DRC error combination
- **GIVEN** any technology
- **WHEN** error_s is painted over error_p on designRuleError
- **THEN** the result is error_ps

### Requirement: compose section line syntax

Each line of the `compose` section SHALL have at least 4 tokens. Otherwise the loader reports `Line must contain at least ruletype, result + pair` and rejects the line. The first token is a rule keyword. It is matched case-insensitively against `compose`, `decompose`, `paint` and `erase`, and any unique prefix is accepted. An exact match wins.

```ebnf
compose-line = compose-kw , result , pair , { pair }
             | paint-kw , have , paint , result-list , [ plane-name ] ;  (* extra tokens ignored *)
compose-kw   = "compose" | "decompose" ;
paint-kw     = "paint" | "erase" ;
pair         = type-name , type-name ;
```

Compatibility: exact

An unknown keyword reports `Unknown rule type <k>.  Must be one of:` and lists the four keywords. An ambiguous keyword, such as one that is too short to tell `compose` and `decompose` apart, reports the same text with `Ambiguous` instead of `Unknown`. In both cases the line is rejected.

In this keyword matcher, an empty string is a prefix of every keyword. Single letters are unique (`c`, `d`, `p`, `e`).

(Magic ref: database/DBtpaint.c:352-391, utils/lookup.c:58-130)

#### Scenario: Abbreviated keyword
- **GIVEN** the line `Comp nfet poly ndiff`
- **WHEN** it is loaded
- **THEN** it is processed as a `compose` rule

### Requirement: compose and decompose rules for non-contact results

For `compose T a1 b1 a2 b2 ...` or `decompose T a1 b1 ...`, the loader SHALL first resolve T noisily. If T does not resolve, the line is rejected. If the number of remaining tokens is odd, it reports `Types on RHS of rule must be in pairs` and rejects the line. If T is a contact, the rule is saved (see "Contact compose and decompose rules"). Otherwise the pairs are processed left to right. For each pair (r, s):

Compatibility: exact

1. Resolve r and s, noisily. If either fails, the line is rejected. Earlier pairs remain applied.
2. If r or s is a contact: `Can't have contact layers on RHS of non-contact rule`, and the line is rejected.
3. Let p be the home plane of r. The home plane of s is not used (see Q-TECH-21).
4. For `compose` only: set `P[p](r,s) := T` and `P[p](s,r) := T`, and mark `udP(r,s)` and `udP(s,r)`.
5. For both rule kinds:
   - `P[p](T,r) := T`
   - `P[p](T,s) := T`
   - `E[p](T,r) := s`
   - `E[p](T,s) := r`
   - Mark `udP(T,r)`, `udP(T,s)`, `udE(T,r)` and `udE(T,s)`.

These assignments are unconditional, and a later line can override an earlier one.

(Magic ref: database/DBtpaint.c:392-442)

#### Scenario: Transistor composition
- **GIVEN** the example technology with `compose nfet poly ndiff`
- **WHEN** the compose section is finalized
- **THEN** P[active](polysilicon, ndiffusion) = ntransistor and P[active](ndiffusion, polysilicon) = ntransistor
- **AND** P[active](ntransistor, polysilicon) = ntransistor
- **AND** E[active](ntransistor, polysilicon) = ndiffusion and E[active](ntransistor, ndiffusion) = polysilicon

#### Scenario: Decompose does not create the type by painting
- **GIVEN** `decompose dfet poly diff`
- **WHEN** the section is finalized
- **THEN** P[p](poly, diff) keeps its default value (diff, when both are on the same plane)
- **AND** E[p](dfet, poly) = diff

### Requirement: Explicit paint and erase rules

For `paint h t R [plane]` or `erase h t R [plane]`, the loader SHALL apply the following:

Compatibility: exact

1. **Arguments.** The line must have at least 3 arguments after the keyword. If it does not, the loader reports `Line must contain at least 3 types` and rejects the line. h and t are single type names, resolved noisily; either may use `/plane` image qualifiers. R is a type list (see capability `tech-layers-and-planes`), yielding a type set Rt and a plane set Rp. If Rt is empty, the line is rejected.
2. **Target planes Pm.**
   - With a fourth argument, Pm = {that plane}, resolved by unique prefix; an unknown or ambiguous plane rejects the line.
   - Without one, if h is space, the loader reports `Must specify plane in paint table for painting space` and rejects the line. Otherwise Pm = Planes(h). For a contact this is all the planes of its images.
3. **Remove result planes.** Pm := Pm \ Rp.
4. **Result types.** For each type r in Rt, in increasing order, with r < U, and for each tech plane q in Rp on which r can appear (q ∈ Planes(r)): set `P[q](h,t) := r` for `paint`, or `E[q](h,t) := r` for `erase`. These are unconditional. When several types land on one plane, the highest-numbered one wins. Stacked types in Rt are ignored.
5. **Remaining planes.** For each tech plane q in Pm: set `P[q](h,t) := space` for `paint`, or `E[q](h,t) := space` for `erase`.
6. **Flag.** Mark `udP(h,t)`. This happens for `erase` rules too. `udE` is not marked, so contact default steps may later overwrite an explicit erase rule (see Q-TECH-20).

The explicit plane argument limits only step 5. Step 4 always writes on every plane of Rp. A result list such as `space` has Rp = every plane except router, so step 4 writes space on every tech plane where h can appear.

Explicit rules are applied as soon as the line is read. They take effect before contact defaults are computed, and because of the user-defined flag those defaults do not override the paint entries.

(Magic ref: database/DBtpaint.c:594-692)

#### Scenario: Well conversion
- **GIVEN** the scmos-style rule `paint pdiff pwell ndiff`, with pdiff and ndiff on plane active
- **WHEN** the rule is loaded
- **THEN** P[active](pdiff, pwell) = ndiff

#### Scenario: Contact image swap per plane
- **GIVEN** the rules `paint pdc/active pwell ndc/active` and `paint pdc/m1 pwell ndc/m1`
- **WHEN** they are loaded (pdc and ndc are contacts on active and metal1)
- **THEN** P[active](pdc, pwell) = ndc and P[metal1](pdc, pwell) = ndc

#### Scenario: Missing plane when painting over space
- **GIVEN** the rule `paint space nwell nwell`
- **WHEN** it is loaded
- **THEN** `Must specify plane in paint table for painting space` is reported and the load fails

### Requirement: Contact compose and decompose rules

When the result T of a `compose` or `decompose` line is a contact, the loader SHALL save the rule and apply it in finalization step 4. Each pair (r, s) is resolved and validated when the line is read:

Compatibility: exact

1. **Resolution.** Both r and s must resolve. If either fails, the line is rejected.
2. **Not both contacts.** At most one of r and s may be a contact. Otherwise: `Only one type in each pair may be a contact`, and the line is rejected.
3. **Subset of planes.** Planes(r) ∪ Planes(s) must be a subset of Planes(T). Otherwise: `Component planes are a superset of result planes`, and the line is rejected.
4. **Equal planes for compose.** For `compose` only, Planes(r) ∪ Planes(s) must equal Planes(T). Otherwise: `Union of pair planes must = result planes`, and the line is rejected.

A rule is saved even when a later pair fails, with the pairs validated before the failure (see Q-TECH-18). The saved-rule list keeps declaration order.

Magic does not clear the saved-rule list when a new technology is loaded. It holds at most 256 rules for the whole session (see Q-TECH-19). Hefesto SHALL clear the list at the start of every `compose` section. That is the behavior Magic shows for the first technology loaded in a session.

(Magic ref: database/DBtpaint.c:403-404, database/DBtpaint.c:462-522, database/DBtpaint.c:58)

#### Scenario: Plane mismatch in a contact compose rule
- **GIVEN** the example technology and `compose pc poly ndiff`
- **WHEN** it is loaded
- **THEN** `Union of pair planes must = result planes` is reported, because {active} differs from {active, metal1}

### Requirement: Compose finalization order

When the `compose` section ends, the loader SHALL run these steps in this order. Each later step can overwrite default entries written by an earlier step, because `setP` and `setE` do not set the user-defined flags.

Compatibility: exact

1. Paint all images (see "Finalization step 1").
2. Residue-preserving paints (see "Finalization step 2").
3. Contact-over-contact and contact-erase rules, then stacked types (see "Finalization step 3a", "Finalization step 3b" and "Finalization step 3c").
4. Saved contact rules (see "Finalization step 4").
5. Paint and erase plane sets (see "Affected-plane tables").
6. Contact locking (see "Contact locking").

"The contact list" in these steps means the user contacts in declaration order, followed by the stacked types in creation order.

(Magic ref: database/DBtpaint2.c:90-125)

#### Scenario: Order matters for defaults
- **GIVEN** the example technology
- **WHEN** finalization runs
- **THEN** the value of P[metal1](pc, m1) written in step 2 (pc) is rewritten by step 3a with the same value, pc

### Requirement: Finalization step 1, paint all images

The loader SHALL apply the following for each user contact c in the contact list (stacked types are skipped), and for each residue r of c in increasing type order, with p = home plane of r:
- for every type s from 9 to N−1 whose home plane is p, `setP(s, c, p, c)`;
- if `udP(space, c)` is false, `P[p](space, c) := c`.

Compatibility: exact

Painting a contact over anything on any of its image planes therefore gives the contact by default.

(Magic ref: database/DBtpaint2.c:197-223)

#### Scenario: Contact painted over metal
- **GIVEN** the example technology
- **WHEN** the tables are finalized
- **THEN** P[metal1](metal1, pc) = pc and P[metal1](space, pc) = pc and P[active](ndiffusion, pc) = pc

### Requirement: Finalization step 2, residue-preserving paints

The loader SHALL apply the following for each contact c in the contact list (stacked types included), for each residue r of c with r < U, and for each type s from 9 to U−1: if painting s does not affect r, then `setP(c, s, home(r), c)`. "Painting s does not affect r" means `P[home(r)](r, s) = r`.

Compatibility: exact

For a stacked type, its residues are its component contacts, and home(r) is the component contact's home plane.

(Magic ref: database/DBtpaint2.c:255-277, database/databaseInt.h:247-248)

#### Scenario: Painting a residue keeps the contact
- **GIVEN** the example technology
- **WHEN** the tables are finalized
- **THEN** P[metal1](pc, metal1) = pc and P[active](pc, metal1) = pc

### Requirement: Finalization step 3a, painting over a contact

For each contact I in the contact list, and each user type T from 9 to U−1 with T ≠ I, the loader SHALL apply the following:

Compatibility: exact

Let S = Planes(I) ∩ Planes(T). For a non-contact T, Planes(T) = {home(T)} and Residue(T, home(T)) = T.

**Case A: S is not empty.** Scan the planes q of S in increasing order. If at some q Residue(T, q) ≠ Residue(I, q), call this "incompatible", stop the scan, and let x = Residue(I, q).

- **A1, compatible** (no difference found). Let C be the set of user contacts (9..U−1) whose residue set equals Res(I) ∪ Res(T). Let G be the stacked type built from {I, T}, if one exists.
  - **A1a.** If C is empty or G exists:
    - If G exists: `setP(I, T, q, G)` for each q in S.
    - Else, if T is a contact and I < U: for each residue r of I in increasing order, `setP(I, T, home(r), T)` when r ∈ Res(T), and `setP(I, T, home(r), r)` otherwise.
    - Else, if T is a contact and T ∉ Res(I): for each r of I in increasing order with r < U and r ∈ Res(T), `setP(I, T, home(r), T)`. For a stacked I this matches nothing.
    - Otherwise: `setP(I, T, home(I), I)`.
  - **A1b.** Otherwise (C is not empty and G does not exist): let k be the lowest-numbered type in C, and `setP(I, T, q, k)` for every q ∈ Planes(k) (see Q-TECH-22).
- **A2, incompatible.** For each residue r of I with r < U and r ≠ x, `setP(I, T, home(r), r)`. No entry is written on the plane where the scan stopped.

**Case B: S is empty and T is a contact.** Let C be as in A1. For every k ∈ C, in increasing order, and every q ∈ Planes(k), `setP(I, T, q, k)`.

**Case C: S is empty and T is not a contact.** No change.

(Magic ref: database/DBtpaint2.c:298-315, database/DBtpaint2.c:354-533)

#### Scenario: Stacking metal2 contact over poly contact
- **GIVEN** the example technology with stacked type 19 = pc+m2c
- **WHEN** the tables are finalized
- **THEN** P[metal1](pc, m2c) = pc+m2c and P[active](pc, m2c) = pc
- **AND** P[metal1](m2c, pc) = pc+m2c and P[metal2](m2c, pc) = m2c

#### Scenario: Painting a non-residue over a contact (verify against Magic)
- **GIVEN** the example technology with `compose nfet poly ndiff`
- **WHEN** the tables are finalized
- **THEN** P[metal1](pc, ndiff) = metal1 (case A2) and P[active](pc, ndiff) = ndiffusion (the default from initialization step 3)
- **AND** the manual's statement that the result is ntransistor on active does not hold at table level (see Q-TECH-23)

#### Scenario: Painting a contact over a different contact on shared planes (verify against Magic)
- **GIVEN** the example technology
- **WHEN** the tables are finalized
- **THEN** P[active](ndc, pc) = pc (from step 1) and P[metal1](ndc, pc) = metal1 (case A2, since the residues on active differ)

### Requirement: Finalization step 3b, erasing from a contact

For each contact I in the contact list, and each user type X from 9 to U−1 (including X = I), the loader SHALL apply the following:

Compatibility: exact

1. **Erase to space.** For each tech plane q in Planes(X): if I < U, or q = home(I), then `setE(I, X, q, space)`.
2. **Self-erase stops here.** If X = I, stop.
3. **No shared plane.** If Planes(I) ∩ Planes(X) is empty, stop.
4. **Candidate set.** Let R be the full residue set of I (see capability `tech-contacts-and-stacking`). Build the set C and an "overlap" flag by scanning the contact list in order. A contact L is added to C when Res(L) ⊆ R and not Res(X) ⊆ Res(L). If Res(L) intersects the union of the residue sets of contacts already in C, set overlap to true. Otherwise add Res(L) to that union.
5. **Overlap.** If overlap is true: `setE(I, X, q, I)` for every q ∈ Planes(I).
6. **No overlap.** Otherwise:
   - Let Q = Planes(I) \ Planes(X).
   - For each k ∈ C in increasing type order:
     - Q := Q \ Planes(k).
     - For each q ∈ Planes(k): if I < U, or q = home(I), then `setE(I, X, q, k)`.
   - Then for each q ∈ Q: `setE(I, X, q, Residue(I, q))`.

(Magic ref: database/DBtpaint2.c:553-613, database/DBtpaint2.c:626-721)

#### Scenario: Erasing metal from a poly contact
- **GIVEN** the example technology
- **WHEN** the tables are finalized
- **THEN** E[metal1](pc, metal1) = space and E[active](pc, metal1) = polysilicon

#### Scenario: Erasing poly from a poly contact
- **GIVEN** the example technology
- **WHEN** the tables are finalized
- **THEN** E[active](pc, poly) = space and E[metal1](pc, poly) = metal1

#### Scenario: Erasing an unrelated active type leaves the contact
- **GIVEN** the example technology
- **WHEN** the tables are finalized
- **THEN** E[active](pc, ndiff) = pc and E[metal1](pc, ndiff) = pc

#### Scenario: Erasing the other contact of a stack
- **GIVEN** the example technology
- **WHEN** the tables are finalized
- **THEN** E[active](pc, m2c) = pc and E[metal1](pc, m2c) = pc

### Requirement: Finalization step 3c, stacked types as applied types

After steps 3a and 3b have run for every pair, the loader SHALL apply the following for every existing type h from 0 to N−1, every stacked type G from U to N−1, and every tech plane q:
- Let G's component contacts be c1 < c2.
- `setP(h, G, q, P[q](P[q](h, c1), c2))`.
- `setE(h, G, q, E[q](E[q](h, c1), c2))`.

Compatibility: exact

Painting a stacked type is therefore defined as painting its component contacts in increasing type order, and erasing it as erasing them in that order. The values used are the tables as they stand after step 3b. The pairs (h, G) are processed with h in the outer loop and G in the inner loop.

(Magic ref: database/DBtpaint2.c:321-340)

#### Scenario: Painting a stacked contact on empty space
- **GIVEN** the example technology
- **WHEN** the tables are finalized
- **THEN** P[metal1](space, pc+m2c) = pc+m2c, P[active](space, pc+m2c) = pc, and P[metal2](space, pc+m2c) = m2c

#### Scenario: Erasing a stacked contact from itself
- **GIVEN** the example technology
- **WHEN** the tables are finalized
- **THEN** E[metal1](pc+m2c, pc+m2c) = space

### Requirement: Finalization step 4, saved contact rules

For each saved rule (result contact T, kind, pairs), in saved order, and each pair (a, b) in order, the loader SHALL apply the following. Let p be the home plane of T.

Compatibility: exact

1. **Decompose (a, b):**
   - `P[p](T,a) := T`, and mark `udP(T,a)`.
   - `E[p](T,a) := Residue(T,p)`, and mark `udE(T,a)`.
   - The second type of the pair is not used for the erase result.
2. **Decompose (b, a):** the same with the roles swapped.
3. **For `compose` rules only:**
   - If p ∈ Planes(a): `P[p](a,b) := T`, and mark `udP(a,b)`.
   - Then, if p ∈ Planes(b): `P[p](b,a) := T`, and mark `udP(b,a)`.

Only T's home plane is written by this step.

(Magic ref: database/DBtpaint2.c:817-919)

#### Scenario: Contact compose rule
- **GIVEN** a technology with contact `pdc pdiff metal1` (home active) and the rule `compose pdc pdiff m1`, where Planes(pdiff) ∪ Planes(m1) = Planes(pdc)
- **WHEN** finalization runs
- **THEN** P[active](pdiff, m1) = pdc, because active ∈ Planes(pdiff)
- **AND** P[active](m1, pdiff) is not written by step 4, because active ∉ Planes(m1)
- **AND** P[active](pdc, pdiff) = pdc and E[active](pdc, pdiff) = pdiff (Residue(pdc, active))
- **AND** E[active](pdc, m1) = pdiff, because the erase result is always Residue(pdc, active) and the remaining type is not used

### Requirement: Affected-plane tables

After finalization, the loader SHALL compute for each type t from 1 to N−1:
- PaintPlanes(t): the set of planes p in 1..K−1 such that `P[p](s,t) ≠ s` for some s below N.
- ErasePlanes(t): the same for E.

Compatibility: exact

For space (t = 0), both sets are every plane except router.

Painting type t modifies exactly the planes in PaintPlanes(t). Erasing it modifies exactly the planes in ErasePlanes(t).

(Magic ref: database/DBtpaint2.c:147-170)

#### Scenario: Contact paints both planes
- **GIVEN** the example technology
- **WHEN** PaintPlanes(pc) is computed
- **THEN** it equals {active, metal1}

### Requirement: Contact locking

When finalization ends, and again whenever the active set changes (see capability `tech-command`), the erase rules of locked contacts SHALL be adjusted.

Compatibility: exact

**At finalization:**
- Every locked user contact c is locked.
- Every stacked type that has a locked component contact is removed from the active set and locked.

**Locking contact c:** for every type n from 9 to N−1 with n ≠ c, and every q ∈ Planes(c), `setE(c, n, q, c)`. If c is stacked, n is a component contact of c, and n is active, then n is skipped. As a result, erasing anything other than c itself leaves c in place.

**Unlocking contact c:** re-run "Finalization step 3b" for I = c and every user type X from 9 to U−1. This restores the default erase rules. Stacked types' rules for applied stacked types are not recomputed.

(Magic ref: database/DBtpaint2.c:107-120, database/DBtpaint2.c:734-794)

#### Scenario: Locked contact survives residue erase
- **GIVEN** a technology with contact pc declared on a locked plane line (`-active polycontact,pc`)
- **WHEN** finalization runs
- **THEN** E[metal1](pc, metal1) = pc and E[active](pc, metal1) = pc

### Requirement: connect section

Each line of the `connect` section SHALL have exactly two tokens, each a type list (see capability `tech-layers-and-planes`). Otherwise the loader reports `Line must contain exactly 2 lists of types` and rejects the line. For every type a in the first list and every type b in the second list, with a and b below N, the loader SHALL record that a connects to b and that b connects to a.

```ebnf
connect-line = type-list , type-list ;
```

Compatibility: exact

- **Init:** when the `connect` header is read, every type connects only to itself.
- **Unknown names** in a list are reported but do not reject the line. The rest of the list is still used.
- **Not transitive.** Connectivity is symmetric. It is not transitive, and the loader does not close it under transitivity. Listing a,b on one side does not connect a to b unless they also appear on opposite sides.

(Magic ref: database/DBtech.c:278-337)

#### Scenario: Poly connectivity
- **GIVEN** the example technology connect section
- **WHEN** it is loaded
- **THEN** polysilicon connects to {polysilicon, polycontact, pc+m2c}
- **AND** polysilicon does not connect to metal1

#### Scenario: Transistor isolation
- **GIVEN** the example technology, where ntransistor appears in no connect line
- **WHEN** it is loaded
- **THEN** ntransistor connects only to itself

### Requirement: Connectivity finalization

When the `connect` section ends, the loader SHALL compute the final connectivity tables as follows:

Compatibility: exact

1. **Stacked types.** For each stacked type G (U..N−1), in increasing order:
   - Conn(G) gains G's component contacts and the full Conn() of each component contact, as Conn() stands at this point.
   - For every stacked type H > G whose component set shares a contact with G's, Conn(G) gains H.
2. **Symmetry.** For all a and b from 9 to N−1: if b ∈ Conn(a), add a to Conn(b).
3. **Not-connected sets.** NotConn(t) is used by connectivity searches.
   - For a non-contact t, it is the complement of Conn(t) over all 256 codes.
   - For each contact c in the contact list, start from Conn(c). Add every contact L in the list whose residue set intersects Res(c). Add every stacked type that has c as a component. Then take the complement.
4. **Contact planes.** ContactPlanes(t) = Planes(t) for contacts in the contact list, and the empty set for all other types.
5. **Cross-plane connections.** For every t from 9 to N−1, CrossPlanes(t) is the plane set of (Conn(t) minus all contacts in the contact list), minus t's home plane, minus ContactPlanes(t). These are the planes where t connects to non-contact material without a contact. Tools search them for connections.

(Magic ref: database/DBtech.c:372-510)

#### Scenario: Stacked contact connectivity
- **GIVEN** the example technology connect section
- **WHEN** it is finalized
- **THEN** Conn(pc+m2c) contains polysilicon, metal1, metal2, polycontact, m2contact and ndcontact

#### Scenario: No cross-plane connection for poly
- **GIVEN** the example technology
- **WHEN** CrossPlanes(polysilicon) is computed
- **THEN** it is empty
