## ADDED Requirements

### Requirement: Platform-independent property arithmetic
Property values SHALL be parsed into IEEE 754 binary64 with a locale-independent parser, and tolerance tests SHALL use binary64 arithmetic in the order the tolerance rules state. Values printed in reports SHALL be formatted by a locale-independent formatter. The same inputs SHALL give the same match decisions and the same report text on every supported platform.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5, docs/decisions/cross-cutting.md#X-1)

#### Scenario: Tolerance edge on two platforms
- **GIVEN** two resistors with `w=1.0` and `w=1.01` and a relative tolerance of `0.01`
- **WHEN** the comparison runs on Linux x86_64 and on macOS arm64
- **THEN** both runs SHALL make the same match decision and write the same property report text
