# Reliability EM Rules

## Purpose
Defines how electromigration (EM) current limits are represented as PDK data in the PDK store, how a limit is looked up for one wire segment, via or contact, and which limits Hefesto bundles for the supported open PDKs (decision P-21). Capability `reliability-em-check` uses these limits. Magic has no EM feature, so every requirement here is `native`.

Research sources (cloned read-only for this spec, not built or run):
- IHP-Open-PDK @ `5e6d592e4002946a4616f798c357f0f3c06cf3b6`: `ihp-sg13g2/libs.doc/doc/SG13G2_os_process_spec.pdf`, Rev. 1.2, §2.15 "Maximum Current Densities" and note A.v (`ihp ref:`)
- gf180mcu-pdk @ `de3240d7529a6970437ac3344820aaae7839f215`: design manual §14.2 "Electro-migration", `docs/physical_verification/design_manual/drm_14_2.rst`, tables `54_Electro-migration1_150.csv` and `54_Electro-migration2_151.csv` (`gf ref:`)
- skywater-pdk @ `7198cf647113f56041e02abf3eb623692820c5e1`: `docs/rules/periphery-rules.rst` rule (x.4) (`sky ref:`)
- open_pdks @ `c6df28c1fccb05c286aa0a15e9c4c7dcd8502607`: gf180mcu variant options in `gf180mcu/Makefile.in:55-63`, and layer type names in the Magic tech files

Units used throughout:
- widths and thicknesses: µm
- currents: mA
- linear current density: mA/µm
- areal current density: mA/µm²
- temperatures: °C (converted to kelvin as T + 273.15 where a formula needs it)

All arithmetic is IEEE-754 double precision.

## Requirements

### Requirement: EM rule file in the PDK store
Each PDK store entry (capability `hefesto-files-and-pdks`) MAY hold an EM rule file at `hefesto/reliability/em-rules.json`. A project MAY override it with `reliability/em-rules.json` in the project folder. The file SHALL be a UTF-8 JSON object:

| Key | Type | Rule |
|---|---|---|
| `schema` | int | `1` |
| `pdk` | string | The PDK variant name, as in the PDK store, e.g. `sg13g2`, `gf180mcuD` |
| `status` | string | `published` (from the foundry's public documents), `user` (supplied by the user or a plugin), or `unavailable` (no public data; `rules` SHALL be empty) |
| `source` | object | `{document, revision, section, url, commit}`; `document` is required, the others are optional strings |
| `basis` | object | `{lifetimeHours, failureFraction, temperatureC}`: the reliability target the limits were derived for; each field optional |
| `rules` | array | Rule objects, defined below |

A rule object:

| Key | Type | Rule |
|---|---|---|
| `id` | string | Unique within the file; the foundry rule name where one exists (e.g. `JMAXM2`) |
| `types` | array of strings | Magic layer type names or aliases from the technology, e.g. `["metal2"]` |
| `kind` | string | `wire`, `via` or `contact` |
| `metric` | string | `avg`, `absavg`, `rms`, `peak` or `peakdir` (defined in capability `reliability-em-check`) |
| `direction` | string | `uni`, `bi` or `any` |
| `limit` | object | `{"type": "perWidth"\|"perArea"\|"perCut"\|"absolute", "values": [{"tC": number, "value": number}, ...]}` |
| `width` | object | Wires only, optional: `{"min": number, "minInclusive": bool, "max": number, "maxInclusive": bool}`; defaults are min 0 inclusive and max +∞ |
| `widthAdjust` | number | Wires only, optional, default 0: subtracted from the drawn width before multiplying a `perWidth` or `perArea` limit |
| `thickness` | number | Required for `perArea`: the conductor thickness in µm |
| `note` | string | Optional free text, shown in reports |

Limit units by `limit.type`:
- `perWidth`: mA/µm of width
- `perArea`: mA/µm² of cross-section
- `perCut`: mA per contact or via cut
- `absolute`: mA per segment

Example (an excerpt of the bundled sg13g2 file):

```json
{
  "schema": 1,
  "pdk": "sg13g2",
  "status": "published",
  "source": {"document": "SG13G2 Process Specification", "revision": "1.2", "section": "2.15",
             "commit": "5e6d592e4002946a4616f798c357f0f3c06cf3b6"},
  "basis": {"lifetimeHours": 96360, "failureFraction": 0.0001, "temperatureC": 105},
  "rules": [
    {"id": "IMAXM1", "types": ["metal1"], "kind": "wire", "metric": "avg", "direction": "any",
     "width": {"min": 0.16, "minInclusive": true, "max": 0.36, "maxInclusive": true},
     "limit": {"type": "absolute", "values": [{"tC": 105, "value": 0.36}]}},
    {"id": "JMAXM1", "types": ["metal1"], "kind": "wire", "metric": "avg", "direction": "any",
     "width": {"min": 0.36, "minInclusive": false},
     "limit": {"type": "perWidth", "values": [{"tC": 105, "value": 1.0}]}},
    {"id": "JMAXVIA1", "types": ["via1"], "kind": "via", "metric": "avg", "direction": "any",
     "limit": {"type": "perCut", "values": [{"tC": 105, "value": 0.4}]}}
  ]
}
```

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21) (ihp ref: SG13G2_os_process_spec.pdf §2.15, note A.v)

#### Scenario: Project override wins
- **GIVEN** sg13g2 installed with its bundled rule file, and a project with `reliability/em-rules.json` whose `JMAXM2` is 1.5 mA/µm
- **WHEN** a limit for metal2 is looked up in that project
- **THEN** the project file SHALL be used, and the report SHALL name the project file as the source

### Requirement: Rule file validation
An EM rule file is untrusted input (capability `hefesto-security`). Loading SHALL reject the file with an `error` diagnostic and use no rules from it when any of these hold:
- the JSON is malformed, or `schema` is not supported (`REL-EMR-001`)
- a required key is missing, or a value has the wrong type or an unknown enum value (`REL-EMR-002`)
- a limit value is ≤ 0, not finite, or `values` is empty (`REL-EMR-003`)
- the `tC` values of one rule are not strictly ascending (`REL-EMR-004`)
- a `perArea` rule has no positive `thickness` (`REL-EMR-005`)
- two rules with the same type, kind, metric and direction have overlapping width ranges (`REL-EMR-006`, naming both ids)
- `status` is `unavailable` and `rules` is not empty (`REL-EMR-007`)
- one type appears in both a `via` rule and a `contact` rule (`REL-EMR-009`), because cut items look up both kinds

A rule naming a type that the loaded technology does not define SHALL produce `warning` `REL-EMR-008`, and only that type entry SHALL be ignored. Aliases SHALL be resolved to the technology's canonical type names before overlap checking.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Overlapping width ranges
- **GIVEN** two metal1 `avg` `any` wire rules, with widths [0.16, 0.36] and [0.30, +∞)
- **WHEN** the file is loaded
- **THEN** it SHALL be rejected with `REL-EMR-006` naming both rule ids

#### Scenario: Unknown layer name
- **GIVEN** a rule with `"types": ["metal1", "metal9"]` on a technology without `metal9`
- **WHEN** the file is loaded
- **THEN** `REL-EMR-008` SHALL name `metal9`, and the rule SHALL still apply to metal1

### Requirement: Rule source precedence
For a design, the effective rule file SHALL be the first that applies of:
1. the project's `reliability/em-rules.json`
2. a file supplied by a PDK adapter plugin, if the user selected that adapter for EM rules in project settings (capability `plugin-api`)
3. the PDK store entry's file, unless its `status` is `unavailable`
4. a file supplied by a PDK adapter plugin that the user did not select explicitly; if several exist, the one whose plugin id sorts first in code-point order

If none applies, the design has no EM rules. Every EM check then reports every item as `unchecked`, with one `warning` `REL-EMR-020` naming the PDK. Rule files SHALL NOT be merged: exactly one file is effective.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21, docs/decisions/product.md#P-20)

#### Scenario: sky130 without user data
- **GIVEN** sky130A, whose bundled file has `status: unavailable`, no project file and no adapter
- **WHEN** an EM check runs
- **THEN** every item SHALL be `unchecked`, and `REL-EMR-020` SHALL state that no EM limits are available for sky130A

### Requirement: Limit lookup
Given a query (type τ, kind k, drawn width w for wires, metric m, direction d, temperature T), the limit SHALL be found as follows:

1. Resolve τ to its canonical type. Candidates are the rules whose `types` contain τ, whose `kind` = k and whose `metric` = m.
2. Direction preference:
   - for d = `uni`: rules with `uni`, then `any`
   - for d = `bi`: rules with `bi`, then `any`, then `uni`

   Take the first preference level that has candidates. Using a `uni` limit for bidirectional current is conservative, because published bidirectional limits are never lower than unidirectional ones.
3. For wires, keep the candidates whose width range contains w, honoring the inclusivity flags. Validation guarantees that at most one remains.
4. No candidate means the item is `unchecked`.
5. Evaluate the rule's limit value L(T) by Requirement "Temperature dependence".
6. Convert L(T) to a current limit I_lim in mA:
   - `perWidth`: L · (w − widthAdjust)
   - `perArea`: L · (w − widthAdjust) · thickness
   - `perCut`: L, compared with the per-cut current
   - `absolute`: L

If (w − widthAdjust) ≤ 0, the item SHALL be reported as `unchecked` with `REL-EMR-011`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Narrow IHP metal1 uses the absolute limit
- **GIVEN** the bundled sg13g2 rules
- **WHEN** the limit is looked up for metal1, wire, w = 0.20 µm, metric avg, direction uni, T = 105 °C
- **THEN** rule `IMAXM1` SHALL apply, with I_lim = 0.36 mA

#### Scenario: Wide IHP metal2 scales with width
- **WHEN** the limit is looked up for metal2, wire, w = 1.00 µm, avg, uni, T = 105 °C
- **THEN** rule `JMAXM2` SHALL apply, with I_lim = 2 · 1.00 = 2.00 mA

#### Scenario: Width boundary
- **WHEN** the limit is looked up for sg13g2 metal1 with w = 0.36 µm exactly
- **THEN** `IMAXM1` SHALL apply (its max is inclusive), and I_lim = 0.36 mA, equal to what `JMAXM1` would give at that width

### Requirement: Temperature dependence
Let a rule's tabulated points be (t₁, v₁) … (tₙ, vₙ) with t₁ < … < tₙ, and let the analysis temperature be T. Then L(T) SHALL be:
- **T equals some tᵢ:** vᵢ.
- **tᵢ < T < tᵢ₊₁:** interpolation of ln v that is linear in 1/T in kelvin, which is exact for Black's equation:
  x = 1/(T+273.15), xᵢ = 1/(tᵢ+273.15)
  L = exp( ln vᵢ + (ln vᵢ₊₁ − ln vᵢ) · (x − xᵢ)/(xᵢ₊₁ − xᵢ) )
- **T < t₁:** v₁. Hefesto takes no credit for cooler operation below the tabulated range.
- **T > tₙ:** no limit. The item is `unchecked`, and `warning` `REL-EMR-010` is emitted once per rule per run, naming the rule and its range.

A rule with one point (n = 1) therefore applies unchanged for every T ≤ t₁. The default analysis temperature SHALL be `basis.temperatureC` when present, and otherwise tₙ of each rule.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21) (gf ref: drm_14_2.rst §14.2.2) (See Q-RELIABILITY-3.)

#### Scenario: gf180 interpolation at 100 °C
- **GIVEN** the gf180mcu metal1 unidirectional points (85, 2.09) and (110, 1.0) mA/µm
- **WHEN** L(100 °C) is evaluated
- **THEN** L SHALL be 1.3271 mA/µm (to 4 decimals)

#### Scenario: Below the table
- **WHEN** the same rule is evaluated at 27 °C
- **THEN** L SHALL be 2.09 mA/µm, the 85 °C value

#### Scenario: Above the table
- **WHEN** an sg13g2 rule (single point at 105 °C) is evaluated at 125 °C
- **THEN** the item SHALL be `unchecked` and `REL-EMR-010` SHALL be emitted

### Requirement: Bundled rules for IHP sg13g2
Hefesto SHALL bundle `em-rules.json` for `sg13g2` with `status: published`, the basis {lifetimeHours 96360 (11 years), failureFraction 0.0001, temperatureC 105}, and the rules below. All rules have metric `avg`, direction `any`, and a single point at 105 °C. The layer-type mapping is that of the open_pdks / IHP Magic technology and is to be verified (Q-RELIABILITY-4):

| id | types | kind | width range (µm) | limit type | value |
|---|---|---|---|---|---|
| IMAXM1 | metal1 | wire | [0.16, 0.36] | absolute | 0.36 mA |
| JMAXM1 | metal1 | wire | (0.36, ∞) | perWidth | 1 mA/µm |
| IMAXM2…IMAXM5 | metal2 … metal5 (one rule each) | wire | [0.20, 0.30] | absolute | 0.6 mA |
| JMAXM2…JMAXM5 | metal2 … metal5 (one rule each) | wire | (0.30, ∞) | perWidth | 2 mA/µm |
| JMAXM6 | metal6 (TopMetal1) | wire | all | perWidth | 15 mA/µm |
| JMAXM7 | metal7 (TopMetal2) | wire | all | perWidth | 16 mA/µm |
| JMAXCNT | every contact type to metal1 (ndiffc, pdiffc, polycont, the substrate and tap contacts, …) | contact | — | perCut | 0.3 mA |
| JMAXVIA1…JMAXVIA4 | via1 … via4 (one rule each) | via | — | perCut | 0.4 mA |
| JMAXTVIA1 | via5 (TopVia1) | via | — | perCut | 1.4 mA |
| JMAXTVIA2 | via6 (TopVia2) | via | — | perCut | 10 mA |

The source publishes only DC (average) limits: no RMS or peak limits, no separate unidirectional and bidirectional values, and no temperature derating (Q-RELIABILITY-1, Q-RELIABILITY-2). Wires narrower than the smallest listed width are below the minimum-width design rule and are reported `unchecked`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21) (ihp ref: SG13G2_os_process_spec.pdf §2.15 table "Maximum Current Densities (11 years @105°C)", note A.v)

#### Scenario: TopVia2 per-cut limit
- **WHEN** the limit is looked up for sg13g2 via6, kind via, metric avg, at 105 °C
- **THEN** I_lim SHALL be 10 mA per cut

### Requirement: Bundled rules for gf180mcu
Hefesto SHALL bundle `em-rules.json` for each of `gf180mcuA`, `gf180mcuB`, `gf180mcuC` and `gf180mcuD`, with `status: published` and basis {temperatureC 110} (Q-RELIABILITY-3). Each rule has points at 85, 110 and 125 °C.

Line rules are `perWidth` (mA/µm of drawn width). Metric `avg` applies with direction `uni`, and metric `peakdir` with direction `bi`:

| Conductor | uni 85 / 110 / 125 °C | bi 85 / 110 / 125 °C |
|---|---|---|
| metal1 to the metal below the top metal | 2.09 / 1.0 / 0.67 | 3.14 / 1.5 / 1.0 |
| top metal, 6 kÅ | 3.3 / 1.6 / 1.07 | 5.03 / 2.4 / 1.6 |
| top metal, 9 kÅ | 3.77 / 1.8 / 1.21 | 5.66 / 2.7 / 1.82 |
| top metal, 11 kÅ | 4.5 / 2.2 / 1.5 | 7 / 3 / 2.2 |
| top metal, 30 kÅ | 16.52 / 8.01 / 5.37 | 24.78 / 12.02 / 8.06 |

Contact and via rules are `perCut` (mA per cut), with the same metric and direction convention:

| Cut | uni 85 / 110 / 125 °C | bi 85 / 110 / 125 °C |
|---|---|---|
| POLY2 contact (polycontact) | 0.43 / 0.21 / 0.14 | 0.65 / 0.3 / 0.21 |
| COMP contact (every diffusion, tap and substrate contact type) | 0.58 / 0.28 / 0.18 | 0.87 / 0.42 / 0.28 |
| via1 … via5 | 0.58 / 0.28 / 0.18 | 0.87 / 0.42 / 0.28 |

The variant determines the top metal and its thickness, following the open_pdks options:

| Variant | Metals | Top metal type | Top metal row |
|---|---|---|---|
| gf180mcuA | 3 | metal3 | 30 kÅ |
| gf180mcuB | 4 | metal4 | 11 kÅ |
| gf180mcuC | 5 | metal5 | 9 kÅ |
| gf180mcuD | 5 | metal5 | 11 kÅ |

No shipped variant uses the 6 kÅ top metal row (see Q-RELIABILITY-12).

The rule file SHALL record in `note` that the published lifetime target is T₀.₁ > 100 000 h at 85 °C, that the formula of §14.2.2 assumes Ea = 0.7 eV and n = 2, and that "CB" in the table headings is undefined in the source (Q-RELIABILITY-5).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21) (gf ref: drm_14_2.rst §14.2–14.2.2, tables 14.3 and 14.4) (open_pdks ref: gf180mcu/Makefile.in:55-63)

#### Scenario: gf180mcuD top metal
- **WHEN** the limit is looked up for gf180mcuD metal5, wire, w = 2.0 µm, metric avg, direction uni, T = 110 °C
- **THEN** the 11 kÅ row SHALL apply, and I_lim = 2.2 · 2.0 = 4.4 mA

#### Scenario: gf180mcuD lower metal
- **WHEN** the limit is looked up for gf180mcuD metal4, wire, w = 1.0 µm, metric peakdir, direction bi, T = 125 °C
- **THEN** I_lim SHALL be 1.0 mA

### Requirement: No bundled rules for sky130
Hefesto SHALL bundle, for `sky130A` and `sky130B`, an `em-rules.json` with `status: unavailable`, an empty `rules` array, and `source.document` naming SkyWater periphery rule (x.4). Rule (x.4) states that electrical design rules cover electromigration, and flags the rule as not checked. The public documentation gives no values. Hefesto SHALL NOT invent sky130 limits. Users or PDK adapter plugins MAY supply a `user` file.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21) (sky ref: docs/rules/periphery-rules.rst rule (x.4)) (See Q-RELIABILITY-1.)

#### Scenario: UI explains the gap
- **WHEN** the user opens EM settings for a sky130A project
- **THEN** the panel SHALL say that SkyWater publishes no electromigration limits for sky130, and SHALL offer to import a rule file

### Requirement: Rule file authoring and inspection
Hefesto SHALL let the user inspect and author rule files. It SHALL:
- show the effective file, its source and its status, and each rule's limit curve over temperature
- create a project override, prefilled from the effective file
- validate edits as Requirement "Rule file validation" specifies before saving
- provide the command `emrules [show|validate <file>|export <file>]` in the command API, usable headless

`emrules show` SHALL print one line per rule, in file order, in this form:

```
<id> <kind> <types joined by ','> <metric>/<direction> <limit type> <value>@<tC>[ ...]
```

Numbers use the shortest representation that round-trips.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-21)

#### Scenario: Show rule
- **GIVEN** the bundled sg13g2 rules
- **WHEN** `emrules show` is run
- **THEN** a line `JMAXM2 wire metal2 avg/any perWidth 2@105` SHALL be printed
