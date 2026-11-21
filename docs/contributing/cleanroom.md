# Clean-room process for the schematic editor

Hefesto's schematic editor (M8) is compatible with xschem, which is licensed GPL-2.0-or-later. Hefesto is Apache-2.0 and must not contain xschem's code. The schematic specs were written by reading xschem's source, so the work is split between two teams that never mix (decision J-1 of `docs/decisions/m8-decision-table.md`; capability `hefesto-licensing`, Requirement "Two-team clean-room process for specs written from GPL source"; design D2 of `openspec/changes/m8-schematic`).

This page is the process. Read it before you take any M8 task. The general rule for all GPL tools (netgen, KLayout, xschem) is in [the clean-room rule](README.md#clean-room-rule).

## The two teams

| Team | Does | May read | Never |
|---|---|---|---|
| **Spec team** | writes and changes the `schematic-*` specs, `docs/decisions/open-questions/SCHEM.md`, the system-library interface table and the capture-fixed texts; answers spec issues; owns this process and its checks | xschem's source, its manual, its behaviour, the PDK data | implements a schematic capability; commits a file from xschem's distribution; writes code paraphrase into a spec |
| **Implementation team** | writes the code, the unit tests, the golden-case inputs and the system symbol library | the specs, the public xschem manual (`xschem_man/*.html`), xschem's outputs captured by CI, the PDK data | reads xschem's source; follows `Evidence:` lines or `(xschem ref: ...)` traces; asks a spec-team member what the source does |

A third role, **outside both teams**, reviews: the reviewer of this process (task 1.1), the security reviewer of the script paths (design D14) and, before Desktop 1.0, legal counsel (task 1.5).

A person or agent session joins one team for the whole of M8 and does not switch. A spec-team member who wants to implement later in M8 cannot; a spec author never implements a capability whose spec cites xschem.

### Workstreams by team (design D3)

| Workstream | Paths | Team |
|---|---|---|
| CLEANROOM | `docs/contributing/cleanroom.md`, `scripts/check_cleanroom.py`, the pull-request template | spec |
| SPEC | `openspec/specs/schematic-*`, `docs/decisions/open-questions/SCHEM.md`, the interface table, the capture-fixed texts | spec |
| PDK | `scripts/schem_inventory.py`, `scripts/fetch_pdk.py` (reads only the Apache-2.0 PDK data) | either |
| GOLDEN | `tests/golden/SCHEM/` inputs, `tools/golden/` | implementation (inputs) |
| SCAFFOLD, COMPAT | `src/core/src/schem/` layout, `compat/`, `settings/` | implementation |
| FORMAT | `schem/file/` | implementation |
| SYMLIB | `schem/sym/`, `schem/lib/`, `schem/tcl/` | implementation |
| DEVLIB | `assets/schematic/devices/` | implementation |
| NETLIST | `schem/net/` | implementation |
| EDIT, CMD, SIMINT | `schem/edit/`, `schem/cmd/`, `schem/sim/` | implementation |
| DESIGN, SHELL | `docs/design/`, `src/shell/desktop/src/schematic_*.cpp` | implementation |
| SECURITY | review record | outside both teams |

## Rules for implementation sessions

Every implementation session, human or agent, follows these rules. Agent sessions get them in their instructions (the block below).

1. **Work in a worktree of this repository.** Its only xschem material is the specs. Do not clone, download, unpack or open xschem's repository or an archive of it, and do not browse its code online.
2. **Stay out of the capture image's source tree.** The golden capture image builds xschem from source in `/opt/src/xschem`. Never open that directory, and never run the image interactively to look inside it. Its outputs come back through CI (`golden-capture.yml`) as `expected/` files, which you may read.
3. **Do not follow source references.** `Evidence:` lines in `SCHEM.md` and `(xschem ref: ...)` traces in the specs exist for traceability. Do not open the files they name, and do not search for them. Do not copy a trace into code or tests (`scripts/check_cleanroom.py` rejects one under `src/` or `tests/`).
4. **The public manual is allowed.** xschem's manual pages (`xschem_man/*.html`, published on the xschem web site) are public documentation. You may read them, as you may run the xschem binary as a black box in the capture runner (through CI).
5. **When the spec does not say, ask.** Raise a spec issue (below). Do not fill a gap by guessing at xschem's code, by asking someone what the code does, or by reading code that quotes it.
6. **Golden inputs are yours or the PDK's.** Inputs under `tests/golden/SCHEM/` are written from the specs, or taken from the Apache-2.0 PDK data sets of `scripts/fetch_pdk.py`. Each `case.toml` names its `provenance` (`"hefesto"`, or `"pdk:<set>:<path>"`). Never commit a file from xschem's distribution (`xschem_library`, its `tests` or `examples`). Three IHP PDK files carry xschem's GPL header and must not be committed either: `sg13g2_pr/inductor.sym`, `sg13g2_pr/inductor3.sym` and `sg13g2_tests_xyce/portsource.sym` (found 2026-11-17).
7. **Draw the system symbols yourself.** DEVLIB takes file names, pins and netlisting properties from the interface table of `schematic-symbols-and-libraries` "System library" only, and draws the graphics independently.
8. **Do not take spec tasks.** A task marked "(spec team)" in `openspec/changes/m8-schematic/tasks.md` is not for you.

Instruction block for implementation agent sessions:

```text
You are on the M8 IMPLEMENTATION team (clean room, docs/contributing/cleanroom.md).
Work only in this worktree. Never fetch, clone, open or search xschem's source code, its repository,
any archive of it, or /opt/src/xschem in the capture image. Never open the files named by `Evidence:`
lines or `(xschem ref: ...)` traces. You may read the specs, the public xschem manual (xschem_man/*.html),
the PDK data of scripts/fetch_pdk.py and the expected/ outputs captured by CI. When a spec does not
answer a question, stop and raise a spec issue (cleanroom.md, "Raising a spec issue"); do not guess.
```

## Rules for spec sessions

1. You may read xschem's source and documentation and run xschem. Record what you read as `Evidence:` lines or `(xschem ref: ...)` traces, for traceability.
2. Write behaviour, not code: no source identifiers that are not file-format or user-visible names, no structure of xschem's awk or Tcl programs, no pseudo-code mirroring C. The paraphrase review (`docs/decisions/m8-paraphrase-review.md`) is the reference for what that means.
3. Extract the system-library interface (file names, pins, positions, netlisting properties) into the spec table; describe no graphics beyond a bounding box.
4. Never commit a file from xschem's distribution, and never paste xschem code into a spec issue answer.
5. Never implement a schematic capability, write its tests or author its golden inputs.
6. Draft new decisions as `proposed` and leave approval to the user.

## Raising a spec issue

An implementer who finds a requirement unclear, incomplete or contradicted by a capture opens a GitHub issue with the **Schematic spec issue** template (`.github/ISSUE_TEMPLATE/schem-spec-issue.md`, label `schem-spec`). It names the capability, the exact requirement heading, the question, and the test or code that needs the answer. It never quotes xschem's source.

The spec team answers within one working day (design R1) by changing the spec in a pull request that links the issue, or by pointing to the text that already answers it, and closes the issue. The answer is spec text, never an excerpt or summary of xschem's code. A gap that blocks a workstream for longer is reported as an INTEGRATION open item.

## Checks

| Check | Where |
|---|---|
| No committed file carries xschem's GPL header; every `tests/golden/SCHEM/` case declares a `provenance` that is Hefesto or a PDK data set; no `xschem ref:` trace under `src/` or `tests/` (captured `expected/` output and Markdown documents excepted) | `scripts/check_cleanroom.py`, run by `.github/workflows/openspec-validate.yml` |
| The author of schematic code confirms they have not read xschem's source | checkbox in [the pull-request template](../../.github/pull_request_template.md) |
| The reviewer confirms the author is not on the spec team, and that golden inputs are Hefesto-authored or PDK data | reviewer items in the same template |
| Spec text describes behaviour | the paraphrase review, before G1 and for every later spec change |
| Counsel review of the process and of the interface table | an M13 exit item (`docs/roadmap.md`, de-risking schedule "Before Desktop 1.0") |

## Roster

Roles, not names; agent sessions are listed by role.

| Team | Role | Who |
|---|---|---|
| Spec | Spec authors | the sessions that wrote the six `schematic-*` specs and `SCHEM.md` (2026-11-06 to 2026-11-13) |
| Spec | M8 gate work | the spec-team session of tasks 1.1–1.5 and 2.1–2.5 (branch `m8-gates`, 2026-11-17) |
| Spec | Second paraphrase reviewer | second spec-team agent session (branch `m8-paraphrase-review-2`, 2026-11-20), assigned by the maintainer |
| Spec | SPEC workstream (tasks 3.6, 4.1–4.3) and spec-issue answers | spec-team sessions, assigned per task |
| Implementation | One session per workstream (SCAFFOLD, COMPAT, FORMAT, SYMLIB, DEVLIB, NETLIST, EDIT, CMD, SIMINT, GOLDEN inputs, DESIGN, SHELL) | implementation-team sessions, each started with the instruction block above |
| Outside both | Clean-room process reviewer (task 1.1) | the maintainer (has not read xschem's source, writes no schematic code), signed 2026-11-20 |
| Outside both | Security reviewer (task 12.1) | *to be assigned* |
| Outside both | Legal counsel (M13) | *to be assigned* |

The maintainer who assigns sessions adds them here by role when they start, and never assigns a spec-team session to an implementation task.

## How this page maps to `hefesto-licensing`

For the reviewer outside both teams: each rule of Requirement "Two-team clean-room process for specs written from GPL source" and the written rule that implements it.

| Rule of the requirement | Written rule on this page |
|---|---|
| Separate roles: who read the source does not implement; implementers do not read it | "The two teams" (table and the one-team-for-M8 rule); implementation rules 1–3, 8; spec rule 5; the reviewer item of the pull-request template |
| No source references followed | implementation rule 3; the `xschem ref:` check of `check_cleanroom.py` |
| No source tree reachable | implementation rules 1–2 and the instruction block |
| Running the xschem binary in the capture runner is allowed | implementation rules 2 and 4 |
| Spec review for paraphrase | spec rule 2; `docs/decisions/m8-paraphrase-review.md`; "Checks" |
| Hefesto-authored golden inputs | implementation rule 6; the `provenance` and GPL-header checks of `check_cleanroom.py` |
| Interface data extracted by the spec side only, graphics drawn independently | spec rule 3; implementation rule 7 |
| Counsel review before Desktop 1.0, not blocking M8 | "Checks", last row; roster |

Reviewer sign-off: the maintainer, outside both teams, confirmed each row on 2026-11-20. One gap found in row "No source references followed": implementation rule 3 forbids an `xschem ref:` trace in code or tests, but `check_cleanroom.py` checked only `src/`; extending the check to `tests/` was handed to the spec team (task 1.3 follow-up).
