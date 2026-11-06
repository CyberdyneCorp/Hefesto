# Hefesto Licensing

## Purpose
Defines Hefesto's licensing obligations. Hefesto's specifications are derived from several sources:
- Magic, under a UC Berkeley permissive notice
- netgen, under the GPL
- KLayout, consulted for OASIS behavior and as the external sign-off runner of decision P-18, under the GPL
- xschem, the reference for the schematic file formats and netlisting (decision P-12), under the GPL
- ngspice, under a modified BSD license. Some ngspice parts use other licenses: KLU and numparam are LGPLv2, OSDI is MPL-2.0, and the `table` code models and FFTW3 are GPL. The GPL parts are excluded. Static linking of the LGPL parts on iPadOS is tracked in Q-SIM-1.

Hefesto also bundles third-party code and data: Tcl, ngspice, zlib, the Raven GUI library and MoltenVK on desktop, fonts, and PDK files. GPL-licensed code is incompatible with App Store distribution and with a permissive Hefesto license. So Hefesto may reproduce the behavior of GPL tools but must never contain their code. Hefesto's own source code is licensed under Apache-2.0 (decision P-9).

## Requirements

### Requirement: Clean-room rule for GPL-sourced behavior
Hefesto SHALL NOT contain code copied, translated or closely paraphrased from GPL-licensed projects, including netgen, KLayout and xschem. Behavior that must match those tools SHALL be implemented only from the Hefesto specifications. Contributors working on the netgen-compatible LVS, the OASIS features and the xschem-compatible schematic editor SHALL work from the specs, not from the GPL sources. KLayout SHALL only ever be run as a separate, user-installed program on desktop (decision P-18). It SHALL NOT be linked or bundled.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5, docs/decisions/product.md#P-6, docs/decisions/product.md#P-12, docs/decisions/product.md#P-18)

#### Scenario: Code review gate
- **WHEN** a pull request adds LVS code
- **THEN** the review checklist SHALL require confirming that no netgen source was used as an implementation reference

#### Scenario: Schematic reader review
- **WHEN** a pull request adds `.sch` parsing code
- **THEN** the review checklist SHALL require confirming that no xschem source was used as an implementation reference

### Requirement: Attribution for permissive sources
Hefesto SHALL reproduce the copyright and permission notices for permissively licensed material that it includes or derives code from. This covers:
- Magic's UC Berkeley notice, for any ported logic and the bundled scmos/minimum tech files, including the scmos MOSIS copyright notice
- Tcl's BSD-style license
- ngspice's license, together with the separately licensed ngspice parts that are linked: KLU (LGPLv2), numparam (LGPLv2 or newer), OSDI (MPL-2.0) and sparse (capability `simulation-engine-embedding`), and the license of each compact model compiled in for OSDI (Q-SIM-2)
- zlib
- the Zstandard decoder used for PDK assets (capability `hefesto-pdk-manager`)
- Raven (Apache-2.0) and the code and fonts it bundles: stb_truetype and stb_image, nanosvg, and Roboto and Roboto Mono
- MoltenVK (Apache-2.0), bundled in the macOS package
- the sparse linear solver library of capability `reliability-ir-drop`, if one is used instead of Hefesto's own (for example Eigen, MPL-2.0; Q-RELIABILITY-9)

Copyleft parts (LGPL, MPL) SHALL be used only in the form that their license and App Store distribution allow, as decided in Q-SIM-1 and Q-RELIABILITY-9.

The notices SHALL appear in an in-app "Acknowledgements" screen and in a third-party notices file in each package.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-9, docs/decisions/product.md#P-11)

#### Scenario: Acknowledgements present
- **WHEN** a user opens About → Acknowledgements
- **THEN** the Magic, Tcl, ngspice (including KLU, numparam and OSDI) and zlib notices SHALL be listed, and on macOS the Raven and MoltenVK notices as well

### Requirement: Bundled fonts and PDK data licensing
Bundled fonts SHALL be distributed under their own licenses with the required notices. These are:
- Inter and JetBrains Mono, the UI fonts of capability `hefesto-design-system`, under the SIL Open Font License 1.1
- Roboto and Roboto Mono, bundled with Raven, under their upstream licenses
- FreeSans, where it is bundled for font labels, under the GPL with the font exception

Fonts SHALL be kept as separate data files and SHALL NOT be compiled into code. PDK data that Hefesto bundles or downloads SHALL keep its upstream license files. The sky130, gf180mcu and IHP PDKs use Apache-2.0, to be confirmed for each pack.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-9, docs/decisions/product.md#P-23)

#### Scenario: Curated PDK pack
- **WHEN** a curated sky130 pack is installed
- **THEN** its upstream LICENSE and NOTICE files SHALL be installed with it

#### Scenario: UI font notices
- **WHEN** a user opens About → Acknowledgements
- **THEN** the Open Font License notices for Inter and JetBrains Mono SHALL be listed

### Requirement: Hefesto is licensed under Apache-2.0
Hefesto's first-party source code SHALL be licensed under the Apache License 2.0. The repository root SHALL contain the license text in `LICENSE`. Every bundled third-party component SHALL be compatible with Apache-2.0 distribution and with App Store distribution.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-9)

#### Scenario: License compatibility gate
- **WHEN** a release candidate is prepared
- **THEN** the release checklist SHALL fail if any bundled component's license is incompatible with Apache-2.0 or App Store distribution
