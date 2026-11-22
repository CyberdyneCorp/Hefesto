## ADDED Requirements

### Requirement: ngspice measure cross-check in debug builds
Hefesto's measurement value SHALL be the authoritative result in every build. In debug builds, when a testbench also contains an equivalent ngspice `.meas` statement, Hefesto SHALL capture ngspice's value too. It SHALL log a warning naming the measurement and both values when they differ by more than the measurement's tolerance. Release builds SHALL NOT run the cross-check.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19) (See Q-SIMADV-10.)

#### Scenario: Rise time differs
- **GIVEN** a debug build and a testbench with Hefesto measurement `trise` and an equivalent `.meas tran trise ...`
- **WHEN** the two values differ by more than the tolerance
- **THEN** the job log SHALL contain a warning with both values, and the result table SHALL show Hefesto's value

### Requirement: Platform-independent statistics
Descriptive statistics, yield and process capability SHALL be computed in binary64, summing in point-index order with compensated summation. The same measurement table SHALL give bit-identical statistics on every supported platform and for every worker count.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-19, docs/decisions/cross-cutting.md#X-1)

#### Scenario: Mean on two platforms
- **GIVEN** the same 10 000-sample measurement table on Linux x86_64 and macOS arm64
- **WHEN** the mean and standard deviation are computed
- **THEN** both platforms SHALL report bit-identical values
