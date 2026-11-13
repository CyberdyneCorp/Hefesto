# Hefesto Licensing

## Purpose
Defines Hefesto's licensing obligations. Hefesto's specifications are derived from several sources:
- Magic, under a UC Berkeley permissive notice
- netgen, under the GPL
- KLayout, consulted for OASIS behavior and as the external sign-off runner of decision P-18, under the GPL
- xschem, the reference for the schematic file formats and netlisting (decision P-12), under the GPL
- ngspice, under a modified BSD license. Some ngspice parts use other licenses: KLU and numparam are LGPLv2, OSDI is MPL-2.0, and the `table` code models and FFTW3 are GPL. The GPL parts are excluded. Static linking of the LGPL parts on iPadOS is tracked in Q-SIM-1.

Hefesto also bundles third-party code and data: Tcl, ngspice (with its LGPL and MPL parts), zlib, Zstandard, a JSON library, a glTF writer, libsodium and libcurl on Linux, the Raven GUI library on desktop, MoltenVK on macOS, fonts, and PDK files. On Apple platforms it uses the system CryptoKit and URLSession frameworks instead of bundled cryptography and networking libraries (requirement "Third-party libraries for cryptography, networking and data formats"). GPL-licensed code is incompatible with App Store distribution and with a permissive Hefesto license. So Hefesto may reproduce the behavior of GPL tools but must never contain their code. Hefesto's own source code is licensed under Apache-2.0 (decision P-9).

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

### Requirement: Two-team clean-room process for specs written from GPL source
Where a Hefesto spec was written by reading GPL source, the spec work and the implementation SHALL be done by two separate teams. (User decision 2026-11-13, judgement items J-1 and J-3 of `docs/decisions/m8-decision-table.md`.) Today this covers the xschem-compatible schematic capabilities (`schematic-file-format`, `schematic-symbols-and-libraries`, `schematic-editing`, `schematic-netlisting`, `schematic-simulation-integration`, `schematic-command`), whose specs and open-question entries (`docs/decisions/open-questions/SCHEM.md`) were written from xschem's source. The process SHALL be:
- **Separate roles.** A contributor or agent session that read the GPL source of a tool SHALL NOT implement a capability whose spec it wrote or that cites that tool. An implementer SHALL NOT read that GPL source.
- **No source references followed.** Implementers SHALL work from the spec text only. They SHALL NOT follow the `Evidence:` lines of open-question entries or the `(xschem ref: ...)` traces of the specs, which exist for traceability only.
- **No source tree reachable.** No xschem source tree, clone, archive or code view SHALL be present on a machine, or in the context, of an implementation session. Running the xschem binary in the golden capture runner is allowed: its outputs are black-box behaviour.
- **Spec review for paraphrase.** Spec text SHALL describe behaviour, not code. Before an implementation workstream starts on a requirement, its text SHALL be reviewed for literal code paraphrase (source variable or function names that are not part of the file format or user interface, the structure of a GPL awk or Tcl program, pseudo-code that mirrors C), and any such text SHALL be rewritten as behaviour.
- **Hefesto-authored golden inputs.** Golden-case inputs SHALL be written by Hefesto contributors or taken from permissively licensed data such as the Apache-2.0 PDK libraries. Files from the GPL tool's own distribution (for example xschem's `xschem_library` or `tests` files) SHALL NOT be committed as inputs or expected outputs.
- **Interface data.** The interface facts of the clean-room system symbol library (capability `schematic-symbols-and-libraries`, Requirement "System library": file names, pin names, order and positions, and the netlisting properties `type`, `format`, `template`, `global`) have no public source other than xschem's GPL `devices/*.sym` files. They SHALL be extracted from those files by the spec side only, as a table in that spec, and implementers SHALL take them from that table. Symbol graphics SHALL be drawn independently.
- **Counsel review.** Legal counsel SHALL review this process once, including the interface table above, before the Desktop 1.0 release. The review SHALL NOT block M8.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-9, docs/decisions/product.md#P-12)

#### Scenario: Implementer never sees xschem source
- **GIVEN** an agent or contributor session assigned to implement `schematic-netlisting`
- **WHEN** the session starts
- **THEN** no xschem source tree SHALL be reachable from it, and its instructions SHALL forbid following `Evidence:` paths and `(xschem ref: ...)` traces

#### Scenario: Spec author does not implement
- **GIVEN** a contributor who wrote a requirement of `schematic-netlisting` by reading xschem's source
- **WHEN** a pull request implementing that capability lists the contributor as its author
- **THEN** the review checklist SHALL reject it

#### Scenario: GPL golden input refused
- **WHEN** a pull request adds a golden case whose input is a file from xschem's own distribution
- **THEN** the review checklist SHALL reject it and ask for a Hefesto-authored input

#### Scenario: Counsel review before Desktop 1.0
- **WHEN** the Desktop 1.0 release checklist runs
- **THEN** it SHALL fail unless the counsel review of the two-team process and of the system-library interface table is recorded

### Requirement: Attribution for permissive sources
Hefesto SHALL reproduce the copyright and permission notices for permissively licensed material that it includes or derives code from. This covers:
- Magic's UC Berkeley notice, for any ported logic and the bundled scmos/minimum tech files, including the scmos MOSIS copyright notice
- Tcl's BSD-style license
- ngspice's license, together with the separately licensed ngspice parts that are linked: KLU (LGPLv2), numparam (LGPLv2 or newer), OSDI (MPL-2.0) and sparse (capability `simulation-engine-embedding`), and the license of each compact model compiled in for OSDI (Q-SIM-2)
- zlib (zlib license)
- Zstandard (`zstd`, BSD-3-Clause), used for PDK assets (capability `hefesto-pdk-manager`)
- the JSON library (nlohmann/json, MIT) and the glTF writer (cgltf, MIT, or tinygltf, MIT) of requirement "Third-party libraries for cryptography, networking and data formats"
- on Linux: libsodium (ISC) or Monocypher (BSD-2-Clause or CC0-1.0), libcurl (curl license, MIT-style) and, where the package bundles it, the TLS library libcurl uses (OpenSSL 3.x, Apache-2.0)
- Raven (Apache-2.0) and the code and fonts it bundles: stb_truetype and stb_image, nanosvg, and Roboto and Roboto Mono
- MoltenVK (Apache-2.0), bundled in the macOS package
- the sparse linear solver library of capability `reliability-ir-drop`, if one is used instead of Hefesto's own (for example Eigen, MPL-2.0; Q-RELIABILITY-9)

Copyleft parts (LGPL, MPL) SHALL be used only in the form that their license and App Store distribution allow, as decided in Q-SIM-1 and Q-RELIABILITY-9.

The notices SHALL appear in an in-app "Acknowledgements" screen and in a third-party notices file in each package.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-9, docs/decisions/product.md#P-11)

#### Scenario: Acknowledgements present
- **WHEN** a user opens About → Acknowledgements
- **THEN** the Magic, Tcl, ngspice (including KLU, numparam and OSDI), zlib, Zstandard, nlohmann/json and glTF writer notices SHALL be listed, on desktop the Raven notices, on macOS the MoltenVK notice, and on Linux the libsodium (or Monocypher) and libcurl notices as well

### Requirement: Third-party libraries for cryptography, networking and data formats
Hefesto SHALL obtain SHA-256 hashing, Ed25519 signature verification and signing, HTTPS, Zstandard and zlib decompression, JSON parsing and writing, and glTF writing from the following components, and SHALL NOT bundle any other library for these functions:

| Function | macOS and iPadOS | Linux | License |
|---|---|---|---|
| SHA-256 | CryptoKit (`SHA256`), Apple system framework | libsodium (`crypto_hash_sha256`) | system framework; libsodium: ISC |
| Ed25519 verify and sign | CryptoKit (`Curve25519.Signing`), Apple system framework | libsodium (`crypto_sign_*`); Monocypher with its `monocypher-ed25519` module MAY replace libsodium | system framework; libsodium: ISC; Monocypher: BSD-2-Clause or CC0-1.0 |
| HTTPS client | URLSession (Foundation), Apple system framework | libcurl, with certificate verification against the system trust store | system framework; libcurl: curl license (MIT-style); its TLS library, OpenSSL 3.x: Apache-2.0 |
| Zstandard | zstd | zstd | BSD-3-Clause (zstd is dual-licensed BSD-3-Clause or GPLv2; Hefesto uses it under BSD-3-Clause) |
| zlib (deflate, gzip) | zlib | zlib | zlib license |
| JSON | nlohmann/json, header-only | nlohmann/json, header-only | MIT |
| glTF 2.0 writing | cgltf (`cgltf_write.h`), header-only; tinygltf MAY replace it | same as Apple platforms | MIT (both) |

Rules:
- The Apple frameworks are reached through the platform-services layer of capability `hefesto-architecture`. CryptoKit and URLSession are Swift and Objective-C APIs, so the desktop macOS build contains a thin bridge to them. No framework type crosses into the C++20 core.
- If Monocypher replaces libsodium on Linux, SHA-256 SHALL come from the TLS library already linked by libcurl (OpenSSL 3.x `libcrypto`), because Monocypher has no SHA-256.
- Every component SHALL be pinned to an exact version in the build (capability `hefesto-build-and-distribution`), and its notice SHALL be listed as required by requirement "Attribution for permissive sources".
- Results SHALL be identical on every platform: the same input bytes give the same SHA-256 digest, an Ed25519 signature that verifies on one platform verifies on all of them, and the same 3D model gives the same glTF content.

Compatibility: native

(Hefesto decision: docs/decisions/audit/DECISIONS.md#D-13, docs/decisions/product.md#P-9)

#### Scenario: Plugin signature verified on Linux and macOS
- **GIVEN** a `.hfplugin` package signed with an Ed25519 key on macOS through CryptoKit
- **WHEN** it is verified on Linux through libsodium
- **THEN** the signature SHALL verify, and the SHA-256 digests in `SIGNATURE` SHALL match those computed on Linux

#### Scenario: No extra crypto library on Apple platforms
- **WHEN** the macOS or iPadOS package is inspected
- **THEN** it SHALL NOT contain libsodium, Monocypher, libcurl or OpenSSL

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
