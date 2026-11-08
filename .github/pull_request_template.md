<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright 2026 The Hefesto Authors -->

## What and why

<!-- What this pull request changes and why. Link the OpenSpec change (openspec/changes/<id>/) or the
     capability and requirement(s) it implements, e.g. `hefesto-output-determinism` / "Magic string hash table". -->

## How it was tested

<!-- Commands you ran (preset, ctest labels, golden cases, fuzz targets) and on which platforms. -->

## Checklist

### Tests and traceability

- [ ] Every bug fix has a regression test in this pull request.
- [ ] New or changed behavior has tests tagged `[cap:<capability>][req:<Requirement name>]` (Catch2) or a golden case whose `case.toml` names the capability and requirement.
- [ ] Failing fuzz or property-test seeds found while working on this change are committed as regression tests.

### Specs and docs

- [ ] Behavior changes are reflected in the OpenSpec specs (`openspec/specs/` or an `openspec/changes/<id>/` delta), and `openspec validate --all --strict` passes.
- [ ] User-facing or developer-facing docs (`docs/`, `README.md`) are updated where behavior or workflow changed.
- [ ] New diagnostic codes are documented in `docs/diagnostics/codes.md`.

### Licensing and clean-room rule (capability `hefesto-licensing`)

- [ ] New first-party files carry the `SPDX-License-Identifier: Apache-2.0` header (`scripts/check_license_headers.py`).
- [ ] New third-party code, data or fonts are pinned in `cmake/Dependencies.cmake` (or vendored with their license) and listed in `THIRD_PARTY_NOTICES.md` with a license compatible with Apache-2.0 and App Store distribution.
- [ ] No code was copied, translated or closely paraphrased from a GPL-licensed project. Behavior that must match netgen, KLayout or xschem was implemented only from the Hefesto specifications.
- [ ] If this pull request adds or changes **LVS / netlist-comparison** code: no netgen source was used as an implementation reference.
- [ ] If this pull request adds or changes **`.sch` / `.sym` schematic** code: no xschem source was used as an implementation reference.
- [ ] If this pull request adds or changes **OASIS** code: no KLayout source was used as an implementation reference, and KLayout is not linked or bundled (it may only be run as a separate, user-installed program on desktop).
- [ ] Not applicable: this pull request touches none of the areas above.
