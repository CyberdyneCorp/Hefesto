# OASIS Command

## Purpose
Defines the `oasis` command: Hefesto's command-level API for reading and writing OASIS files. Its options mirror the Magic `gds` (`calma`) command, so that scripts written for GDS carry over by changing only the command name. It also defines the OASIS-only options and how Hefesto recognises OASIS files when they are opened by other means.

The semantics of each option are owned by capabilities `oasis-read` and `oasis-write`, which in turn refer to `gds-read` and `gds-write` for the shared options. Command-line splitting, prefix matching of command names and dispatch are owned by capability `ui-command-dispatch`. The scripting binding is owned by capability `hefesto-scripting`.

## Requirements

### Requirement: Command name and forms
Hefesto SHALL provide a global command `oasis` with these forms:

Compatibility: native

```ebnf
oasis-cmd = "oasis"
          | "oasis", "read", file
          | "oasis", "write", file
          | "oasis", option, [ value ]
          | "oasis", "help" ;
```

- `oasis` with no arguments writes the root cell of the current layout view, with the bare-command naming rule of capability `oasis-write`.
- `oasis read <file>` reads as specified in capability `oasis-read`.
- `oasis write <file>` writes as specified in capability `oasis-write`. Any number of arguments other than exactly one file name is an argument error, and nothing is written.
- `oasis help` lists the options with a one-line description each. The text is informative.
- `oasis` has no alias. In particular, `calma` and `gds` SHALL NOT read or write OASIS.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Bare command
- **GIVEN** the root cell of the window is `top`
- **WHEN** `oasis` is issued
- **THEN** `top.oas` is written

#### Scenario: Write with two names
- **WHEN** `oasis write a b` is issued
- **THEN** an argument error is reported and no file is written

### Requirement: Option syntax
The `oasis` options SHALL follow the conventions of the `gds` command:

Compatibility: behavioral

1. The option keyword and enumerated values are matched by unique prefix, as for `gds`. An ambiguous or unknown keyword is an error, and nothing changes.
2. Boolean values accept `no|false|off|0` (false) and `yes|true|on|1` (true).
3. `allow|disallow` values accept `disallow|0` and `allow|1`.
4. An option given without a value reports its current value and changes nothing.
5. An invalid value is an argument error, and the setting is unchanged.
6. Settings persist for the session and apply to later reads and writes.

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: commands/CmdCD.c:131-1075)

#### Scenario: Prefix match
- **WHEN** `oasis nodup t` is issued
- **THEN** `noduplicates` is enabled

#### Scenario: Query current value
- **GIVEN** `oasis flatten 5` was issued earlier
- **WHEN** `oasis flatten` is issued
- **THEN** the current state (enabled, limit 5) is reported and nothing changes

### Requirement: Options shared with the gds command
The `oasis` command SHALL accept every option of the `gds` command listed below, with the same value syntax and default. Each option's effect is defined by `oasis-read` or `oasis-write`:

Compatibility: behavioral

| Option | Values | Default | Applies to |
|---|---|---|---|
| `abstract` | allow/disallow | disallow | write |
| `addendum` | bool | no | write |
| `arrays` | bool | no | write |
| `contacts` | bool | no-op | write (accepted; `yes` reports "unimplemented") |
| `datestamp` | `yes`, `no`, integer | `yes` | read (write: no effect, OASIS has no dates) |
| `nodatestamp` | bool | `no` | read (alias of `datestamp no` / `yes`) |
| `drccheck` | bool | `yes` | read |
| `flatten` | bool or integer limit | `no`, limit 10 | read |
| `flatglob` | pattern or `none` | `*_CDNS_*` | read |
| `labels` | bool | `yes` | write |
| `library` | bool | `no` | write |
| `lower` | bool | `yes` | write |
| `magscale` | float or `default` | 1.0 | accepted, no effect on OASIS (no text magnification) |
| `maskhints` | bool or comma list of input layers | `no` | read |
| `merge` | bool | `no` | write |
| `noduplicates` | bool | `no` | read |
| `ordering` | bool | `off` | read |
| `polygon subcells` | `none`/`temporary`/`keep`/bool | `none` | read |
| `path subcells` | bool | `no` | read |
| `readonly` | bool | `no` | read |
| `rescale` | bool | `yes` | read (`no` also sets `warning limit`, as for `gds`) |
| `savepaths` | bool | `no` | read |
| `undefined` | allow/disallow | disallow | write |
| `unique` | bool | `no` | read |
| `warning` | `default`/`none`/`align`/`limit`/`redirect [file]` | `default` | read |
| `compress` | integer 0–9 | **6** (differs from `gds`) | write: CBLOCK DEFLATE level; 0 disables CBLOCKs |

Values of `compress` outside 0–9 are rejected with an error, and the setting is kept, as for `gds`. OASIS output is never gzip-wrapped (see Q-OASIS-91).

(Hefesto decision: docs/decisions/product.md#P-6; Magic ref: commands/CmdCD.c:97-125, commands/CmdCD.c:232-1008)

#### Scenario: Compression out of range
- **GIVEN** `oasis compress 3`
- **WHEN** `oasis compress 12` is issued
- **THEN** an error is reported and the level stays 3

#### Scenario: Rescale implies warning limit
- **GIVEN** `oasis warning default`
- **WHEN** `oasis rescale no` is issued
- **THEN** the `oasis` warning level becomes `limit`

### Requirement: OASIS-only options
The `oasis` command SHALL accept these additional options:

Compatibility: native

| Option | Values | Default | Effect |
|---|---|---|---|
| `strict` | bool | `yes` | write a strict-mode file with name tables (`oasis-write` "Strict and non-strict modes") |
| `validation` | `none`, `crc32`, `checksum` | `crc32` | END validation scheme on write |
| `repetitions` | bool | `yes` | shape repetition detection on write |
| `layernames` | bool | `yes` | write LAYERNAME records |
| `circlepoints` | integer 8–4096 | 64 | vertices used to approximate CIRCLE on read |

An out-of-range `circlepoints` is an argument error, and the setting is unchanged.

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Non-strict output
- **GIVEN** `oasis strict no`
- **WHEN** a cell `top` is written
- **THEN** the file has no name tables, and the CELL record carries the name `top` inline

#### Scenario: Circle points bound
- **WHEN** `oasis circlepoints 4` is issued
- **THEN** an argument error is reported and the value stays 64

### Requirement: Settings independent of the gds command
The settings of the `oasis` command SHALL be stored separately from those of the `gds` command. Changing a `gds` option does not change the `oasis` option of the same name, and the reverse is also true. Both start from the defaults of their own tables. The input and output styles (`cif istyle`, `cif ostyle`) and the `cif` hierarchy options are shared, because they belong to the technology rather than to the file format. See Q-OASIS-90 for the alternative of shared settings.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Independent flatten setting
- **GIVEN** `gds flatten yes` and no `oasis flatten` command issued
- **WHEN** an OASIS file is read
- **THEN** flattening is off for the OASIS read

### Requirement: Format recognition on open
When a layout file is opened by any means other than the `gds` or `oasis` commands (the platform document picker, drag and drop, an iPadOS Files "Open in" request, or the scripting API's generic open), Hefesto SHALL recognise OASIS by its first 13 bytes `%SEMI-OASIS\r\n`, after gzip decompression if the file is gzip-wrapped, whatever its extension. It SHALL then read the file with `oasis read` semantics and the current `oasis` settings. A file that is neither OASIS, GDS nor a Hefesto/Magic cell file is reported as an unknown format.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: OASIS with a misleading extension
- **GIVEN** a file `chip.gds` whose content is OASIS
- **WHEN** it is opened from the iPadOS Files app
- **THEN** Hefesto reads it as OASIS

### Requirement: Scripting access
Every form of the `oasis` command SHALL be available through Hefesto's scripting interface (capability `hefesto-scripting`) with the same arguments, the same results and the same errors. A query form returns the current value as the command's result instead of printing it.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-6)

#### Scenario: Script query
- **GIVEN** default settings
- **WHEN** a script evaluates the command `oasis compress`
- **THEN** the result is `6`
