# Simulation Optimization Hooks

## Purpose
Defines an optional evaluation interface through which optimisation loops drive simulation (decision P-19). The main client is the AI agent (decision P-10): its sizing and post-layout tuning loops (capability `agent-schematic-driven-layout`, Requirements "Post-layout comparison against the schematic" and "Iterative improvement") propose candidate parameter values and get back measurements, specification verdicts and a cost. User scripts (capability `hefesto-scripting`) MAY use the same interface. This capability covers:
- design variables, objectives and the cost of a candidate
- robustness levels: nominal, corners, Monte Carlo, or both
- the evaluation call, its determinism and its result cache
- budgets, isolation and approval of the outcome
- the evaluation history and the `sim opt` command option

Hefesto does not specify an optimisation algorithm. The interface only evaluates candidates, using run sets (capability `simulation-sweeps`), corners (capability `simulation-corners`), Monte Carlo (capability `simulation-monte-carlo`) and specifications (capability `simulation-measurements-and-statistics`). Agent structure, tools, scratch sessions and limits are in capabilities `agent-architecture` and `agent-safety-and-approval`.

Open questions: `docs/decisions/open-questions/SIMADV.md` (Q-SIMADV-n).

## Requirements

### Requirement: Design variables
An optimisation setup SHALL declare design variables. Each is a top-level testbench parameter (capability `simulation-sweeps`, Requirement "Sweep variables", kind `param:`) with either:
- a continuous range `lo < hi`, a scale `lin` or `log` (`log` requires `lo > 0`), and an optional grid `g > 0`; or
- a discrete list of allowed values.

A candidate SHALL assign one value to every design variable. A value outside `[lo, hi]`, or not in the discrete list, SHALL be rejected with `Candidate: <name>=<value> is outside its range`. With a grid, the value SHALL be snapped to `lo + round((x − lo)/g)·g`, rounding halves away from zero, and both the requested and the snapped value SHALL be recorded. Values SHALL be written into decks in C `%.12g` format, as sweep levels are. Variables that set device geometry SHOULD use the PDK's manufacturing grid as `g`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19, docs/decisions/product.md#P-10)

#### Scenario: Snapping to a 5 nm grid
- **GIVEN** a design variable `wn` with range 0.42 to 10 (µm) and grid 0.005
- **WHEN** a candidate requests `wn` = 1.2337
- **THEN** the evaluated value SHALL be 1.235, and the record SHALL hold both 1.2337 and 1.235

### Requirement: Objective and cost
An optimisation setup SHALL name one objective (a measurement or measurement expression of capability `simulation-measurements-and-statistics`, with direction `minimize` or `maximize`) and a set of constraint specifications (by default every specification of the testbench). For a candidate, with objective value `f`, reference value `f_ref` (the objective of the reference candidate, see below, or 1 if that is 0 or has no value), direction sign `σ` (+1 for minimize, −1 for maximize), penalty weight `P` (default 100) and, for each constraint `k`, a violation `v_k`:

```
v_k = max(0, −μ_k)     when the specification has a value and a defined margin μ_k
v_k = 0 or 1           when the margin is undefined: 0 if it passes, 1 if it fails
v_k = 1                when the measurement has no value
cost J = σ · f / |f_ref| + P · Σ_k v_k²
```

A candidate is `feasible` when every constraint passes. If `f` has no value, `J` SHALL be `+∞`. Candidates SHALL be ranked feasible first, then by `J` ascending; ties SHALL be broken by evaluation order. The reference candidate is the testbench's current parameter values; it SHALL be evaluated first by every optimisation session.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Penalised cost
- **GIVEN** objective `power`, minimize, reference value 1 mW, `P` = 100
- **WHEN** candidate A has `power` = 0.8 mW and passes every constraint, and candidate B has `power` = 0.5 mW and one constraint with margin −0.2
- **THEN** `J(A)` SHALL be 0.8, `J(B)` SHALL be 4.5, and A SHALL rank above B

### Requirement: Robustness levels
Each evaluation SHALL run the candidate at one robustness level:

| Level | Run set |
|---|---|
| `nominal` | one point at the testbench's selected corner |
| `corners` | the corner dimension of the selected corner set |
| `mc` | a Monte Carlo dimension with given mode, run count and base seed |
| `corners+mc` | the corner dimension outside a Monte Carlo dimension |

For levels with more than one point, `f` SHALL be the worst value over the points (the maximum for minimize, the minimum for maximize) unless the setup selects `mean`, and each `μ_k` SHALL be the minimum margin over the points. The result SHALL also report the yield (capability `simulation-measurements-and-statistics`, Requirement "Yield") for levels that include Monte Carlo. All candidates of one optimisation session that use Monte Carlo SHALL use the same base seed, so that differences between candidates are not sampling noise (common random numbers), unless the setup asks for a new seed per evaluation.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19)

#### Scenario: Worst-case objective over corners
- **GIVEN** objective `tpd`, minimize, at level `corners`, and a candidate whose `tpd` over 5 corners is 80p, 131p, 55p, 97p and 102p
- **WHEN** the candidate is evaluated
- **THEN** `f` SHALL be 131p

### Requirement: Evaluation interface
Hefesto SHALL provide an evaluation call that takes a list of candidates and a robustness level and returns, for each candidate in input order:
- the evaluated (snapped) variable values
- every measurement value or its "no value" reason, per point and aggregated
- each constraint's result and margin
- `feasible`, `f`, `J`, and the yield where applicable
- the run set id or ids that produced it

Each candidate SHALL be evaluated as a run set built from the testbench's base deck with the candidate's values as parameter overrides (capability `simulation-sweeps`, Requirement "Point decks"); several candidates MAY share one run set, as a DOE-like dimension of candidate rows. The base deck SHALL be prepared once per session and re-prepared only when the session is told that the circuit changed (for example, after the agent edits the scratch layout). Evaluations SHALL use the execution model of capability `simulation-sweeps` (parallel workers on Linux and macOS, sequential in process on iPadOS) and SHALL give the same results whatever the worker count.

Results SHALL be cached for the session under the key `SHA-256(base deck hash, robustness level and its settings, the snapped values in %.17g in variable order)`. A repeated candidate SHALL be answered from the cache without simulation, and the answer SHALL be marked `cached`.

The call SHALL be available as the agent tool `sim_evaluate`, together with the read-only tool `sim_describe`, which returns the setup (variables, objective, constraints, robustness levels, budgets used and remaining) and the best candidates so far. Both tools SHALL map to `sim opt` commands (Requirement "Optimisation command options"), as capability `agent-architecture`, Requirement "Tools map to the command API", requires.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19, docs/decisions/product.md#P-10)

#### Scenario: Cached candidate
- **GIVEN** an optimisation session in which candidate `wn=1.235 wp=2.5` was evaluated at level `nominal`
- **WHEN** the agent calls `sim_evaluate` again with `wn=1.2337 wp=2.5` (which snaps to the same values) at level `nominal`
- **THEN** the result SHALL be returned from the cache, marked `cached`, and no job SHALL be submitted

### Requirement: Budgets, isolation and approval
An optimisation session SHALL have limits on the number of evaluations, the number of simulated points and the wall time, in addition to the agent limits of capability `agent-safety-and-approval` (Requirement "Resource and scope limits"). When a limit is reached, further evaluation calls SHALL fail with `Optimisation budget exhausted: <limit>`, and the session SHALL keep its history.

Evaluations SHALL NOT change the user's testbench, schematic or cells. When the agent runs them, they SHALL use the scratch session (capability `agent-architecture`, Requirement "Scratch session isolation"), and their run sets SHALL be stored under the scratch session, not in the project, until accepted.

Applying an optimisation result SHALL be a proposal (capability `agent-safety-and-approval`, Requirement "Proposals require explicit acceptance"): the parameter or device-size changes, with the reference and proposed results side by side. A proposal SHALL be marked "verified" only if the proposed candidate has been evaluated at level `corners`, or at the highest level the setup declares, and is feasible there (capability `agent-architecture`, Requirement "Verification gates are tools, not opinions"). Accepting a proposal SHALL be one undo step.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19, docs/decisions/product.md#P-10)

#### Scenario: Unverified best candidate
- **GIVEN** a session whose best candidate was evaluated only at level `nominal`, with the setup's highest level `corners`
- **WHEN** the agent presents it as a proposal
- **THEN** the proposal SHALL be marked "unverified" until the candidate passes at level `corners`

#### Scenario: Evaluation budget
- **GIVEN** a session limited to 50 evaluations, all used
- **WHEN** `sim_evaluate` is called
- **THEN** it SHALL fail with `Optimisation budget exhausted: evaluations`, and the history SHALL still list the 50 evaluations

### Requirement: Evaluation history
Each optimisation session SHALL keep an ordered history of evaluations: the candidate, the robustness level, the results of Requirement "Evaluation interface", the caller (agent task id or script), and a timestamp. Hefesto SHALL show the history as a table and as a plot of `J` against evaluation number, marking feasible candidates, and SHALL let the user open any evaluation's run set. The history SHALL be exportable as CSV in the format rules of capability `simulation-measurements-and-statistics`, Requirement "Result export", with columns `eval`, `level`, the variables, `f`, `J`, `feasible`, `yield`, then one `<spec>:margin` column per constraint. When a proposal is accepted, the session id SHALL be recorded in the agent provenance (capability `agent-safety-and-approval`, Requirement "Provenance").

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19, docs/decisions/product.md#P-10)

#### Scenario: History export
- **GIVEN** a session with 12 evaluations and 2 constraints `gain` and `pm`
- **WHEN** the history is exported
- **THEN** the CSV SHALL have 12 data rows and a header ending in `gain:margin,pm:margin`

### Requirement: Optimisation command options
The `sim` command (capability `simulation-command`) SHALL accept the option `opt`:

| Syntax | Effect / result |
|---|---|
| `opt var <name> <lo> <hi> [-log] [-grid <g>]` \| `opt var <name> -list <v…>` \| `opt var delete <name>` | Declare design variables |
| `opt objective <measurement> minimize\|maximize [-mean]` | Set the objective |
| `opt constraints all\|<spec…>` | Choose constraint specifications |
| `opt session new [-evals <n>] [-points <n>] [-time <s>] [-penalty <P>]` \| `opt session close` | Start or end a session; `new` returns the session id |
| `opt eval <level> <name>=<value>… [; <name>=<value>…]` | Evaluate candidates; return one result list per candidate |
| `opt describe` | Return the setup, budgets and best candidates |
| `opt history [-export <file>]` | Return or export the history |

`opt eval` SHALL fail with `sim opt: no session` when no session is open. All other rules SHALL follow capability `simulation-command`. The options SHALL work headless and on every platform.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-1, Q-SIMADV-12.)

#### Scenario: Evaluate without a session
- **WHEN** `sim opt eval nominal wn=1` is executed with no open session
- **THEN** it SHALL fail with `sim opt: no session`
