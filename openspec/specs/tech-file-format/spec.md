# Technology File Format

## Purpose

Defines the lexical and structural format of a Magic-compatible technology (`.tech`) file: how lines are read and split into tokens, comments, line continuation, quoting, the `include` and `repeat` directives, how sections are delimited, the section registry with its prerequisites and required/optional status, error recovery, and the contents of the `tech` and `version` sections, including the format version number and the behavior it gates.

The contents of most sections are owned by other capabilities:
- `planes`, `types` and `aliases`: see capability `tech-layers-and-planes`.
- `contact` (alias `images`): see capability `tech-contacts-and-stacking`.
- `compose` and `connect`: see capability `tech-compose-and-connect`.
- `styles`: see capability `tech-styles-section`.
- `cifoutput` and `cifinput`: see the `mask-*` capabilities.
- `drc`: see the `drc-*` capabilities.
- `extract`: see the `extract-*` capabilities.
- `lef`: see the `lef-*` capabilities.
- `mzrouter`, `router`, `wiring`, `plowing` and `plot`: see the router, wiring, plow and plot capabilities.

How files are located, loaded, reloaded and scaled is specified in capability `tech-loading-and-scaling`.

## Requirements

### Requirement: Physical line reading and comment lines

The reader SHALL read a technology file as a sequence of physical lines terminated by LF (`\n`). If the character before the LF is CR (`\r`), the reader SHALL drop it. A physical line whose first non-whitespace character is `#` is a comment line, and the reader SHALL discard all of it. A `#` that appears after the first non-whitespace character of a line SHALL NOT start a comment; it is an ordinary character.

Compatibility: exact

Whitespace means the C-locale `isspace` set: space, TAB, LF, VT, FF and CR.

A logical line that contains no tokens is skipped, and the reader moves on to the next line. This applies to empty lines, lines containing only whitespace, and comment lines.

Each physical line read increments the current line number, including comment lines, blank lines and continuation lines. Line numbers count from 1 within each file, and an included file has its own count (see "include directive"). Error messages report the line number of the last physical line consumed.

The user manual says that `#` starts a comment anywhere on a line. The code recognizes `#` only at the start of a line (see Q-TECH-29).

(Magic ref: utils/tech.c:1004-1040, utils/tech.c:1097-1098)

#### Scenario: Indented comment line is ignored
- **GIVEN** a section body line `    # metal rules follow`
- **WHEN** the line is read
- **THEN** it produces no tokens and no section line is dispatched

#### Scenario: Mid-line hash is an ordinary token
- **GIVEN** the line `poly # comment`
- **WHEN** the line is tokenized
- **THEN** the tokens are `poly`, `#`, `comment` (3 tokens)

#### Scenario: CRLF line ending
- **GIVEN** a file saved with CRLF line endings containing the line `tech\r\n`
- **WHEN** the line is tokenized
- **THEN** the single token is `tech`, with no trailing CR

### Requirement: Line continuation

A physical line that ends in a backslash (`\`) immediately before its line terminator (after any CR is removed) SHALL be joined to the next physical line. The reader SHALL remove the backslash and the line terminator and SHALL NOT insert any separator, so the next line's text, including its leading whitespace, follows directly.

Compatibility: exact

A comment line inside a continuation is discarded, and the continuation goes on with the next non-comment physical line. The combined logical line is limited to the line-buffer size (see "Line length and token limits").

The user manual says that the backslash-newline is treated as a blank. The code concatenates the lines with no blank (see Q-TECH-29).

(Magic ref: utils/tech.c:1026-1039)

#### Scenario: Continuation with leading blanks
- **GIVEN** the two physical lines `width allDiff 2 \` and `    "Diffusion width must be at least 2"`
- **WHEN** they are read
- **THEN** one logical line is produced with tokens `width`, `allDiff`, `2`, `Diffusion width must be at least 2`

#### Scenario: Continuation without whitespace joins tokens
- **GIVEN** the physical lines `metal\` and `1`
- **WHEN** they are read
- **THEN** the logical line has the single token `metal1`

#### Scenario: Comment line inside a continuation (verify against Magic)
- **GIVEN** the physical lines `connect a \`, `# note`, `b`
- **WHEN** they are read
- **THEN** one logical line with tokens `connect`, `a`, `b` is produced

### Requirement: Tokenization and quoting

The reader SHALL split each logical line into tokens using these rules, applied left to right:

```ebnf
logical-line   = { ws } , [ token , { ws , { ws } , token } ] , { ws } ;
token          = quoted-token | bare-token ;
bare-token     = bare-char , { bare-char } ;          (* begins with any char except ws and '"' *)
bare-char      = escaped | ( any-char - ws - "\" ) ; (* '"' inside a bare token is literal *)
quoted-token   = '"' , { escaped | ( any-char - '"' - "\" ) } , [ '"' ] ;
escaped        = "\" , any-char ;                    (* yields any-char literally *)
ws             = " " | TAB | VT | FF | CR | LF ;
```

Compatibility: exact

The tokenizer works as follows:
- A token that begins with `"` is quoted. It extends up to the next unescaped `"`, which closes it, or to the end of the line if no closing quote follows. The quote characters are not part of the token value. Whitespace inside quotes is kept.
- A token that does not begin with `"` ends at the first unescaped whitespace character. A `"` inside such a token is literal.
- In both kinds of token, a backslash takes the next character literally and the backslash itself is dropped. A backslash at the very end of the logical line is dropped.
- The character right after a closing quote begins the next token, even if it is not whitespace. So `"a b"c` gives the two tokens `a b` and `c`.
- `""` gives one empty token.

Tokens are case-sensitive. Keyword matching is defined by each section owner.

(Magic ref: utils/tech.c:1043-1095)

#### Scenario: Quoted token with spaces
- **GIVEN** the line `description "Minimum technology file structure"`
- **WHEN** it is tokenized
- **THEN** the tokens are `description` and `Minimum technology file structure`

#### Scenario: Escapes
- **GIVEN** the line `a\ b "x\"y" z"w`
- **WHEN** it is tokenized
- **THEN** the tokens are `a b`, `x"y`, `z"w`

#### Scenario: Text adjacent to a closing quote
- **GIVEN** the line `"a b"c`
- **WHEN** it is tokenized
- **THEN** the tokens are `a b` and `c`

### Requirement: Line length and token limits

The reader SHALL accept logical lines of up to 1023 characters, counting the joined continuation lines. A longer logical line SHALL be truncated, with the warning `long line truncated`. A logical line SHALL have at most 30 tokens.

Compatibility: behavioral

Magic's behavior beyond these limits is undefined. A physical line longer than the buffer, a final line with no LF, and more than 30 tokens all cause memory-safety faults. Hefesto MUST NOT crash. It SHALL handle these cases as follows:
- A final line without a terminating LF SHALL be accepted as if it had one.
- Lines longer than 1023 characters and lines with more than 30 tokens are recorded in Q-TECH-3 and Q-TECH-4 for decision. Until the decision is made, Hefesto SHALL report an error on that line and SHALL keep reading.

(Magic ref: utils/tech.c:120-121, utils/tech.c:1007-1041)

#### Scenario: Final line without newline
- **GIVEN** a file whose last bytes are `end` with no trailing LF
- **WHEN** the file is read
- **THEN** `end` is recognized as a complete line

### Requirement: Section structure

A technology file SHALL consist of sections. A section starts with a header line that has exactly one token: a registered section name or section alias. Matching is exact and case-sensitive. The section ends with a line that has exactly one token, `end`. Every logical line between the header and the `end` line SHALL be passed, as its token vector, to every handler registered for that section, in registration order.

Compatibility: exact

When no section is open, the reader handles each logical line as follows:
- A line with more than one token SHALL produce the error `Bad section header line`. The reader then enters skip mode.
- A one-token line that is not a registered name or alias SHALL produce `Unrecognized section name: <name>`. The reader then enters skip mode. This also applies to a stray `end`, which is not a section name (see Q-TECH-6).
- After either error, the reader prints `[Skipping to "end"]`.

In skip mode, every line is ignored until the next line that is exactly `end`. That `end` leaves skip mode, and no handler runs for it.

Inside an open section, only a line consisting of exactly the token `end` closes it. A line such as `end foo` is an ordinary section line.

When a header is accepted, these steps happen in order:
1. The section's init procedures run in registration order.
2. The body lines are dispatched to the line handlers.
3. At `end`, the section is marked as read, its bit is added to the "sections read" set, and its final procedures run in registration order.

A section may appear more than once. Each appearance runs init again, and for most sections this discards what the earlier appearance defined (see Q-TECH-7).

The order of sections in the file is free, except for the prerequisite rule (see "Section registry and prerequisites").

(Magic ref: utils/tech.c:677-763)

#### Scenario: Header with trailing comment text
- **GIVEN** a line `drc # rules` outside any section
- **WHEN** it is read
- **THEN** the error `Bad section header line` is reported
- **AND** all lines up to and including the next `end` are ignored

#### Scenario: Stray end swallows the next section
- **GIVEN** the lines `end`, `planes`, `metal1`, `end` outside any section
- **WHEN** they are read
- **THEN** `Unrecognized section name: end` is reported
- **AND** the lines `planes` and `metal1` are skipped and the second `end` leaves skip mode, so no `planes` section is read

### Requirement: Section registry and prerequisites

The loader SHALL recognize exactly the sections in the table below, in this registration order. The section number is its position in the table, starting at 0. The loader SHALL enforce the listed prerequisites and the required/optional status.

Compatibility: exact

| # | Section name (alias) | Prerequisites (must already be read) | Status | Owner capability |
|---|---|---|---|---|
| 0 | `tech` | none | required | tech-file-format |
| 1 | `version` | none | optional | tech-file-format |
| 2 | `planes` | none | required | tech-layers-and-planes |
| 3 | `types` | planes | required | tech-layers-and-planes |
| 4 | `styles` | types | required | tech-styles-section |
| 5 | `contact` (`images`) | types, planes | required | tech-contacts-and-stacking |
| 6 | `aliases` | planes, types, contact | optional | tech-layers-and-planes |
| 7 | `compose` | types, planes, contact | required | tech-compose-and-connect |
| 8 | `connect` | types, planes, contact | required | tech-compose-and-connect |
| 9 | `cifoutput` | none | required | mask-* |
| 10 | `cifinput` | none | required | mask-* |
| 11 | `mzrouter` | types, planes | optional | router capabilities |
| 12 | `drc` | types, planes | required | drc-* (two handlers: DRC rules, then plowing rules derived from the same lines) |
| 13 | `lef` | types, planes | optional | lef-* |
| 14 | `extract` | types, connect | required | extract-* |
| 15 | `wiring` | types | optional | wiring capability |
| 16 | `router` | types | optional | router capabilities |
| 17 | `plowing` | types, connect, contact | optional | plow capability |
| 18 | `plot` | types | optional | plot capability |

The loader checks prerequisites when it reads a header line. If any prerequisite has not been read (or is not excluded by a partial reload; see capability `tech-loading-and-scaling`), the loader SHALL report `Section <name> appears too early.` and `\tMissing prerequisite sections:`. It then lists each missing section name, in registration order, on its own line prefixed by two TABs. The whole section is then skipped.

After the whole file has been read, the loader SHALL report `Section "<name>" was missing from <file>.` for every required section that was not read and is not excluded. These are reported in registration order, and each one makes the load fail.

Header matching is exact. A section name that differs only in case, such as `Planes`, is unrecognized.

A Magic build without the CIF, router, plot or LEF modules registers do-nothing handlers for those sections, except `lef`, which is then unrecognized. Hefesto SHALL register every section in the table.

(Magic ref: utils/main.c:631-716, utils/tech.c:358-403, utils/tech.c:696-706, utils/tech.c:773-784)

#### Scenario: Types before planes
- **GIVEN** a file whose first section is `types` with no earlier `planes` section
- **WHEN** it is loaded
- **THEN** the loader reports `Section types appears too early.` followed by the missing prerequisite `planes`
- **AND** the `types` section is skipped, and at the end `Section "types" was missing from <file>.` is reported and the load fails

#### Scenario: Optional sections may be omitted
- **GIVEN** a file containing only the sections tech, planes, types, styles, contact, compose, connect, cifoutput, cifinput, drc and extract, each possibly empty
- **WHEN** it is loaded
- **THEN** no missing-section error is reported

#### Scenario: Images alias
- **GIVEN** a section header `images`
- **WHEN** it is read
- **THEN** the lines are handled exactly as for a `contact` section

### Requirement: Error reporting and recovery

Errors in a section line SHALL NOT stop reading. Each line handler reports whether the line was accepted. If any handler rejects a line, the loader SHALL record that section as having errors, mark the whole load as failed, and keep reading at the next line. At the end of reading, if any section had errors, the loader SHALL print `The following sections of <file> contained errors:`, followed by one line per such section (four spaces and then the section name), in registration order.

Compatibility: behavioral

A load is successful only when both of these hold:
- No line handler rejected a line.
- No required section is missing.

Some diagnostics are warnings only, and the owning capability lists them. For example, `Line must contain at least 2 fields` in `types` does not fail the load.

Diagnostics are prefixed by the location header `<current file>: line <n>: section <name>:` and then a newline and a TAB. When no section is open, `(none)` is used as the section name. `<current file>` is the full path of the file being read, which is the included file while inside an include.

The exact message text is `informative`. Whether a load succeeds or fails, and which sections are flagged, is `behavioral`.

(Magic ref: utils/tech.c:746-758, utils/tech.c:765-771, utils/tech.c:890-912)

#### Scenario: One bad line fails the load but parsing continues
- **GIVEN** a `connect` section containing the line `m1` (one token) followed by a valid line `m1 m2c`
- **WHEN** the file is loaded
- **THEN** `Line must contain exactly 2 lists of types` is reported for the first line
- **AND** the second line is still applied
- **AND** the load result is failure, with `connect` listed among the sections that contained errors

### Requirement: include directive

A logical line whose first token is `include` and that has at least two tokens SHALL be treated as a file inclusion wherever it appears, inside or outside a section. The loader SHALL open the file named by the second token and read its lines in place of the `include` line, as though they were inserted there. Tokens after the second are ignored. Inclusion nests to any depth. When an included file reaches end-of-file, reading continues after the `include` line in the including file.

Compatibility: exact

To find the include file, the loader appends the current technology-file suffix to the name. This is `.tech`, or `.tech27` if the top-level file was found through the `.tech27` fallback (see Q-TECH-1). It then tries these places in order:
1. The name resolved as described in capability `tech-loading-and-scaling` ("File search rules"): `~`/`$` expansion, absolute and `./`/`../` paths, then the directory `.`, then the system library search path.
2. The directory part of the file currently being read (the including file).

If neither is found, the loader SHALL warn `Warning: Couldn't find include file <name>`. Magic then processes the `include` line itself as an ordinary line: outside a section it is a bad header line, and inside a section it goes to the section handlers (see Q-TECH-2).

While an included file is being read, error locations name the included file and its own line numbers.

A one-token line `include` is not a directive. It is processed as an ordinary line.

Section boundaries are not checked across files. An included file may open or close sections.

(Magic ref: utils/tech.c:635-675, utils/tech.c:1010-1022, utils/path.c:686-800)

#### Scenario: Include resolved next to the including file
- **GIVEN** `/pdk/magic/foo.tech` containing `include foo-drc` and a file `/pdk/magic/foo-drc.tech`, with the working directory not `/pdk/magic`
- **WHEN** `/pdk/magic/foo.tech` is loaded and neither `./foo-drc.tech` nor a system-path copy exists
- **THEN** the lines of `/pdk/magic/foo-drc.tech` are read in place of the include line

#### Scenario: Include name gets the suffix appended (verify against Magic)
- **GIVEN** a line `include common.inc`
- **WHEN** the include is resolved
- **THEN** the file looked for is `common.inc.tech`, not `common.inc`

### Requirement: repeat directive

A logical line consisting of exactly the two tokens `repeat <N>` SHALL start a repeat block, and a logical line consisting of exactly `endrepeat` SHALL end it. The loader SHALL process the lines between them N times in total. Both directive lines are consumed and never reach section handlers. They are recognized anywhere, including in skip mode and outside sections.

Compatibility: exact

The repeat block works as follows:
- `<N>` must be an optional sign followed by decimal digits. Otherwise the error `Error: "repeat" with invalid count <tok>` is reported, the pending count is set to 0, and the following lines are read once.
- On `repeat N`, the loader saves the current position in the current file and sets the remaining count to N−1. A count of 0 or less therefore reads the block once.
- On `endrepeat`, if the remaining count is greater than 0, it is decremented and reading resumes at the saved position. Otherwise `endrepeat` is ignored.
- There is one counter and one saved position. Nested repeat blocks are not supported: an inner `repeat` replaces the outer state. The saved position belongs to whichever file was current at `repeat`. A block that spans an include boundary is undefined (see Q-TECH-5).
- Line numbers keep increasing across repetitions. They are not reset.

(Magic ref: utils/tech.c:607-632)

#### Scenario: Repeat three times
- **GIVEN** inside a `connect` section the lines `repeat 3`, `m1 m1`, `endrepeat`
- **WHEN** the section is read
- **THEN** the connect handler receives the line `m1 m1` exactly 3 times

#### Scenario: Repeat zero
- **GIVEN** the lines `repeat 0`, `X`, `endrepeat`
- **WHEN** they are read
- **THEN** `X` is processed exactly once

### Requirement: tech section

The `tech` section SHALL accept these line forms:

```ebnf
tech-line    = tech-name | format-line ;
tech-name    = token ;                          (* exactly one token *)
format-line  = format-kw , token ;              (* exactly two tokens *)
format-kw    = ? any token beginning with "format" or with "version" ? ;
```

Compatibility: exact

- **Init:** when the `tech` header is read, the format version is set to 27.
- **One token:** the token becomes the technology name. If the section has several one-token lines, the last one wins.
- **Two tokens with the first beginning with `format` or `version`:** this is a prefix test, so `formatted 30` also qualifies. If the second token is an integer (optional sign, then digits), the format version is set to its value. Otherwise `Bad format version number. . . assuming 27` is reported and the line is accepted, but the format version is not changed (see Q-TECH-9).
- **Any other line:** `Badly formed technology name` is reported, and the line is rejected, which fails the load.

If no name line is present, the technology name is left unchanged from the previously loaded technology (see Q-TECH-8).

The technology name is the string that `.mag` files must match in their `tech` header (see the `cell-*` capabilities). It is also reported by `tech name` (see capability `tech-command`).

(Magic ref: database/DBtech.c:70-82, database/DBtech.c:104-134, utils/tech.h:40)

#### Scenario: Name and format
- **GIVEN** the section `tech` / `format 35` / `sky130A` / `end`
- **WHEN** it is loaded
- **THEN** the technology name is `sky130A` and the format version is 35

#### Scenario: Missing format line
- **GIVEN** the section `tech` / `scmos` / `end`
- **WHEN** it is loaded
- **THEN** the format version is 27

#### Scenario: Three-token line
- **GIVEN** the line `my tech name` in the `tech` section
- **WHEN** it is loaded
- **THEN** `Badly formed technology name` is reported and the load fails

### Requirement: version section

The optional `version` section SHALL accept these line forms. Every line must have at least two tokens, and keywords are matched exactly:

```ebnf
version-line     = "version" , token , { token }
                 | "description" , token , { token }
                 | "requires" , token ;            (* extra tokens ignored *)
```

Compatibility: exact

- `version`: the technology version string becomes the second token. Each further token is appended, preceded by one LF character. If the section has several `version` lines, the last one wins.
- `description`: the same, for the description string.
- `requires`: the required Magic version is checked as follows. Leading non-digit characters of the second token are skipped, so `magic-8.3.700` becomes `8.3.700`. The rest must parse as `<int>.<int>.<int>`. If it does not, `Badly formed magic version string, should be major.minor.rev` is reported and the line is rejected. The running version (major, minor, revision) is compared lexicographically with the required version. If the running version is lower, the error `Error:  Magic version R1.R2.R3 is required by this techfile, but this version of magic is M1.M2.M3.` is reported and the line is rejected, which fails the load.
- Any other first keyword, or a line with fewer than two tokens: `Badly formed version line` and the usage text are reported, and the line is rejected.

Hefesto SHALL compare `requires` against the Magic version it claims compatibility with. For this baseline that is 8.3.694. It SHALL NOT compare against its own product version (see Q-TECH-28).

The version and description strings are not reset when a new technology is loaded. A technology without a `version` section keeps the previous technology's strings (see Q-TECH-8).

(Magic ref: database/DBtech.c:147-259)

#### Scenario: Newer Magic required
- **GIVEN** a `version` section with `requires 8.3.700`, and a running compatibility level of 8.3.694
- **WHEN** the file is loaded
- **THEN** `Error:  Magic version 8.3.700 is required by this techfile, but this version of magic is 8.3.694.` is reported
- **AND** the load fails

#### Scenario: Satisfied requirement with prefix
- **GIVEN** `requires magic-8.2.100` and running 8.3.694
- **WHEN** the file is loaded
- **THEN** the line is accepted

#### Scenario: Multi-token version
- **GIVEN** the line `version 1.0 beta`
- **WHEN** it is loaded
- **THEN** the version string is `1.0` LF `beta`

### Requirement: Format version gating

The technology format version SHALL default to 27 and SHALL be set only by the `tech` section. In the reference version it gates exactly the behaviors listed below, and nothing else.

Compatibility: exact

1. **Filename fallback.** When a technology named `X` is not found as `X.tech`, the file `X.tech27` is tried (see capability `tech-loading-and-scaling`). 27 is the last format version that was encoded in the file suffix.
2. **Fringe-halo extraction default.** When a technology is loaded in full and its `extract` section is finalized, the extraction option `fringehalo` is enabled if the format version is 34 or more, and disabled otherwise. A later reload of just the extract style (an extraction style change) does not re-apply this default, so a user's `extract do|no fringehalo` choice is kept. See the `extract-*` capabilities.

Because the format version is read when the `extract` section is finalized, a `tech` section that appears after the `extract` section does not affect the fringe-halo default for that load. The value then in effect is the one set by the previous technology load, or 27 if it was reset by an earlier `tech` section init (see Q-TECH-32).

Formats 28 to 35 change no other parsing rule in the reference version. Older syntax restrictions mentioned in the manual, such as one style per line in format 27 and earlier, are not enforced.

(Magic ref: utils/tech.h:40, database/DBtech.c:78, database/DBtech.c:113-119, extract/ExtTech.c:64, extract/ExtTech.c:1133, extract/ExtTech.c:3580-3587, utils/tech.c:524)

#### Scenario: Format 34 enables fringe halo
- **GIVEN** a technology with `format 34` declared in a `tech` section that precedes `extract`
- **WHEN** it is loaded with `tech load`
- **THEN** the extraction option fringehalo is on

#### Scenario: Format 33 disables fringe halo
- **GIVEN** a technology with `format 33`
- **WHEN** it is loaded
- **THEN** the extraction option fringehalo is off

### Requirement: Complete annotated example

Hefesto SHALL accept the following file and produce the stated results. The stated results are the normative anchor for the other `tech-*` capabilities.

Compatibility: exact

```
# tiny.tech -- minimal annotated example          (comment line)
tech
    format 35                     # sets format version 35 (the '#' is a token here; see below)
    tiny
end
```

The `tech` section above is NOT valid as written. `format 35 # sets ...` has 6 tokens, so it is reported as `Badly formed technology name`. A valid file:

```
tech
    format 35
    tiny
end

version
    version 1.0
    description "Tiny demo process"
end

planes
    well,w
    active,a
    metal1,m1
    metal2,m2
end

types
    well    nwell,nw
    active  polysilicon,poly,p
    active  ndiffusion,ndiff
    active  ntransistor,nfet
    metal1  metal1,m1
    metal2  metal2,m2
    active  polycontact,pcontact,pc
    active  ndcontact,ndc
    metal1  m2contact,m2c,via
    -well   pwell,pw
end

contact
    pc   poly  metal1
    ndc  ndiff metal1
    m2c  metal1 metal2
    stackable pc m2c pm12c
end

aliases
    allm1  *m1
end

styles
    styletype mos
    poly 1
    ndiff 2
    nfet 6 7
    metal1 20
    metal2 21
    pc 1 20 32
end

compose
    compose nfet poly ndiff
end

connect
    poly,pc      poly,pc
    ndiff,ndc    ndiff,ndc
    *m1          *m1
    m2,m2c       m2,m2c
end

cifoutput
end

cifinput
end

drc
end

extract
end
```

Loading this file gives the following results. The planes, types, contacts and paint results are defined in the capabilities named in the Purpose.
- Technology name `tiny`, format version 35, version `1.0`, description `Tiny demo process`.
- Planes: 0 router, 1 designRuleCheck, 2 designRuleError, 3 mhint, 4 fhint, 5 rhint, 6 well, 7 active, 8 metal1, 9 metal2.
- User types: 9 nwell, 10 polysilicon, 11 ndiffusion, 12 ntransistor, 13 metal1, 14 metal2, 15 polycontact, 16 ndcontact, 17 m2contact, 18 pwell (locked). Then the stacked type 19 `pc+m2c`, with alias `pm12c`.

(Magic ref: utils/tech.c:425-870, scmos/minimum.tech.in)

#### Scenario: Annotated example loads cleanly
- **GIVEN** the valid `tiny.tech` above, with `mos` display style files available
- **WHEN** it is loaded
- **THEN** the load succeeds with the results listed above
- **AND** `tech layers` lists the types 9..18 and the built-ins magnet, fence, rotate (see capability `tech-command`)
