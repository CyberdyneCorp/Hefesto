# Plugin Security and Platforms

## Purpose
Defines the security model for Hefesto plugins and their availability on each platform (decision P-20). It covers:
- capability-based permissions
- how permissions are enforced for script plugins and for native plugins
- the consent UI
- developer mode and safe mode
- network and file access
- the platform matrix

The iPadOS position follows App Store rules: no native code is loaded at runtime. Plugins there are built-in (compiled in) or script plugins in the sandboxed Tcl runtime of capability `hefesto-scripting`.

Related capabilities: `plugin-api`, `plugin-packaging-and-distribution`, `hefesto-security`, `hefesto-scripting`, `hefesto-ipad-feature-scope`, `hefesto-platform-targets`.

## Requirements

### Requirement: Trust model
Hefesto SHALL distinguish two enforcement levels and SHALL state the level to the user:
- **Enforced.** Script plugins. They run in the Tcl sandbox, which is enforced in the core (capability `hefesto-security`). A script plugin SHALL be unable to do anything its granted permissions do not allow.
- **Declared.** Native plugins. They run as machine code in the Hefesto process, with the process's operating-system privileges. The host SHALL check every host-API call against the granted permissions. It SHALL NOT claim that this contains a malicious native plugin, which can bypass the API through system calls.

Built-in plugins are part of the reviewed app build and are trusted as app code.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Disclosure for native plugins
- **WHEN** the consent prompt is shown for a native plugin
- **THEN** it SHALL state that native plugins run with the same access as Hefesto itself, and that permissions limit only what the plugin can do through Hefesto

### Requirement: Permissions
Every host-API function and every script-plugin command SHALL require one of these permissions:

| Permission | Allows |
|---|---|
| `layout.read` | Snapshots of cells, technology and connectivity; selection and box |
| `layout.write` | `run_command` and `post_command` with commands that change design data |
| `project.read` / `project.write` | The project file and the plugin's `plugins/<id>/` project data folder |
| `files.read:<scope>` / `files.write:<scope>` | Files outside the project, where scope is `user-picked` (only files the user picks in a host file dialog in this session) or `plugin-data` (the plugin's own app data directory) |
| `simulation.read` | Stored simulation jobs and their vectors |
| `simulation.run` | Starting simulation jobs |
| `pdk.read` | PDK store contents |
| `pdk.adapter` | Supplying PDK data (capability `plugin-api`, Requirement "PDK adapters") |
| `ui.panel`, `ui.overlay` | Registering panels and overlays |
| `network:<host>` | HTTPS requests to that exact host name through `host->http_request`; several entries allowed; no wildcards |
| `process.exec` | Running external programs through the platform-services process interface; desktop only |
| `clipboard` | Reading and writing the clipboard |

Commands, importers, exporters and checks need no permission of their own, beyond what their implementations call. A call without the needed permission SHALL return `HF_E_PERMISSION` (a Tcl error for script plugins), and SHALL emit `PLUGIN-SEC-001` naming the plugin, the function and the missing permission. Permissions SHALL NOT be inherited through dependencies: each plugin holds only its own grants.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Missing permission
- **GIVEN** a script plugin granted only `layout.read`
- **WHEN** it calls `hefesto::plugin::run_command "paint m1"`
- **THEN** it SHALL get a Tcl error, `PLUGIN-SEC-001` SHALL name `layout.write`, and the layout SHALL be unchanged

#### Scenario: Network host not granted
- **GIVEN** a plugin granted `network:api.example.org`
- **WHEN** it requests `https://evil.example.net/upload`
- **THEN** the request SHALL fail with `HF_E_PERMISSION` before any connection is opened

### Requirement: Consent UI
Before a plugin is first enabled, the host SHALL show a consent sheet with:
- the name, id, version and publisher
- the trust level (`hefesto`, `publisher` or `unsigned`) and the signing key id
- the kind (native, script or built-in) and the enforcement statement from Requirement "Trust model"
- each permission in plain language, grouped into data access, network, file system, and process execution
- the license

The user can allow or deny the whole set. Partial grants are not offered, because a plugin cannot be assumed to work with a subset. Grants SHALL be stored per (plugin id, permission set, signing key id). They SHALL be asked for again when:
- an update adds a permission
- the signing key changes
- the kind changes

The settings SHALL list every plugin's grants and allow revoking them. Revoking disables the plugin. `process.exec`, and `network:` for a host not listed in the catalog entry, SHALL be shown with a warning style.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Key change requires consent
- **GIVEN** viafix 1.2.0 signed by key K1 and approved
- **WHEN** viafix 1.2.1, signed by key K2 with the same permissions, is installed
- **THEN** the consent sheet SHALL be shown again and SHALL state that the signing key changed

### Requirement: Script plugin sandbox
Each script plugin SHALL run in its own Tcl interpreter, with the sandbox of capability `hefesto-scripting`: no `exec`, no `open |`, no sockets, no `load`, and file access only as the plugin's permissions allow. In addition:
- **Time limit.** Each callback has a wall-clock limit of 10 s by default; the user can change it per plugin, from 1 to 600 s. On expiry the interpreter SHALL be interrupted with the error `plugin time limit exceeded`.
- **Memory limit.** Each interpreter has a memory limit of 512 MiB by default, configurable per plugin. On excess, the call SHALL fail and the interpreter SHALL be reset.
- **Isolation.** Interpreters SHALL NOT share variables, namespaces or channels. Plugins communicate only through commands and project data.
- **Diagnostics.** Every denied operation SHALL produce a diagnostic naming the plugin, the script file and the line (capability `hefesto-security`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/cross-cutting.md#X-3)

#### Scenario: Runaway loop
- **GIVEN** a script plugin command containing `while 1 {}`
- **WHEN** it runs with the default limit
- **THEN** after 10 s the command SHALL fail with `plugin time limit exceeded`, and the host SHALL stay responsive

### Requirement: Native plugin hardening on desktop
The desktop host SHALL apply these measures to native plugins:
- **Location.** It SHALL load native code only from an installed `bin/<platform>/` path inside a plugin root, after verifying the hashes (capability `plugin-packaging-and-distribution`).
- **Library search.** It SHALL load with search paths restricted to the plugin's own `bin/<platform>/` directory and the system libraries. `LD_LIBRARY_PATH` and `DYLD_*` variables SHALL NOT affect plugin loading.
- **Unsigned code.** It SHALL refuse unsigned packages unless developer mode is on.
- **Executable data.** It SHALL refuse native plugins on any platform that forbids loading external code.
- **Safe mode.** It SHALL honor safe mode (capability `plugin-api`).

On macOS, the app's hardened runtime SHALL allow third-party libraries to load only in the way decided in Q-PLUGIN-3.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Unsigned native plugin without developer mode
- **WHEN** a user installs an unsigned native `.hfplugin` with developer mode off
- **THEN** installation SHALL fail with `PLUGIN-SIG-001`, stating that unsigned plugins need developer mode

### Requirement: Developer mode
Developer mode SHALL be a desktop setting, off by default. When it is on:
- unsigned plugins MAY be installed and loaded, each with the same consent sheet marked `unsigned`
- plugins MAY be loaded from extra roots
- script plugins MAY be reloaded without restarting

The main window SHALL show a persistent indicator while developer mode is on. Developer mode SHALL NOT exist on iPadOS.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Indicator
- **WHEN** developer mode is enabled
- **THEN** the status bar SHALL show "Developer mode: unsigned plugins allowed" until it is disabled

### Requirement: Platform matrix
Plugin support per platform SHALL be:

| Plugin form | Linux | macOS | iPadOS |
|---|---|---|---|
| Built-in (compiled into the app, registered statically) | yes | yes | yes |
| Script plugin (sandboxed Tcl), user-installed | yes | yes | yes, subject to Q-PLUGIN-4 |
| Native plugin (shared library loaded at runtime) | yes | yes | no |
| Developer mode and unsigned plugins | yes | yes | no |

Every extension point of capability `plugin-api` SHALL be available to script plugins on every platform. Permission `process.exec` SHALL be refused on iPadOS: a plugin that declares it SHALL be installable there only if it marks it optional, as `"process.exec?"`, and then sees the permission as denied.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/product.md#P-13)

#### Scenario: Native plugin offered on iPad
- **GIVEN** a project that requires native plugin `org.example.viafix`
- **WHEN** it is opened on iPadOS
- **THEN** the plugin SHALL be listed as "not available on iPad (native code)", and the project SHALL open with its plugin data preserved

### Requirement: No runtime code loading on iPadOS
On iPadOS, the host SHALL NOT map, download or execute native code from a plugin or any other source. It SHALL NOT generate executable code at runtime. Every built-in plugin SHALL be linked into the app and registered through a static table. That table is built from the same `hf_plugin_entry` contract, with each entry point renamed at build time to `hf_plugin_entry_<prefix>`, so that several plugins can be linked into one binary.

A `.hfplugin` with `kind: native` SHALL be refused on iPadOS with `PLUGIN-PLAT-001`, without opening any file under `bin/`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/cross-cutting.md#X-3)

#### Scenario: Native package on iPad
- **WHEN** a user opens `viafix.hfplugin` (kind native) in the Files app with Hefesto on iPad
- **THEN** Hefesto SHALL refuse it with `PLUGIN-PLAT-001` `native plugins cannot run on iPad`, and SHALL extract nothing from `bin/`

### Requirement: Script plugins on iPadOS under App Review
On iPadOS, user-installed script plugins SHALL be enabled by default, as long as Apple accepts them under App Store guidelines 2.5.2 and 4.7 (Q-PLUGIN-4). If App Review rejects them, a build-time switch SHALL put the iPadOS app in curated mode. In curated mode:
- only built-in plugins and script plugins bundled with the app or with curated PDK packs SHALL load
- user-installed script plugins SHALL be listed as "not allowed in this build", and their data SHALL be preserved

The desktop builds SHALL be unaffected by curated mode.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/cross-cutting.md#X-3)

#### Scenario: Curated mode
- **GIVEN** an iPadOS build in curated mode
- **WHEN** the user imports a script `.hfplugin` from Files
- **THEN** it SHALL NOT be installed, and the message SHALL say that this build runs only bundled plugins

### Requirement: Plugin network and privacy rules
A plugin SHALL reach the network only through `host->http_request` (HTTPS only, with certificates verified by the platform), and only to hosts granted by `network:` permissions. The host implements `http_request` with URLSession on macOS and iPadOS and with libcurl on Linux (capability `hefesto-licensing`, requirement "Third-party libraries for cryptography, networking and data formats"; decision D-13). Every plugin request SHALL be logged in the session log with the plugin id, method, host and byte counts, and without bodies. Hefesto SHALL show an indicator while a plugin request is in flight.

Nothing in the plugin API SHALL send design data anywhere by itself. A plugin that uploads data does so through its own `http_request` calls, which its consent sheet disclosed.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/product.md#P-22)

#### Scenario: Plain HTTP refused
- **WHEN** a plugin with `network:api.example.org` requests `http://api.example.org/`
- **THEN** the request SHALL fail with `HF_E_PERMISSION` and the text `only https is allowed`
