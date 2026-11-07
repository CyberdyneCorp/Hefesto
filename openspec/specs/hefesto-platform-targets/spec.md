# Hefesto Platform Targets

## Purpose
Defines what Hefesto is built from and where it runs: the language standard, the supported compilers, and the supported operating systems and architectures. Every other `hefesto-*` capability assumes these targets.

## Requirements

### Requirement: C++20 implementation language
Hefesto SHALL be implemented in ISO C++20. Specifically:
- All first-party code SHALL compile in C++20 mode (`-std=c++20` or the equivalent).
- No first-party code SHALL depend on compiler extensions, except inside isolated, platform-specific source files.
- First-party code MAY use C++20 library features that all supported toolchains provide: concepts, ranges, `std::span`, `std::format` where available, and coroutines.
- Swift is permitted only in the iPadOS shell, and Objective-C++ only in the Apple platform-services layer of the shells (see capability `hefesto-architecture`, decision P-11). The desktop shell on macOS is C++20 on Raven, like the Linux shell.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-1, docs/decisions/product.md#P-11)

#### Scenario: Strict standard build
- **GIVEN** a clean checkout
- **WHEN** the core library is configured with `CMAKE_CXX_STANDARD=20` and `CMAKE_CXX_EXTENSIONS=OFF`
- **THEN** it SHALL compile without errors on every supported toolchain

### Requirement: Supported toolchains
Hefesto SHALL build with the following minimum compilers:

| Compiler | Minimum version | Platform |
|---|---|---|
| GCC | 11 | Linux |
| Clang | 14 | Linux |
| Apple Clang (Xcode) | 15 | macOS, iPadOS |

CI SHALL build with the oldest and newest supported version of each compiler.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-1)

#### Scenario: Oldest GCC
- **WHEN** Hefesto is built on Ubuntu 22.04 with GCC 11
- **THEN** the build and the unit tests SHALL pass

### Requirement: Supported operating systems
Hefesto SHALL run on the following platforms:

| Platform | Minimum version | Architectures |
|---|---|---|
| Linux | glibc 2.31 | x86_64, aarch64 |
| macOS | 14 | arm64 (Apple silicon) only |
| iPadOS | 17 | — |

macOS on Intel (x86_64) processors is not supported (decision D-12, amending P-2). Hefesto SHALL NOT ship x86_64 or universal macOS binaries.

Delivery SHALL be phased (decision P-11): Linux and macOS first, and iPadOS after the desktop release criteria of the roadmap are met. Linux and macOS SHALL provide the full feature set. iPadOS SHALL provide the feature set defined by capability `hefesto-ipad-feature-scope`. Any other platform exception SHALL be listed in the capability it affects and logged as a `[platform]` decision.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-2, docs/decisions/product.md#P-13, docs/decisions/audit/DECISIONS.md#D-12)

#### Scenario: iPad core flow
- **GIVEN** an iPad running iPadOS 17
- **WHEN** a user opens a sky130 layout
- **THEN** the user SHALL be able to edit it, run DRC, extract it and write a SPICE netlist, just as on desktop platforms

#### Scenario: Unsupported OS
- **WHEN** installation is attempted on macOS 13
- **THEN** the package SHALL refuse to install, stating that the minimum is macOS 14

#### Scenario: Intel Mac
- **WHEN** the macOS app is opened on an Intel (x86_64) Mac running macOS 14
- **THEN** it SHALL NOT run, and the published system requirements SHALL state that an Apple silicon Mac is required

### Requirement: Magic data compatibility is a product guarantee
Hefesto SHALL meet every `exact` and `behavioral` requirement of the Magic-compatibility capabilities on all supported platforms. A given input SHALL produce the same results on every platform, subject to the comparison policy of cross-cutting decision X-1.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-1)

#### Scenario: Cross-platform determinism
- **WHEN** the same tech file and layout are extracted on Linux x86_64 and on iPadOS
- **THEN** the `.ext` files SHALL be equivalent under the X-1 comparison policy
