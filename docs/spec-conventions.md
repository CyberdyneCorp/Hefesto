# Hefesto Specification Conventions

Hefesto is a VLSI layout editor written in C++20 for Linux, macOS and iPadOS. It is **data-compatible with Magic VLSI**. The specs in `openspec/specs/` are the normative contract. Each one is reverse-engineered from Magic and written so that it does not depend on Magic's implementation.

- **Magic reference commit:** `RTimothyEdwards/magic` @ `fc5daa9e` (version 8.3.694). A local checkout is at `/Users/leonardo/trabalho/magic`.
- **Overview of Magic:** non-normative summaries are in `docs/reference/magic/`.

## 1. What "compatible" means

Every requirement belongs to one of three compatibility classes. State the class on its own line in the requirement body, right after the SHALL sentence:

```
Compatibility: exact | behavioral | informative | native
```

| Class | Meaning | Typical content |
|---|---|---|
| `exact` | Hefesto MUST produce output that is identical to Magic, or numerically equal within the stated rounding. It MUST accept the same inputs. | File grammars (.tech, .mag, .ext, .res.ext, .net, .sim, SPICE, GDS/CIF/LEF/DEF records), coordinate scaling and rounding, DRC error geometry, generated mask geometry, extracted values, node naming, record ordering |
| `behavioral` | Hefesto MUST give the same results and semantics. The interaction surface may differ (touch, GUI, scripting language). | Editing operations, selection semantics, hierarchy rules, undo grouping, command options as capabilities |
| `informative` | Documents Magic's behavior for reference. Hefesto MAY differ. | Exact message text, Magic-only UI details, host-specific mechanics (X11, Tcl, fork/exec, env vars, /tmp) |
| `native` | A Hefesto product requirement with no Magic counterpart. It ends with `(Hefesto decision: <ref>)` instead of a Magic ref. | Language and toolchain, platforms, architecture, rendering, 3D, touch input, distribution, performance |

## 2. Writing rules

1. **OpenSpec format.** Each spec has the following structure:
   - `# Title`
   - `## Purpose`
   - `## Requirements`
   - `### Requirement: <name>` entries. The first sentence of each MUST contain SHALL or MUST.
   - At least one `#### Scenario:` per requirement, using `- **GIVEN** / **WHEN** / **THEN** / **AND**` bullets.

   `openspec validate <capability> --type spec --strict` MUST pass.
2. **Implementation-independent.** Do not name Magic C functions, structs or globals in requirement text. Describe observable behavior, data semantics and algorithms in mathematical or geometric terms.
3. **Traceability.** End each requirement body with `(Magic ref: path/file.c:line[, ...])`. This is the only place Magic code is named. `native` requirements end with `(Hefesto decision: docs/decisions/<file>#<id>)` instead. Product specs use the `hefesto-` capability prefix.
4. **Be precise enough to reimplement.** Specify all of the following:
   - Units, integer vs float, rounding and truncation direction, overflow behavior, and limits.
   - Defaults, allowed values, and keyword prefix-matching rules.
   - Record order and the exact output formatting for `exact` items (for example, printf formats like `%.3f` and spacing).
   - Edge cases: empty input, degenerate geometry, zero, negative values, duplicates, non-manhattan geometry, arrays, transforms.
   - For algorithms (DRC rules, mask operators, extraction formulas), the exact geometric or numeric definition. Use formulas and pseudo-code where it helps.
5. **Grammars.** Give each file format as EBNF in a fenced code block, inside the relevant requirement. Also give one complete annotated example per format.
6. **Scenarios.** Use concrete numbers and concrete inputs and outputs worked out from the code. Prefer scenarios that can become automated compatibility tests. A scenario whose expected value you could not derive with confidence gets the suffix `(verify against Magic)` in its name.
7. **Quirks and bugs.** Specify what Magic actually does. If the behavior looks accidental, add an entry to `docs/decisions/open-questions/<area>.md` with an ID (`Q-<AREA>-<n>`) and refer to that ID in the requirement (`See Q-DRC-3.`). The Hefesto team decides later whether to replicate or fix it. Never silently "fix" behavior in a spec.
8. **Host-specific behavior.** Some Magic behavior depends on the host: Tcl, X11, environment variables, `fork`/`exec`, `fcntl` locks, `/tmp`, signals, or the `$CAD_ROOT` layout. Specify the abstract intent as `behavioral`, put the Magic mechanism in an `informative` requirement, and log a platform decision in the open-questions file (tag `[platform]`). iPadOS has no `fork`/`exec`, no environment variables, sandboxed files and touch input. Tcl is available on every platform, iPadOS included, as the sandboxed interpreter of decision X-3 (`docs/decisions/cross-cutting.md#X-3`).
9. **Scope ownership.** Write only the capability folders assigned to you. Refer to other capabilities by name (for example, "see capability `tech-file-format`"). Do not duplicate them.
10. **Size.** Split a large area into several capabilities so that each `spec.md` stays under roughly 1500 lines. Prefix the capability names as assigned.

## 3. Test-case inventory (for later golden-output capture)

For each area, write `tests/golden/<area>/CASES.md`. It is a table of test cases to be produced later by running Magic headless. Columns:

`ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered`

Magic is not run yet. This file only lists the cases.

## 4. Open-questions file format

`docs/decisions/open-questions/<area>.md` holds one entry per item:

```
### Q-<AREA>-<n>: <short title>
- Kind: bug | quirk | doc-mismatch | platform | ambiguity
- Magic behavior: ...
- Evidence: path/file.c:line
- Options: replicate | fix (describe) | platform alternative
- Recommendation: ...
```
