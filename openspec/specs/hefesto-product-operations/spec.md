# Hefesto Product Operations

## Purpose
Defines how Hefesto runs as a shipped product (decision P-22):
- opt-in crash reporting and usage telemetry that never collect design data
- signed desktop updates with stable and beta channels
- App Store updates on iPadOS
- published minimum system requirements

## Requirements

### Requirement: Opt-in crash reporting
Hefesto SHALL capture crashes on every platform. A crash report SHALL contain:
- a stack trace
- the app version, the commit, and the OS and GPU versions
- the last diagnostic codes, from capability `hefesto-diagnostics`

A crash report SHALL NOT contain any of the following:
- layout, schematic or netlist content
- cell, net, label or file names
- PDK files
- agent prompts

Reports SHALL be sent only if the user has opted in. When the user has not opted in, the next launch SHALL offer to show the pending report and to send it once. On iPadOS, the system crash reporting SHALL be used, subject to the user's system-level choice.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-22)

#### Scenario: No design data in crash reports
- **GIVEN** a crash while editing cell `secret_adc`
- **WHEN** the crash report is generated
- **THEN** it SHALL NOT contain the string `secret_adc` or any geometry

### Requirement: Opt-in usage telemetry
Usage telemetry SHALL be off by default. When the user enables it, it SHALL collect only counts and timings of:
- feature use, such as commands invoked and dialogs opened
- performance metrics from capability `hefesto-performance`

It SHALL never collect design content or names. Before enabling, the user SHALL see the data categories. The user SHALL be able to view the queued data, and to disable telemetry and delete the queued data at any time. The iPadOS privacy disclosure SHALL match the collected categories.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-22)

#### Scenario: Inspect before sending
- **WHEN** a user opens "Telemetry → View pending data"
- **THEN** the exact queued records SHALL be shown in readable form

### Requirement: Desktop update channels
The desktop app SHALL check for updates on two channels, stable (the default) and beta. Updates SHALL be:
- downloaded over HTTPS
- verified against a signature from the Hefesto release key before installation
- installed only after user confirmation

The update mechanism by platform SHALL be:
- macOS: the notarized app, updated in place
- Linux AppImage: AppImage update information (zsync)
- Linux native packages: the platform package manager

Update checks SHALL be disableable. An update SHALL never change a project's pinned PDK version or the `magic-compat` setting.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-22)

#### Scenario: Tampered update
- **WHEN** a downloaded update's signature does not verify
- **THEN** the update SHALL be discarded, and an error diagnostic SHALL be shown

### Requirement: iPadOS updates
On iPadOS, updates SHALL be delivered only through the App Store, with TestFlight used for beta builds. Hefesto SHALL NOT download executable code. Content that is not code MAY be downloaded, for example PDK packs and templates. Such content SHALL be signed and verified, as specified in capability `hefesto-pdk-manager`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-22)

#### Scenario: Template update on iPad
- **WHEN** new project templates are published
- **THEN** the iPad app SHALL download them as verified content, without an app update

### Requirement: Minimum system requirements
Hefesto SHALL publish minimum and recommended system requirements, and the installers and the app SHALL check them at startup:

| Platform | OS minimum | GPU | Memory |
|---|---|---|---|
| Linux | glibc 2.31 or later, x86_64 or aarch64 | Vulkan 1.2 capable | minimum 8 GB, recommended 16 GB |
| macOS | 14 or later | Metal-capable, used through MoltenVK | minimum 8 GB, recommended 16 GB |
| iPadOS | 17 or later | M1 or later recommended, A14 minimum | minimum 4 GB, recommended 8 GB (the iPad memory bound of `hefesto-performance` is set on an 8 GB M1 iPad Pro) |

On a system below the minimum, the app SHALL explain what is missing and SHALL NOT crash. The values SHALL be revised against the benchmarks of capability `hefesto-performance`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-22)

#### Scenario: No Vulkan driver
- **WHEN** the desktop app starts on a Linux machine without a Vulkan 1.2 driver
- **THEN** it SHALL show a message that names the missing requirement, and the headless CLI SHALL still work
