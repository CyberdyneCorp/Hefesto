# Hefesto Licensing

## Purpose
Defines Hefesto's licensing obligations. Hefesto's specifications are derived from several sources:
- Magic, under a UC Berkeley permissive notice
- netgen, under the GPL
- KLayout, consulted for OASIS behavior, under the GPL
- ngspice, under a modified BSD license. Some ngspice parts use other licenses: KLU and numparam are LGPLv2, OSDI is MPL-2.0, and the `table` code models and FFTW3 are GPL. The GPL parts are excluded. Static linking of the LGPL parts on iPadOS is tracked in `docs/decisions/open-questions/SIM.md`.

Hefesto also bundles third-party data: PDK files, fonts and Tcl. GPL-licensed code is incompatible with App Store distribution and with a permissive Hefesto license. So Hefesto may reproduce the behavior of GPL tools but must never contain their code. Hefesto's own source code is licensed under Apache-2.0 (decision P-9).

## Requirements

### Requirement: Clean-room rule for GPL-sourced behavior
Hefesto SHALL NOT contain code copied, translated or closely paraphrased from GPL-licensed projects, including netgen and KLayout. Behavior that must match those tools SHALL be implemented only from the Hefesto specifications. Contributors working on the netgen-compatible LVS and the OASIS features SHALL work from the specs, not from the GPL sources.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5, docs/decisions/product.md#P-6)

#### Scenario: Code review gate
- **WHEN** a pull request adds LVS code
- **THEN** the review checklist SHALL require confirming that no netgen source was used as an implementation reference

### Requirement: Attribution for permissive sources
Hefesto SHALL reproduce the copyright and permission notices for permissively licensed material that it includes or derives code from. This covers:
- Magic's UC Berkeley notice, for any ported logic and the bundled scmos/minimum tech files, including the scmos MOSIS copyright notice
- Tcl's BSD-style license
- ngspice's license
- zlib

The notices SHALL appear in an in-app "Acknowledgements" screen and in a third-party notices file in each package.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7)

#### Scenario: Acknowledgements present
- **WHEN** a user opens About → Acknowledgements
- **THEN** the Magic, Tcl, ngspice and zlib notices SHALL be listed

### Requirement: Bundled fonts and PDK data licensing
Bundled fonts SHALL be distributed under their own licenses with the required notices, for example FreeSans under the GPL with the font exception. Fonts SHALL be kept as separate data files and SHALL NOT be compiled into code. PDK data that Hefesto bundles or downloads SHALL keep its upstream license files. The sky130, gf180mcu and IHP PDKs use Apache-2.0, to be confirmed for each pack.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-2)

#### Scenario: Curated PDK pack
- **WHEN** a curated sky130 pack is installed
- **THEN** its upstream LICENSE and NOTICE files SHALL be installed with it

### Requirement: Hefesto is licensed under Apache-2.0
Hefesto's first-party source code SHALL be licensed under the Apache License 2.0. The repository root SHALL contain the license text in `LICENSE`. Every bundled third-party component SHALL be compatible with Apache-2.0 distribution and with App Store distribution.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-9)

#### Scenario: License compatibility gate
- **WHEN** a release candidate is prepared
- **THEN** the release checklist SHALL fail if any bundled component's license is incompatible with Apache-2.0 or App Store distribution
