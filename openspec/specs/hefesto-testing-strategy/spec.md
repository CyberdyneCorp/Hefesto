# Hefesto Testing Strategy

## Purpose
Defines how Hefesto proves that it meets its specifications. Testing has these layers:
- unit tests
- golden compatibility tests against Magic, netgen and ngspice, using the canonical comparison of capability `hefesto-output-determinism`
- property-based geometry tests
- fuzzing of every file parser
- cross-platform determinism checks
- performance benchmarks

## Requirements

### Requirement: Requirement-traceable tests
Every `exact` and `behavioral` requirement SHALL have at least one automated test that names the capability and the requirement in its metadata. CI SHALL publish a coverage report listing requirements that have no test.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-1)

#### Scenario: Untested requirement surfaced
- **WHEN** a new requirement is added without a test
- **THEN** the CI traceability report SHALL list it as uncovered

### Requirement: Golden compatibility suite
For each case in `tests/golden/<AREA>/CASES.md`, the suite SHALL store:
- the input bundle: tech file, layout and command script
- the reference output captured from the pinned reference tool: Magic 8.3.694 at commit fc5daa9e, the pinned netgen commit, or the pinned ngspice release

The headless CLI SHALL replay each case. Its outputs SHALL be compared using the canonical comparison of capability `hefesto-output-determinism`, with `magic-compat` ON. A capture script SHALL be able to regenerate the reference outputs.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-6)

#### Scenario: Golden DRC case
- **GIVEN** case DRC-width-01 with its captured Magic `drc listall why` output
- **WHEN** the suite runs the case in Hefesto
- **THEN** the error areas and why-strings SHALL match after canonicalization

### Requirement: Geometry property tests
Geometry operations SHALL be tested with randomized property tests. The tested operations are:
- paint and erase
- mask operators
- transforms
- DRC rule checks

Each test SHALL check invariants, for example:
- erase after paint restores the original
- a grow followed by a shrink of the same distance contains the original
- transform composition is consistent
- results are independent of the order in which paint is applied

Failing seeds SHALL be recorded as regression tests.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-1)

#### Scenario: Paint order independence
- **WHEN** the same set of rectangles is painted in two random orders
- **THEN** the resulting planes SHALL be identical, including their canonical tile decomposition

### Requirement: Parser fuzzing
Every reader of external input SHALL have a coverage-guided fuzz target. The readers are:
- `.tech`, `.mag`, `.ext`, `.res.ext`
- GDS, OASIS, CIF, LEF, DEF
- SPICE
- `.dstyle`, `.cmap`, `.net`
- netgen setup files

Malformed input SHALL never cause a crash, hang or memory-safety violation. CI SHALL run each fuzz target for a bounded time on every pull request and continuously on main.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-2)

#### Scenario: Truncated GDS
- **WHEN** a GDS file truncated in the middle of a record is read
- **THEN** Hefesto SHALL report a format error and SHALL NOT crash

### Requirement: Cross-platform determinism checks
CI SHALL run the golden suite on Linux x86_64, Linux aarch64, macOS arm64 and the iPadOS simulator. It SHALL fail if any platform's output differs byte-wise from the Linux x86_64 output, except for documented time-dependent fields.

Compatibility: native

(Hefesto decision: docs/decisions/cross-cutting.md#X-1)

#### Scenario: Platform drift
- **WHEN** a change makes the SPICE output on aarch64 differ from x86_64
- **THEN** CI SHALL fail

### Requirement: Performance benchmarks
CI SHALL run the reference workloads of capability `hefesto-performance` on dedicated runners, and SHALL flag any regression larger than 10% against the main-branch baseline.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-1)

#### Scenario: Regression flagged
- **WHEN** a pull request makes extraction 15% slower on the reference block
- **THEN** the benchmark check SHALL fail with the measured delta
