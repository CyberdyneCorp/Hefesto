# Hefesto Build and Distribution

## Purpose
Defines how Hefesto is built with CMake, how third-party dependencies are managed, what CI verifies, and how Hefesto is packaged on each platform.

## Requirements

### Requirement: CMake build system
Hefesto SHALL use CMake 3.25 or later as its only build system. The build SHALL:
- provide CMake presets for each supported platform and configuration
- support out-of-tree builds
- generate Xcode projects for macOS and iPadOS

Building SHALL NOT require any other build tool except per-platform packaging tools.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-1)

#### Scenario: Preset build
- **WHEN** `cmake --preset linux-release && cmake --build --preset linux-release` is run
- **THEN** the core, the CLI and the Linux app SHALL be built

#### Scenario: iPadOS project
- **WHEN** the `ipados` preset is configured
- **THEN** CMake SHALL generate an Xcode project targeting iPadOS 17 or later

### Requirement: Dependency policy
Third-party dependencies SHALL be:
- pinned to exact versions
- fetched or vendored reproducibly (CMake FetchContent or a vendored tree)
- license-compatible with Hefesto's license

The core library SHALL NOT depend on any library unavailable on iPadOS. Raven SHALL be pinned to an exact commit, and only the desktop shell may use it. The macOS package SHALL bundle MoltenVK.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-1)

#### Scenario: Offline rebuild
- **GIVEN** a populated dependency cache
- **WHEN** the project is rebuilt with networking disabled
- **THEN** the build SHALL succeed

### Requirement: Continuous integration matrix
CI SHALL build and test the following on every pull request:
- Linux x86_64 with GCC and Clang
- Linux aarch64
- macOS arm64 (the only macOS architecture, decision D-12; there is no macOS x86_64 leg)
- an iPadOS simulator build, from the start of the iPadOS milestone

CI SHALL also run:
- `openspec validate --all --strict`
- unit tests
- golden compatibility tests, once golden data exists

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-2)

#### Scenario: Spec drift blocked
- **WHEN** a pull request changes a spec so that it no longer validates
- **THEN** CI SHALL fail

### Requirement: Packaging per platform
Hefesto SHALL be distributed as follows:

| Platform | Package | Requirements |
|---|---|---|
| Linux | AppImage, plus at least one native package format | runs on glibc 2.31 or later |
| macOS | signed and notarized app bundle | arm64 only (Apple silicon), macOS 14 or later; no x86_64 or universal binary (decision D-12) |
| iPadOS | App Store / TestFlight build | — |

Each package SHALL include:
- the default technologies (the equivalents of `minimum`, `scmos` and `gdsquery`)
- the display style files
- the fonts needed by `exact` output features

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-2, docs/decisions/audit/DECISIONS.md#D-12)

#### Scenario: Fresh macOS install
- **WHEN** the notarized app is opened on macOS 14 for the first time
- **THEN** it SHALL start without Gatekeeper warnings and offer the bundled technologies
