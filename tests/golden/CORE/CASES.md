# CORE golden cases

Cross-cutting core behavior with no area of its own (capabilities `hefesto-output-determinism`, `hefesto-magic-compat-mode`, `hefesto-diagnostics`). Captured cases live in `cases/<ID>/`; the workflow is in `tools/golden/README.md`.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| CORE-HASH-01 | hefesto-output-determinism | no tech, no layout; `input/commands.hef`: `hefesto::debug::hashorder` runs covering the spec scenarios, size rounding, growth at 3 × size, signed key bytes and removal. Reference tool: `magic-hash-harness` (Magic's own `utils/hash.c` at the pinned commit behind a driver). Hefesto side uses the M0-only command `hefesto::debug::hashorder` (design D8) | stdout: one line per command, keys in iteration order (`bytes`) | Magic string hash table |

Status: CORE-HASH-01 passes against `hefesto-cli`, but its `expected/` is still provisional (`reference_captured: false` in `provenance.json`): it was produced by the spec model runner, not yet by the reference harness. Re-capture with `capture.py --runner local` or the container runner to close task 4.5 of `m0-foundations`.
