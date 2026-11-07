# Open Questions — PLUGIN (plugin and extension API)

Capabilities: `plugin-api`, `plugin-packaging-and-distribution`, `plugin-security-and-platforms`, `plugin-sdk`. Product decision: `docs/decisions/product.md#P-20`.

These capabilities are `native`. Magic has no plugin API: it is extended through Tcl scripts and Tcl `load` of shared libraries, and Hefesto's sandbox forbids `load`. The "Magic behavior" field therefore records the nearest Magic mechanism, where there is one, and "Evidence" points at the Hefesto spec or the external rule involved.

### Q-PLUGIN-1: Stable C ABI versus a C++ API
- Kind: ambiguity
- Magic behavior: none. Magic exposes Tcl commands only, and Tcl extensions use Tcl's own C stubs ABI.
- Evidence: openspec/specs/plugin-api/spec.md (Requirements "Stable C ABI", "Header-only C++20 wrapper")
- Options: (a) a stable C ABI plus a header-only C++20 wrapper (specified); (b) a C++20 API with an exported vtable ABI, which ties plugins to one compiler and standard library per platform; (c) out-of-process plugins speaking a message protocol (JSON-RPC or Cap'n Proto) over pipes.
- Recommendation: (a). C++ ABI stability across GCC/libstdc++, Clang/libc++ and Apple Clang releases cannot be guaranteed, and Hefesto would have to freeze its toolchain. A C ABI with `struct_size` versioning is the proven pattern (Vulkan, OpenFX, CLAP, Tcl stubs). Keep (c) in mind for Q-PLUGIN-2. Confirm with a prototype that the wrapper adds no measurable cost on the paint-iteration path (snapshot rectangle queries on 10⁷ tiles).

### Q-PLUGIN-2: [platform] Crash isolation for native plugins
- Kind: platform
- Magic behavior: a crash in a Tcl extension loaded into Magic terminates Magic.
- Evidence: openspec/specs/plugin-api/spec.md (Requirement "Fault handling")
- Options: (a) in process, with breadcrumbs and safe mode (specified); (b) an optional out-of-process host for native plugins on desktop (a `hefesto-plugin-host` helper with shared-memory snapshots), selectable per plugin; (c) a WebAssembly runtime for "portable native" plugins, which would also run on iPadOS if App Review accepts it.
- Recommendation: (a) for 1.0. Prototype (b) for checks and importers, the extension points with the largest inputs and the highest crash risk. Evaluate (c) once the WASI component model and an embeddable runtime with a permissive license are stable.

### Q-PLUGIN-3: [platform] macOS hardened runtime and library validation
- Kind: platform
- Magic behavior: none. Magic is not distributed as a notarized app.
- Evidence: openspec/specs/plugin-security-and-platforms/spec.md (Requirement "Native plugin hardening on desktop"); openspec/specs/hefesto-build-and-distribution/spec.md (signed and notarized macOS bundle)
- Options: (a) ship the notarized app with the `com.apple.security.cs.disable-library-validation` entitlement, and require plugin dylibs to be code-signed (any Developer ID) and notarized by their publisher; (b) keep library validation and load only plugins signed by Hefesto's team ID, which means Hefesto counter-signs registry plugins; (c) native plugins only in an out-of-process host (Q-PLUGIN-2 b) that carries the entitlement, while the main app keeps library validation.
- Recommendation: (a) for direct downloads, with the package signature (Ed25519) as Hefesto's own integrity check. If a Mac App Store build is ever planned, it must use (b) or (c), and that build is then likely limited to script plugins.

### Q-PLUGIN-4: [platform] User-installed script plugins on iPadOS and App Review
- Kind: platform
- Magic behavior: Magic runs arbitrary Tcl, including `exec`; there is no iPad Magic.
- Evidence: App Store Review Guidelines 2.5.2 (no downloading of executable code that changes features) and 4.7 (mini apps, plug-ins and similar software not embedded in the binary, with conditions); docs/decisions/cross-cutting.md#X-3 (the same risk for PDK scripts); openspec/specs/plugin-security-and-platforms/spec.md (Requirement "Script plugins on iPadOS under App Review")
- Options: (a) allow user-installed sandboxed-Tcl script plugins (specified default), arguing that they are interpreted, sandboxed, cannot change the app's primary purpose, and that Hefesto offers a reporting channel as 4.7 asks; (b) curated mode only: built-in plugins plus script plugins bundled in the app or in curated PDK packs; (c) no third-party plugins on iPad.
- Recommendation: ask App Review together with the X-3 PDK-script question, before the iPad beta. Keep (b) ready as a build switch, as specified. Never ship native or JIT code on iPad.

### Q-PLUGIN-5: Plugin licensing and GPL plugins
- Kind: ambiguity
- Magic behavior: none.
- Evidence: openspec/specs/hefesto-licensing/spec.md (Apache-2.0, clean-room rule for GPL sources); openspec/specs/plugin-sdk/spec.md (Requirement "SDK contents and license")
- Options: (a) allow any license, including GPL-3.0 (compatible with Apache-2.0 in that direction), and never bundle GPL plugins with Hefesto; (b) allow only permissive licenses in the official catalog; (c) no restriction at all.
- Recommendation: (a). The official catalog requires an SPDX expression and displays it. Plugins bundled in the app, or as built-in iPad plugins, must be Apache-2.0-compatible and must not be GPL, because of App Store distribution. Get legal review on whether a GPL plugin that links the Apache-2.0 SDK headers creates obligations for Hefesto. It should not, because the headers flow into the plugin and not the reverse.

### Q-PLUGIN-6: Expressiveness of declarative panels
- Kind: ambiguity
- Magic behavior: Magic's Tk GUI lets scripts build arbitrary Tk widgets (capability `ui-gui-managers`).
- Evidence: openspec/specs/plugin-api/spec.md (Requirement "Panels and views")
- Options: (a) the fixed node set (specified), extended in minor API versions; (b) add a `canvas` node with a host-defined 2D drawing list (lines, rectangles, text), so plugins can draw custom views without platform APIs; (c) allow native desktop widgets through Raven, which breaks iPad parity and the Raven confinement rule.
- Recommendation: (a) now, and (b) in API 1.1 if sample authors need it. Reject (c).

### Q-PLUGIN-7: Catalog governance and hosting
- Kind: ambiguity
- Magic behavior: none.
- Evidence: openspec/specs/plugin-packaging-and-distribution/spec.md (Requirement "Plugin catalog")
- Options: (a) an official catalog run by the Hefesto project, with conformance-suite gating and manual review for native plugins; (b) a federated model in which anyone can host a signed catalog and users add catalogs by URL; (c) both.
- Recommendation: (c). The official catalog stays disabled until the user enables it, as P-20 says. Define key rotation and revocation (a signed revocation list inside `index.json`) before the first public catalog.

### Q-PLUGIN-8: Access to the `magic::` Tcl namespace from script plugins
- Kind: ambiguity
- Magic behavior: Magic Tcl scripts call `magic::<cmd>` directly.
- Evidence: openspec/specs/plugin-api/spec.md (Requirement "Script plugins"); openspec/specs/hefesto-scripting/spec.md
- Options: (a) script plugins reach Magic commands only through `hefesto::plugin::run_command` (specified), so that every call goes through the permission check; (b) expose `magic::` directly in the plugin interpreter, wrapped with permission checks per command.
- Recommendation: (a) for 1.0, because it gives one audited path. Revisit (b) if porting existing Magic Tcl utilities as plugins proves too cumbersome. Each `magic::` command would need a read-only or mutating classification.
