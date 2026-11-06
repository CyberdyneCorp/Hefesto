# Plugin API

## Purpose
Defines what Hefesto plugins can do and the binary contract they use (decision P-20). A plugin extends Hefesto through a fixed set of extension points: commands in the single command API, panels and views, file importers and exporters, DRC and analysis checks, overlays, and PDK adapters. It does not patch the core.

The host-plugin contract is a stable, versioned C ABI. A header-only C++20 wrapper sits on top of it. This capability covers the extension points, the ABI rules, versioning and deprecation, the plugin lifecycle, threading, and error handling.

Related capabilities:
- `plugin-packaging-and-distribution`: manifests, packages, signing, install and update
- `plugin-security-and-platforms`: permissions, sandboxing, consent, and platform availability, including iPadOS
- `plugin-sdk`: headers, samples, test harness and documentation
- `hefesto-architecture`: the single command API, platform services and the concurrency model
- `hefesto-scripting`: the sandboxed Tcl runtime that script plugins use
- `hefesto-diagnostics`: diagnostic records and command results

Terms used below:
- **host**: the Hefesto process that loads plugins: a desktop app, the iPadOS app, or `hefesto-cli`.
- **native plugin**: a shared library that implements the C ABI.
- **script plugin**: a set of Tcl files that run in the sandboxed interpreter.
- **built-in plugin**: a native plugin compiled into the host at build time and registered statically.
- **plugin id**: the reverse-DNS identifier from the manifest, for example `org.example.viafix`.
- **prefix**: the plugin's short command prefix from the manifest, for example `viafix`.

## Requirements

### Requirement: Extension points
A plugin SHALL extend Hefesto only through these extension points:

| Kind | Id | What the plugin supplies |
|---|---|---|
| Command | `command` | A named operation in the single command API |
| Panel or view | `panel` | A declarative UI tree, shown as a dockable panel (desktop) or a sheet or sidebar (iPadOS) |
| Importer | `importer` | A reader from a byte stream into layout, schematic or netlist data, through commands |
| Exporter | `exporter` | A writer from a read-only snapshot to a byte stream |
| Check | `check` | A DRC-style or analysis check that produces diagnostics and markers |
| Overlay | `overlay` | A dataset drawn over the 2D layout view and the 3D view |
| PDK adapter | `pdk-adapter` | PDK data for the PDK store: layer maps, simulation model descriptors, EM rule files, LVS setup, and device-generator bindings |

A plugin SHALL declare every extension point it registers in its manifest (capability `plugin-packaging-and-distribution`). The host SHALL reject a registration of a kind the manifest does not declare. The error is `HF_E_UNDECLARED` with diagnostic `PLUGIN-REG-001`.

Plugins SHALL NOT replace or wrap built-in commands, readers, writers, DRC rules or renderers, and SHALL NOT receive raw pointers into the layout database.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Undeclared extension point
- **GIVEN** a plugin whose manifest declares only `command`
- **WHEN** its init function registers an overlay
- **THEN** the registration SHALL fail with `HF_E_UNDECLARED`, diagnostic `PLUGIN-REG-001` SHALL be emitted, and the plugin's commands SHALL still be registered

### Requirement: Stable C ABI
The contract between the host and native plugins SHALL be a C ABI declared in the single header `hefesto/plugin.h`. That header SHALL compile as C11 and as C++20. The ABI rules are:

1. **Symbols.** A native plugin exports exactly one symbol: `hf_plugin_entry`, with C linkage and the platform's default calling convention:
   ```c
   HF_EXPORT hf_status hf_plugin_entry(const hf_host_api* host, hf_plugin_api* out);
   ```
   The host resolves every other function through function-pointer tables, never by symbol lookup.
2. **Types.** Only fixed-width integers (`int32_t`, `uint64_t` and so on), `double`, `bool` (C11 `_Bool`), opaque handle types (`typedef struct hf_cell_s* hf_cell;`), plain structs of these, and function pointers cross the boundary. No C++ types, no `long`, no `size_t` in structs (use `uint64_t`), no bit-fields, and no `enum` types in structs: enumerations are `int32_t` with `#define` or `enum` constants.
3. **Struct versioning.** Every struct that crosses the boundary SHALL begin with `uint32_t struct_size`. The receiver SHALL read only fields that lie entirely within `struct_size`. Fields beyond it take their documented default (zero unless stated). New fields SHALL be added only at the end.
4. **Strings.** Strings are UTF-8 `hf_str {const char* data; uint64_t len;}`. They are not required to be NUL-terminated. Embedded NUL bytes SHALL be rejected with `HF_E_INVALID_ARG`.
5. **Ownership.** Memory is freed by the side that allocated it. Host-returned buffers are released with `host->release(handle)`. Plugin-returned buffers are released through the plugin's `release` callback. Borrowed pointers are valid only until the callback that received them returns, unless the API names a longer lifetime.
6. **Errors.** Every function returns `hf_status` (`int32_t`). `HF_OK` is 0, and errors are negative. Details are attached as diagnostic records through `host->emit_diagnostic`. No C++ exception, `longjmp` or signal SHALL cross the boundary.
7. **Coordinates.** Layout coordinates are `int64_t` internal units with the cell's scale available from the host. Physical quantities are `double` in SI-prefixed units named in the function's documentation.

A plugin that returns from `hf_plugin_entry` with `out->struct_size` smaller than the version-1.0 size SHALL be refused with `PLUGIN-LOAD-003`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20) (See Q-PLUGIN-1.)

#### Scenario: Older plugin, newer struct
- **GIVEN** a plugin built against API 1.0, whose `hf_check_desc` has `struct_size` = 48
- **AND** a host at API 1.3, whose `hf_check_desc` is 64 bytes, with a field `max_markers` at offset 48
- **WHEN** the plugin registers a check
- **THEN** the host SHALL read only the first 48 bytes and SHALL use the default `max_markers` = 0 (no limit)

#### Scenario: Exception at the boundary
- **GIVEN** a native plugin compiled with exceptions enabled, whose command handler throws `std::runtime_error` and does not catch it
- **WHEN** the handler is built with the C++20 wrapper
- **THEN** the wrapper SHALL catch it and return `HF_E_INTERNAL` with the exception's `what()` text as a diagnostic, and no exception SHALL reach host code

### Requirement: Header-only C++20 wrapper
The SDK SHALL provide `hefesto/plugin.hpp`, a header-only C++20 layer over the C ABI. It SHALL:
- depend only on `hefesto/plugin.h` and the C++20 standard library
- wrap handles in move-only RAII types that release them in their destructors
- expose strings as `std::string_view` and arrays as `std::span`
- return `hf::Result<T>`, a value or a status plus diagnostics, instead of throwing
- convert every exception that escapes a plugin callback into `HF_E_INTERNAL` at the boundary, and mark every callback trampoline `noexcept`
- provide a `HF_PLUGIN(ClassName)` macro that defines `hf_plugin_entry`

Everything in the wrapper SHALL be `inline` or a template, so that no wrapper symbol is exported and two plugins built with different compilers or standard libraries can be loaded in one host. The C ABI is the compatibility contract; the wrapper MAY change between SDK releases as long as it keeps compiling against the same C ABI version.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Two compilers in one host
- **GIVEN** plugin A built with GCC 13 and libstdc++, and plugin B built with Clang 18 and libc++, both against API 1.2
- **WHEN** both are loaded into the same Linux host
- **THEN** both SHALL load, register and run, because only C types cross the boundary

### Requirement: API versioning
The plugin API SHALL carry a version `major.minor`, given in `hefesto/plugin.h` as `HF_API_VERSION_MAJOR` and `HF_API_VERSION_MINOR`. The rules are:
- **Minor releases** only add: new functions at the end of function tables, new fields at the end of structs, new status codes, new enum constants, and new extension-point options. They SHALL NOT change the meaning, signature or layout of anything already published.
- **Major releases** MAY remove deprecated items or change signatures. Each major release ships a migration guide (capability `plugin-sdk`).
- **Negotiation.** `hf_host_api` carries the host's `(major, minor)` and a list of supported majors. The manifest declares `api.major` and `api.minorMin`. The host SHALL load a plugin if and only if it supports the plugin's major, and its minor for that major is ≥ `api.minorMin`. Otherwise it SHALL refuse with `PLUGIN-LOAD-001` and text naming both versions.
- **Support window.** A host SHALL support the current major and the previous major. It SHALL keep supporting a major for at least 24 months after the next major ships, and through at least two host major releases, whichever is longer.
- **Calling newer functions.** A plugin built for minor m that runs on a host at minor m' ≥ m SHALL see the function tables at their m' size. A plugin built with `api.minorMin` < m and wanting to use a function added in m SHALL check `host->struct_size` or `hf_host_has(host, HF_FN_<name>)` before calling it.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Minor too new
- **GIVEN** a host at API 2.3 and a plugin with `api.major` = 2 and `api.minorMin` = 5
- **WHEN** the host loads it
- **THEN** the load SHALL be refused with `PLUGIN-LOAD-001` and text `plugin requires API 2.5 or later; this Hefesto provides 2.3`

#### Scenario: Previous major still supported
- **GIVEN** a host whose current API is 3.0 and which supports major 2 at minor 7
- **WHEN** a plugin with `api.major` = 2 and `api.minorMin` = 4 is loaded
- **THEN** it SHALL load through the major-2 compatibility tables

### Requirement: Deprecation policy
An API item SHALL be deprecated before it is removed, and SHALL be removed only in a major release. Deprecation SHALL:
- mark the item with `HF_DEPRECATED("<replacement>")` in the header, which compilers report as a warning
- list it in the SDK changelog with the replacement and the earliest major that may remove it
- keep the item working for at least the rest of that major, and for at least 2 minor releases and 12 months after the release that deprecated it

When a loaded plugin calls a deprecated function, the host SHALL emit one `info` diagnostic `PLUGIN-DEPR-001` per plugin, function and session, naming the plugin, the function and its replacement.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Deprecated call reported once
- **GIVEN** a plugin that calls the deprecated `hf_layout_get_paint` 10 000 times in a session
- **WHEN** the session ends
- **THEN** exactly one `PLUGIN-DEPR-001` diagnostic SHALL have been emitted for that plugin and function

### Requirement: Plugin lifecycle
A plugin SHALL pass through these states in order: `discovered`, `verified`, `loaded`, `initialized`, `active`, and finally `disabled` or `unloaded`.

1. **Discovered.** The host found its installed package (capability `plugin-packaging-and-distribution`).
2. **Verified.** The signature and file hashes check out, and the user has consented to its permissions (capability `plugin-security-and-platforms`).
3. **Loaded.** Native: the library is mapped and `hf_plugin_entry` returned `HF_OK`. Script: a dedicated sandboxed interpreter has sourced the entry script.
4. **Initialized.** The plugin's `init(ctx)` returned `HF_OK`. All registrations SHALL be made inside `init`. Registrations attempted later SHALL fail with `HF_E_STATE`.
5. **Active.** The registered extension points are visible.
6. **Disabled.** On user request or after a fault, the host calls `shutdown(ctx)`, removes every registration, and closes the plugin's panels and overlays. A native library SHALL stay mapped until the process exits, because unloading code is not reliable on every platform. Enabling it again re-runs `init` in the same process. Script plugins are unloaded by deleting their interpreter.

Plugins SHALL be loaded in dependency order (dependencies first). They SHALL be initialized before the first document is opened. A plugin whose `init` fails SHALL be marked `failed`, with its diagnostics shown, and its dependents SHALL NOT be loaded. Startup SHALL continue.

The host SHALL provide a safe mode (desktop: `--safe-mode` or holding Shift at launch; iPadOS: a Settings switch) in which no third-party plugin is loaded.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Failing init does not block startup
- **GIVEN** plugins A and B, where B depends on A and A's `init` returns `HF_E_INTERNAL`
- **WHEN** Hefesto starts
- **THEN** A SHALL be `failed`, B SHALL be skipped with diagnostic `PLUGIN-LOAD-004` naming A, and the main window SHALL open normally

#### Scenario: Disable at runtime
- **GIVEN** an active native plugin with prefix `viafix` and command `viafix::stitch`
- **WHEN** the user disables it
- **THEN** `viafix::stitch` SHALL return "unknown command", its panels SHALL close, and the library SHALL remain mapped until exit

### Requirement: Plugin commands in the command API
Every plugin command SHALL be a command of the single command API (capability `hefesto-architecture`), named `<prefix>::<name>`. `<prefix>` is the manifest prefix, and `<name>` matches `[a-z][a-z0-9_]{0,47}`. A plugin command SHALL:
- be callable from the GUI, key bindings, scripts (as `<prefix>::<name>` in the Tcl `magic::` binding) and `hefesto-cli`, exactly like a built-in command
- declare its arguments as an ordered schema: name, type (`int`, `double`, `string`, `bool`, `coord`, `layer`, `cell`, `enum(<values>)`), whether it is required, and a default
- return a status, a result value (`none`, `int`, `double`, `string`, or a list of these) and diagnostics, as capability `hefesto-diagnostics` requires
- be recorded in the command log with its arguments
- declare `mutates: true|false`

The host SHALL validate arguments against the schema before calling the plugin. A type error SHALL fail the command with `PLUGIN-CMD-002` without calling the plugin. A command with `mutates: true` SHALL run on the command thread, inside one undo group that the host opens before the call and closes after it. If the command fails, the host SHALL roll back that undo group, so a failed plugin command leaves the layout unchanged.

Prefixes SHALL be unique among installed plugins. A prefix SHALL NOT equal a built-in command name or a built-in Tcl namespace (`magic`, `hefesto`, `tcl`, `ns`, `oo`). Abbreviated (unique-prefix) matching of command names SHALL NOT apply to plugin commands.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/cross-cutting.md#X-3)

#### Scenario: Plugin command is one undo step
- **GIVEN** plugin command `viafix::stitch` that issues 40 `paint` commands
- **WHEN** it is run and the user then undoes once
- **THEN** all 40 paint operations SHALL be undone together

#### Scenario: Failure rolls back
- **GIVEN** `viafix::stitch` paints 3 rectangles and then returns `HF_E_INTERNAL`
- **WHEN** the command completes
- **THEN** the layout SHALL be identical to its state before the command, and the script caller SHALL receive an error with the plugin's diagnostic

#### Scenario: Argument type checked by the host
- **WHEN** a script runs `viafix::stitch -pitch abc`, where `pitch` is declared `double`
- **THEN** the command SHALL fail with `PLUGIN-CMD-002` and the plugin handler SHALL NOT be called

### Requirement: Layout access
A plugin SHALL read design data only through read-only snapshot handles, and SHALL change it only by invoking commands through `host->run_command`. A snapshot SHALL expose:
- the technology: layer types, planes, contacts, the connectivity table, and extract-section parameters
- cells: bounding box, paint per layer as rectangles (clipped to an optional area), labels, ports, properties, and child uses with their transforms and arrays
- connectivity queries: the net containing a point or label
- the current selection, the box, and the edit cell

A snapshot is immutable: it SHALL reflect the database at the moment it was taken, and SHALL remain valid until released, even while the command thread makes later edits. Commands run through `host->run_command` from a mutating plugin command SHALL join the caller's undo group. A plugin MAY store its own data only in cell or label properties whose keys start with `<prefix>:`. The host SHALL preserve these keys when the plugin is missing (capability `plugin-packaging-and-distribution`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/cross-cutting.md#X-3)

#### Scenario: Snapshot isolation
- **GIVEN** a background check holding a snapshot of cell `inv`
- **WHEN** the user paints metal1 in `inv` on the command thread
- **THEN** the check SHALL keep seeing the paint as it was when its snapshot was taken

#### Scenario: Namespaced property
- **WHEN** plugin `viafix` sets property `viafix:pitch` to `0.34` on cell `inv` and the cell is saved
- **THEN** the `.mag` file SHALL contain `string viafix:pitch 0.34` (capability `layout-properties`)

### Requirement: Panels and views
A plugin panel SHALL be described as a declarative UI tree. The plugin SHALL NOT draw with platform widgets or GPU APIs. The node types are:
- `column` and `row` (children, spacing)
- `label` (text)
- `button` (title, the command line to run)
- `text-field`, `number-field` and `toggle` (each bound to a named panel variable)
- `choice` (enum values, bound to a variable)
- `table` (columns and rows; a row MAY carry a cross-probe target: cell plus area, or net)
- `image` (RGBA8 pixels or PNG bytes)
- `plot` (x/y series, with axis labels and units)

Each shell SHALL render the tree in its own toolkit: Raven on the desktop, SwiftUI on iPadOS. A plugin updates its panel by replacing the tree or by setting variables. Panel callbacks SHALL be delivered on the UI thread, and SHALL interact with the layout only by running commands.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/product.md#P-11)

#### Scenario: Same panel on both shells
- **GIVEN** a script plugin whose panel has a `number-field` bound to `pitch` and a button running `viafix::stitch -pitch $pitch`
- **WHEN** it is opened on macOS and on iPadOS
- **THEN** both shells SHALL show the field and the button, and pressing the button SHALL issue the same command in both

### Requirement: Importers and exporters
An importer or exporter SHALL declare:
- a format id (`<prefix>.<name>`)
- a display name
- the file extensions
- the data it handles: `layout`, `schematic`, `netlist` or `other`

An importer SHALL receive its input as a host byte stream, never as a file path. It SHALL create design data only by running commands. Its run is one undo group. An importer is a file reader under capability `hefesto-security`: it SHALL bound its allocations and reject malformed input with diagnostics. An exporter SHALL receive a read-only snapshot and a host output stream.

Importers and exporters SHALL appear in the open, import and export UI next to the built-in formats. They SHALL be reachable through the commands `import -format <id> <file>` and `export -format <id> <file>`. If a plugin format claims an extension that a built-in format also uses, the built-in format SHALL stay the default for that extension.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Extension collision
- **GIVEN** a plugin exporter claiming extension `.gds`
- **WHEN** the user saves `top.gds` without choosing a format
- **THEN** the built-in GDS writer SHALL be used, and the plugin format SHALL be offered only when chosen explicitly

### Requirement: Checks
A plugin check SHALL declare:
- an id (`<prefix>.<name>`)
- a title
- a category (`drc`, `electrical`, `reliability`, `manufacturing` or `other`)
- a default severity
- whether it runs incrementally on edited areas or only on demand

A check SHALL run on a background worker with a read-only snapshot and a cancellation token. It SHALL report results only as diagnostic records with codes `<PREFIX>-<CHECK>-<nnn>` (the prefix upper-cased), each with a cell and an area or a related object.

For the same snapshot and parameters, a check SHALL produce the same records in the same order. The host SHALL sort the records by (cell name, area lower-left y, x, code) before showing them. A check MAY register as an optional item of the sign-off checklist (capability `tapeout-signoff-checklist`). It then takes part in the checklist's staleness rules, and its failures SHALL be `warning` unless the program profile marks the item blocking.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/product.md#P-15)

#### Scenario: Cancelled check
- **GIVEN** a running plugin check
- **WHEN** the user edits the cell that the check is examining
- **THEN** the host SHALL signal cancellation, discard the partial results, and restart the check on a new snapshot

### Requirement: Overlays
A plugin overlay SHALL supply a dataset that the host draws in the 2D layout view and, where it has a 3D meaning, in the 3D view (capability `hefesto-3d-visualization`). A dataset SHALL be one of:
- **region scalars:** rectangles or polygons in a cell, each with a value
- **markers:** points or rectangles with a label
- **edges:** pairs of points with a value
- **net scalars:** a value per net

Every dataset SHALL carry a name, a unit, a value range, and a color-map choice from the host palette set. The host SHALL draw the legend. Datasets MAY be time series with a shared time axis, and the host's time cursor then selects the sample. Overlays SHALL NOT modify the layout and SHALL NOT be saved in cells.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/product.md#P-4)

#### Scenario: Overlay is not saved
- **GIVEN** an active plugin overlay of density per 10 µm window on cell `top`
- **WHEN** `top` is saved
- **THEN** the `.mag` file SHALL be byte-identical to one saved with the overlay off

### Requirement: PDK adapters
A PDK adapter SHALL supply PDK data for a named PDK family and version range. It can supply:
- GDS layer maps
- simulation model descriptors (capability `simulation-netlist-preparation`)
- EM rule files (capability `reliability-em-rules`)
- LVS setup files
- bindings that expose a PDK's device generators as commands

Adapter output SHALL go through the same validation as files installed in the PDK store. It SHALL be recorded in the PDK store with provenance `plugin:<id>@<version>`. When both an adapter and the installed PDK supply the same item, the PDK's own file SHALL win unless the user selects the adapter for that item in project settings.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/product.md#P-14)

#### Scenario: Adapter supplies EM rules for sky130A
- **GIVEN** sky130A installed with no EM rule file, and an adapter plugin that supplies one
- **WHEN** an EM check runs on a sky130A design
- **THEN** the adapter's rules SHALL be used, and the report SHALL name `plugin:<id>@<version>` as the rule source

### Requirement: Threading rules
Plugin callbacks SHALL be called on these threads:

| Callback | Thread |
|---|---|
| `init`, `shutdown` | main thread, before any document is open or after all are closed |
| command handler, `mutates: true` | the command thread |
| command handler, `mutates: false` | the command thread |
| importer | the command thread |
| exporter, check, overlay producer | a host background worker |
| panel callbacks | the UI thread |

Every host API function SHALL be documented as either `any-thread` or `command-thread`. Calling a `command-thread` function from another thread SHALL return `HF_E_WRONG_THREAD` without side effects. `host->run_command` is `command-thread`. Background work that needs a mutation SHALL post a command with `host->post_command`, which is `any-thread` and runs the command later on the command thread.

Plugins SHOULD use `host->submit_task` (the host's background executor) rather than create threads. A plugin that creates threads SHALL join them in `shutdown`. A callback on the command thread or the UI thread that runs longer than 2 s SHALL produce a `warning` diagnostic `PLUGIN-PERF-001` naming the plugin. Long work SHALL be moved to a task that reports progress and honors cancellation.

A plugin SHALL NOT call back into the host from inside a host-owned lock. The only nesting the host supports is `run_command` from a command handler, up to a nesting depth of 16. Deeper nesting SHALL fail with `HF_E_RECURSION`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/product.md#P-1)

#### Scenario: Mutation from a background check
- **WHEN** a plugin check calls `host->run_command("paint m1")` from its worker
- **THEN** the call SHALL return `HF_E_WRONG_THREAD`, and the layout SHALL be unchanged

#### Scenario: Posted command
- **WHEN** the same check calls `host->post_command("paint m1")`
- **THEN** the paint SHALL run later on the command thread as its own undo group, and SHALL be logged

### Requirement: Fault handling
The host SHALL treat plugin faults as follows:
- **Errors.** An error status from a plugin callback SHALL be reported as diagnostics attributed to the plugin. The host SHALL continue.
- **Repeated errors.** A plugin that returns `HF_E_INTERNAL` from 5 consecutive callbacks SHALL be disabled for the session, with a `warning` diagnostic `PLUGIN-FAULT-001`.
- **Native crashes.** Before each native plugin callback, the host SHALL record a breadcrumb naming the plugin. If the process terminates abnormally with a breadcrumb open, the next launch SHALL name the plugin and offer to start with it disabled. Crash reports (capability `hefesto-product-operations`) SHALL include the plugin id and version.
- **Script plugins.** A Tcl error SHALL behave like an error status. A script that exceeds its time limit SHALL be interrupted by the sandbox (capability `plugin-security-and-platforms`).

Native plugins run in process and are not isolated from crashes (Q-PLUGIN-2).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Crash attribution
- **GIVEN** native plugin `org.example.viafix` 1.2.0 dereferences a null pointer inside `viafix::stitch`
- **WHEN** Hefesto is started again
- **THEN** it SHALL show that the previous session ended during a call into `org.example.viafix` 1.2.0, and offer "start with this plugin disabled"

### Requirement: Script plugins
A script plugin SHALL use the same manifest, extension points and permissions as a native plugin. Its implementation is Tcl that runs in a dedicated interpreter of the sandbox of capability `hefesto-scripting`. The interpreter SHALL provide a `hefesto::plugin` namespace that mirrors the C API one to one:
- `register_command`, `register_check`, `register_panel`, `register_importer`, `register_exporter`, `register_overlay` and `register_pdk_item`
- snapshot queries
- `run_command` and `post_command`
- `emit_diagnostic`

A script plugin SHALL see the `magic::` commands only through `run_command`, and SHALL get the same results as a native plugin calling the same API. Script plugins SHALL be supported on every platform.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/cross-cutting.md#X-3)

#### Scenario: Script command
- **GIVEN** a script plugin with prefix `hello` whose entry script calls `hefesto::plugin::register_command greet {name string required} {return "hi $name"}`
- **WHEN** a script runs `hello::greet -name ada`
- **THEN** the result SHALL be the string `hi ada`

### Requirement: Plugins in headless runs
`hefesto-cli` SHALL load enabled plugins as the GUI does. It SHALL provide `--no-plugins` and `--plugin <id>[@<version>]` (repeatable) to control the set. In headless runs, the following SHALL be inert, and registering them SHALL succeed:
- panels
- overlay drawing (the overlay datasets stay computable through commands)
- UI callbacks

A plugin's commands, importers, exporters, checks and PDK adapters SHALL behave identically in headless and GUI runs.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/cross-cutting.md#X-6)

#### Scenario: Plugin command in batch
- **WHEN** `hefesto-cli --plugin org.example.viafix -T sky130A -c "load top; viafix::stitch; save top; quit"` is run
- **THEN** the command SHALL run, `top.mag` SHALL be written, and the tool SHALL exit 0
