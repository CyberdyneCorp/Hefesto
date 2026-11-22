## ADDED Requirements

### Requirement: Process boundary for external engines
Hefesto SHALL keep each external engine's licence boundary visible in its build:
- **netgen.** netgen SHALL NOT be linked, vendored, distributed or started by any Hefesto product binary. It MAY run only as a subprocess in the golden-capture and benchmark containers, as a reference. netgen SHALL therefore not appear in the third-party notices.
- **ngspice on Linux and macOS.** ngspice SHALL be linked only into the worker executable `hefesto-simd`, as the shared library `libngspice-hefesto`, which a user can replace with a rebuilt one. The editor, the headless command-line tool and the core library SHALL NOT link ngspice.
- **ngspice on iPadOS.** It SHALL be linked as decision D-28 settles.
- **Build manifest.** It SHALL record ngspice's source, patches and configure options. The source archive of each release SHALL include the ngspice sources it was built from, the MPL-2.0 OSDI files included.

A CI check SHALL fail when an ngspice symbol appears in a binary other than `hefesto-simd` and `libngspice-hefesto`, or when the ngspice API header is included outside the engine adapter.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5, docs/decisions/product.md#P-7) (See Q-SIM-20, Q-SIM-26, Q-LVS-30, D-28.)

#### Scenario: Engine boundary check
- **GIVEN** a change that calls `ngSpice_Command` from a file in the core library
- **WHEN** CI runs
- **THEN** the engine-boundary check SHALL fail and name the file

#### Scenario: Notices of a desktop release
- **WHEN** the third-party notices of a Linux release are generated
- **THEN** they SHALL list ngspice with KLU, numparam, OSDI and sparse, and SHALL NOT list netgen
