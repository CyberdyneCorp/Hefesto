# Plugin Packaging and Distribution

## Purpose
Defines how Hefesto plugins are packaged, described, signed, found, installed, updated and removed (decision P-20). It covers:
- the `.hfplugin` package and its `plugin.json` manifest
- signatures and trust levels
- version and dependency compatibility
- discovery locations
- the optional plugin catalog
- per-project plugin requirements
- preserving plugin data when a plugin is missing

Permissions and consent are in capability `plugin-security-and-platforms`. The API is in capability `plugin-api`.

## Requirements

### Requirement: Package format
A plugin SHALL be distributed as a `.hfplugin` file. The file is a ZIP archive (stored or deflate entries only, with no encryption and no ZIP64 extensions beyond what the size requires), laid out as follows:

```
plugin.json                      manifest (required)
SIGNATURE                        detached signature (required except in developer mode)
LICENSE                          license text (required)
README.md                        optional
bin/linux-x86_64/<lib>.so        native code per platform (native plugins)
bin/linux-aarch64/<lib>.so
bin/macos-arm64/<lib>.dylib
scripts/...                      Tcl sources (script plugins)
resources/...                    icons, data files, panel images
```

Entry names SHALL be relative UTF-8 paths using `/`. Unpacking SHALL follow the safe-unpacking rules of capability `hefesto-pdk-manager`. These rules reject:
- absolute paths
- `..` components
- links
- duplicate names after case folding
- an entry count above 10 000
- an uncompressed total above 1 GiB

A native package MAY contain only some platforms. It is installable only where its `bin/<platform>/` entry exists.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Path traversal rejected
- **GIVEN** a `.hfplugin` with an entry `../../.zshrc`
- **WHEN** it is installed
- **THEN** installation SHALL fail with `PLUGIN-PKG-002`, and nothing SHALL be written outside the staging directory

### Requirement: Manifest format
`plugin.json` SHALL be a UTF-8 JSON object (RFC 8259) with these keys:

| Key | Type | Required | Rule |
|---|---|---|---|
| `schema` | int | yes | Manifest schema version; this spec defines `1` |
| `id` | string | yes | Reverse-DNS: `^[a-z][a-z0-9-]*(\.[a-z0-9-]+){2,}$`, at most 128 characters |
| `name` | string | yes | Display name, 1–64 characters |
| `version` | string | yes | SemVer 2.0.0 |
| `prefix` | string | yes | Command prefix: `^[a-z][a-z0-9_]{1,31}$` |
| `kind` | string | yes | `native` or `script` |
| `api` | object | yes | `{"major": int, "minorMin": int}` (capability `plugin-api`) |
| `hefesto` | string | no | SemVer range of host versions, default `*` |
| `platforms` | array | native: yes | Subset of `linux-x86_64`, `linux-aarch64`, `macos-arm64` (macOS is Apple silicon only, decision D-12); script plugins MAY omit it, meaning every platform including `ipados-arm64` |
| `entry` | object or string | yes | Native: `{"<platform>": "bin/<platform>/<lib>"}`. Script: the path of the entry `.tcl` file |
| `provides` | array | yes | Extension-point ids declared (capability `plugin-api`, Requirement "Extension points") |
| `permissions` | array | yes | Permission strings (capability `plugin-security-and-platforms`); may be empty |
| `dependencies` | array | no | `[{"id": string, "version": SemVer range}]` |
| `license` | string | yes | SPDX license expression |
| `authors` | array | no | Strings |
| `homepage` | string | no | `https` URL |
| `description` | string | no | At most 1000 characters |

SemVer ranges SHALL use the npm range grammar subset: `*`, `x.y.z`, `^x.y.z`, `~x.y.z`, comparators `>= > <= <` joined by spaces (AND), and `||` (OR).

Validation:
- A manifest whose `schema` is greater than the host supports SHALL be refused with `PLUGIN-MAN-001`.
- Unknown top-level keys SHALL produce a `warning` and be ignored.
- A missing required key, a wrong type, or a value that breaks a rule SHALL be refused with `PLUGIN-MAN-002`, naming the key.

Example:

```json
{
  "schema": 1,
  "id": "org.example.viafix",
  "name": "Via Fixer",
  "version": "1.2.0",
  "prefix": "viafix",
  "kind": "native",
  "api": {"major": 1, "minorMin": 2},
  "hefesto": "^1.4.0",
  "platforms": ["linux-x86_64", "macos-arm64"],
  "entry": {"linux-x86_64": "bin/linux-x86_64/libviafix.so",
            "macos-arm64": "bin/macos-arm64/libviafix.dylib"},
  "provides": ["command", "check", "panel"],
  "permissions": ["layout.read", "layout.write", "ui.panel"],
  "dependencies": [{"id": "org.example.geomkit", "version": "^2.0.0"}],
  "license": "Apache-2.0",
  "authors": ["Example Corp"],
  "homepage": "https://example.org/viafix"
}
```

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Bad prefix
- **WHEN** a manifest has `"prefix": "Via-Fix"`
- **THEN** installation SHALL fail with `PLUGIN-MAN-002` naming `prefix`

#### Scenario: Future schema
- **WHEN** a host that supports manifest schema 1 reads `"schema": 2`
- **THEN** it SHALL refuse with `PLUGIN-MAN-001`: `plugin manifest schema 2 requires a newer Hefesto`

### Requirement: Package signatures
`SIGNATURE` SHALL be a UTF-8 JSON object:

```json
{"alg": "ed25519", "keyId": "<16 hex: first 8 bytes of SHA-256 of the public key>",
 "files": {"<path>": "<sha256 hex>", ...}, "sig": "<base64 signature>"}
```

SHA-256 and Ed25519 SHALL be computed with CryptoKit on macOS and iPadOS and with libsodium on Linux, and JSON SHALL be parsed and written with nlohmann/json (capability `hefesto-licensing`, requirement "Third-party libraries for cryptography, networking and data formats"; decision D-13).

`files` SHALL list every archive entry except `SIGNATURE`, and no other paths. `sig` is the Ed25519 signature over the canonical form of `{"alg","keyId","files"}`: keys sorted by code point, no whitespace, and strings escaped minimally as in RFC 8785.

Verification SHALL fail if any of these hold:
- an entry is missing from `files`, or `files` names a path that is not in the archive
- a hash differs
- the key is unknown
- the signature does not verify

The resulting trust level SHALL be one of:
- `hefesto`: signed by a Hefesto catalog key built into the app
- `publisher`: signed by a key the user has trusted, with the key id, publisher name and the date it was trusted recorded
- `unsigned`: accepted only in developer mode on desktop (capability `plugin-security-and-platforms`)

Hashes SHALL be re-verified at every load, against the installed files, before any native library is mapped or any script is sourced. On macOS, native libraries SHALL additionally carry a valid code signature (Q-PLUGIN-3).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Tampered after install
- **GIVEN** an installed signed plugin
- **WHEN** one byte of its `.so` is changed on disk and Hefesto starts
- **THEN** the plugin SHALL NOT be loaded, and diagnostic `PLUGIN-SIG-003` SHALL name the file whose hash differs

### Requirement: Dependency and version compatibility
Before installing or enabling a plugin set, the host SHALL resolve it as follows:
1. Every enabled plugin's `hefesto` range SHALL contain the host version, and its `api` SHALL be supported (capability `plugin-api`).
2. For each dependency, choose the highest installed version that satisfies every range that names it. If none exists and a catalog is configured, choose the highest catalog version that does.
3. Only one version of a plugin id SHALL be active at a time.
4. The dependency graph SHALL be acyclic. A cycle SHALL be refused with `PLUGIN-DEP-003`, listing the cycle.
5. Load order is a topological order, with ties broken by plugin id in code-point order.

If no assignment satisfies every constraint, the operation SHALL be refused with `PLUGIN-DEP-001`, naming each conflicting requirement, and SHALL change nothing. Dependencies between plugins grant no permissions (capability `plugin-security-and-platforms`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Highest compatible version
- **GIVEN** geomkit 2.0.0, 2.3.1 and 3.0.0 installed, plugin A requiring `^2.0.0` and plugin B requiring `>=2.1.0 <3.0.0`
- **WHEN** both are enabled
- **THEN** geomkit 2.3.1 SHALL be active

#### Scenario: Conflict
- **GIVEN** plugin A requires geomkit `^2.0.0` and plugin B requires geomkit `^3.0.0`
- **WHEN** the user enables B while A is enabled
- **THEN** the host SHALL refuse with `PLUGIN-DEP-001`, naming both requirements, and A SHALL stay enabled

### Requirement: Discovery locations
The host SHALL discover installed plugins only in these plugin roots, in this order:
1. **User root.** Linux: `$XDG_DATA_HOME/hefesto/plugins` (default `~/.local/share/hefesto/plugins`). macOS: `~/Library/Application Support/Hefesto/Plugins`. iPadOS: `Library/Application Support/Plugins` inside the app container.
2. **System root (desktop only).** Linux: `/usr/share/hefesto/plugins` and `/usr/local/share/hefesto/plugins`. macOS: `/Library/Application Support/Hefesto/Plugins`.
3. **Built-in plugins**, compiled into the host.
4. **Extra roots** given by `hefesto-cli --plugin-path <dir>` (repeatable), or by the developer-mode setting.

Each root holds `<id>/<version>/` directories in installed form. If the same id and version appear in more than one root, the first root in order wins. Hefesto SHALL NOT read plugin locations from environment variables, and SHALL NOT load a plugin from a project or document folder.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/cross-cutting.md#X-4)

#### Scenario: Project folder is not a plugin root
- **GIVEN** a project folder containing `plugins/org.evil.x/1.0.0/plugin.json`
- **WHEN** the project is opened
- **THEN** that plugin SHALL NOT be discovered or loaded

### Requirement: Installation
Installing from a `.hfplugin` file SHALL:
1. Unpack into a staging directory inside the target root.
2. Validate the manifest.
3. Verify the signature.
4. Check platform availability, API and host version, and dependencies.
5. Ask for consent to the declared permissions (capability `plugin-security-and-platforms`).
6. Atomically rename the staging directory to `<root>/<id>/<version>/`.

Any failure SHALL remove the staging directory and leave existing installs unchanged. Installing a version that is already installed SHALL be a no-op unless `--reinstall` is given. Several versions of one id MAY be installed, and the newest compatible version SHALL become active unless the user pins another. Installing into the system root requires the platform's administrator mechanism and is not offered from the GUI.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Declined consent
- **WHEN** the user declines the permission prompt during installation
- **THEN** no directory for the plugin SHALL remain in the user root

### Requirement: Plugin catalog
Hefesto MAY be configured with one or more plugin catalogs (governance and hosting: see Q-PLUGIN-7). A catalog is an HTTPS URL serving `index.json`:

```json
{"schema": 1, "generated": "<ISO 8601>",
 "plugins": [{"id": "...", "name": "...", "versions": [
   {"version": "1.2.0", "url": "https://...", "sha256": "<hex>", "size": <bytes>,
    "platforms": [...], "api": {"major": 1, "minorMin": 2}, "hefesto": "^1.4.0",
    "permissions": [...], "license": "Apache-2.0"}]}]}
```

`index.json` SHALL be accompanied by `index.json.sig`, an Ed25519 signature of the exact bytes, made with a key the host trusts for that catalog. An index that fails verification SHALL NOT be used.

The download SHALL be verified against `sha256` and `size` before installation proceeds as in Requirement "Installation". The official Hefesto catalog is disabled until the user enables it. Browsing and installing SHALL work offline from local files. Catalog fetches SHALL send no data about the user's designs.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/product.md#P-14)

#### Scenario: Catalog hash mismatch
- **GIVEN** a catalog entry with `sha256` h1
- **WHEN** the downloaded file hashes to h2 ≠ h1
- **THEN** the download SHALL be discarded with `PLUGIN-CAT-002`, and nothing SHALL be installed

### Requirement: Updates
The host SHALL check for plugin updates only:
- when the user asks, or
- if the user has enabled automatic checks, at most once per 24 hours

An update SHALL be installed only with the user's consent, unless the user has enabled automatic updates for that plugin. Even then, an update that adds a permission, changes `kind`, or changes the signing key SHALL require fresh consent and SHALL NOT be applied automatically. The previous version SHALL be kept until the new one has loaded and initialized successfully once. A failed update SHALL roll back to the previous version. Updates SHALL take effect at the next launch for native plugins, and immediately for script plugins.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Permission escalation blocks auto-update
- **GIVEN** viafix 1.2.0 with permissions `layout.read layout.write` and automatic updates on
- **WHEN** viafix 1.3.0, which adds `network:api.example.org`, is found
- **THEN** it SHALL NOT be installed until the user approves the new permission

#### Scenario: Rollback
- **GIVEN** an update to 1.3.0 whose `init` fails at the next launch
- **WHEN** the failure is detected
- **THEN** 1.2.0 SHALL be re-activated, and the user SHALL be told that 1.3.0 failed to start

### Requirement: Uninstall and disable
Disabling a plugin SHALL keep its files and record its state in user settings. Uninstalling SHALL remove `<root>/<id>/<version>/`, and with "all versions", the whole `<id>/` directory. Neither action SHALL modify any design file.

Uninstalling a plugin that another enabled plugin depends on SHALL require confirmation, and SHALL disable the dependents. Plugin-owned settings and data, stored under the app's per-plugin data directory `<app data>/plugin-data/<id>/`, SHALL be removed only if the user chooses "remove data".

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Dependents disabled
- **GIVEN** viafix depends on geomkit
- **WHEN** the user uninstalls geomkit and confirms
- **THEN** viafix SHALL be disabled, with a diagnostic naming the missing dependency

### Requirement: Project plugin requirements
A project MAY declare the plugins it needs in its project file, as `plugins: [{"id", "version"}]` with SemVer ranges. When such a project is opened:
- A required plugin that is installed and compatible SHALL be enabled for that project, with its existing consent.
- A required plugin that is missing or incompatible SHALL produce a `warning` that offers to install it through the normal installation flow, including consent.
- Requirements SHALL NEVER be installed or enabled silently.

The project SHALL open and edit normally without its plugins.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Missing required plugin
- **GIVEN** a project that requires `org.example.viafix ^1.0.0`, which is not installed
- **WHEN** the project is opened
- **THEN** a warning SHALL offer installation, and the layout SHALL open and be editable

### Requirement: Preservation of plugin data
Design data written by plugins SHALL survive opening and saving in a Hefesto that does not have the plugin. This means:
- **Properties.** Cell and label properties whose key starts with `<prefix>:` SHALL be preserved byte-for-byte on save.
- **Project data.** Files under the project's `plugins/<id>/` data folder SHALL NOT be deleted or rewritten by the host.
- **Unknown formats.** Unknown project-file keys under `pluginData.<id>` SHALL be preserved.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Round trip without plugin
- **GIVEN** cell `inv` with property `viafix:pitch 0.34`, saved by a host with viafix
- **WHEN** it is opened, edited and saved by a host without viafix
- **THEN** the saved file SHALL still contain `string viafix:pitch 0.34`

### Requirement: Command-line management
`hefesto-cli` SHALL provide plugin management subcommands. Each SHALL exit 0 on success and non-zero on failure, and SHALL print diagnostics as capability `hefesto-diagnostics` requires:

| Subcommand | Effect |
|---|---|
| `plugin list [--json]` | List installed plugins: id, version, kind, state and trust |
| `plugin install <file\|id[@range]> [--yes-permissions <list>]` | Install; without `--yes-permissions` covering every declared permission, the command SHALL fail rather than prompt |
| `plugin remove <id> [--all-versions] [--remove-data]` | Uninstall |
| `plugin enable <id>` / `plugin disable <id>` | Change the enabled state |
| `plugin verify <id\|file>` | Check manifest, signature and hashes |
| `plugin update [<id>] [--check-only]` | Update with the same consent rules as the GUI |

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/cross-cutting.md#X-6)

#### Scenario: Non-interactive install needs explicit permissions
- **WHEN** `hefesto-cli plugin install viafix.hfplugin` is run for a plugin declaring `layout.write`
- **THEN** it SHALL exit non-zero with `PLUGIN-SEC-004`, which names the permissions that `--yes-permissions` must list
