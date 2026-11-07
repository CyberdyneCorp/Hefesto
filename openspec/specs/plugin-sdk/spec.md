# Plugin SDK

## Purpose
Defines the Hefesto plugin SDK (decision P-20). The SDK contains:
- the C ABI header and the C++20 wrapper
- the build integration and the packaging and signing tool
- the required sample plugins
- the headless test and conformance harness
- the documentation that plugin authors use

The API contract itself is in capability `plugin-api`. Packaging rules are in `plugin-packaging-and-distribution`, and security rules in `plugin-security-and-platforms`.

## Requirements

### Requirement: SDK contents and license
Each Hefesto release SHALL publish an SDK archive, versioned with the plugin API version it targets (for example `hefesto-plugin-sdk-1.3.0`). It SHALL contain:

| Item | Content |
|---|---|
| `include/hefesto/plugin.h` | The C ABI header (capability `plugin-api`) |
| `include/hefesto/plugin.hpp` | The header-only C++20 wrapper |
| `cmake/HefestoPluginSDKConfig.cmake` | A CMake package: `find_package(HefestoPluginSDK 1.3 REQUIRED)`, providing target `Hefesto::PluginSDK` and function `hefesto_add_plugin(<target> MANIFEST plugin.json SOURCES ...)` |
| `lib/pkgconfig/hefesto-plugin.pc` | pkg-config file for non-CMake builds |
| `bin/hefesto-plugin` | The packaging tool (Requirement "Packaging tool") |
| `bin/hefesto-plugin-test` | The test harness (Requirement "Test harness") |
| `samples/` | The sample plugins (Requirement "Sample plugins") |
| `tcl/hefesto-plugin.tcl` | Stub definitions of `hefesto::plugin` for editor completion |
| `docs/` | Generated reference and guides (Requirement "Documentation") |
| `LICENSE` | Apache-2.0 |

The headers, samples and harness SHALL be licensed Apache-2.0, so plugins can use any license compatible with Apache-2.0 (Q-PLUGIN-5). The SDK SHALL contain no GPL code.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/product.md#P-9)

#### Scenario: CMake consumer
- **GIVEN** a plugin project with `find_package(HefestoPluginSDK 1.3 REQUIRED)` and `hefesto_add_plugin(viafix MANIFEST plugin.json SOURCES viafix.cpp)`
- **WHEN** it is configured and built on Linux with only the SDK installed (no Hefesto source)
- **THEN** it SHALL produce `libviafix.so` exporting exactly the symbol `hf_plugin_entry`

### Requirement: Header guarantees
`plugin.h` SHALL include only `<stdint.h>`, `<stddef.h>` and `<stdbool.h>`. It SHALL compile without warnings at `-Wall -Wextra -pedantic`:
- as C11 with GCC and Clang
- as C++20 with GCC, Clang and Apple Clang

For every public struct, the header SHALL contain static size and alignment assertions for the 64-bit platforms in the platform matrix. Release CI SHALL compare the ABI of each release's headers with the previous release of the same major, using an ABI checker such as libabigail, and SHALL fail on any change other than the additions that `plugin-api` allows.

`plugin.hpp` SHALL compile as C++20 with the same compilers. It SHALL NOT require RTTI. It SHALL work with exceptions disabled, in which case the exception-to-status conversion is compiled out.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: ABI break caught in CI
- **GIVEN** a change that inserts a field in the middle of `hf_command_desc`
- **WHEN** release CI runs the ABI comparison against API 1.2
- **THEN** CI SHALL fail, naming the struct and the field offset that changed

### Requirement: Packaging tool
`hefesto-plugin` SHALL provide these subcommands:

| Subcommand | Effect |
|---|---|
| `new <template> <dir>` | Create a project from a sample template (`c-command`, `cpp-command`, `cpp-check`, `script`) |
| `lint <dir\|file>` | Validate `plugin.json` against the manifest rules, check that `provides` matches what the code registers (by running the plugin in the harness), and check that every declared permission is used |
| `pack <dir> -o <file>.hfplugin` | Build a package with the layout of capability `plugin-packaging-and-distribution`; entries SHALL be sorted by path, with fixed timestamps (1980-01-01 00:00:00), so that the same inputs give byte-identical output |
| `sign <file> --key <ed25519 private key file>` | Add or replace `SIGNATURE` |
| `keygen -o <file>` | Create an Ed25519 key pair and print its key id. Keys, signatures and SHA-256 digests use CryptoKit on macOS and libsodium on Linux (capability `hefesto-licensing`, decision D-13) |
| `verify <file>` | Verify as the host would |

Each subcommand SHALL exit 0 on success and non-zero on failure, and SHALL print diagnostics in text or JSON lines.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/product.md#P-22)

#### Scenario: Reproducible package
- **WHEN** `hefesto-plugin pack` is run twice on the same unchanged directory
- **THEN** the two `.hfplugin` files SHALL be byte-identical

### Requirement: Sample plugins
The SDK SHALL include these samples. Each SHALL build and pass its own harness tests in Hefesto CI on Linux and macOS, and every script sample SHALL also pass in the iPadOS simulator:

| Sample | Language | Demonstrates |
|---|---|---|
| `hello-c` | C11 | `hf_plugin_entry`, one command, diagnostics |
| `hello-cpp` | C++20 wrapper | the same, with RAII and `hf::Result` |
| `rect-csv-io` | C++20 | an importer and exporter of a CSV of `layer,x1,y1,x2,y2` rectangles |
| `min-area-check` | C++20 | a background check with snapshots, cancellation and markers |
| `density-overlay` | C++20 | an overlay of metal density per window, with a time-independent dataset |
| `em-rules-adapter` | script (Tcl) | a PDK adapter that supplies an EM rule file (capability `reliability-em-rules`) |
| `panel-demo` | script (Tcl) | a declarative panel bound to a command |
| `posted-mutation` | C++20 | `post_command` from a worker thread |

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Script sample on iPad simulator
- **WHEN** the `panel-demo` sample is installed in the iPadOS simulator build
- **THEN** its panel SHALL open and its button SHALL run its command, with the same result as on macOS

### Requirement: Test harness
`hefesto-plugin-test` SHALL be a headless host built from the same core as `hefesto-cli`. It SHALL:
- load one plugin, from a directory or a `.hfplugin`, plus its dependencies
- run test files: Tcl scripts with assertion commands `expect_ok`, `expect_error <code>`, `expect_result <value>`, `expect_layout_equal <cell> <golden.mag>` and `expect_diagnostics <golden.jsonl>`, against technology and layout fixtures
- run the conformance suite (Requirement "Conformance suite") when given `--conformance`
- for native plugins, run every test under AddressSanitizer and UndefinedBehaviorSanitizer builds of the harness when given `--sanitize`
- for importers, provide a libFuzzer entry point `--fuzz-importer <format-id>` that feeds arbitrary bytes to the importer
- write JUnit XML when given `--junit <file>`
- exit 0 only if every test passed

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/cross-cutting.md#X-6)

#### Scenario: Golden layout test
- **GIVEN** a test file `load inv; viafix::stitch; expect_layout_equal inv golden/inv.mag`
- **WHEN** the harness runs it against the `minimum` technology fixture
- **THEN** it SHALL exit 0 if the saved `inv` equals the golden file byte for byte, and otherwise SHALL print the first differing line

### Requirement: Conformance suite
The conformance suite SHALL check, for any plugin:
1. `hf_plugin_entry` is the only exported symbol (native).
2. The manifest validates, and `provides` equals the set of kinds registered in `init`.
3. Every host-API call is covered by a declared permission. The suite runs the plugin's own tests with every undeclared permission denied, and reports any `PLUGIN-SEC-001`.
4. No `command-thread` function is called from another thread.
5. No exception or signal escapes a callback, and every callback returns a valid status.
6. `struct_size` handling: the plugin is loaded against host tables at the plugin's `api.minorMin` and at the newest minor, and every test passes in both.
7. Determinism: each registered check, run twice on the same fixture, gives identical diagnostics.
8. `shutdown` returns within 2 s and leaves no running threads that the plugin created.

A catalog SHALL accept a plugin version only if its conformance report passes.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Undeclared permission found
- **GIVEN** a plugin that declares only `layout.read` but calls `run_command` with a paint
- **WHEN** the conformance suite runs
- **THEN** item 3 SHALL fail, and the report SHALL name `layout.write` and the call site test

### Requirement: Documentation
The SDK documentation SHALL include:
- an API reference generated from the comments in `plugin.h`, with the thread class (`any-thread` or `command-thread`), required permission, ownership rules and the minor version that introduced it, for every function
- a getting-started guide for each sample
- guides for each extension point, threading, permissions and packaging
- a changelog listing additions and deprecations per minor version
- a migration guide for each major version

Every code example in the documentation SHALL be compiled or executed in CI. Documentation for a published API version SHALL stay available for the support window of capability `plugin-api`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20, docs/decisions/product.md#P-16)

#### Scenario: Reference shows thread class
- **WHEN** a developer looks up `hf_host_run_command` in the reference
- **THEN** the entry SHALL show `command-thread`, permission `layout.write` (for mutating commands), and "since 1.0"

### Requirement: SDK and host version compatibility
A plugin built with SDK version `M.n` and `api.minorMin = k` (k ≤ n) SHALL load and run on every host that supports API major M at a minor ≥ k. A newer SDK of the same major SHALL build, without source changes, every plugin that an older SDK of that major built, except for deprecation warnings.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-20)

#### Scenario: Old plugin, newer SDK
- **GIVEN** the `hello-cpp` sample from SDK 1.0
- **WHEN** it is built with SDK 1.4
- **THEN** it SHALL compile, possibly with deprecation warnings, and SHALL pass its tests on a 1.4 host
