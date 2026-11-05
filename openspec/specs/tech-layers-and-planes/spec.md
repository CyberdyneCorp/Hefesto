# Technology Layers and Planes

## Purpose

Defines how planes and tile types are declared in the `planes`, `types` and `aliases` sections of a technology file. It covers the built-in planes and types, how numbers and names are assigned, locked types, name lookup by unique prefix, type aliases, the type-list mini-language that every other section uses to name groups of types, and the derived type and plane tables that other capabilities read.

The lexical format is defined in capability `tech-file-format`. Contacts and stacked types are defined in capability `tech-contacts-and-stacking`. Where this spec says a type is a contact, has residues, or is a stacked type, those terms are defined there.

## Requirements

### Requirement: Built-in planes

Every technology SHALL have six built-in planes with the fixed numbers and names below. Technology planes SHALL be numbered from 6 upward, in the order they are declared.

Compatibility: exact

| Number | Names | Purpose |
|---|---|---|
| 0 | `router` | router/plow scratch plane; never holds paint |
| 1 | `designRuleCheck` | DRC areas to be re-checked |
| 2 | `designRuleError` | DRC error areas |
| 3 | `mhint` | magnet hints |
| 4 | `fhint` | fence hints |
| 5 | `rhint` | rotate hints |

A technology may have at most 64 planes in total, counting the built-in planes. That leaves at most 58 technology planes. Planes 1 and up, below the plane count, are "paint planes".

(Magic ref: database/database.h.in:46, database/database.h.in:195-204, database/DBtechtype.c:71-80, database/DBtechtype.c:119-156)

#### Scenario: First technology plane number
- **GIVEN** a `planes` section whose first line is `well,w`
- **WHEN** it is loaded
- **THEN** plane `well` has number 6 and the plane count is at least 7

### Requirement: planes section

Each line of the `planes` section SHALL have exactly one token: a comma-separated list of names for a new plane.

```ebnf
planes-line = name-list ;
name-list   = { "," } , name , { "," , { "," } , name } , { "," } ;   (* empty names are skipped *)
name        = ? one or more characters other than "," ? ;
```

Compatibility: exact

- **Init:** when the `planes` header is read, all technology planes are discarded, and the plane name table is reset to the built-in plane names.
- **Each line:** the line defines plane number N, where N is the current plane count, and then increments the count.
  - The first name in the list is the plane's long name.
  - The shortest name is its short name. If several names share the shortest length, the first of them is the short name.
  - All the names are added to the plane-name table.
- **Errors:** each of these rejects the line, which fails the load:
  - A line with a token count other than 1: `Line must contain names for plane`.
  - A plane count already at 64: `Too many tile planes (max=64)`.
  - A name already in the table: `Duplicate name: <n>`. Names earlier in the same list stay in the table, pointing at the plane number N, but the plane count is not incremented, so the next plane line takes the same number (see Q-TECH-11).

Plane names follow the lookup rules in "Name lookup by unique prefix".

(Magic ref: database/DBtechtype.c:271-295, database/DBtechtype.c:737-833)

#### Scenario: Plane long and short names
- **GIVEN** the planes line `active,diffusion,polysilicon,a`
- **WHEN** it is loaded as the second technology plane
- **THEN** plane 7 has long name `active` and short name `a`

### Requirement: Built-in tile types

Every technology SHALL have nine built-in tile types, numbered 0 to 8. Technology types SHALL be numbered from 9 upward.

Compatibility: exact

| Number | Names (long first) | Home plane | Listed by `tech layers` |
|---|---|---|---|
| 0 | `space` | none (appears on every plane except `router`) | no |
| 1 | `checkpaint`, `CP` | designRuleCheck | no |
| 2 | `checksubcell`, `CS` | designRuleCheck | no |
| 3 | `error_p`, `EP` | designRuleError | no |
| 4 | `error_s`, `ES` | designRuleError | no |
| 5 | `error_ps`, `EPS` | designRuleError | no |
| 6 | `magnet`, `mag` | mhint | yes |
| 7 | `fence`, `f` | fhint | yes |
| 8 | `rotate`, `r` | rhint | yes |

Built-in names take part in name lookup like any other name. Because `f`, `r` and `mag` are built-in, a user name such as `fet` cannot be shortened to `f`, and `m` is always ambiguous.

At most 254 type numbers (0 to 253) are available. The last 2 of the 256 type codes are reserved. Stacked types count toward this limit.

(Magic ref: database/database.h.in:85-86, database/database.h.in:126-137, database/DBtechtype.c:83-95, database/DBtechtype.c:200-250)

#### Scenario: Built-in short name
- **GIVEN** any loaded technology
- **WHEN** the type name `EP` is looked up
- **THEN** it resolves to type 3 (error_p)

### Requirement: types section

Each line of the `types` section SHALL have one of these forms:

```ebnf
types-line  = plane-ref , name-list , { token }       (* tokens after the 2nd are ignored *)
            | "alias" , alias-name , type-list ;      (* see "Type aliases" *)
plane-ref   = [ "-" ] , plane-name ;                  (* "-" marks the type locked *)
```

Compatibility: exact

- **Init:** when the `types` header is read, these steps happen:
  1. The type-name table is reset to the built-in names.
  2. The alias table is emptied.
  3. The locked set is cleared.
  4. The type count is set to 9.
- **Each non-alias line:**
  1. The line defines type number N, where N is the current type count.
  2. All names in `name-list` are added to the type-name table. The first name is the long name. The shortest name (the first of several equally short ones) is the primary short name.
  3. If `plane-ref` starts with `-`, type N is marked locked (see "Locked types").
  4. The plane name after the optional `-` is resolved by unique prefix. That plane is the home plane of type N, and the type count is incremented.
- **Line not rejected:** a line with fewer than 2 tokens reports `Line must contain at least 2 fields` but is not rejected.
- **Lines rejected:** each of these rejects the line, which fails the load:
  - The type count is already 254: `Too many tile types (max=254)`.
  - A duplicate name: `Duplicate name: <n>`.
  - An unknown plane: `Unrecognized plane name "<p>"`.
  - An ambiguous plane: `Ambiguous plane name "<p>"`.

  With a duplicate name, an unknown plane or an ambiguous plane, the names already added stay in the table bound to number N, which the next type line will reuse (see Q-TECH-11).
- **Final:** at the section `end`, the number of user layers U is fixed to the type count. Types 9 to U−1 are the user types. Stacked types created later are numbered U and up (see capability `tech-contacts-and-stacking`).

A type's home plane is the plane it is stored on. A non-contact type exists only on its home plane. Built-in types 1 to 8 have the home planes listed in "Built-in tile types". Type 0 (space) has no home plane.

The long name of a type is the name written to `.mag` files and printed in messages. It is an `exact` compatibility item (see the `cell-*` capabilities).

(Magic ref: database/DBtechtype.c:200-250, database/DBtechtype.c:422-484, database/DBtechtype.c:559-575)

#### Scenario: Type numbering and names
- **GIVEN** the planes from the example in capability `tech-file-format`, and the types lines `well nwell,nw` then `active polysilicon,poly,p`
- **WHEN** they are loaded
- **THEN** type 9 is `nwell` (short `nw`, home plane well) and type 10 is `polysilicon` (short `p`, home plane active)

#### Scenario: Unknown plane
- **GIVEN** the types line `metal3 metal3,m3` where no plane named with prefix `metal3` exists
- **WHEN** it is loaded
- **THEN** `Unrecognized plane name "metal3"` is reported and the load fails

### Requirement: Locked types

A type whose plane reference in the `types` section begins with `-` SHALL be locked when the technology is loaded. All other types numbered 6 and above, including stacked types, SHALL be active (editable).

Compatibility: behavioral

- **Tech-file active set.** When the `types` section ends, the tech-file active set is fixed. It is (all type numbers 6..255) minus the locked types. The current active set starts equal to it.
- **Stacked types.** When the `compose` section ends, a stacked type is removed from the current active set if any of its component contacts is locked.
- **Contacts.** A locked contact (user or stacked) has its erase rules changed. See capability `tech-compose-and-connect` ("Contact locking").
- **Editing.** Editing operations must not change geometry of locked types. That is specified in the editing capabilities.
- **At run time.** The active set can be changed with `tech locked|unlocked|revert` (see capability `tech-command`).

(Magic ref: database/DBtechtype.c:466-471, database/DBtechtype.c:584-589, database/database.h.in:1074-1075, database/DBtpaint2.c:107-120)

#### Scenario: Locked well type
- **GIVEN** the types line `-well pwell,pw`
- **WHEN** the technology is loaded
- **THEN** `pwell` is reported by `tech locked` and is not in the active set

### Requirement: Name lookup by unique prefix

Type names and plane names SHALL be resolved against their own name table. The lookup is case-sensitive and works as follows. Let S be the set of names in the table that begin with the query string q.
1. If S is empty, the result is "not found".
2. Otherwise, if S has exactly one name, the result is that name's value.
3. Otherwise, if S contains a name exactly equal to q, the result is that name's value.
4. Otherwise the result is "ambiguous". This holds even when all names in S belong to the same type or plane (see Q-TECH-14).

Compatibility: exact

An empty query is ambiguous. Comparison is byte-wise and case-sensitive. The type-name table holds all user names and alias names: names added by single-type aliases and stackable alias names, as described in capability `tech-contacts-and-stacking`.

For keyword lookups that are not names, such as rule keywords, the different case-insensitive keyword matcher of the owning capability applies.

(Magic ref: database/DBtechtype.c:618-714)

#### Scenario: Exact match beats longer names
- **GIVEN** the example technology from capability `tech-file-format`
- **WHEN** `p` is looked up as a type
- **THEN** it resolves to type 10 (polysilicon), even though `pc`, `pcontact`, `pw`, `pwell`, `poly`, `polycontact` and `polysilicon` also begin with `p`

#### Scenario: Ambiguous prefix
- **GIVEN** the example technology
- **WHEN** `po` is looked up
- **THEN** the result is ambiguous (`poly`, `polycontact` and `polysilicon` match and none equals `po`)

#### Scenario: Prefix of two names of the same type
- **GIVEN** the example technology, where type 11 has the names `ndiffusion` and `ndiff`
- **WHEN** `ndif` is looked up
- **THEN** the result is ambiguous

#### Scenario: Case sensitivity
- **GIVEN** any technology without a user name beginning `cp`
- **WHEN** `cp` is looked up
- **THEN** the result is not found (the built-in name is `CP`)

### Requirement: Single type-name resolution

Resolving one type name, as used for single-type arguments such as a contact base, a compose result, a style entry or a paint command type, SHALL work as follows.

```ebnf
type-name = name , [ "/" , plane-name ] ;
```

Compatibility: exact

1. Split at the first `/`. Look up the part before it in the type-name table, using "Name lookup by unique prefix".
2. If that is not found or is ambiguous, and the name is a key in the alias table, use the alias's type set restricted to user types 9..U−1. If the set holds exactly one type, that type is the result. If it holds more than one, the result is ambiguous. If it holds none, the result is not found.
3. If there is a `/plane` part and step 1 or 2 gave a type t: look up the plane by unique prefix. The result is t if t has an image on that plane, meaning the plane is in t's contact plane set, or is t's home plane for a non-contact. Otherwise, or if the plane is not found, the result is not found.

A "noisy" resolution reports `Ambiguous layer (type) name "<n>"` or `Unrecognized layer (type) name "<n>"`.

Exact-name resolution, used to check whether an alias shadows a type, matches only complete names and fails for any name that contains `/`.

(Magic ref: database/DBtechname.c:64-135, database/DBtechname.c:226-246)

#### Scenario: Contact image qualifier
- **GIVEN** the example technology, where pc has images on active and metal1
- **WHEN** `pc/metal1` is resolved
- **THEN** the result is type 15 (polycontact)

#### Scenario: Qualifier for a plane without an image
- **GIVEN** the example technology
- **WHEN** `poly/metal1` is resolved
- **THEN** the result is not found

### Requirement: Type aliases

A type alias SHALL be defined either by a line of the `aliases` section or by an `alias` line in the `types` section:

```ebnf
aliases-line     = alias-name , type-list , { token } ;     (* tokens after the 2nd ignored *)
types-alias-line = "alias" , alias-name , type-list , { token } ;
```

Compatibility: exact

1. **Fewer than 2 fields.** If the line has fewer than 2 fields after any `alias` keyword, the loader reports `Line must contain at least 2 fields` and ignores the line, which is not rejected.
2. **Shadowing a type.** If `alias-name` exactly equals an existing type name, the loader reports `Type alias "<a>" shadows a defined type` and ignores the line, which is not rejected.
3. **Parse the list.** `type-list` is parsed with the type-list language. A noisy parse reports unknown names.
4. **Single type.** If `type-list`, taken as a single type name, resolves to a type t, and the parsed set is exactly {t}, then `alias-name` is added to the type-name table as an extra name of t. It is never primary, and it is excluded from `tech layers` listings.
5. **Multi-type alias.** Otherwise `alias-name` is stored in the alias table, mapped to the parsed type set. This is a snapshot of the set at definition time. If the name is already in the alias table, the loader reports `Type "<a>" is already defined and cannot be an alias` and keeps the first definition.
6. **Wildcard in the types section.** In the `types` section, a `type-list` containing `*` is refused with `Type alias "<l>" contains the wildcard character "*" (alias ignored).` followed by a hint. A `types`-section alias line with only two tokens (`alias name`) crashes Magic (see Q-TECH-12). Hefesto SHALL report `Line must contain at least 2 fields` instead.

Alias names take part in type-list parsing and single-name resolution, but the precedence differs (see Q-TECH-13):
- In a type list, a token that is an alias-table key always resolves to the alias set first.
- In single-name resolution, the type-name prefix lookup is tried first. The alias table is consulted only if that lookup fails.

The alias table is emptied when the `types` section begins.

(Magic ref: database/DBtechtype.c:346-399, database/DBtechtype.c:443-459, database/DBtechname.c:85-103, database/DBtechname.c:689-696)

#### Scenario: Multi-type alias
- **GIVEN** the example technology and the aliases line `allm1 *m1`
- **WHEN** `paint allm1` resolves its type list
- **THEN** the set is {13, 15, 16, 17, 19} (metal1, polycontact, ndcontact, m2contact, pc+m2c)

#### Scenario: Single-type alias becomes a name
- **GIVEN** the aliases line `diff ndiff`
- **WHEN** `diff` is looked up as a type name
- **THEN** it resolves to type 11
- **AND** `diff` is not listed by `tech layers`

#### Scenario: Alias shadowing a type
- **GIVEN** the aliases line `poly ndiff`
- **WHEN** it is loaded
- **THEN** `Type alias "poly" shadows a defined type` is reported, and the alias is not defined, but the load does not fail

### Requirement: Type-list language

Every section that takes a set of types SHALL parse it with the following language, unless the owning capability says otherwise.

```ebnf
type-list  = [ segment , { "," , { "," } , segment } ] ;
segment    = [ "~" ] , ( "(" , type-list , ")" | atom ) , [ "/" , plane-name ] ;
atom       = "0"                                   (* only when it is the whole remaining text; see below *)
           | alias-name
           | [ "*" ] , type-name-no-slash ;
```

Compatibility: exact

The parse goes left to right, one segment at a time. Each segment yields a type set M, and the result is the union of all segment sets.

1. **Parentheses.** For `(`, the text up to the matching `)` (counting nesting) is parsed recursively as a type-list, and M is its result. If no matching `)` exists, the loader reports `Unmatched parenthesis in layer name "<list>".`, and the group extends to the end of the string.
2. **Atom.** Otherwise the atom is the text up to the next `/`, `,` or end of string.
   - **Empty atom:** `Missing layer name in "<list>".` is reported and M is empty.
   - **`0`:** if the remaining text, from the atom to the end of the whole string, is exactly `0`, then M is empty. A `0` followed by more text, as in `0,poly` or `0/active`, is treated as a type name and is unrecognized (see Q-TECH-15).
   - **Alias:** if the atom, including any leading `*`, is an alias-table key, M is the alias set.
   - **Type name:** otherwise strip a leading `*`, if present, and resolve the rest as a type name with plain prefix lookup (single-name resolution, which may itself consult the alias table). If it resolves to t, M = {t}. If it does not, M is empty and a noisy parse prints an error.
     - **With `*`:** also add every user type r (9..U−1) whose residue set contains t. These are all the contacts with t as a residue.
     - **Always** (with or without `*`): also add every stacked type s (U..N−1) whose component-contact set shares at least one type with M. Here N is the total type count.
3. **Negation.** If the segment began with `~`, M becomes its complement over all 256 type codes. That includes space, the built-ins, the stacked types and the unused codes.
4. **Plane restriction.** If `/plane` follows, the plane is resolved by unique prefix. If its number is greater than 0, M is intersected with the set of types that can appear on that plane (see "Derived type and plane tables"). If the plane is unknown, ambiguous or `router`, no restriction is applied; a noisy parse still reports unknown and ambiguous planes (see Q-TECH-16).

The type list also yields a plane set P, built up as the segments are parsed:
- A segment with an effective `/plane` restriction sets P to exactly that one plane. This replaces whatever earlier segments contributed (see Q-TECH-16).
- A segment without one adds to P the plane set of every user type in M (types 9..U−1).

When the parse ends, if the result type set is empty or is exactly {space}, and P is empty, then P becomes "every plane except router". That is the 64-bit mask with only bit 0 cleared.

There must be no whitespace inside a type list, because the tokenizer splits on it.

(Magic ref: database/DBtechname.c:598-804)

#### Scenario: Plain name selects only that type
- **GIVEN** the example technology
- **WHEN** `m1` is parsed
- **THEN** the set is {13} and P = {metal1}

#### Scenario: Contact name includes stacked types
- **GIVEN** the example technology with stacked type 19 = pc+m2c
- **WHEN** `pc` is parsed
- **THEN** the set is {15, 19}

#### Scenario: Asterisk expansion
- **GIVEN** the example technology
- **WHEN** `*m1` is parsed
- **THEN** the set is {13, 15, 16, 17, 19}
- **AND** P = {active, metal1, metal2}

#### Scenario: Complement restricted to a plane
- **GIVEN** the example technology
- **WHEN** `~(pc,ndc)/active` is parsed
- **THEN** the set is every type that can appear on plane active except 15, 16 and 19, which is {0, 10, 11, 12}
- **AND** P = {active}

#### Scenario: Null list
- **GIVEN** any technology
- **WHEN** `0` is parsed
- **THEN** the set is empty and P is every plane except router

#### Scenario: Zero followed by more text
- **GIVEN** any technology without a type named with prefix `0`
- **WHEN** `0,poly` is parsed noisily
- **THEN** `Unrecognized layer (type) name "0"` is reported, and the result set is {poly}

### Requirement: Derived type and plane tables

After the `types` and `contact` sections are finalized, the loader SHALL compute the derived tables below. Other capabilities use these definitions.

Compatibility: exact

| Table | Definition |
|---|---|
| user types | types 9..U−1, plus space when "user layer set" is mentioned |
| all-but-space-and-DRC set | type numbers 6..255 |
| plane set of type t, Planes(t) | space: every plane except router (bits 1..63). A type with home plane p > 0: {p}, plus for a contact all the planes it touches (see capability `tech-contacts-and-stacking`). |
| types on plane p, OnPlane(p) | space; every non-contact type whose home plane is p; every contact (user or stacked) whose plane set contains p. For router (p = 0) the set is empty. |
| home-plane types, Home(p) | every type t from 1 to N−1 whose home plane is p (each type appears exactly once) |
| plane set of a type set | the union of Planes(t) over the set, minus the router plane. If the set contains space, every plane 1..plane count−1. |

Space appears on every paint plane. No type is ever stored on the router plane.

(Magic ref: database/DBtechtype.c:559-595, database/DBtcontact.c:747-805, database/DBtechname.c:390-411)

#### Scenario: Contact appears on both planes
- **GIVEN** the example technology
- **WHEN** the derived tables are computed
- **THEN** OnPlane(metal1) contains 13, 15, 16, 17, 19 and space
- **AND** Home(metal1) contains 13, 17, 19

### Requirement: Short and long name reporting

The system SHALL report type and plane names as follows.
- The long name is the first name of the declaring line. For stacked types it is the generated name.
- The short name is the primary name: the shortest name of the declaring line, and the first of several equally short ones. If a type or plane has no primary name, the short name falls back to the long name. If there is no long name either, it is `???`. A negative type prints as `ERROR`.

Compatibility: exact

Alias names added later, through single-type aliases or stackable aliases, never become primary.

(Magic ref: database/DBtechname.c:331-367, database/DBtechtype.c:737-780)

#### Scenario: Equal-length short names
- **GIVEN** the types line `metal1 m2contact,m2c,via`
- **WHEN** the short name of the type is requested
- **THEN** it is `m2c` (first among the 3-character names `m2c` and `via`)
