# M9 decision table: LVS, simulation and advanced analog simulation

M9 ("LVS, simulation and advanced analog simulation → Desktop Beta", `docs/roadmap.md`) cannot start until every open question its specs cite has a `Resolution:` line. This table lists what still has to be decided:

| Group | Count |
|---|---|
| Entries reported as blocking by `python3 scripts/check_milestone_readiness.py --milestone M9` (13 Q-LVS, 15 Q-SIM, 6 Q-SIMADV) | 34 |
| Entries resolved only in draft or in part: Q-SIM-1 (a draft that waits for D-28) and Q-SIMADV-5 (the open `mm_ok` part) | 2 |
| Audit decisions marked "blocks M9" in `docs/decisions/audit/DECISIONS.md` | 7 (D-22 to D-28) |
| New entries raised by the M9 plan (`openspec/changes/m9-lvs-simulation`): Q-LVS-30 to -36, Q-SIM-20 to -28, Q-SIMADV-13, Q-SCHEM-39 | 18 |
| Planning questions from the design's "Open Questions" | 4 |

**Status: drafted 2026-11-22, approved by the user 2026-11-22.** Every row was approved with its recommended option, with no option changed ("Decisions of 2026-11-22" below). INTEGRATION recorded the answers (m9 tasks 1.1–1.4): each entry now has `Status: approved 2026-11-22` and a `Resolution:` line, D-22 to D-28 have a `Status:` line, and `check_milestone_readiness.py --milestone M9` reports `M9: ready`. The tables below keep the recommendations as drafted; the column "Recommendation" is the approved decision.

**Round 1 status (2026-11-25).** No new user decision this round. Still open, awaiting the user: F1 (ngspice-47 statistical `.param` patch; task 3.3 waits), F2 (patch 0004 approval), Q-SIM-27 (Beta cell drawn in Hefesto), the netgen header provenance re-check at `93dcda14`, the clean-room roster attestation, the outside reviewer for the licensing mapping table (`docs/contributing/cleanroom.md`), and the Q-SIM-22 enforcement gap. None of these is approved.

**Closed in the round-1 fix pass without a user decision:** OI-1 (`lvs-netlist-input` cell lookup now folds brackets as Q-LVS-3's approved Resolution says; the spec text was aligned with the decision and the code). **Raised in round 1, awaiting the user (not decided).** LVS spec team: OI-3 (behavioural rewordings of the `Resolution:` lines of Q-LVS-3, -5, -23, -24; `m9-paraphrase-review.md`). LVSREAD: the Hefesto guard of 65536 `.INCLUDE` reads and 64 x the logical-line limit of include bytes per read (`LVS-READ-025`) as an addition to Q-LVS-36; the include-depth limit counting the top file; the device limit counted per netlist database. SIMENG/SIMPROBE: layout probes use `getnode`'s normal naming, not the positional `@=` names of `getnode fast`; an array element outside the array's range fails with `SIM-PROBE-003`. NGBUILD: the per-file SPDX mapping of KLU, AMD, COLAMD, BTF and SuiteSparse_config (for the outside licensing reviewer) and the task 5.2 route for static OSDI registration (OpenVAF emits linked `.osdi` modules with identical `OSDI_*` symbols).

**Round 2 status (2026-11-26).** The user approved, each with the recommended option: on 2026-11-25 F1 (a), F2 (patch 0004), the Q-SIM-27 layout amendment, the netgen header re-check by the spec team, the Q-SIM-22 enforcement gap, the refusal of a worker count above the pool limit, the exclusion of `d_process`, the per-file SPDX licences of KLU and its SuiteSparse parts, cppduals named in D2, OpenVAF built from source in CI for macOS arm64, and the synced capture-image copy with its CI check; on 2026-11-26, under the user's standing rule for crafted-input resource limits in both modes, the Q-LVS-36 addition `LVS-READ-025` with the include depth counting the top file and the device limit per netlist database. Each is recorded below under "Round 2 decisions" with a `Status:` and a `- Resolution:` line, and in the open-questions entry where one exists (Q-SIM-22, Q-SIM-27 in `SIM.md`; Q-LVS-36 in `LVS.md`). The netgen header re-check found the first and copyright lines of `base/netgen.c` identical at `93dcda14` and at the tag `8df5f0d9`; `check_cleanroom.py` keeps its table. The second spec-team signature of the paraphrase review (task 2.2) is recorded in `m9-paraphrase-review.md`. **Still awaiting the user (not approved):** the clean-room roster attestation and the outside reviewer of the `hefesto-licensing` mapping table (both need the user personally). The six round-1 items that were listed here (probe naming, `SIM-PROBE-003` for an out-of-range array element, static OSDI registration, the OI-3 rewordings, the job log cap, the result budget) were approved on 2026-11-27 ("Round 3 status" below).

**Round 3 status (2026-11-27).** The user approved on 2026-11-27, each with the recommended option, the six round-1 items listed as awaiting in the round 2 status: layout probes use `getnode`'s normal naming (R3-1); an array element outside the array's range fails with `SIM-PROBE-003` (R3-2); static OSDI registration through a generated per-module symbol prefix (R3-3); the OI-3 rewordings of the `Resolution:` lines of Q-LVS-3, -5, -23 and -24 (R3-4, applied in `LVS.md`; OI-3 closed); the 64 MiB in-memory cap of the job log (R3-5, also under the standing rule for crafted-input limits); the 4 GiB desktop result budget until Q-SIM-25 decides it (R3-6). Each is recorded under "Round 3 decisions" with a `Status:` and a `- Resolution:` line, in the open-questions entry where one exists (Q-SIM-2, Q-SIM-16, Q-SIM-24, Q-SIM-25 in `SIM.md`; Q-LVS-3, -5, -23, -24 in `LVS.md`) and as scenarios under existing requirement headings ("Spec changes the recommendations imply", round 3 table). The LVS spec team also wrote into the `lvs-*` specs the choices the implementation team made where the specs were silent (R2 and LVSREAD R1) and that agree with netgen; eleven that differ from netgen, OI-4's `(<flag>)` value among them, are a new item for the user, R3-7 (not approved). **Still awaiting the user (not approved):** R3-7; R3-10 (texts where the round-3 captures differ from the spec; recorded by INTEGRATION, with R3-8, the crafted-input limits approved under the standing rule, and R3-9, behaviours implemented under the standing rule); and, needing the user personally, the clean-room roster attestation and the outside reviewer of the `hefesto-licensing` mapping table.

**Verify rows at G0.** Four rows (Q-SIM-8, Q-SIMADV-3, -4, -9) are settled by reference runs that need the patched capture image, which is built only after G0. They are therefore approved at G0 as rules, recorded as conditional `Resolution:` lines ("verify: (a) if the D-22 run shows the later `.param` taking effect, otherwise (b)"), which the readiness check counts as resolved. Task 3.3 later adds each row's `Verified` line with the run id. A result that no branch of the approved rule covers goes back to the user before gate G2-SIM.

## Decisions of 2026-11-22

The user approved the whole table on 2026-11-22, every row with its recommended option and no option changed. INTEGRATION recorded the answers on branch `m9-round0-decide` (m9 tasks 1.1–1.4):

1. **Q-LVS-30 (J-1), option (a).** Two teams (LVS spec team, LVS implementation team); the MODIFIED `hefesto-licensing` requirement "Two-team clean-room process for specs written from GPL source" adds the eight `lvs-*` capabilities, `LVS.md` and the `(netgen ref: ...)` traces; roster rule: a session that read netgen source is on the LVS spec team for the whole milestone and never takes LVSCMD (task 12) or SCHEMSIM 16.3. The requirement text now says "for the whole milestone", with the scenario "netgen reader stays on the spec team for the milestone".
2. **Q-SIM-7, option "pin ngspice-47".** ngspice-47, tag commit `a80f6e3e`, with a Hefesto-written CMake build. Q-SIM-23 (a): roadmap principle 2 names this pin in place of `7aa47164`; `tools/golden/pins.toml` moves in task 3.2.
3. **Q-SIM-20 and Q-SIM-26 (J-2, J-3; D-28 desktop part), option (a).** A replaceable shared `libngspice-hefesto` behind the `hefesto-ngspice-adapter` plugin, loaded only by the sandboxed `hefesto-simd`; legal review before the Desktop Beta; the iPadOS part is decided before M14.
4. **Q-SIM-22, option (a).** The in-process engine exists only in developer builds, through the same adapter plugin.
5. **Q-SIM-24, option (a).** A `layoutName` field in the net name map, with a connectivity fallback for probe resolution.
6. **Q-SIM-27 (J-4), option (a).** The Beta flow: schematic and layout drawn in Hefesto, DRC with the PDK's Magic tech file, a fixed FO4 load, post-layout `tpd` within 30 % of the schematic run.
7. **Q-SIM-8, Q-SIMADV-3, -4, -9: conditional verify rules.** Each `Resolution:` names the option for every outcome of its reference run (design D1). No `Verified` line is added before task 3.3.
8. **Q-SIMADV-13, option (a).** `N = max(1, min(4, physical_cores − 1, ⌊B_total / B_job⌋))`; a user setting can only lower it. Scenarios "Single-core host", "Setting cannot raise the pool" and "Large post-layout deck limits the pool" are in `simulation-engine-embedding` "Multiple simulations and corner parallelism"; `simulation-corners` "Corner jobs on the worker pool" keeps its own "Large post-layout deck limits the pool".
9. **Everything else as written.** The 34 blocking entries, Q-SIM-1, the rest of Q-SIMADV-5, the 18 plan-raised entries (Q-LVS-30 to -36, Q-SIM-20 to -28, Q-SIMADV-13, Q-SCHEM-39), D-22 to D-28 (`docs/decisions/audit/DECISIONS.md`, a `Status:` line each) and design Open Questions 1–4, all (a) (recorded in `design.md` "Open Questions").

Each entry carries `Status: approved 2026-11-22` and a `Resolution:` line ending "Approved by the user 2026-11-22." The four verify rows say "approved 2026-11-22 as a conditional verify rule". `python3 scripts/check_milestone_readiness.py --milestone M9` reports `M9: ready`.

## Method and sources

No netgen or xschem source was read while drafting this table. For netgen, only the licence file and the public reference manual were fetched. The `Evidence:` lines of `LVS.md` cite netgen source; they are repeated only by entry ID and were not followed. Magic's source (read-only, fc5daa9e) was read for the parts of M9 that touch Magic: `getnode` naming, the netlist window and IRSIM bridge (both M12) and the Tcl glue. The ngspice licence and API documentation are public.

Evidence tags:
- **[E]**: the Hefesto entry in `docs/decisions/open-questions/`;
- **[S]**: a Hefesto capability spec;
- **[D]**: a Hefesto decision (`product.md`, `cross-cutting.md`, `audit/DECISIONS.md`, `roadmap.md`);
- **[code]**: Hefesto at origin/main 072f50c8;
- **[magic]**: Magic 8.3.694 at fc5daa9e;
- **[pdk]**: the pinned PDK trees;
- **[web]**: public documentation (netgen reference manual, ngspice `COPYING` and shared-library page).

## Recommendation kinds

The **Kind** column of the tables below is the kind of the recommendation, as X-2 (`docs/decisions/cross-cutting.md`) defines it, with `netgen-compat` (D-2, default ON) in place of `magic-compat` for the LVS entries. It is not the entry's own `- Kind:` line, which classifies the question (bug, ambiguity, doc-mismatch, platform). The two can differ: Q-LVS-36's entry is a `bug` whose recommendation is a `fix`; Q-SIM-23's entry is a `doc-mismatch` whose recommendation is a process choice (`other`).
- **replicate**: do what netgen does, in both modes.
- **compat**: replicate with `netgen-compat` ON, and use the corrected behaviour with it OFF.
- **fix**: corrected behaviour in both modes. This covers tier 1 (crashes, undefined behaviour, invalid output) and cases where netgen's output is unspecified.
- **platform**: a Hefesto platform decision (X-3, X-4).
- **verify**: settled by a reference run in the capture image (X-6), not by preference.
- **other**: a scope, process or schedule decision.

## Round 2 decisions (2026-11-25 and 2026-11-26)

Recorded by the LVS spec team on branch `m9-round2-decide`. Entries with a Q-row are also recorded in that entry.

### F1: statistical `.param` draws in ngspice-47
- Status: approved 2026-11-25 (user decision, recommended option (a))
- Resolution: (a) — the upstream ngspice change that draws a top-level `.param` with `agauss` and the other statistical functions once per circuit load is carried as Hefesto patch `0005-stat-param-once-per-load.patch`, in the engine (`third_party/ngspice/patches/`) and in the capture image's copy; the engine version becomes `47-hefesto2`. SIMREF implements it. `simulation-monte-carlo` "Statistical functions in ngspice (informative)" (scenario "Top-level statistical parameter drawn once per load") and `simulation-engine-embedding` "ngspice build configuration and license hygiene" (scenario "Patch list in the manifest") name patch 0005. Task 3.3 (the D-22 reference runs and the `Verified` lines of Q-SIM-8, Q-SIMADV-3, -4 and -9) is unblocked.

### F2: `setseed` and the `trnoise` pool
- Status: approved 2026-11-25 (user decision)
- Resolution: patch `0004-deterministic-initialisation.patch` is kept: `setseed` refills the `trnoise`/`trrandom` Wallace pool and the `getpid()` seed is removed, in the engine and in the capture image. `simulation-engine-embedding` "Deterministic engine initialisation" no longer says pending and gains scenario "Reused engine repeats trnoise".

### Q-SIM-27 amendment: Beta layout drawn in Hefesto
- Status: approved 2026-11-25 (user decision)
- Resolution: the `beta_inv` layout has its transistors and its diffusion, poly, contacts and metal1 drawn in Hefesto, with no instance of the PDK inverter cell; SIMREF draws the cells under `tests/e2e/beta/<pdk>/`. Recorded in `SIM.md` Q-SIM-27, J-4 below, design D19 "Beta flow inputs" and task 20.2.

### netgen header provenance re-check at `93dcda14`
- Status: approved 2026-11-25 (user decision: done by a spec-team session through a pinned web fetch)
- Resolution: done 2026-11-26 by the LVS spec team: the pinned raw `base/netgen.c` at `93dcda14d36349d0b0ca63168d3cbbab20fe2ece` opens with the same first line and copyright line as at the tag 1.5.327 (`8df5f0d9`). The header table of `scripts/check_cleanroom.py` is unchanged; its comment records the re-check and a fixture test covers the header as at the spec pin (task 2.3).

### Q-SIM-22 enforcement gap
- Status: approved 2026-11-25 (user decision)
- Resolution: the gap of `check_engine_boundary.py` recorded under task 4.3 is accepted until SIMD task 5.4's release-configuration leg runs the check without `--developer-build` (`SIM.md` Q-SIM-22).

### Worker count above the pool limit
- Status: approved 2026-11-25 (user decision)
- Resolution: a worker-count setting above the host's pool maximum is refused with `SIM-ENGINE-033` (already implemented); `simulation-engine-embedding` scenario "Setting cannot raise the pool" names the code.

### NGBUILD licence and build items
- Status: approved 2026-11-25 (user decision)
- Resolution: the XSPICE `d_process` code model is excluded (`pin.json`, the manifest, the spec's feature table and `THIRD_PARTY_NOTICES.md` already say so); KLU, AMD, COLAMD, BTF and SuiteSparse_config carry an SPDX licence per file in the manifest (`file_licenses`; `simulation-engine-embedding` scenario "Per-file licenses of the SuiteSparse parts"), and the outside licensing reviewer is still to be named by the user; design D2 names cppduals (MPL-2.0) (checked: present); OpenVAF is built from source in CI for macOS arm64 (`ngspice.yml` job `openvaf`); the synced capture-image copy (`tools/golden/container/ngspice/`, kept in step by `third_party/ngspice/tools/sync_image_copy.py`) stays, with its CI check.

### Q-LVS-36 addition: `LVS-READ-025`, include depth, device count
- Status: approved 2026-11-26 (raised by LVSREAD in round 1; approved under the user's standing rule for crafted-input resource limits in both modes, with the recommended values)
- Resolution: a read stops with `LVS-READ-025` after 65,536 `.INCLUDE` reads and after 64 × the logical-line limit of included bytes; the include depth counts the top file; the device limit is counted per netlist database. `lvs-netlist-input` "Input limits" (heading unchanged) gains scenarios "Include depth counts the top file", "Include count cap", "Include byte cap" and "Device limit counts the whole database"; the LVS implementation workstream writes the tests against heading "Input limits" (`LVS.md` Q-LVS-36).

## Round 3 decisions (2026-11-27)

Recorded by the LVS spec team on branch `m9-round3-decide`. The user approved each item on 2026-11-27 with the recommended option. Entries with a Q-row are also recorded in that entry. Spec scenarios are added under existing requirement headings only (no heading added or renamed), so the code workstreams tag their tests to those headings.

### R3-1: layout probes use `getnode`'s normal naming
- Status: approved 2026-11-27 (user decision, recommended option; raised by SIMENG/SIMPROBE in round 1)
- Resolution: a layout probe names the selected node as plain `getnode` does (`irsim-interface` "Hierarchical node names": the region's preferred label, else the extractor's generated name), never with the positional `@=g<x>,<y>`, `@=s<x>,<y>` or `@=d<x>,<y>` names of `getnode fast` ("Fast node names"); a label whose text starts with `@=` is still a label and still wins by rule 1 of "Node name preference order". Same in both `magic-compat` modes (the probe is Hefesto-only). Already implemented (`sim::node_name`, R1). `simulation-ui-and-crossprobe` "Probing a layout net" gains scenario "Probe uses the normal node name", and its scenario "Netlist name differs from the node name" now names the unlabelled node by its generated name instead of an `@=` name; `irsim-interface` "Hierarchical node names" gains a note (`SIM.md` Q-SIM-24).

### R3-2: array element outside the array gives `SIM-PROBE-003`
- Status: approved 2026-11-27 (user decision, recommended option; raised by SIMENG/SIMPROBE in round 1)
- Resolution: when a probe's use path names an array element outside the array's index range (for example `u[5]` of an array `u[0:3]`), the probe fails with `SIM-PROBE-003` `no instance <element> in cell <cell>`, as for any other use-path element that does not exist, in both `magic-compat` modes; no element is clamped or wrapped. `simulation-ui-and-crossprobe` "Probing a layout net" gains scenario "Array element outside the array" (`SIM.md` Q-SIM-24).

### R3-3: static OSDI registration through a generated per-module symbol prefix
- Status: approved 2026-11-27 (user decision, recommended option; raised by NGBUILD in round 1, task 5.2)
- Resolution: each OpenVAF-compiled module's `OSDI_DESCRIPTORS`, `OSDI_NUM_DESCRIPTORS`, `OSDI_VERSION_MAJOR`, `OSDI_VERSION_MINOR` and `OSDI_DESCRIPTOR_SIZE` symbols are renamed at build time with a prefix generated from the module name (for example `hefesto_osdi_psp103_`), and the module is registered in the static table of patch `0001-static-osdi-registry.patch` under its basename; no `.osdi` file is loaded at run time for a built-in module. NGBUILD implements it in task 5.2 (the route: OpenVAF's object output or the linked module's symbols renamed by the build, whichever the pinned OpenVAF allows). Implemented in round 3 (branch `m9-round3-sim`, task 5.2 ticked): `third_party/ngspice/tools/osdi_static.py` renames each module's exports to `hefesto_osdi_<module>_<name>` with `llvm-objcopy --redefine-sym` and generates the static table; ngspice.yml run 46044749511 green. `simulation-engine-embedding` "OSDI compact models" gains scenario "Statically registered modules with per-module symbol prefixes" (`SIM.md` Q-SIM-2).

### R3-4: OI-3 rewordings of the `Resolution:` lines of Q-LVS-3, -5, -23, -24
- Status: approved 2026-11-27 (user decision; proposed by the first paraphrase review, OI-3, after the second spec-team signature)
- Resolution: the four `Resolution:` lines are replaced by the behavioural wordings proposed in `m9-paraphrase-review.md` OI-3, with the same meaning and the same decided behaviour (Q-LVS-3, -5, -23 fix in both modes; Q-LVS-24 compat). The original decision date stays on each line. OI-3 is closed in `m9-paraphrase-review.md`. The other OI-3 candidates (Q-LVS-16, -22, -25) were not part of the proposal and keep their lines.

### R3-5: job log in-memory cap of 64 MiB
- Status: approved 2026-11-27 (user decision, recommended option; under the standing rule for crafted-input resource limits in both modes)
- Resolution: a job keeps at most 64 MiB of log text in memory; the cap is a setting that can be lowered, never raised past twice the default (design D18); a line that would pass the cap is not kept and is counted, and the job records the number of dropped lines; the first `Error` line still fails the job and appears in its diagnostic even when it is dropped from the log. The cap and the count are implemented (`kMaxJobLogBytes`, `JobRecord::log_dropped`, `first_error`, R1 fix pass); making the cap a lowerable setting is left to SIMENG (open item of this round). Implemented in round 3 (branch `m9-round3-sim`): `JobManagerOptions::max_log_bytes`, refused at zero or above twice the default with `SIM-ENGINE-039`. `simulation-analyses-and-results` "Job lifecycle" gains scenario "Job log capped in memory"; design D18's cap table gains the row.

### R3-6: result budget 4 GiB until Q-SIM-25 decides it
- Status: approved 2026-11-27 (user decision, recommended option)
- Resolution: the default result memory budget per job stays 4 GiB on Linux and macOS (512 MiB on iPadOS, unchanged) until a later Q-SIM-25 decision changes it; `worker::Limits::max_result_bytes` and `kDesktopResultBudget` keep 4 GiB. `simulation-analyses-and-results` "Saved vectors and memory budget" gains scenario "Default result budget"; `SIM.md` Q-SIM-25 and Q-SIM-16 note the interim value. Fix pass: the iPadOS half is implemented as `kIpadResultBudget` (512 MiB); `JobManagerOptions::result_budget` defaults to the platform's value (`kPlatformResultBudget`, chosen with `TARGET_OS_IOS`) and its 2x ceiling follows it.

### R3-7: LVS implementation choices that differ from netgen (new, for the user)
- Status: awaiting the user (raised 2026-11-27 by the LVS spec team; not approved)
- Question: while writing the implementation team's choices where the `lvs-*` specs were silent (design "As landed (LVSALG, LVSRED, LVSPROP; M9 R2)", and the LVSREAD round-1 choices of task 6.2) into the specs, the spec team checked each against netgen 1.5.327 at the spec pin `93dcda14` (pinned raw web fetches of `base/netcmp.c`, `base/netgen.c`, `base/flatten.c` and `base/spice.c`; nothing saved to disk). The rows below differ from what netgen does. Each touches a requirement whose compatibility is `exact`. No code was changed and no spec text was written for them; the specs say "not yet specified" where they would have to choose.

| Row | Requirement | Hefesto (as landed) | netgen at `93dcda14` |
|---|---|---|---|
| a | `lvs-comparison-algorithm` "Match verification" | a device pair that failed its property check is not checked again on a later verification | only a pair that passed is skipped; a failed pair is checked again at each verification |
| b | `lvs-device-reduction` "Parallel combination" item 4 (OI-2) | a parallel merge leaves the series-group markers (`_tag`) unchanged | a parallel merge adds group-open and group-close markers to the records when series markers are present |
| c | `lvs-device-reduction` "Series combination" | a device without property records contributes `{M = 1}` to a series merge | such a device contributes a record that holds only the series marker |
| d | `lvs-property-matching` "When properties are checked" (OI-4) | `(<flag>)` prints `1` for no connects and `2` for tied together | it prints `0` for tied together and a non-zero value for no connects (which value is to be fixed by the NG-PROP capture of task 3.8) |
| e | `lvs-property-matching` "Series/parallel network reconciliation", "Retry" (network dump) | one line per record, ` key=value` pairs | a header line naming the circuit and instance followed by `network:`, then each record's series marker alone on a line and each property on its own line, indented two spaces, as `<key> = <value>` |
| f | `lvs-property-matching` "Value types and promotion" (type mismatch line) | ` <key> (property type mismatch)` | ` <key> circuit1: <a>   <b>  (property type mismatch)`, each value printed as its type prints it |
| g | `lvs-property-matching` sort rule | records ordered by exact values; values equal within tolerance but not exactly are ordered by the primary value | within tolerance, records are ordered by the secondary key of the spec's sort table (as the spec already says) |
| h | `lvs-device-reduction` "Zero-valued device removal" | both nets ports: pin 0's net survives; a current source merges its nets like a resistor | both ports: the port that comes first in the cell's port order survives; a zero-valued current source is treated as open (its nets are not merged) |
| i | `lvs-netlist-input` "SPICE primitive device cards" | `V`, `I`, `E` without a model use `v`, `i`, `e` | they use `vsrc`, `isrc` and `vcvs`, and a model token on these cards is ignored |
| j | `lvs-netlist-input` "SPICE primitive device cards" | `<kind>` in `has wrong number of ports for a <kind>.` is the device class name | a fixed word per card: `MOSFET`, `BJT`, `capacitor`, `resistor`, `diode`, `inductor`, `transmission line`, `voltage source`, `current source`, `controlled voltage source` |
| k | `lvs-netlist-input` "Globals, includes and parameters" | `Cannot open include file '<f>'.` | `Error in SPICE file include: No file <f>` (reading continues in both) |

- Options: (A) replicate netgen in every row (the specs are `exact`; the LVS implementation team changes the code in round 4, from spec text the spec team writes, and the NG-ALG, NG-RED, NG-PROP and NG-READ captures of task 3.8 pin the texts); (B) keep Hefesto's choices and mark each row `netgen-compat`-OFF behaviour, with netgen's behaviour for ON; (C) row by row.
- Recommendation: (A) for rows b–f and h–k (output and reduction results that golden cases compare byte for byte); for row a, (A) as well, unless the capture shows the repeated check duplicating report lines, in which case keep Hefesto's choice and document the difference; for row g, (A): the spec's sort table already states it, and Q-LVS-8 asked only for a consistent order. Row d supersedes the 1/2 values of the R2 as-landed note; OI-4 stays open until the user decides.
- Confirmed and written into the specs this round (no difference found, or no netgen evidence against): `Circuit <c> contains no ...` names the cell; `Netlists do not match.` only for `verify only` and `verify properties`; `restart` forgets property results (netgen's behaviour was not established from the fetched text; written as Hefesto's choice); a port's net is never a no-connect in parallel reduction; a fold keeps `M` (or `S`) on a record with no additive value; records collapse when every unfolded value matches within tolerance; zero-valued removal keeps the port net when one terminal is a port, else pin 0's net; `L`/`T` default models `l`/`t`; a call's extra connections are ignored and missing ones left unconnected; a missing include does not stop the read; warnings and the 100-warning cap counted per file. The Verilog placeholder redefined in SPICE is already specified ("SPICE subcircuit definitions"); `CORE-NOTIMPL-009` there is the interim state until task 6.3.

### R3-8: crafted-input limits of round 3 (standing rule)
- Status: approved 2026-11-27 (user's standing rule of 2026-11-25: new crafted-input resource limits in both modes are approved with the recommended value; recorded by INTEGRATION of round 3)
- Resolution: all in both `netgen-compat`/`magic-compat` modes, each a setting that can be lowered and never raised past twice the default (D18) unless marked fixed:
  - LVS setup evaluation: step budget 50 million Tcl commands and wall-time backstop 60 s per evaluation (`SetupContext::step_budget`, `time_budget`; `LVS-SETUP-004`; a bad setting `LVS-SETUP-008`). The time backstop exists because Tcl counts only invoked commands, so `while {1} {}` never reaches a step budget.
  - LVS setup file size: 16 MiB, fixed (`LVS-SETUP-007`).
  - Report column widths of `format`: 1 to 4096 (`LVS-REPORT-001`; the Q-LVS-16 implementation, widths at or below zero rejected per its `Resolution:`).
  - `lvs -noflatten`: at most 65 536 entries of at most 4096 bytes each, fixed (`LVS-CMD-010`). `flatten` during setup reuses the 50-million device limit (Q-LVS-36).
  - Simulation run sets: the level count of one sweep or DOE block is refused before allocation once it passes the 100 000-point limit (`SIM-RUNSET-001`; the crafted case was a `lin` step of 1e-300); the run-set point and Monte Carlo sample limits (100 000) are settings refused at zero or above twice the default (`SIM-RUNSET-015`).
  - Job log cap and result budget settings (R3-5, R3-6): refused at zero or above twice the default (`SIM-ENGINE-039`).
  - Fix pass (review of PR #140): flattening during the `lvs` flow (unmatched subcells, pre-match, mismatched pairs) stops when one cell would hold more than 50 million device instances (`LVS-HIER-006`, input error; `RunOptions::device_limit`, lowerable only), reusing the Q-LVS-36 device limit; a netlist whose subcircuits each hold two instances of the next would otherwise reach 2^depth devices. The setup `flatten <class>` form stops at the same limit, as `flatten` of a cell's contents already did. The `-noflatten` file is read at most 65 536 × 4097 bytes and its entry limit is checked line by line.

### R3-9: netgen behaviours no Q-row covers, implemented under the standing rule (round 3)
- Status: implemented 2026-11-27 under the standing rule (a Magic/netgen behaviour no Q-row covers is implemented and reported); for the user to confirm
- netgen 1.5.327 exits with signal 11 (exit 139) on the `ignore` commands of NG-SET-10 and on a stray `.ends` before any definition (the first NG-READ-04 input). Hefesto never crashes there, in both modes (crash fixes apply in both modes). NG-SET-10 stays capture-blocked.
- `lvs` with a setup file that does not exist: `lvs-setup-file` "Setup file evaluation" says it ends the command (`Error:  Setup file <f> does not exist.`). LVSCMD had made it a warning (`LVS-CMD-009`) with default settings; INTEGRATION follows the spec (`LVS-SETUP-002`, headless status 2) and retires `LVS-CMD-009`. A name containing `nosetup` continues without a setup.
- A non-equivalent pair logs the net and device mismatch reports, then `Netlists do not match.`; until LVSALG exposes its class partition all mismatch fragments of a pair form one class (only the grouping can differ from netgen in canonical comparison).

### R3-10: setup and reader texts where the CI captures differ from the spec (new, for the user)
- Status: awaiting the user (raised 2026-11-27 by LVSSETUP and LVSREAD findings in the round-3 captures; not approved)
- Question: the NG-SET and NG-PDK captures (black-box netgen output, allowed to the implementation team) show behaviour the `lvs-*` specs state differently:
  - a. Cell reference syntax: `{cell fnum}` and `{fnum cell}` fail with `No such cell or bad file number!` (spec: `No such cellname!` / `No such file number!`). Hefesto follows the capture.
  - b. `equate classes` failure is a Tcl error with result 0 after the printed message. Hefesto follows the capture.
  - c. `property` options match by unique prefix, and a non-option third word adds the property while leaving the bad-option message as the result (NG-SET-06); tolerance lists print doubles for string types. Hefesto follows the capture.
  - d. `Unable to permute ...` and the `nosetup` note go to stderr. Hefesto follows the capture.
  - e. Q-LVS-16: the `Resolution:` (fix, reject widths at or below zero) contradicts the spec text and the capture (NG-SET-13: netgen applies `0 0`). Hefesto follows the `Resolution:`, so NG-SET-13 needs a decision deviation entry.
  - f. `global top VDD*` returned empty and changed nothing in NG-SET-12 (spec: returns the count).
  - g. LVSREAD: netgen's placeholder for an undefined subcircuit call takes the call's parameters as properties (no `No property w/l` messages in NG-PDK-01) and `model` reports its class as `subcircuit` (NG-SET-11); Hefesto's placeholder has only `M` and class `module`. netgen's `M`-card model lists `W L M` (NG-SET-03, -05) and the `R`-card `r` model only `{value double 0.01}`; Hefesto has `L W M` and `value` plus `M`.
- Options: (A) the LVS spec team amends the specs to the captured behaviour (a-d, f, g) and the implementation team aligns the code in round 4 (g); for e, keep the `Resolution:` and add the NG-SET-13 deviation entry; (B) keep the spec texts and change the code back.
- Recommendation: (A); the captures are netgen's observable behaviour and the specs are `exact`.

### Still awaiting the user personally (not approved)
- The clean-room roster attestation (`docs/contributing/cleanroom.md` "LVS roster").
- The outside reviewer of the `hefesto-licensing` mapping table (`docs/contributing/cleanroom.md`), which also covers the per-file SPDX mapping of the SuiteSparse parts.

## Judgement items

Six items need the user's decision on product, legal, schedule or process grounds. Each lists the recommended option.

- **J-1 (Q-LVS-30), how the LVS clean room is applied.** The approved `hefesto-licensing` requirement "Two-team clean-room process for specs written from GPL source" already covers specs written from GPL source, which the `lvs-*` specs were, so a single team is not an option. (a) Apply it as M8 did: separate spec and implementation teams, a MODIFIED delta that adds the eight `lvs-*` capabilities, `LVS.md` and `netgen ref:` traces to the requirement, a roster rule that keeps netgen readers off LVSCMD and SCHEMSIM 16.3, a paraphrase review, `check_cleanroom.py` extended, Hefesto-authored golden inputs, and the counsel review joined to the Desktop 1.0 item. (b) would rewrite the specs from the public manual and black-box runs only.
- **J-2 (Q-SIM-20, Q-SIM-22, D-28 desktop part), the ngspice licence boundary on desktop.** (a) Build a shared `libngspice-hefesto` behind a separate adapter plugin, `hefesto-ngspice-adapter`, that in release builds only the worker executable `hefesto-simd` loads (a developer build may load it in the editor for debugging). The editor, the CLI and the core never link ngspice, and a CI check enforces this. On macOS `hefesto-simd` alone disables library validation, so that users can actually replace the library, and it runs sandboxed. The intended LGPL position (a replaceable library in a separate process, no relink kit) goes through D-28's legal review before the Desktop Beta; the review can change the link type without touching the editor.
- **J-3 (Q-SIM-26), when each linking question is settled.** D-28 is about the iPadOS app and already says "must be settled before M14", as the roadmap does; there is no conflict. (a) Decide the new desktop question (J-2) in R0 and review it before the Desktop Beta, and keep D-28's iPadOS choice before M14 after the same review. The roadmap and D-28 gain a sentence naming the desktop part.
- **J-4 (Q-SIM-27), the Desktop Beta sign-off.** (a) A beta flow for each of the three PDKs on Linux x86_64 and macOS arm64:
  - an inverter schematic `beta_inv.sch` drawn in Hefesto and simulated in a shared testbench with a fixed FO4 load;
  - a layout cell `beta_inv` drawn in Hefesto, transistors, diffusion, poly, contacts and metal1 included, with no instance of the library inverter (amended 2026-11-25; first drafted as "around the library inverter"), clean under Hefesto's DRC with the PDK's Magic tech file (KLayout sign-off is not part of Beta), written to GDS;
  - extraction;
  - an LVS match against `beta_inv.sch`;
  - post-layout `rc` simulation of `beta_inv` in the same testbench, with `tpd` within 30 % of the schematic run.
  
  Each flow is an end-to-end CI case, followed by the user's sign-off with real input, as for Desktop Alpha.
- **J-5 (design Open Question 1), how spec changes are recorded.** (a) As spec deltas in the change, synced to the living specs once G0 passes. M7 and M8 edited the living specs after approval instead. Deltas let the recommendations be reviewed before anything is approved.
- **J-6 (design Open Question 2), when M9 starts.** (a) R0 and R1 (decisions, clean room, data sets, capture, engine build, scaffold, worker service) start when the plan is approved. R2–R3 (the LVS core and the simulation engine on SPICE and deck inputs) start in parallel with M7 and M8 implementation. R4 (the layout and schematic sources, cross-probing, schematic integration) starts after M7 and M8 exit.

## Decision table: the blocking entries

### LVS

| ID | Question | Kind | Recommendation | Evidence |
|---|---|---|---|---|
| Q-LVS-1 | Legacy `.ext` reader drops modern device records | **other** | Keep `readnet ext` for scripts and replicate its gap. Offer no GUI entry. LVS layout input always goes through the internal `ext2spice lvs` path. | [E]; [S] `lvs-command` |
| Q-LVS-2 | Report order inherits netgen hash and partition order | **fix** | Use a deterministic order. The comparator treats the hash-order sections as sets (X-1). | [E]; [S] `lvs-reports` "Canonical golden comparison" |
| Q-LVS-4 | Hash collisions and pseudo-random labels | **fix** | Use a seeded deterministic generator, plus a structural-signature check that splits colliding classes. | [E] |
| Q-LVS-6 | `property add` single-word and `{key tol}` parsing | **replicate** + tier-1 fix | Copy the single-word rule. A tolerance that is not a number raises an error in both modes (it is uninitialised in netgen). | [E] |
| Q-LVS-10 | Property comparison failure reported as a property error | **replicate** | Keep netgen's codes. Hefesto's structured result also carries the detail `property-check-failed`. | [E] |
| Q-LVS-11 | Pre-match flattening heuristic | **replicate** (behavioural) | Match netgen's behaviour, validated by NG-HIER. Record any divergence as a new entry. | [E] |
| Q-LVS-13 | JSON file naming and escaping | **fix** | Use full JSON escaping and valid empty lists. Keep netgen's naming when the name has an extension, and append `.json` otherwise. | [E] |
| Q-LVS-14 | `.include` home-directory expansion | **platform** | On iPadOS, `~` maps to the user's Hefesto documents root and `~user` is rejected. Desktop keeps netgen's behaviour. | [E]; [D] X-4 |
| Q-LVS-17 | Pin-match results −1 and −2 are never produced | **replicate** | Document them as dead in `lvs-hierarchy`. | [E] |
| Q-LVS-19 | List and JSON values for value-type and expression properties | **fix** | Write the numeric value, or `(unresolved expression)`. Only structured output changes. | [E] |
| Q-LVS-21 | Extra `.` line after a unique match | **replicate** | Keep the exact format. | [E] |
| Q-LVS-26 | No-connect pins merged as one net for parallel detection | **replicate** | This is intended behaviour, and `property parallel connected` turns it off. | [E] |
| Q-LVS-29 | `equate classes -unique` uses a random suffix | **fix** | Use a deterministic unique identity from the seeded generator of Q-LVS-4. | [E] |

### Simulation engine

| ID | Question | Kind | Recommendation | Evidence |
|---|---|---|---|---|
| Q-SIM-1 | LGPL components linked into the iPadOS app | **platform** | Confirm the draft through J-3. The iPad answer comes from D-28 before M14. The GPL `table` code models are excluded in every case. | [E]; [D] D-28 |
| Q-SIM-3 | XSPICE code models without `dlopen` | **platform** | (a) A Hefesto patch links the `.cm` sources statically and registers them at `ngSpice_Init`, on every platform. Verify that no supported PDK model needs `table.cm`. | [E] |
| Q-SIM-4 | Crash containment on iPadOS | **platform** | (a) In-process, with a mandatory autosave before each run (M14). A remote engine can come later. | [E] |
| Q-SIM-5 | One instance per process; parallelism | **platform** | (a) Worker processes on desktop and serial runs on iPadOS. The pool size follows Q-SIMADV-13. | [E] |
| Q-SIM-6 | ngspice lowercases names | **fix** (as specified) | Rename on collision to `<name>__c<k>`. Confirm vector naming with SIM-NET-03 and SIM-UI-04. | [E] |
| Q-SIM-7 | ngspice version pin and the CMake build | **other** | Pin ngspice-47 with Hefesto's own CMake wrapper. Checked 2026-11-22: the ngspice-47 tag (`a80f6e3e`) has no `cmake/` directory or top-level `CMakeLists.txt`, and no later release exists, so the choice is not conditional. Move to the first release that ships the upstream CMake build when one exists. Carry the Hefesto patches and offer them upstream. The golden pin (`7aa47164`) and roadmap principle 2 move to the same release (Q-SIM-23), and the SIM spec team re-checks every `(ngspice ref: ...)` trace against it (task 1.4). | [E]; [code] `tools/golden/pins.toml:46-52`; [web] ngspice git tag `ngspice-47` |
| Q-SIM-8 | PDK model descriptors need verification | **verify** | Approve at G0 as a rule: the descriptors are the ones the entry lists, each confirmed or corrected by the D-26 run against the pinned PDK sets (including the sg13g2 HBT, RES and CAP sections); sky130 sets `ng_nomodcheck` only if the run without it fails on `combined`. `Verified` line at G2-SIM. | [E]; [D] D-26 |
| Q-SIM-9 | `.control` sections and the `shell` command | **platform** | Keep the spec's behaviour: strip `.control` by default and block dangerous commands. The iPad subset (`let`, `meas`, `print`, `alter`) is reviewed against App Store guideline 2.5.2 in M14. | [E] |
| Q-SIM-10 | Cross-platform numeric reproducibility | **platform** | Build with `-ffp-contract=off` everywhere and keep tolerance-based golden comparison. Re-tune the tolerances with SIM-XP. | [E] |
| Q-SIM-11 | Recovery after an ngspice error | **platform** | Always replace the worker after any fault on desktop. On iPadOS, use `ngSpice_Reset` with fuzz tests (D-25). | [E]; [D] D-25 |
| Q-SIM-12 | OpenMP on Apple platforms | **platform** | Off on iPadOS. Measure on macOS before bundling libomp. Off on every platform in M9. | [E] |
| Q-SIM-14 | Progress tracks only two thread identities | **other** (as specified) | Combine the status strings with the streamed time point. Check the third run with SIM-RUN-03. | [E] |
| Q-SIM-15 | Callback threads and print latency | **other** (as specified) | Keep the default `low_latency` build. Handlers copy their data and never block. | [E] |
| Q-SIM-16 | Default result memory budgets | **platform** | Keep 512 MiB on iPadOS and 4 GiB on desktop. Revisit them after the `sim-bench` profiles. | [E]; [S] `hefesto-performance` |
| Q-SIM-17 | Raw header fields are not reproducible | **fix** | Golden comparisons ignore `Date:` and `Command:`. Data is always little-endian. | [E] |
| Q-SIM-19 | Background execution on iPadOS | **platform** | Pause and resume as specified, with an in-app notice (M14). | [E] |

### Advanced analog simulation

| ID | Question | Kind | Recommendation | Evidence |
|---|---|---|---|---|
| Q-SIMADV-3 | sky130 Monte Carlo switches | **verify** | Approve at G0 as a rule: (a) if the D-22 run shows a later `.param MC_MM_SWITCH=1` taking effect, otherwise (b), a Hefesto-side section. Do not expose the variation-scaling factors in M9. `Verified` line at G2-SIM. | [E]; [D] D-22 |
| Q-SIMADV-4 | gf180mcu statistical switches and defaults | **verify** | Approve at G0 as a rule: explicit `.param` overrides if D-22 shows that the later definition wins, otherwise a Hefesto-side section as in Q-SIMADV-3 (b); `ngbehavior` per D-27. Report the comment mismatch upstream. `Verified` line at G2-SIM. | [E]; [D] D-22, D-27 |
| Q-SIMADV-5 (rest) | `mm_ok=1` for extracted sg13g2 devices | **fix** | (a) Simulation preparation adds `mm_ok=1` when the Monte Carlo mode includes mismatch, inside the deck only (D-23). `ext2spice` output does not change. | [E]; [D] D-23 |
| Q-SIMADV-8 | Monte Carlo around non-typical corners | **other** | (a) Reject `process` and `both` at non-typical corners unless the descriptor names a statistical counterpart. Evaluate (b) with the PDK maintainers later. | [E] |
| Q-SIMADV-9 | Overriding parameters in point decks | **verify** | Approve at G0 as a rule: if the D-22 run shows that the last definition wins, append override lines after the last model line (b); otherwise change only top-level text and report "cannot sweep" (a). `Verified` line at G2-SIM. | [E]; [D] D-22 |
| Q-SIMADV-10 | Hefesto measurements against ngspice `.meas` | **other** | (a) Hefesto's value is authoritative. (b) Debug builds also capture ngspice's value and warn on a difference (SIMADV-MEAS-04). | [E] |
| Q-SIMADV-11 | Waveform retention and stale measurements | **other** | Add the `measured` mode now. It becomes the default above 100 points only after `sim-bench` has measured its storage cost and the user approves (D-24). | [E]; [D] D-24 |

## Audit decisions D-22 to D-28

| Decision | Question | Recommendation | How it closes |
|---|---|---|---|
| D-22 | Does a later top-level `.param` override an earlier one in the pinned ngspice? | Approve the conditional rules of Q-SIMADV-3, -4, -9 at G0; run one reference deck per PDK before G2-SIM (task 3.3). | `Verified` lines under Q-SIMADV-3, -4, -9 |
| D-23 | sg13g2 `mm_ok=1` for extracted devices | (a) Simulation preparation adds it. | the `simulation-netlist-preparation` delta "sg13g2 mismatch flag on extracted devices" |
| D-24 | `measured` retention mode | Add it. It becomes the default only after measurement. | the `simulation-sweeps` delta "Measured waveform retention"; task 19.2 |
| D-25 | Engine recovery after a fault | Replace the worker on desktop. Reset on iPadOS, with fuzz tests (M14). | the `simulation-engine-embedding` deltas "Crash containment on desktop" and "Cancellation and fault states" |
| D-26 | sky130 descriptor: `combined`, `ng_nomodcheck` | Scan `combined`. Decide `ng_nomodcheck` with a reference run. | task 3.3; Q-SIM-8 |
| D-27 | gf180mcu `hs` or `hsa` | Follow the PDK (`hs`) unless a reference run shows a difference that `hsa` fixes. | task 3.3 |
| D-28 | Linking ngspice's LGPL and MPL parts | D-28's own recommendation is a legal review first. The new desktop question is J-2 (a shared library behind the adapter plugin, loaded only by `hefesto-simd` in release builds), reviewed before the Desktop Beta. The iPad part (a framework or static linking) stays before M14, after the same review (J-3). | Q-SIM-20, Q-SIM-26; the `hefesto-licensing` delta; task 20.1 |

Two related audit decisions are already resolved and are used as they stand: D-10 (native undo events for `sim` testbench edits) and D-34 (the `netgen` helper only names `lvs` in a diagnostic).

## Entries raised by the M9 plan

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-LVS-30 | How the approved two-team process applies to the `lvs-*` specs | **other** | (a) The M8 process, a MODIFIED delta of the requirement and a roster rule (J-1) |
| Q-LVS-31 | The IHP setup file is `ihp-sg13g2_setup.tcl` | **fix** (doc) | (a) Correct the texts and add the bare-name scenario |
| Q-LVS-32 | `lvs -layout` extracts incrementally | **other** | (a) `ensure_extracted` (M7 D18) |
| Q-LVS-33 | Name map from netlists back to layout and schematic objects | **platform** | (a) Additive `render_with_map` and `NetlistResult::names` |
| Q-LVS-34 | LVS threading | **platform** | (a) Single-threaded per pair, with no globals |
| Q-LVS-35 | LVS performance target | **platform** | (a) Callgrind counts ≤ netgen, then baseline + 2 % |
| Q-LVS-36 | LVS input and run limits | **fix** | (c) Configurable limits with the proposed defaults |
| Q-SIM-20 | How `hefesto-simd` links ngspice on desktop | **platform** | (a) Shared `libngspice-hefesto` behind the adapter plugin, loaded only by the worker in release builds; replaceable on macOS; legal review before Beta (J-2) |
| Q-SIM-21 | Worker IPC transport and platform contract | **platform** | (a) `platform::WorkerHost`, framed pipes, heartbeats |
| Q-SIM-22 | In-process simulation on desktop | **platform** | (a) Developer builds only, through the same adapter plugin |
| Q-SIM-23 | Reference ngspice build for SIM and SIMADV captures | **other** | (a) Patched build from the engine's pin, with OpenVAF |
| Q-SIM-24 | Node names for layout probes before M12 | **platform** | (a) Bring the naming function forward, with no length limit; `getnode` stays M12 |
| Q-SIM-25 | Simulation input and transport limits | **fix** | (a) Configurable limits with the proposed defaults |
| Q-SIM-26 | When D-28 must be settled | **other** | (a) Desktop question in R0 and reviewed before Beta; iPad part before M14, as D-28 says (J-3) |
| Q-SIM-27 | Desktop Beta sign-off | **other** | (a) Three PDK flows with Hefesto-drawn schematic and layout, Hefesto DRC, FO4 testbench; CI plus the user's sign-off (J-4) |
| Q-SIM-28 | Simulation performance gate | **platform** | (a) Overhead ≤ 10 % against ngspice's `rusage` analysis time; Monte Carlo scaling ≥ 0.6 × `w` for `w` up to `min(4, cores − 1)` of the runner |
| Q-SIMADV-13 | Worker pool size and result order | **platform** | (a) A memory-aware pool, results in point-index order |
| Q-SCHEM-39 | SCHEM-LVS-01 needs the LVS engine | **other** | (a) Defer to M9, as SCHEM-SIM-01/02 were |

The full options and reasons are in each entry.

## Planning questions raised by the M9 plan

These come from `openspec/changes/m9-lvs-simulation/design.md` "Open Questions". Options are listed with the recommended one first.

| # | Question | Options (recommended first) | Shapes |
|---|---|---|---|
| 1 | How spec changes are recorded | (a) the change's `specs/` deltas, synced at G0; (b) direct edits after approval with `skip_specs: true` (the M7 and M8 precedent); (c) both | design D1; tasks 1.2 |
| 2 | When implementation starts relative to M7 and M8 | (a) R0–R1 now, R2–R3 in parallel with M7 and M8, R4 after their exits; (b) everything after M8 exits; (c) only R1, then re-plan | design D1 |
| 3 | Desktop scope | (a) LVS browser and waveform boards before SHELL, including the run-set, corner and Monte Carlo views; (b) minimal panels with no boards; (c) browser and viewer now, run-set views in M10 | design D23; tasks 17 |
| 4 | Who owns the additive M7 and M8 APIs | (a) a MAPS workstream of the M7 and M8 owners after each exit; (b) M9 workstreams edit them directly; (c) add them before M7 and M8 exit | design D3, D24; tasks 11 |

## Counts

| Kind | Count | IDs (without `Q-`) |
|---|---|---|
| replicate | 6 | LVS-6, LVS-10, LVS-11, LVS-17, LVS-21, LVS-26 |
| fix | 10 | LVS-2, LVS-4, LVS-13, LVS-19, LVS-29, SIM-6, SIM-17, SIMADV-5 (rest), LVS-36, SIM-25 |
| fix (doc) | 1 | LVS-31 |
| platform | 20 | LVS-14, SIM-1, SIM-3, SIM-4, SIM-5, SIM-9, SIM-10, SIM-11, SIM-12, SIM-16, SIM-19, LVS-33, LVS-34, LVS-35, SIM-20, SIM-21, SIM-22, SIM-24, SIM-28, SIMADV-13 |
| verify | 4 | SIM-8, SIMADV-3, SIMADV-4, SIMADV-9 |
| other | 13 | LVS-1, SIM-7, SIM-14, SIM-15, SIMADV-8, SIMADV-10, SIMADV-11, LVS-30, LVS-32, SIM-23, SIM-26, SIM-27, SCHEM-39 |

Total: 54 entries (34 blocking, Q-SIM-1, the rest of Q-SIMADV-5, and 18 new), plus D-22 to D-28 and four planning questions. No row in this table recommends compat. The four `netgen-compat` branch points (Q-LVS-7, -9, -20, -24) were resolved compat on 2026-11-07, before this table. Design D5 says how they are implemented.

## Spec changes the recommendations imply

These were recorded as spec deltas in `openspec/changes/m9-lvs-simulation/specs/` and, after the approval of 2026-11-22, synced into the living specs at G0 (task 1.2; `openspec archive` applied +29 added and ~11 modified requirements, none removed). The change then set `skip_specs: true` and dropped its synced `specs/` folder, because `openspec validate --strict` rejects `skip_specs` beside delta files; a dry-run archive applies no spec change:

| Capability | Delta | Rows |
|---|---|---|
| `lvs-netlist-input` | ADDED "In-memory netlist sources", "Input limits" | Q-LVS-33, -36 |
| `lvs-setup-file` | ADDED "PDK setup files evaluate unchanged", "Setup evaluation is bounded and cancellable" | Q-LVS-31, -36 |
| `lvs-comparison-algorithm` | ADDED "Single-threaded comparison per pair" | Q-LVS-34, -4 |
| `lvs-device-reduction` | ADDED "Reduction cost" | Q-LVS-35 |
| `lvs-property-matching` | ADDED "Platform-independent property arithmetic" | X-1 |
| `lvs-hierarchy` | ADDED "Hierarchy depth limit" | Q-LVS-36 |
| `lvs-reports` | ADDED "Layout object references in structured results" | Q-LVS-33 |
| `lvs-command` | MODIFIED "Layout netlist from extraction" (incremental, with the stale-header rule), "Setup file and input resolution"; ADDED "LVS status chip" | Q-LVS-31, -32, -33 |
| `simulation-engine-embedding` | MODIFIED "ngspice embedded as a library", "Crash containment on desktop", "ngspice build configuration and license hygiene", "Cancellation and fault states" (reset only on iPadOS), "In-process execution on iPadOS" (link type per D-28), "Multiple simulations and corner parallelism" (the pool formula); ADDED "Worker channel limits", "Worker confinement" | Q-SIM-5, -7, -11, -20, -21, -22, -25, -26, Q-SIMADV-13, D-25, D-28 |
| `simulation-netlist-preparation` | MODIFIED "Net name map" (layout point and `layoutName` computed at it); ADDED "sg13g2 mismatch flag on extracted devices", "Deck limits" | Q-SIM-24, D-23, Q-SIM-25 |
| `simulation-analyses-and-results` | ADDED "Chunked result transfer", "Raw files in golden comparisons" | Q-SIM-21, -17 |
| `simulation-ui-and-crossprobe` | MODIFIED "Probing a layout net" (lookup by `layoutName`, then connectivity); ADDED "Probe names without length limits", "Simulate button" | Q-SIM-24, Q-LVS-33 |
| `simulation-command` | ADDED "Schematic launcher calls" | m8 D13 |
| `simulation-sweeps` | ADDED "Measured waveform retention", "Results in point order" | D-24, Q-SIMADV-13 |
| `simulation-corners` | ADDED "Corner jobs on the worker pool" (the pool size is referenced, not repeated) | Q-SIMADV-13 |
| `simulation-monte-carlo` | ADDED "Sample independence from worker assignment", "Sample count limit" | Q-SIMADV-13, Q-SIM-25 |
| `simulation-measurements-and-statistics` | ADDED "ngspice measure cross-check in debug builds", "Platform-independent statistics" | Q-SIMADV-10 |
| `hefesto-licensing` | MODIFIED "Two-team clean-room process for specs written from GPL source" (netgen added); ADDED "Process boundary for external engines" | Q-SIM-20, -22, -26, Q-LVS-30, D-28 |
| `hefesto-architecture` | ADDED "Long-lived worker processes" | Q-SIM-21 |
| `hefesto-performance` | ADDED "LVS and simulation throughput" | Q-LVS-35, Q-SIM-28 |

Edits made to the deltas before the sync, because of the approved answers:
- `hefesto-licensing` "Two-team clean-room process for specs written from GPL source": the roster rule keeps a netgen reader on the LVS spec team "for the whole milestone" (Q-LVS-30), with the scenario "netgen reader stays on the spec team for the milestone";
- `simulation-engine-embedding` "ngspice embedded as a library": the pin is ngspice-47, `a80f6e3e`, built with Hefesto's CMake build (Q-SIM-7), with the scenario "Pinned source in the build manifest"; "ngspice build configuration and license hygiene": the iPadOS link type is settled by D-28 before M14 (Q-SIM-1), replacing "pending Q-SIM-1"; "Multiple simulations and corner parallelism": the scenario "Large post-layout deck limits the pool" (Q-SIMADV-13);
- every `(ngspice ref: ...)` trace of the deltas, re-checked against ngspice-47 (task 1.4).

Edits made directly to the living specs at G0:
- the "ngspice reference" line of the six `simulation-*` specs that have one names ngspice-47, `a80f6e3e` (Q-SIM-7), and their `(ngspice ref: ...)` traces were re-checked (task 1.4, "Trace re-check against ngspice-47" below);
- `simulation-monte-carlo` "Statistical functions in ngspice (informative)" and `simulation-engine-embedding` "Deterministic engine initialisation" note where ngspice-47 differs (findings F1, F2).

Deltas cannot carry these edits, so they are made directly at G0 (all done 2026-11-22 except the two items marked):
- the `lvs-setup-file` Purpose names `ihp-sg13g2_setup.tcl` (Q-LVS-31);
- `tests/golden/LVS/CASES.md` uses that name (not done here: the file belongs to GOLDEN; NG-PDK-03 still says "sg13g2 setup");
- the roadmap's M9 "Early de-risking" line and D-28 gain a sentence naming the desktop linking question (Q-SIM-20), decided in R0 and reviewed before the Desktop Beta; D-28's iPadOS deadline is unchanged (Q-SIM-26);
- roadmap principle 2 names the new ngspice pin (Q-SIM-7, Q-SIM-23);
- `docs/contributing/cleanroom.md` covers netgen (Q-LVS-30) (task 2.1, CLEANROOM, R1);
- `tests/golden/deviations.toml` defers SCHEM-LVS-01 to M9 (Q-SCHEM-39), through `tools/golden/authoring/schem_cases.py` (`LATER`), which generates that block, except its `inv_1.spice` output, which stays due at the M8 exit (`LATER_OUTPUTS`, written as `deferred_outputs = { "inv_1.spice" = "M8" }`; `goldenlib/deviations.py` and `replay.py --milestone` enforce it); `tests/golden/SCHEM/CASES.md`, m8 design D21 and the roadmap's M8 exit line say so (task 1.3);
- the roadmap's Desktop 1.0 counsel row of the early de-risking schedule names LVS (Q-LVS-30) (task 2.4, roadmap half).

### Round 3 spec changes (2026-11-27)

Made directly in the living specs on branch `m9-round3-decide` (LVS spec team). Scenarios only under existing requirement headings; no heading added, renamed or removed, so tests tag `[req:<heading>]` as before.

| Capability | Requirement | Change | Source |
|---|---|---|---|
| `simulation-ui-and-crossprobe` | Probing a layout net | step 1 names the node with plain `getnode` naming, never `@=`, and fails an absent use-path element (an out-of-range array element included) with `SIM-PROBE-003`; scenarios "Probe uses the normal node name", "Array element outside the array"; scenario "Netlist name differs from the node name" now uses a generated `#` name instead of an `@=` name | R3-1, R3-2 |
| `irsim-interface` | Hierarchical node names | note: probes use these names, never "Fast node names" | R3-1 |
| `simulation-engine-embedding` | OSDI compact models | paragraph "Static registration"; scenario "Statically registered modules with per-module symbol prefixes" | R3-3 |
| `simulation-analyses-and-results` | Job lifecycle | paragraph "Job log limit"; scenario "Job log capped in memory" | R3-5 |
| `simulation-analyses-and-results` | Saved vectors and memory budget | interim sentence; scenario "Default result budget" | R3-6 |
| `lvs-comparison-algorithm` | Comparison graph | `<c>` is the cell name; scenario "Empty circuit named by its cell" | LVSALG choice, confirmed |
| `lvs-comparison-algorithm` | verify command | `only` and `properties` named; scenario "Mismatch text only for only and properties" | LVSALG choice, confirmed |
| `lvs-comparison-algorithm` | Reset and stepping commands | `restart` forgets property results; scenario "restart forgets property results" | LVSALG choice |
| `lvs-device-reduction` | Parallel combination | a port's net is never a no-connect; scenario "Port net is not a no-connect" | LVSRED choice, confirmed |
| `lvs-device-reduction` | Zero-valued device removal | "Surviving net" paragraph; scenarios "Port net survives the removal", "First terminal's net survives between internal nets" | LVSRED choice, confirmed (both-ports case: R3-7 h) |
| `lvs-property-matching` | Series/parallel network reconciliation | comb keeps `M`/`S` without an additive value; collapse needs every unfolded value to match; scenarios "Multiplier kept without an additive property", "Collapse needs every unfolded value to match" | LVSPROP choices, confirmed |
| `lvs-netlist-input` | SPICE lexical structure | warnings, cap and summary per file; scenario "Warnings counted per file" | LVSREAD choice, confirmed |
| `lvs-netlist-input` | SPICE primitive device cards | `L`/`T` default models `l`/`t`; scenario "Inductor without a model" | LVSREAD choice, confirmed (`V`/`I`/`E` and `<kind>`: R3-7 i, j) |
| `lvs-netlist-input` | SPICE subcircuit calls and placeholders | the instance is created; extra connections ignored, missing ports on no net; scenario "Call with too few connections" | LVSREAD choice, confirmed |
| `lvs-netlist-input` | Globals, includes and parameters | a missing include is skipped with a warning; scenario "Missing include does not stop the read" | LVSREAD choice, confirmed (message text: R3-7 k) |

| `simulation-analyses-and-results` | Saved vectors and memory budget | a job over its result budget ends `failed` (design D18: every cap ends a simulation as `failed`), keeps its partial result and is marked `partial`; scenario "Budget exceeded on iPad" reworded from `cancelled` (INTEGRATION, round 3; the code of branch `m9-round3-sim` already does this, `SIM-RESULT-011`) | D18, R3-6 |

Open-questions and review records changed with them: `SIM.md` Q-SIM-2, -16, -24, -25 (amendment `Status:` and `Resolution` lines); `LVS.md` Q-LVS-3, -5, -23, -24 (OI-3 wording); `m9-paraphrase-review.md` (OI-3 closed; OI-4 linked to R3-7 d); design D18 (job log row).

## Verification runs needed before G2

| Run | Settles | Where it is recorded |
|---|---|---|
| `.param` precedence, one deck per PDK | D-22, Q-SIMADV-3, -4, -9 (approved at G0 as conditional rules) | `Verified` lines in `SIMADV.md` |
| sky130 with and without `ng_nomodcheck` on `combined` | D-26, Q-SIM-8 (approved at G0 as a conditional rule) | `Verified` line in `SIM.md` |
| gf180mcu `hs` against `hsa` on the PDK testbenches | D-27, Q-SIMADV-4 | `SIMADV.md` |
| `ihp-sg13g2_setup.tcl` evaluated by netgen 1.5.327 | Q-LVS-31 | NG-PDK capture |

## Trace re-check against ngspice-47 (task 1.4)

On 2026-11-22 every `(ngspice ref: ...)` trace of the nine `simulation-*` specs and every ngspice line reference in `SIM.md` and `SIMADV.md` was re-checked against the approved pin, ngspice-47 (tag commit `a80f6e3e95d51534905b1f23410a951802666656`). Method: each referenced file was read at `7aa4716494c0` (the tree the specs were written from) and at `a80f6e3e` through SourceForge's raw file view, without cloning; lines were mapped by a line diff, and every range whose text changed was read at both commits. The session read no netgen or xschem source.

**112 traces checked:** 66 in the specs (below, numbered by capability and requirement as of origin/main) and 46 in the open-questions files (32 in `SIM.md`, 14 in `SIMADV.md`). Results: 51 at the same lines (four of them with a small text difference, noted below), 35 moved (line numbers corrected), 6 rewritten from `cmake/` and `CMakeLists.txt` (which ngspice-47 does not have) to the pinned tree's autotools files (`configure.ac`, `src/Makefile.am`, `src/xspice/icm/GNUmakefile.in`) or to the Hefesto wrapper (`third_party/ngspice/CMakeLists.txt`), and 20 tied to the five findings below, where the cited behaviour differs.

### Findings

The specs were written from development after ngspice-47. In five places the pinned release behaves differently. None changes a decision recorded above, but F1 and F2 affect normative requirements, so they were raised to the user (both approved 2026-11-25: F1 (a) as patch 0005, F2 as patch 0004) (they are not open-question entries, and the readiness check does not see them):

- **F1. Statistical `.param` draws (`simulation-monte-carlo` "Statistical functions in ngspice (informative)"; SIMADV.md Q-SIMADV-7).** In `7aa47164` a top-level `.param` with `agauss` and similar functions is drawn once per circuit load, and `stat_param_as_func` restores the old behaviour. ngspice-47 has no `stat_param_as_func`: it turns every such `.param` into a function re-evaluated at each reference (inpcom.c:8226-8238). The sky130 process-variation model (one draw per die through `X_mcd = 'X_mcz'`) and Q-SIMADV-7's Resolution ("post-47 semantics pinned") assume the newer behaviour. The informative requirement now says so. Options: (a) carry the upstream change as a Hefesto patch on ngspice-47 (task 5.1) and in the capture image (task 3.2); (b) pin a later commit; (c) accept ngspice-47's semantics and revise `simulation-monte-carlo` and Q-SIMADV-3/-7. Recommended: (a). **Status: approved 2026-11-25, option (a)** ("Round 2 decisions"): patch `0005-stat-param-once-per-load.patch` in the engine and the capture image, engine version `47-hefesto2`. The D-22 reference runs of task 3.3 (the `Verified` lines of Q-SIMADV-3, -4 and -9) depend on `.param` evaluation; they are unblocked.
- **F2. `setseed` and the `trnoise` pool (`simulation-engine-embedding` "Deterministic engine initialisation"; Q-SIMADV-7).** In `7aa47164`, `setseed` also re-initialises the Wallace pool used by `trnoise` and `trrandom`. In ngspice-47, `setseed` (randnumb.c:292-322) reseeds only the main generator, and `initw` seeds the pool with `srand(getpid())` once, from `ngSpice_Init` (wallace.c:76-85, sharedspice.c:1079-1081). Without a patch, `trnoise` results depend on the process id, which the requirement forbids. Recommended: a Hefesto patch that makes `setseed` re-initialise the pool from the new seed, in both the engine and the capture image. **Status: carried by patch 0004, approved 2026-11-25** ("Round 2 decisions"). `third_party/ngspice/patches/0004-deterministic-initialisation.patch` (and its copy in the capture image, `tools/golden/container/ngspice/patches/`) already makes `setseed` refill the pool (`reinitw`) and removes the `getpid()` seed; `test_trnoise_same_in_fresh_and_reused_engine` checks it. The patch landed in R0 before the user chose.
- **F3. `.option seed` (`simulation-monte-carlo` "Statistical functions in ngspice" and "Sample seeds").** In ngspice-47, `.option seed=` is evaluated when a deck is loaded (inp.c:426-470), not by the `option` command (com_option.c has no seed handling). Hefesto strips these lines from Monte Carlo decks anyway, so behaviour is unchanged; the trace now points at inp.c.
- **F4. `PHASE_MARGIN` and `GAIN_MARGIN` (`simulation-measurements-and-statistics` "Import of ngspice measure statements"; Q-SIMADV-10).** ngspice-47's `.meas` has no such forms (com_measure2.c:51-57, :207-257). Hefesto already leaves them in the deck as "computed by ngspice only"; with ngspice-47 the ngspice side does not compute them either. No change to Hefesto's behaviour; noted under Q-SIMADV-10.
- **F5. `snp` command (`simulation-engine-embedding` "OSDI compact models").** `7aa47164` lists a `snp`/`pre_snp` command next to `osdi`; ngspice-47 does not have it. The trace now covers the `osdi` entry only (commands.c:288-293).

Smaller differences that need no action: `sharedspice.h:1-160` differs only in its copyright year; `dev.c:290-420` adds `hicum2` to the dynamic device list; the OSDI loader at `osdiregistry.c:339-451` accepts OpenVAF-reloaded descriptors 0.4 and later, as the spec requires.

### Spec traces

| # | Capability | Requirement | Trace at `7aa47164` | Trace at ngspice-47 `a80f6e3e` | Result |
|---|---|---|---|---|---|
| 1 | `simulation-engine-embedding` | ngspice embedded as a library | `src/include/ngspice/sharedspice.h:1-160` | `src/include/ngspice/sharedspice.h:1-160` | same lines; copyright year differs |
| 2 | `simulation-engine-embedding` | ngspice embedded as a library | `cmake/NgspiceOptions.cmake:86` | `configure.ac:178-180`, `src/Makefile.am:498-616`, the Hefesto wrapper (third_party/ngspice/CMakeLists.txt) | no `cmake/` tree in ngspice-47; rewritten |
| 3 | `simulation-engine-embedding` | ngspice build configuration and license hygiene | `COPYING:1-60` | `COPYING:1-60` | unchanged |
| 4 | `simulation-engine-embedding` | ngspice build configuration and license hygiene | `cmake/NgspiceOptions.cmake:21-74` | `configure.ac:138-196`, `configure.ac:936-944`, `configure.ac:1320-1330`, the Hefesto wrapper (third_party/ngspice/CMakeLists.txt) | no `cmake/` tree in ngspice-47; rewritten |
| 5 | `simulation-engine-embedding` | Engine adapter boundary | `src/include/ngspice/sharedspice.h:95-110` | `src/include/ngspice/sharedspice.h:95-110` | unchanged |
| 6 | `simulation-engine-embedding` | Engine adapter boundary | `src/include/ngspice/sharedspice.h:500-530` | `src/include/ngspice/sharedspice.h:475-505` | moved |
| 7 | `simulation-engine-embedding` | Deterministic engine initialisation | `src/sharedspice.c:803-830` | `src/sharedspice.c:803-830` | unchanged |
| 8 | `simulation-engine-embedding` | Deterministic engine initialisation | `src/sharedspice.c:856-1092` | `src/sharedspice.c:856-1091` | moved |
| 9 | `simulation-engine-embedding` | Deterministic engine initialisation | `src/sharedspice.c:930-951` | `src/sharedspice.c:930-935`, `src/sharedspice.c:1079-1081` | content differs, finding F2 |
| 10 | `simulation-engine-embedding` | Deterministic engine initialisation | `src/maths/misc/randnumb.c:71-92` | `src/maths/misc/randnumb.c:71-92` | unchanged |
| 11 | `simulation-engine-embedding` | Deterministic engine initialisation | `src/maths/misc/randnumb.c:287-330` | `src/maths/misc/randnumb.c:286-322` | content differs, finding F2 |
| 12 | `simulation-engine-embedding` | Deterministic engine initialisation | `src/frontend/trannoise/wallace.c:76` | `src/frontend/trannoise/wallace.c:76` | content differs, finding F2 |
| 13 | `simulation-engine-embedding` | Single engine instance per address space | `src/sharedspice.c:390-470` | `src/sharedspice.c:390-470` | unchanged |
| 14 | `simulation-engine-embedding` | Engine threading model | `src/sharedspice.c:24-40` | `src/sharedspice.c:24-40` | unchanged |
| 15 | `simulation-engine-embedding` | Engine threading model | `src/sharedspice.c:640-730` | `src/sharedspice.c:640-730` | unchanged |
| 16 | `simulation-engine-embedding` | Engine threading model | `src/sharedspice.c:795-800` | `src/sharedspice.c:795-800` | unchanged |
| 17 | `simulation-engine-embedding` | Callback contract | `src/include/ngspice/sharedspice.h:240-330` | `src/include/ngspice/sharedspice.h:240-328` | moved |
| 18 | `simulation-engine-embedding` | Callback contract | `src/sharedspice.c:1700-1720` | `src/sharedspice.c:1698-1718` | moved |
| 19 | `simulation-engine-embedding` | Callback contract | `src/sharedspice.c:1810-1820` | `src/sharedspice.c:1808-1818` | moved |
| 20 | `simulation-engine-embedding` | Cancellation and fault states | `src/sharedspice.c:547-590` | `src/sharedspice.c:547-590` | unchanged |
| 21 | `simulation-engine-embedding` | Cancellation and fault states | `src/sharedspice.c:1473-1479` | `src/sharedspice.c:1471-1477` | moved |
| 22 | `simulation-engine-embedding` | Cancellation and fault states | `src/sharedspice.c:2134-2190` | `src/sharedspice.c:2132-2188` | moved |
| 23 | `simulation-engine-embedding` | No runtime loading of executable code on iPadOS | `src/osdi/osdiregistry.c:32` | `src/osdi/osdiregistry.c:31` | moved |
| 24 | `simulation-engine-embedding` | No runtime loading of executable code on iPadOS | `src/spicelib/devices/dev.c:290-420` | `src/spicelib/devices/dev.c:290-420` | same lines; ngspice-47 adds `hicum2` to the device list |
| 25 | `simulation-engine-embedding` | OSDI compact models | `src/osdi/osdiregistry.c:340-470` | `src/osdi/osdiregistry.c:339-451` | moved |
| 26 | `simulation-engine-embedding` | OSDI compact models | `src/frontend/commands.c:298-306` | `src/frontend/commands.c:288-293` | content differs, finding F5 |
| 27 | `simulation-engine-embedding` | ngspice host mechanics (informative) | `src/sharedspice.c:24-40` | `src/sharedspice.c:24-40` | unchanged |
| 28 | `simulation-engine-embedding` | ngspice host mechanics (informative) | `src/sharedspice.c:547-590` | `src/sharedspice.c:547-590` | unchanged |
| 29 | `simulation-engine-embedding` | ngspice host mechanics (informative) | `src/sharedspice.c:914-1069` | `src/sharedspice.c:914-1052` | moved |
| 30 | `simulation-engine-embedding` | ngspice host mechanics (informative) | `src/sharedspice.c:1955-2110` | `src/sharedspice.c:1953-2108` | moved |
| 31 | `simulation-engine-embedding` | ngspice host mechanics (informative) | `src/sharedspice.c:2134-2190` | `src/sharedspice.c:2132-2188` | moved |
| 32 | `simulation-engine-embedding` | ngspice host mechanics (informative) | `src/frontend/inpcom.c:2160-2314` | `src/frontend/inpcom.c:2136-2290` | moved |
| 33 | `simulation-netlist-preparation` | Model library resolution | `src/frontend/inpcom.c:2040-2130` | `src/frontend/inpcom.c:2016-2106` | moved |
| 34 | `simulation-netlist-preparation` | Model library resolution | `src/frontend/inpcom.c:2160-2314` | `src/frontend/inpcom.c:2136-2290` | moved |
| 35 | `simulation-netlist-preparation` | PDK initialisation commands | `src/frontend/inpcompat.c:61-95` | `src/frontend/inpcompat.c:61-95` | unchanged |
| 36 | `simulation-netlist-preparation` | PDK initialisation commands | `src/frontend/inpcom.c:1048-1056` | `src/frontend/inpcom.c:1048-1056` | unchanged |
| 37 | `simulation-netlist-preparation` | Deck delivery | `src/include/ngspice/sharedspice.h:118-140` | `src/include/ngspice/sharedspice.h:118-140` | same lines; one comment line differs |
| 38 | `simulation-netlist-preparation` | Deck delivery | `src/include/ngspice/sharedspice.h:470-480` | `src/include/ngspice/sharedspice.h:445-455` | moved |
| 39 | `simulation-analyses-and-results` | Supported analyses | `src/frontend/commands.c:420-520` | `src/frontend/commands.c:407-459` | moved |
| 40 | `simulation-analyses-and-results` | Progress reporting | `src/sharedspice.c:1955-2110` | `src/sharedspice.c:1953-2108` | moved |
| 41 | `simulation-analyses-and-results` | Pause, resume and cancel | `src/sharedspice.c:640-730` | `src/sharedspice.c:640-730` | unchanged |
| 42 | `simulation-analyses-and-results` | Final vector retrieval | `src/include/ngspice/sharedspice.h:150-160` | `src/include/ngspice/sharedspice.h:150-160` | unchanged |
| 43 | `simulation-analyses-and-results` | Final vector retrieval | `src/sharedspice.c:1340-1355` | `src/sharedspice.c:1338-1353` | moved |
| 44 | `simulation-analyses-and-results` | Final vector retrieval | `src/frontend/rawfile.c:160-200` | `src/frontend/rawfile.c:160-200` | unchanged |
| 45 | `simulation-analyses-and-results` | Raw file format | `src/frontend/rawfile.c:31` | `src/frontend/rawfile.c:31` | unchanged |
| 46 | `simulation-analyses-and-results` | Raw file format | `src/frontend/rawfile.c:60-300` | `src/frontend/rawfile.c:60-300` | unchanged |
| 47 | `simulation-analyses-and-results` | Raw file format | `src/frontend/typesdef.c:40-43` | `src/frontend/typesdef.c:40-43` | unchanged |
| 48 | `simulation-sweeps` | Point decks | `src/frontend/inpcom.c:1140-1160` | `src/frontend/inpcom.c:1140-1160` | unchanged |
| 49 | `simulation-monte-carlo` | Statistical functions in ngspice (informative) | `src/frontend/numparam/xpressn.c:45-85` | `src/frontend/numparam/xpressn.c:45-85` | unchanged |
| 50 | `simulation-monte-carlo` | Statistical functions in ngspice (informative) | `src/maths/misc/randnumb.c:101-116` | `src/maths/misc/randnumb.c:101-116` | unchanged |
| 51 | `simulation-monte-carlo` | Statistical functions in ngspice (informative) | `src/maths/misc/randnumb.c:220-235` | `src/maths/misc/randnumb.c:220-235` | unchanged |
| 52 | `simulation-monte-carlo` | Statistical functions in ngspice (informative) | `src/maths/misc/randnumb.c:287-330` | `src/maths/misc/randnumb.c:286-322` | content differs, finding F2 |
| 53 | `simulation-monte-carlo` | Statistical functions in ngspice (informative) | `src/frontend/com_option.c:142-152` | `src/frontend/inp.c:426-470` | content differs, finding F3 |
| 54 | `simulation-monte-carlo` | Statistical functions in ngspice (informative) | `src/sharedspice.c:930-936` | `src/sharedspice.c:930-935` | content differs, finding F2 |
| 55 | `simulation-monte-carlo` | Statistical functions in ngspice (informative) | `src/frontend/inpcom.c:8380-8405` | `src/frontend/inpcom.c:8226-8238` | content differs, finding F1 |
| 56 | `simulation-monte-carlo` | Statistical functions in ngspice (informative) | `src/frontend/inp.c:1030-1042` | `src/frontend/inp.c:940-952` | moved |
| 57 | `simulation-monte-carlo` | Statistical functions in ngspice (informative) | `src/frontend/runcoms2.c:173-185` | `src/frontend/runcoms2.c:173-185` | unchanged |
| 58 | `simulation-monte-carlo` | Statistical functions in ngspice (informative) | `src/frontend/inp.c:1729-1735` | `src/frontend/inp.c:1639-1645` | moved |
| 59 | `simulation-monte-carlo` | Sample seeds | `src/maths/misc/randnumb.c:287-330` | `src/maths/misc/randnumb.c:286-322` | content differs, finding F2 |
| 60 | `simulation-monte-carlo` | Sample seeds | `src/frontend/com_option.c:142-152` | `src/frontend/inp.c:426-470` | content differs, finding F3 |
| 61 | `simulation-monte-carlo` | Sample seeds | `src/sharedspice.c:930-936` | `src/sharedspice.c:930-935` | content differs, finding F2 |
| 62 | `simulation-monte-carlo` | Portable random numbers | `src/maths/misc/randnumb.c:101-116` | `src/maths/misc/randnumb.c:101-116` | unchanged |
| 63 | `simulation-monte-carlo` | Portable random numbers | `src/maths/misc/randnumb.c:140-150` | `src/maths/misc/randnumb.c:140-150` | unchanged |
| 64 | `simulation-monte-carlo` | User statistical parameters | `src/frontend/numparam/xpressn.c:45-85` | `src/frontend/numparam/xpressn.c:45-85` | unchanged |
| 65 | `simulation-measurements-and-statistics` | Import of ngspice measure statements | `src/frontend/com_measure2.c:55-62` | `src/frontend/com_measure2.c:51-57` | content differs, finding F4 |
| 66 | `simulation-measurements-and-statistics` | Import of ngspice measure statements | `src/frontend/com_measure2.c:215-262` | `src/frontend/com_measure2.c:207-257` | content differs, finding F4 |

### Open-questions traces

| # | File | Entry | Trace at `7aa47164` | Trace at ngspice-47 `a80f6e3e` | Result |
|---|---|---|---|---|---|
| 67 | `SIM.md` | Q-SIM-1 | `COPYING:1-45` | `COPYING:1-45` | unchanged |
| 68 | `SIM.md` | Q-SIM-2 | `src/osdi/osdiregistry.c:32` | `src/osdi/osdiregistry.c:31` | moved |
| 69 | `SIM.md` | Q-SIM-2 | `src/osdi/osdiregistry.c:340-470` | `src/osdi/osdiregistry.c:339-451` | moved |
| 70 | `SIM.md` | Q-SIM-2 | `src/osdi/osdi.h:20-21` | `src/osdi/osdi.h:20-21` | unchanged |
| 71 | `SIM.md` | Q-SIM-3 | `src/spicelib/devices/dev.c:290-420` | `src/spicelib/devices/dev.c:290-420` | unchanged |
| 72 | `SIM.md` | Q-SIM-3 | `cmake/NgspiceOptions.cmake:21-22` | `configure.ac:141-143`, `src/xspice/icm/GNUmakefile.in:9-10` | no CMake build in ngspice-47; rewritten |
| 73 | `SIM.md` | Q-SIM-4 | `src/sharedspice.c:2134-2190` | `src/sharedspice.c:2132-2188` | moved |
| 74 | `SIM.md` | Q-SIM-4 | `src/sharedspice.c:914` | `src/sharedspice.c:914` | unchanged |
| 75 | `SIM.md` | Q-SIM-5 | `src/sharedspice.c:390-470` | `src/sharedspice.c:390-470` | unchanged |
| 76 | `SIM.md` | Q-SIM-5 | `src/include/ngspice/sharedspice.h:330-345` | `src/include/ngspice/sharedspice.h:383-398` | re-pointed: the old range was a comment block; now the `ngSpice_Init`/`ngSpice_Init_Sync` declarations with `ident` |
| 77 | `SIM.md` | Q-SIM-6 | `src/frontend/inpcom.c` | `src/frontend/inpcom.c` | unchanged |
| 78 | `SIM.md` | Q-SIM-6 | `src/frontend/rawfile.c:160-200` | `src/frontend/rawfile.c:160-200` | unchanged |
| 79 | `SIM.md` | Q-SIM-7 | `CMakeLists.txt:150-260` | `configure.ac:178-180`, `src/Makefile.am:19-32` | no CMake build in ngspice-47; rewritten |
| 80 | `SIM.md` | Q-SIM-7 | `cmake/NgspiceOptions.cmake:86-91` | `src/Makefile.am:498-616` | no CMake build in ngspice-47; rewritten |
| 81 | `SIM.md` | Q-SIM-7 | `NEWS:1` | `NEWS:1` | unchanged |
| 82 | `SIM.md` | Q-SIM-9 | `src/frontend/commands.c:188-520` | `src/frontend/commands.c:187-459` | moved |
| 83 | `SIM.md` | Q-SIM-9 | `src/sharedspice.c:24-40` | `src/sharedspice.c:24-40` | unchanged |
| 84 | `SIM.md` | Q-SIM-11 | `src/sharedspice.c:2134-2190` | `src/sharedspice.c:2132-2188` | moved |
| 85 | `SIM.md` | Q-SIM-11 | `src/sharedspice.c:1473-1479` | `src/sharedspice.c:1471-1477` | moved |
| 86 | `SIM.md` | Q-SIM-11 | `src/sharedspice.c:2534-2545` | `src/sharedspice.c:2529-2540` | moved |
| 87 | `SIM.md` | Q-SIM-12 | `cmake/NgspiceOptions.cmake:53` | `configure.ac:1320-1330` | no CMake build in ngspice-47; rewritten |
| 88 | `SIM.md` | Q-SIM-13 | `src/frontend/inpcom.c:2040-2130` | `src/frontend/inpcom.c:2016-2106` | moved |
| 89 | `SIM.md` | Q-SIM-13 | `src/frontend/inpcom.c:2160-2314` | `src/frontend/inpcom.c:2136-2290` | moved |
| 90 | `SIM.md` | Q-SIM-14 | `src/sharedspice.c:1955-2110` | `src/sharedspice.c:1953-2108` | moved |
| 91 | `SIM.md` | Q-SIM-15 | `src/sharedspice.c:18-22` | `src/sharedspice.c:18-22` | unchanged |
| 92 | `SIM.md` | Q-SIM-15 | `src/sharedspice.c:1100-1110` | `src/sharedspice.c:1099-1109` | moved |
| 93 | `SIM.md` | Q-SIM-16 | `src/sharedspice.c` | `src/sharedspice.c` | unchanged |
| 94 | `SIM.md` | Q-SIM-17 | `src/frontend/rawfile.c:114-130` | `src/frontend/rawfile.c:114-130` | unchanged |
| 95 | `SIM.md` | Q-SIM-17 | `src/frontend/rawfile.c:224-258` | `src/frontend/rawfile.c:224-258` | unchanged |
| 96 | `SIM.md` | Q-SIM-18 | `src/include/ngspice/sharedspice.h:185-210` | `src/include/ngspice/sharedspice.h:185-210` | unchanged |
| 97 | `SIM.md` | Q-SIM-18 | `src/include/ngspice/sharedspice.h:395-430` | `src/include/ngspice/sharedspice.h:370-405` | moved |
| 98 | `SIM.md` | Q-SIM-20 | `COPYING:1-45` | `COPYING:1-45` | unchanged |
| 99 | `SIMADV.md` | Q-SIMADV-6 | `src/maths/misc/randnumb.c:101-116` | `src/maths/misc/randnumb.c:101-116` | unchanged |
| 100 | `SIMADV.md` | Q-SIMADV-6 | `src/maths/misc/randnumb.c:140-150` | `src/maths/misc/randnumb.c:140-150` | unchanged |
| 101 | `SIMADV.md` | Q-SIMADV-6 | `src/maths/misc/randnumb.c:220-235` | `src/maths/misc/randnumb.c:220-235` | unchanged |
| 102 | `SIMADV.md` | Q-SIMADV-6 | `src/maths/misc/randnumb.c:304-330` | `src/maths/misc/randnumb.c:292-322` | content differs, finding F2 |
| 103 | `SIMADV.md` | Q-SIMADV-7 | `src/sharedspice.c:930-936` | `src/sharedspice.c:930-935`, `src/sharedspice.c:1079-1081` | content differs, finding F2 |
| 104 | `SIMADV.md` | Q-SIMADV-7 | `src/maths/misc/randnumb.c:71-92` | `src/maths/misc/randnumb.c:71-92` | unchanged |
| 105 | `SIMADV.md` | Q-SIMADV-7 | `src/maths/misc/randnumb.c:287-330` | `src/maths/misc/randnumb.c:286-322` | content differs, finding F2 |
| 106 | `SIMADV.md` | Q-SIMADV-7 | `src/maths/cmaths/cmath2.c:137` | `src/maths/cmaths/cmath2.c:137` | unchanged |
| 107 | `SIMADV.md` | Q-SIMADV-7 | `src/frontend/inpcom.c:8380-8412` | `src/frontend/inpcom.c:8226-8245` | content differs, finding F1 |
| 108 | `SIMADV.md` | Q-SIMADV-7 (Resolution) | `randnumb.c:292-330` | kept: it records the 2026-11-06 check of `7aa47164`; the `Re-checked` line gives randnumb.c:292-322 | content differs, finding F2 |
| 109 | `SIMADV.md` | Q-SIMADV-9 | `src/frontend/inpcom.c:1140-1160` | `src/frontend/inpcom.c:1140-1160` | unchanged |
| 110 | `SIMADV.md` | Q-SIMADV-9 | `src/frontend/inp.c:1831-1834` | `src/frontend/inp.c:1741-1744` | moved |
| 111 | `SIMADV.md` | Q-SIMADV-10 | `src/frontend/com_measure2.c:55-62` | `src/frontend/com_measure2.c:51-57` | content differs, finding F4 |
| 112 | `SIMADV.md` | Q-SIMADV-10 | `src/frontend/com_measure2.c:215-262` | `src/frontend/com_measure2.c:207-257` | content differs, finding F4 |
