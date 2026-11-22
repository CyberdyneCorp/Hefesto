## ADDED Requirements

### Requirement: Chunked result transfer
On platforms where the engine runs in a worker process, result vectors SHALL arrive in chunks. The core SHALL assemble each vector into Hefesto-owned storage in point order. The final vectors SHALL be bit-identical to the vectors the same engine build returns in process for the same deck. A job whose transfer is interrupted by a fault SHALL keep the points that arrived completely, and SHALL be `failed`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-21.)

#### Scenario: Large transient result
- **GIVEN** a transient run that saves 200 vectors of 1 000 000 points each
- **WHEN** it completes on Linux
- **THEN** every stored vector SHALL equal, bit for bit, the vector from the developer-build in-process engine for the same deck

### Requirement: Raw files in golden comparisons
Golden comparisons of raw files SHALL ignore the `Date:` and `Command:` header lines. They SHALL compare the other header lines exactly, and the data values within the tolerances of capability `simulation-engine-embedding`, Requirement "Cross-platform result consistency". Hefesto SHALL write raw binary data little-endian on every platform.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-7) (See Q-SIM-17, Q-SIM-10.)

#### Scenario: Same run on another day
- **GIVEN** a raw file captured as a golden reference
- **WHEN** Hefesto writes the raw file of the same deck a day later on macOS arm64
- **THEN** the comparison SHALL pass even though the `Date:` lines differ
