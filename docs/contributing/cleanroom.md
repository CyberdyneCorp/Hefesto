# Clean-room process for the schematic editor and LVS

This page holds two clean rooms under the same requirement: xschem for the schematic editor (M8, the sections below) and netgen for LVS (M9, section [netgen clean room for LVS (M9)](#netgen-clean-room-for-lvs-m9)). The two rosters are separate: being on one team of one clean room says nothing about the other.

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
| DEVLIB | `assets/schematic/xschem_library/devices/` | implementation |
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

## netgen clean room for LVS (M9)

Hefesto's LVS (M9) is compatible with netgen, which is licensed GPL. netgen is never linked, vendored, shipped or started by a Hefesto product; it runs only in the golden-capture and benchmark containers, as a reference (`hefesto-licensing`, Requirement "Process boundary for external engines"). The eight `lvs-*` specs and `docs/decisions/open-questions/LVS.md` were written by reading netgen's source, so Requirement "Two-team clean-room process for specs written from GPL source" applies to them as it does to xschem (user decision Q-LVS-30 (a), 2026-11-22; design D2 and D3 of `openspec/changes/m9-lvs-simulation`). Read this section before you take any M9 LVS task.

### The two LVS teams

| Team | Does | May read | Never |
|---|---|---|---|
| **LVS spec team** | writes and changes the eight `lvs-*` specs (`lvs-netlist-input`, `lvs-setup-file`, `lvs-comparison-algorithm`, `lvs-device-reduction`, `lvs-property-matching`, `lvs-hierarchy`, `lvs-reports`, `lvs-command`), `docs/decisions/open-questions/LVS.md` and the texts that depend on captures; answers LVS spec issues; owns this process and its checks | netgen's source, its manual, its behaviour, the PDK data | implements an LVS capability; writes LVS tests or authors LVS golden inputs; commits a file from netgen's distribution; writes code paraphrase into a spec |
| **LVS implementation team** | writes `src/core/src/lvs/`, its unit tests and the LVS golden inputs (`tests/golden/LVS/`), from the specs only | the specs, netgen's public reference manual, netgen's outputs captured by CI, the PDK data | reads netgen's source; follows `Evidence:` lines or `(netgen ref: ...)` traces; asks a spec-team member what the source does |

A third role, **outside both teams**, reviews: the reviewer of this section against `hefesto-licensing` (task 2.1), the M9 security reviewer (design D22) and, before Desktop 1.0, legal counsel.

The simulation workstreams are on neither team: the `simulation-*` specs were written from ngspice, which is not GPL. A session on neither LVS team may take them, and may take PDK, NGBUILD and the other "either" workstreams below.

### Workstreams by team (design D3)

| Workstream | Paths | Team |
|---|---|---|
| SPEC | `openspec/specs/lvs-*`, `docs/decisions/open-questions/LVS.md`, the `docs/decisions/` texts fixed by captures | LVS spec |
| CLEANROOM | this page, `scripts/check_cleanroom.py`, the pull-request template | LVS spec |
| LVSREAD | `lvs/net/`, `lvs/read/` | LVS implementation |
| LVSSETUP | `lvs/setup/` | LVS implementation |
| LVSALG | `lvs/cmp/` | LVS implementation |
| LVSRED | `lvs/reduce/` | LVS implementation |
| LVSPROP | `lvs/prop/` | LVS implementation |
| LVSHIER | `lvs/hier/` | LVS implementation |
| LVSREP | `lvs/report/` | LVS implementation |
| LVSCMD | `lvs/cmd/` | LVS implementation, and never a netgen reader (roster rule) |
| GOLDEN, LVS inputs | `tests/golden/LVS/` inputs (task 3.5) | LVS implementation |
| GOLDEN, other | capture image, comparators, SIM and SIMADV inputs (`tools/golden/`, `tests/golden/{SIM,SIMADV}/`) | either |
| SCHEMSIM | `schem/sim/`, the simulation subcommands of `schem/cmd/xschem_command.cpp` | M8 implementation team (xschem clean room); 16.3 never a netgen reader (roster rule) |
| SECURITY | review record | outside both LVS teams |
| PDK, NGBUILD, SIMD, PLATFORM, SCAFFOLD, SIMENG, SIMDECK, SIMRES, SIMPROBE, SIMCMD, SIMADV, SIMMEAS, MAPS, DESIGN, SHELL, PERF, INTEGRATION | as design D3 | either |

### Roster rule

A session that read netgen source, for any reason and at any time in M9, is on the LVS spec team for the whole of M9 and does not switch. It never takes:
- LVSCMD (task 12, `lvs-command`), or
- SCHEMSIM 16.3 (schematic-to-layout cross-probing and the schematic-driven LVS command),

because both implement LVS behaviour. It also never takes another LVS implementation workstream or authors LVS golden inputs (task 3.5). The maintainer who assigns sessions checks the roster below before assigning any of these tasks, and refuses the assignment otherwise (`hefesto-licensing` scenario "netgen reader stays off the LVS command").

### Rules for LVS implementation sessions

1. **Work in a worktree of this repository.** Its only netgen material is the specs. Do not clone, download, unpack or open netgen's repository or an archive of it, and do not browse its code online.
2. **Stay out of the capture image's source tree.** The golden capture image builds netgen 1.5.327 from source in `/opt/src/netgen`. Never open that directory, and never run the image interactively to look inside it. netgen's outputs come back through CI (`golden-capture.yml`) as `expected/` files, which you may read.
3. **Do not follow source references.** `Evidence:` lines in `LVS.md` and `(netgen ref: ...)` traces in the `lvs-*` specs exist for traceability. Do not open the files they name, and do not search for them. Do not copy a trace into code or tests (`scripts/check_cleanroom.py` rejects one under `src/` or `tests/`).
4. **Public manual and black-box runs are allowed.** netgen's public reference manual is user documentation; you may read it. You may run the netgen binary as a black box in the capture runner (through CI) and read what it prints.
5. **When the spec does not say, ask.** Raise an LVS spec issue (below). Do not fill a gap by guessing at netgen's code, by asking someone what the code does, or by reading code that quotes it.
6. **Golden inputs are yours or the PDK's.** Inputs under `tests/golden/LVS/` are written from the specs, or taken from the Apache-2.0 PDK data sets of `scripts/fetch_pdk.py` (the NG-PDK cases use the three setup files of Q-LVS-31: `sky130A_setup.tcl` from `sky130-xschem`, and the `gf180mcu-netgen` and `ihp-netgen` sets). Each `case.toml` names its `provenance` (`"hefesto"`, or `"pdk:<set>:<path>"`). Never commit a file from netgen's distribution (its test netlists, its `tech` files) or from `/opt/src/netgen`.
7. **Do not take spec tasks.** A task of `openspec/changes/m9-lvs-simulation/tasks.md` marked for the LVS spec team (section 2, task 3.8) is not for you.

Instruction block for LVS implementation agent sessions:

```text
You are on the M9 LVS IMPLEMENTATION team (netgen clean room, docs/contributing/cleanroom.md).
Work only in this worktree. Never fetch, clone, open or search netgen's source code, its repository,
any archive of it, or /opt/src/netgen in the capture image. Never open the files named by `Evidence:`
lines or `(netgen ref: ...)` traces. You may read the lvs-* specs, netgen's public reference manual,
the PDK data of scripts/fetch_pdk.py and the expected/ outputs captured by CI. When a spec does not
answer a question, stop and raise an LVS spec issue (cleanroom.md, "Raising an LVS spec issue"); do not guess.
```

### Rules for LVS spec sessions

1. You may read netgen's source and documentation and run netgen. Read the source without putting it on the shared machine: fetch single files of a pinned tag (netgen 1.5.327) and never clone, download or save netgen's source into a worktree, the capture cache or any other directory an implementation session can reach. Record what you read as `Evidence:` lines or `(netgen ref: ...)` traces, for traceability.
2. Write behaviour, not code: no source identifiers that are not file-format, command or user-visible names, no structure of netgen's C or Tcl, no pseudo-code mirroring C. The paraphrase review (task 2.2, `docs/decisions/m9-paraphrase-review.md`) is the reference; it starts with `lvs-comparison-algorithm` and `lvs-device-reduction`, which describe internal procedures, and needs a second spec-team signature.
3. Never commit a file from netgen's distribution, and never paste netgen code into a spec issue answer.
4. Never implement an LVS capability, write its tests or author its golden inputs; never take LVSCMD or SCHEMSIM 16.3 (roster rule).
5. Draft new decisions as `proposed` and leave approval to the user.

### Raising an LVS spec issue

As for the schematic editor (above), with the capability one of the eight `lvs-*` specs and the label `lvs-spec`. Use the **LVS spec issue** template (`.github/ISSUE_TEMPLATE/lvs-spec-issue.md`). The answer is spec text, never an excerpt or summary of netgen's code, within one working day.

### LVS checks

| Check | Where |
|---|---|
| No committed file carries netgen's GPL header; every `tests/golden/LVS/` case declares a `provenance` that is Hefesto or a PDK data set, and none names netgen's distribution or `/opt/src/netgen`; no `netgen ref:` trace under `src/` or `tests/` (captured `expected/` output and Markdown documents excepted). A tree without `tests/golden/LVS/` passes. | `scripts/check_cleanroom.py`, run by `.github/workflows/openspec-validate.yml` |
| The author of LVS code, tests or golden inputs confirms they have not read netgen's source | checkbox in [the pull-request template](../../.github/pull_request_template.md) |
| The reviewer confirms the author is not on the LVS spec team, and that an LVSCMD or SCHEMSIM 16.3 author is no netgen reader | reviewer items in the same template |
| Spec text describes behaviour | the paraphrase review (task 2.2), before G1 and for every later `lvs-*` change |
| Counsel review of the LVS process | joins the counsel review already due before Desktop 1.0 (M13 exit item; `docs/roadmap.md`, de-risking schedule "Before Desktop 1.0", which names Q-LVS-30) |

### LVS roster

Roles, not names; agent sessions are listed by role.

| Team | Role | Who |
|---|---|---|
| LVS spec | Spec authors | the sessions that wrote the eight `lvs-*` specs and `LVS.md` |
| LVS spec | CLEANROOM (tasks 2.1, 2.3, the template half of 2.4), PDK data sets (3.1) and the comparators (3.4) | the M9 round 0 session of branch `m9-round0-cleanroom-pdk` (2026-11-24); it read netgen's licence header (`base/netgen.c`, lines 1-2) at the git tag `1.5.327` for `check_cleanroom.py`. That tag is commit `8df5f0d9…`, not the spec pin `93dcda14` of `tools/golden/pins.toml` ("the tag is NOT used"); a spec-team session is to re-check the two lines at `93dcda14`. It never takes LVSCMD, SCHEMSIM 16.3 or task 3.5 |
| LVS spec | Paraphrase review (2.2) and its second signature; capture-fixed texts (3.8) | spec-team sessions, assigned per task |
| LVS spec | Paraphrase review (2.2), first reviewer | the M9 round 1 session of branch `m9-round1-lvsspec` (2026-11-25); it read one netgen file (`base/netgen.c` at the spec pin `93dcda14`) by a single web fetch of the raw file, saved nothing to disk, and is on the LVS spec team for the whole of M9. It never takes LVSCMD, SCHEMSIM 16.3, task 3.5 or another LVS implementation task |
| LVS implementation | One session per workstream (LVSREAD, LVSSETUP, LVSALG, LVSRED, LVSPROP, LVSHIER, LVSREP, LVSCMD, GOLDEN LVS inputs) | implementation-team sessions, each started with the instruction block above |
| LVS implementation | SCAFFOLD (task 4.2: `src/core/include/hefesto/lvs/**`, `src/core/src/lvs/**` including the LVSCMD-owned `lvs/cmd/commands.cpp`, `tests/unit/lvs/scaffold_test.cpp`) | the M9 round 0 session of branch `m9-round0-scaffold` (2026-11-24). Attestation: it never read netgen's source; its session log has no web fetch and no tool use that opens a netgen repository, archive or `/opt/src/netgen` (audited 2026-11-24 by the R0 fix pass) |
| LVS implementation | INTEGRATION of round 0 (registration of `lvs` in `cmd/all_commands.cpp`, shared files, task notes) | the M9 round 0 session of branch `m9-round0` (2026-11-24). Attestation: it never read netgen's source; same audit, no web fetch at all (its only `/opt/src/netgen` mentions are task-note text it wrote) |
| LVS implementation | LVSREAD (tasks 6.1-6.2: `lvs/net/`, `lvs/read/`) | the M9 round 1 session of branch `m9-round1-lvsread` (2026-11-25), assigned as an implementation-team session that never reads netgen source and started with the instruction block above. Its attestation is part of the clean-room roster attestation that awaits the user |
| LVS implementation | R0 fix pass (the `lvs` usage text in `lvs/cmd/commands.cpp`, the scaffold stubs) | the M9 round 0 fix session (2026-11-24); it never read netgen's source and read only the two sessions' tool-use logs, filtered to the netgen source paths, for the audit above |
| Outside both | Clean-room process reviewer (task 2.1) | *to be assigned* (has not read netgen's source, writes no LVS code) |
| Outside both | Security reviewer (design D22) | *to be assigned* |
| Outside both | Legal counsel (M13, before Desktop 1.0) | *to be assigned* |

The maintainer adds sessions here by role when they start, and never assigns an LVS spec-team session to an LVS implementation task, LVSCMD or SCHEMSIM 16.3.

### How this section maps to `hefesto-licensing`

For the reviewer outside both teams: each rule of Requirement "Two-team clean-room process for specs written from GPL source", as modified for netgen (Q-LVS-30), and the written rule that implements it.

| Rule of the requirement | Written rule in this section |
|---|---|
| Separate roles: who read the source does not implement; implementers do not read it | "The two LVS teams"; implementation rules 1–3, 7; spec rule 4; the LVS items of the pull-request template |
| A netgen reader never works on the LVS command or schematic-driven LVS | "Roster rule"; spec rule 4; the reviewer item of the template; roster |
| No source references followed | implementation rule 3; the `netgen ref:` check of `check_cleanroom.py` |
| No source tree reachable; running the netgen binary and reading the public documentation allowed | implementation rules 1, 2 and 4; spec rule 1; the instruction block |
| Spec review for paraphrase | spec rule 2; task 2.2; "LVS checks" |
| Hefesto-authored golden inputs, no file of netgen's distribution | implementation rule 6; the `provenance` and GPL-header checks of `check_cleanroom.py` |
| Counsel review for each tool before Desktop 1.0, not blocking M9 | "LVS checks", last row; roster |

Reviewer sign-off: *pending* (task 2.1 asks a reviewer outside both teams to confirm each row).
