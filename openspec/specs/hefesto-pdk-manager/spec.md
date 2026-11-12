# Hefesto PDK Manager

## Purpose
Defines the in-app PDK manager. On every platform, users browse, download, install, update, pin and remove open PDKs from inside Hefesto. On iPadOS this is the only way to get a PDK. On Linux and macOS, the manager's install layout is the same as the one used by `ciel` (and its predecessor `volare`). PDKs installed by Hefesto therefore work with LibreLane, ciel and Magic through `PDK_ROOT`, and PDKs installed by ciel appear in Hefesto.

The store that holds and resolves installed PDKs is capability `hefesto-files-and-pdks`. Script trust is capability `hefesto-security`. The project file is capability `hefesto-settings`. The PDK startup contract is capability `scripting-pdk-integration`.

Reference sources (read-only, never executed):
- ciel `fossi-foundation/ciel` @ `d2eb3cf1c43f0f8522e93c537f8fca50781216d5`, cited as `ciel ref:`. Paths are relative to the repository root.
- volare `efabless/volare` @ `13cf10af24965452f0421fbd2823e2b2d0016ab7`, cited as `volare ref:`.

## Requirements

### Requirement: PDK families and variants
The PDK manager SHALL organise PDKs as families, and each family SHALL contain one or more variants. Every variant SHALL belong to exactly one family. The built-in family table SHALL be:

| family | variants (default first) | upstream identity |
|---|---|---|
| `sky130` | `sky130A` (default), `sky130B` | open_pdks commit |
| `gf180mcu` | `gf180mcuD` (default), `gf180mcuA`, `gf180mcuB`, `gf180mcuC` | open_pdks commit |
| `ihp-sg13` | `ihp-sg13g2` (default), `ihp-sg13cmos5l` | IHP-Open-PDK commit |

A selector string SHALL be resolved as follows:
1. If it is a variant name, it resolves to that variant and its family.
2. If it is a family name, it resolves to the family. Where a single variant is needed, it resolves to the family's default variant.
3. The legacy selector `ihp_sg13g2` resolves to family `ihp-sg13`.
4. Anything else is an error that names the selector.

Families that are not manufacturable, such as ciel's `freepdk45`, MAY be listed. They SHALL be marked "not manufacturable" and SHALL be excluded from tapeout program profiles (capability `tapeout-shuttle-packages`).

Compatibility: behavioral

(ciel ref: ciel/families.py:117-260, ciel/families.py:262-311)

#### Scenario: Family selector picks default variant
- **WHEN** the user asks to install `gf180mcu` without naming a variant
- **THEN** the manager SHALL select variant `gf180mcuD`

#### Scenario: Legacy IHP selector
- **WHEN** a project file names PDK family `ihp_sg13g2`
- **THEN** it SHALL resolve to family `ihp-sg13`

#### Scenario: Unknown selector
- **WHEN** the selector `sky90` is given
- **THEN** the manager SHALL report that `sky90` is not a valid PDK family or variant, and SHALL change nothing

### Requirement: Library sets per variant
Every family SHALL declare which cell libraries each variant can have and which of them are installed by default. A library request SHALL be resolved as follows:
- `all` selects every library available for the variant
- `default` selects the default set
- a library name selects that library

A name that the variant does not have SHALL be an error. When no libraries are requested, `default` SHALL apply. The built-in default sets SHALL be:
- `sky130A`: `sky130_fd_io`, `sky130_fd_pr`, `sky130_fd_sc_hd`, `sky130_fd_sc_hvl`, `sky130_ml_xx_hd`, `sky130_sram_macros`
- `sky130B`: the `sky130A` set plus `sky130_fd_pr_reram`
- `gf180mcu*`: `gf180mcu_fd_io`, `gf180mcu_fd_pr`, `gf180mcu_fd_sc_mcu7t5v0`, `gf180mcu_fd_sc_mcu9t5v0`, `gf180mcu_fd_ip_sram`
- `ihp-sg13g2`: `sg13g2_io`, `sg13g2_pr`, `sg13g2_sram`, `sg13g2_stdcell`
- `ihp-sg13cmos5l`: `sg13cmos5l_io`, `sg13cmos5l_sram`, `sg13cmos5l_stdcell`

The family table and library sets SHALL be data in the catalog (see Requirement: PDK catalog). They SHALL NOT be compiled-in constants only, so that a catalog update can add a library.

Compatibility: behavioral

(ciel ref: ciel/families.py:59-95, ciel/families.py:117-207)

#### Scenario: Default set for sky130B
- **WHEN** sky130B is installed with no library list
- **THEN** the installed `libs.ref` SHALL contain `sky130_fd_pr_reram` in addition to the six sky130A defaults

#### Scenario: Unknown library
- **WHEN** library `sky130_fd_sc_xx` is requested for sky130A
- **THEN** the request SHALL fail with an error naming the unknown library, before any download starts

### Requirement: Version identity and ordering
A PDK version SHALL be identified by the full 40-hex-digit commit hash of the upstream repository that defines the build:
- open_pdks for `sky130` and `gf180mcu`
- IHP-Open-PDK for `ihp-sg13`

Each version SHALL carry:
- an optional commit date (UTC, ISO 8601 `YYYY-MM-DDTHH:MM:SSZ`)
- an optional upload date
- a prerelease flag

Versions SHALL be ordered by commit date, newest first. A version with no commit date SHALL sort as the oldest. Wherever a short form is shown, the UI SHALL show the hash's first 7 digits and the date as `YYYY.MM.DD`. Prerelease versions SHALL NOT be offered as updates unless the user turns on prereleases.

Compatibility: behavioral

(ciel ref: ciel/common.py:76-99, ciel/source.py:79-114)

#### Scenario: Ordering with missing date
- **GIVEN** remote versions a (2025-01-02), b (no date) and c (2026-03-01)
- **WHEN** they are listed
- **THEN** the order SHALL be c, a, b

### Requirement: PDK catalog
Hefesto SHALL obtain the list of available PDK versions from a catalog. The catalog SHALL consist of JSON documents:
- `<base>/<family>/manifest.json` = `{"pdk": <family>, "versions": [{"version": <hash>, "date": <ISO8601>, "prerelease": <bool, optional>}...]}`
- `<base>/<family>/<hash>/manifest.json` = `{"assets": [{"content": <"common" | library name>, "filename": <name>.tar.zst, "url": <https URL>, "sha256": <64 hex>, "size": <bytes>}...]}`

These are the same as ciel's static-web data source, with `sha256` and `size` added. A reader SHALL ignore unknown keys. The default catalog SHALL be a Hefesto-operated mirror. The catalog SHALL also be able to read any ciel static-web base URL, with ciel's default being `https://fossi-foundation.github.io/ciel-releases`. On desktop it SHALL also read GitHub-releases sources (see Requirement: GitHub releases source compatibility). The catalog index SHALL be signed by Hefesto's catalog key (see Q-TAPEOUT-3). A catalog whose signature does not verify SHALL be shown as "unverified", and its PDKs SHALL install only after explicit user consent.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-14)

#### Scenario: Browse catalog
- **WHEN** the user opens the PDK manager while online
- **THEN** it SHALL list every family with its variants, the newest stable version, its date, the download size of the default library set and its license, and SHALL mark which versions are installed and which one each open project pins

#### Scenario: Unverified catalog
- **GIVEN** the user adds a custom ciel static-web base URL without a Hefesto signature
- **WHEN** a version from it is installed
- **THEN** the manager SHALL warn that the source is unverified and SHALL require confirmation before downloading

### Requirement: GitHub releases source compatibility
On Linux and macOS, the manager SHALL be able to use GitHub releases as a source, as ciel does:
- Only non-draft releases count.
- A release tag SHALL have the form `<family-or-variant>-<hash>`. It SHALL be split at the last `-`. Releases whose prefix is neither the family nor one of its variants SHALL be ignored.
- The commit date SHALL be taken from the first match of `released on ([\d\-:TZ]+)` in the release body.
- The release `published_at` SHALL be the upload date.
- `prerelease` SHALL come from the release.
- Assets SHALL be the release assets whose names end in `.tar.zst`. Each asset's content name SHALL be its file name without the `.tar.zst` suffix.
- To fetch a version, the tag `<family>-<hash>` SHALL be tried first. If that returns HTTP 404, `<variant>-<hash>` SHALL be tried for each variant in table order.

When a GitHub asset carries a published SHA-256 digest, that digest SHALL be used. Otherwise the requirement for integrity verification (see Requirement: Download integrity verification) SHALL apply.

Compatibility: behavioral

(ciel ref: ciel/source.py:54-153)

#### Scenario: Renamed IHP release tag
- **GIVEN** a release tagged `ihp-sg13g2-0123456789abcdef0123456789abcdef01234567` exists and no `ihp-sg13-…` tag exists
- **WHEN** that version of family `ihp-sg13` is fetched
- **THEN** the manager SHALL find the release through the variant tag

### Requirement: Download integrity verification
Every downloaded asset SHALL be verified before it is unpacked:
- Its byte length SHALL equal the catalog `size`.
- Its SHA-256 SHALL equal the catalog `sha256`.

On any mismatch the manager SHALL:
- delete the downloaded file
- install nothing from that version
- report which asset failed, with the expected and actual values

An asset with no known SHA-256 SHALL NOT be installed silently. The manager SHALL either obtain the digest from the signed Hefesto catalog or ask the user to accept an unverified install. Downloads SHALL use HTTPS with platform certificate validation. Downloads SHALL be resumable (HTTP range requests), and the resumed file SHALL be verified in full. HTTPS goes through URLSession on macOS and iPadOS and libcurl on Linux; SHA-256 and the Ed25519 check of the signed catalog go through CryptoKit on Apple platforms and libsodium on Linux (capability `hefesto-licensing`, requirement "Third-party libraries for cryptography, networking and data formats"; decision D-13).

Note: ciel itself does not verify a digest of the release tarballs. A version hash identifies the source commit, not the archive content. See Q-TAPEOUT-2.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-14)

#### Scenario: Corrupted asset
- **GIVEN** the catalog lists `common.tar.zst` with sha256 `ab…01`
- **WHEN** the downloaded file hashes to `cd…02`
- **THEN** nothing SHALL be written under the version directory, and the error SHALL name `common.tar.zst` with both digests

#### Scenario: Resume after network loss
- **GIVEN** 60% of an asset was downloaded when the connection dropped
- **WHEN** the download resumes
- **THEN** it SHALL continue from the stored offset, and the whole file SHALL then be hashed and verified

### Requirement: Safe archive unpacking
Assets SHALL be Zstandard-compressed tar archives, decompressed with the zstd library (BSD-3-Clause; capability `hefesto-licensing`, decision D-13). Unpacking SHALL treat them as untrusted input (capability `hefesto-security`):
- Directory entries SHALL be skipped. Directories SHALL be created as needed for regular files.
- Every regular file SHALL be written to `<version_dir>/<entry name>`.
- An entry name that is absolute, contains a `..` component, or resolves outside the version directory SHALL abort the install.
- Symbolic links, hard links, device files and FIFOs SHALL NOT be created.
- The total unpacked size SHALL be bounded by a configured limit, with a default of 4× the catalog size of the asset.

Unpacking SHALL go to a staging directory next to the version directory. The staging directory SHALL be renamed into place only after every requested asset has unpacked. On failure or cancellation, every path the install created SHALL be removed and nothing partial SHALL remain.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-14)

#### Scenario: Path traversal entry
- **WHEN** an archive contains the entry `../../.profile`
- **THEN** the install SHALL abort, report the entry, and leave no new files

#### Scenario: Cancelled install
- **WHEN** the user cancels after the first of three assets has unpacked
- **THEN** the version directory SHALL not exist afterwards

### Requirement: Install layout compatible with ciel
On Linux and macOS, the PDK root SHALL be resolved in this order:
1. an explicit root chosen in Hefesto settings
2. the `PDK_ROOT` environment variable
3. `~/.ciel`

The resolved root SHALL be shown in the manager. Under that root, installs SHALL use exactly ciel's layout:

```
<root>/ciel/<family>/versions/<hash>/<variant>/libs.tech/...
<root>/ciel/<family>/versions/<hash>/<variant>/libs.ref/<library>/...
<root>/ciel/<family>/versions/<hash>/<variant>/SOURCES
<root>/ciel/<family>/current            # text file: the enabled <hash>
<root>/<variant> -> ciel/<family>/versions/<hash>/<variant>   (symlink, one per variant)
```

`SOURCES` SHALL be created for each installed variant when it is absent. It SHALL contain the single line `<repo-name> <hash>\n`, where `<repo-name>` is `open_pdks` or `IHP-Open-PDK`.

A version SHALL count as installed when its version directory exists. Its common part SHALL count as present when `<variant0>/libs.tech` exists, where `variant0` is the first variant in the family table. A library SHALL count as present when `libs.ref/<library>` exists under any variant.

Hefesto SHALL also detect installs in the legacy volare layout `<root>/volare/<family>/versions/<hash>/…` with its own `current` file. It SHALL list them as read-only and SHALL NOT modify them.

Compatibility: behavioral

(ciel ref: ciel/common.py:45-148, ciel/manage.py:150-280; volare ref: volare/common.py:44-66)

#### Scenario: Shared root with ciel
- **GIVEN** ciel has installed sky130 version `0fe599b2afb6708d281543108caf8310912f54af` into `~/.ciel`, with `current` naming it
- **WHEN** Hefesto starts without `PDK_ROOT` set
- **THEN** the manager SHALL list that version as installed and enabled, without downloading anything

#### Scenario: SOURCES file
- **WHEN** sky130 version `0fe599b2afb6708d281543108caf8310912f54af` is installed
- **THEN** `<root>/ciel/sky130/versions/0fe599b…/sky130A/SOURCES` SHALL contain `open_pdks 0fe599b2afb6708d281543108caf8310912f54af` followed by a newline

### Requirement: Incremental library downloads
The catalog asset `common` SHALL hold `libs.tech` and any shared data. Each other asset SHALL hold one library. When a version is requested:
- If the common part is missing, the manager SHALL download `common` plus every requested library.
- Otherwise, it SHALL download only the requested libraries that are missing.

Adding a library to an installed version SHALL NOT re-download `common`. Removing a library SHALL delete `libs.ref/<library>` under every variant of that version.

Compatibility: behavioral

(ciel ref: ciel/manage.py:150-200)

#### Scenario: Add a library later
- **GIVEN** sky130A is installed with the default libraries
- **WHEN** the user adds `sky130_fd_sc_hs`
- **THEN** only the `sky130_fd_sc_hs` asset SHALL be downloaded

### Requirement: Enabling a version
Enabling version V of family F SHALL do the following:
1. Install V if it is missing.
2. For each variant of F, create or replace the symlink `<root>/<variant>` that points to `ciel/F/versions/V/<variant>`.
3. Write V to `<root>/ciel/F/current`.

If `<root>/<variant>` exists and is not a symlink, enabling SHALL fail with an error that names the path, and SHALL change nothing.

Enabling SHALL be a global, user-visible action. Opening or pinning a project SHALL NOT enable a version. Projects resolve their pinned version directly (see Requirement: Per-project version pinning). This way Hefesto never changes what an external ciel or LibreLane run sees without asking.

Compatibility: behavioral

(ciel ref: ciel/manage.py:282-350)

#### Scenario: Non-symlink conflict
- **GIVEN** `<root>/sky130A` is a real directory
- **WHEN** a sky130 version is enabled
- **THEN** enabling SHALL fail with a message to remove `<root>/sky130A`, and `current` SHALL be unchanged

### Requirement: Per-project version pinning
A project (capability `hefesto-settings`, `hefesto.project`) SHALL pin:
- the PDK family
- the variant
- the full version hash
- the library set

When a project is opened, Hefesto SHALL load the technology from `ciel/<family>/versions/<hash>/<variant>`. It SHALL NOT use `<root>/<variant>` and SHALL NOT use whichever version is enabled. If the pinned version or a pinned library is not installed, Hefesto SHALL offer to install exactly that version. It SHALL NOT substitute another version unless the user explicitly re-pins. A new project SHALL pin the newest installed stable version of the chosen variant.

On desktop, scripts and child processes that Hefesto starts for a project SHALL receive `PDK_ROOT=<root>` and `PDK=<variant>`. When the pinned version is not the enabled one, they SHALL receive a private view instead (see Q-TAPEOUT-14).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-14)

#### Scenario: Pinned version differs from enabled
- **GIVEN** project P pins sky130A `aaaa…`, and `current` for sky130 is `bbbb…`
- **WHEN** P is opened
- **THEN** the technology SHALL be loaded from `ciel/sky130/versions/aaaa…/sky130A`

#### Scenario: Pinned version missing
- **GIVEN** project P pins gf180mcuD `cccc…`, which is not installed
- **WHEN** P is opened
- **THEN** Hefesto SHALL offer to download `cccc…`, and SHALL open the layout read-only with the `minimum` tech if the user declines

### Requirement: Updates
The manager SHALL check the catalog for newer stable versions when the user asks. It SHALL also check automatically at most once per 24 hours while online, and the user can turn this off. An available update SHALL be shown per family, with the dates and short hashes of the installed and offered versions.

Installing an update SHALL install the new version next to the old one. It SHALL NOT change any project pin. Re-pinning a project SHALL be an explicit action. Before the pin changes, Hefesto SHALL warn that DRC, extraction and sign-off results recorded for the project become stale (capability `tapeout-signoff-checklist`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-14)

#### Scenario: Update does not move pins
- **GIVEN** project P pins sky130A `aaaa…`
- **WHEN** the user installs newer version `dddd…`
- **THEN** P SHALL still open with `aaaa…`

### Requirement: Uninstall and cleanup
Uninstalling a version SHALL do the following:
1. If the version is current, remove the `<root>/<variant>` symlinks and the `current` file.
2. Remove the version directory.

The manager SHALL list each installed version with:
- its disk size
- the projects known to pin it (from the recent-projects list)
- whether it is current

It SHALL refuse to uninstall a version that a known project pins, unless the user confirms. It SHALL offer to remove versions that are neither current nor pinned. Uninstalling a version that is not installed SHALL report an error.

Compatibility: behavioral

(ciel ref: ciel/common.py:101-126)

#### Scenario: Uninstall current version
- **GIVEN** sky130 `aaaa…` is current
- **WHEN** it is uninstalled
- **THEN** `<root>/sky130A`, `<root>/sky130B` and `<root>/ciel/sky130/current` SHALL no longer exist

### Requirement: Optional file deduplication
The manager MAY deduplicate identical files within one installed version, as ciel's optimize step does:
1. Walk every variant directory.
2. Hash each regular file with SHA-256.
3. Replace each later file whose digest equals an earlier file's digest with a relative symlink to that earlier file.

Deduplication SHALL be off by default on iPadOS (see Requirement: iPadOS sandbox storage). It SHALL NOT cross version boundaries.

Compatibility: behavioral

(ciel ref: ciel/manage.py:352-383)

#### Scenario: Deduplicated variants
- **GIVEN** `sky130A/libs.ref/sky130_fd_sc_hd/gds/x.gds` and `sky130B/libs.ref/sky130_fd_sc_hd/gds/x.gds` are byte-identical
- **WHEN** deduplication runs
- **THEN** the second SHALL become a relative symlink to the first, and both paths SHALL read the same bytes

### Requirement: Offline PDK packs
Hefesto SHALL export and import offline packs. An offline pack is a single file `<family>-<hash>.hefestopdk`. It SHALL be an uncompressed tar archive that contains:
- `pack.json`, which holds:
  - `family`, `variants`, `version`, `date`
  - `libraries`
  - `assets`, with `content`, `filename`, `sha256` and `size` for each asset
  - `license` (SPDX identifiers)
  - `source` (catalog URL or "local")
  - an optional Hefesto catalog signature over the `assets` list
- the asset `.tar.zst` files named in `pack.json`

Importing a pack SHALL verify every asset against `pack.json`, and then install as for a download. A pack whose signature verifies against the Hefesto catalog key SHALL be treated like a catalog install. Any other pack SHALL be untrusted for script execution until the user consents (capability `hefesto-security`). Exporting SHALL rebuild the assets from the installed files only when the original assets are no longer cached. The exported `pack.json` SHALL record that the assets were rebuilt, and rebuilt assets SHALL NOT carry the catalog signature.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-14)

#### Scenario: Air-gapped workstation
- **GIVEN** a pack exported on an online machine is copied to an offline Linux machine
- **WHEN** it is imported
- **THEN** the version SHALL install with no network access, and projects that pin it SHALL open

#### Scenario: Tampered pack
- **WHEN** one asset in a pack was modified after export
- **THEN** import SHALL fail on that asset's SHA-256 check

### Requirement: iPadOS sandbox storage
On iPadOS, the PDK root SHALL be `Application Support/PDKs/` inside the app container. It SHALL use the same `ciel/<family>/versions/<hash>/<variant>` layout. The per-variant symlinks and the `current` file SHALL be kept, because they are internal to the container. Installed PDKs SHALL be:
- excluded from iCloud backup by default, because they can be downloaded again
- never placed in `Caches`, which the system may purge

Before a download, the manager SHALL check free space for the asset size plus the unpacked estimate. It SHALL refuse to start when space is short. Downloads SHALL use a background transfer session, so that an install continues when the app is suspended. Unpacking SHALL resume when the app next runs. PDK files SHALL NOT be exposed in the Files app. Offline packs SHALL be imported through the document picker. The default library set on iPadOS is open (see Q-TAPEOUT-17). Script execution from installed PDKs SHALL follow capability `hefesto-scripting`, including its App Store fallback.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-14)

#### Scenario: Low storage
- **GIVEN** 1.2 GB is free and the sky130A default set needs 2.5 GB unpacked
- **WHEN** the user taps Install
- **THEN** the manager SHALL refuse, state both sizes, and offer a smaller library selection

#### Scenario: Install continues in background
- **WHEN** the user switches apps during a download
- **THEN** the download SHALL continue, and the install SHALL complete when Hefesto is next in the foreground

### Requirement: Licenses and provenance
Before the first install of a family, the manager SHALL show the upstream license of each selected asset and SHALL record that the user accepted it. The upstream PDKs are Apache-2.0, to be confirmed per pack (capability `hefesto-licensing`). Each version SHALL keep its upstream LICENSE and NOTICE files. For each installed version, the manager SHALL show:
- source repository
- commit hash and date
- catalog URL
- per-asset SHA-256

The catalog SHALL list only PDKs whose license allows redistribution. PDKs under NDA SHALL NOT be offered.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-14)

#### Scenario: Provenance view
- **WHEN** the user opens details for an installed gf180mcu version
- **THEN** the manager SHALL show `open_pdks`, the full hash, the commit date, the catalog URL and the SHA-256 of every installed asset

### Requirement: Script trust for managed PDKs
A version installed from the signed Hefesto catalog, or from a signed offline pack, SHALL be recorded as trusted for that exact version in the PDK store (capability `hefesto-security`). A version installed from an unverified source, found in an external `PDK_ROOT`, or imported from an unsigned pack SHALL require the one-time consent prompt before its startup or device scripts run. Updating to a new version SHALL NOT carry trust over from the old version unless the new version also comes from a signed source. The record SHALL keep the date the user first trusted the version (removed when trust is revoked), and the PDK panel SHALL show it with the trusted state.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-14)

#### Scenario: Externally installed version
- **GIVEN** a sky130A version installed by ciel, outside Hefesto
- **WHEN** a layout that uses it is opened for the first time
- **THEN** Hefesto SHALL ask for consent before running `sky130A.magicrc`

### Requirement: Headless and scripted operation
On desktop, every manager operation SHALL be available headless and from the scripting API, with the same semantics as the UI:
- list remote versions
- list installed versions
- install
- enable
- pin
- uninstall
- export or import a pack

Each operation SHALL return structured results (capability `hefesto-diagnostics`) and SHALL exit non-zero on failure. A headless install SHALL never prompt. An unverified source or a license that needs acceptance SHALL make the operation fail, unless an explicit acceptance flag is given.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-14)

#### Scenario: CI install
- **WHEN** a CI job runs a headless install of sky130A at a pinned hash, with the license-acceptance flag
- **THEN** the install SHALL complete without prompts, and the process SHALL exit 0
