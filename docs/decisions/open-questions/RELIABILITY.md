# Open Questions — RELIABILITY (electromigration and IR drop)

Capabilities: `reliability-em-rules`, `reliability-ir-drop`, `reliability-em-check`, `reliability-visualization`. Product decision: `docs/decisions/product.md#P-21`.

These capabilities are `native`. Magic has no EM or IR-drop analysis. The nearest Magic features are `extresist` (resistance networks, capability `extresist-*`) and the FastHenry export. "Evidence" therefore points at PDK documents.

The following sources were cloned read-only into the session scratchpad, and were neither built nor run:
- IHP-Open-PDK @ `5e6d592e4002946a4616f798c357f0f3c06cf3b6`, docs `ihp-sg13g2/libs.doc/doc/`
- gf180mcu-pdk @ `de3240d7529a6970437ac3344820aaae7839f215`
- skywater-pdk @ `7198cf647113f56041e02abf3eb623692820c5e1`
- open_pdks @ `c6df28c1fccb05c286aa0a15e9c4c7dcd8502607`

### Q-RELIABILITY-1: sky130 publishes no EM limits; no PDK publishes RMS or peak limits
- Kind: doc-mismatch
- Magic behavior: none.
- Evidence: skywater-pdk docs/rules/periphery-rules.rst rule (x.4): "Electrical DR cover layout guidelines for electromigration", flagged NC (not checked). No table of values exists anywhere in the public docs; a search of `docs/` for "electromigration", "current density" and "mA/um" finds only this line. open_pdks has no EM data for any PDK. IHP SG13G2 process spec §2.15 gives DC limits only. The gf180mcu design manual §14.2 gives average limits (unidirectional) and averaged-over-the-dominant-direction limits (bidirectional), but no RMS (Joule heating) or peak limits.
- Options: (a) ship sky130 with `status: unavailable` and no limits, and leave RMS and peak checks to user-supplied files (specified); (b) derive sky130 limits from the SkyWater/Google design documents that circulate unofficially, which is not allowed because they are not publicly licensed; (c) ask SkyWater, Efabless' successors and the open_pdks maintainers to publish an EM table.
- Recommendation: (a) plus (c). Hefesto must not invent limits. The EM panel explains the gap, and the rule-file format already supports RMS and peak metrics for when data appears.

### Q-RELIABILITY-2: IHP SG13G2 metric and temperature dependence
- Kind: ambiguity
- Magic behavior: none.
- Evidence: SG13G2_os_process_spec.pdf Rev 1.2 §2.15 "Maximum Current Densities (11 years @105°C)" and note A.v ("determined via electromigration measurements … less than 0.01% failure in 11 years of operation at 105°C").
- Questions: the table does not say whether the limits are for DC or average current, or how to treat AC or bidirectional currents. It gives no derating to other temperatures, and no Ea or n. It also gives no limit for poly-resistor heads beyond "limited by contacts, chapter 2.9", RPPD 1.2 mA/µm and RHIGH 0.6 mA/µm (these are resistor-body limits, not interconnect).
- Options: (a) treat the values as `avg`, direction `any`, a single point at 105 °C, unchanged below 105 °C and unchecked above (specified); (b) apply Black's equation with assumed Ea and n to extend above 105 °C; (c) ask IHP.
- Recommendation: (a) plus (c). Consider adding the poly-resistor body limits as a later rule kind (`resistor`), once device-level EM is in scope.

### Q-RELIABILITY-3: gf180mcu reliability basis and temperature formula
- Kind: doc-mismatch
- Magic behavior: none.
- Evidence: gf180mcu-pdk docs/physical_verification/design_manual/drm_14_2.rst. The text states a lifetime of "T0.1 > 100K Hours at 85 deg C". The tables give values at 85, 110 and 125 °C. The derating formula `I = I0 [exp(Ea/k (1/T − 1/T0))]^(1/n)` uses T0 = 110 °C, Ea = 0.7 eV and n = 2, and is stated as valid "below 110 deg C". The formula reproduces the table: from 1.0 mA/µm at 110 °C it gives 2.095 at 85 °C (table: 2.09) and 0.671 at 125 °C (table: 0.67). The 125 °C column therefore goes beyond the stated validity.
- Options: (a) use the tabulated points with log-linear interpolation in 1/T between them (equivalent to the formula), no credit below 85 °C, and unchecked above 125 °C (specified); (b) apply the formula for every T < 110 °C, which gives 18.7× the 110 °C limit at 27 °C; (c) use only the 110 °C column.
- Recommendation: (a). Set the default analysis temperature to 110 °C (`basis.temperatureC`), because the formula is anchored there. Confirm with GlobalFoundries or the gf180mcu maintainers which lifetime target applies at which temperature.

### Q-RELIABILITY-4: Mapping foundry layer names to Magic types
- Kind: ambiguity
- Magic behavior: the IHP Magic technology (ihp-sg13g2.tech, `types` section) names TopMetal1 and TopMetal2 `metal6` and `metal7`, and the vias `via1`…`via6`. gf180mcu.tech names the top metal `metal3`, `metal4` or `metal5` depending on the variant.
- Evidence: ihp-sg13g2/libs.tech/magic/ihp-sg13g2.tech `types`; open_pdks gf180mcu/magic/gf180mcu.tech `types`; gf180mcu/Makefile.in:55-63 (gf180mcuA = 3 metals with 3.0 µm top metal, B = 4 metals with 1.1 µm, C = 5 metals with 0.9 µm, D = 5 metals with 1.1 µm).
- Questions: confirm that IHP `via5` and `via6` are TopVia1 and TopVia2. Confirm the full list of IHP and gf180 contact types to which the contact limits apply (diffusion, tap, substrate, poly, and the HV and MV variants). Decide whether the resistor-terminal types (`rmetal*`) and fill types get the parent metal's limits.
- Options: (a) bundle explicit type lists, verified against each pinned PDK version (specified, with these lists pending); (b) derive the lists from the tech file's planes and contact residues at load time.
- Recommendation: (a), with a CI test that every rule type exists in the pinned Magic tech file. Give `rmetal*` the parent metal's limits, and exclude fill types (they carry no current).

### Q-RELIABILITY-5: gf180mcu "CB" and the bidirectional definition
- Kind: ambiguity
- Magic behavior: none.
- Evidence: gf180mcu table 14.3 heads its rows "Unidirectional (CB)" and "Bi-directional (CB)". "CB" is not defined in the manual. §14.2.1 defines bidirectional current as "the value of the current when averaged over one direction with the higher peak current (Ip maximum). The lower peak in the opposite direction is neglected."
- Questions: is "averaged over one direction" a time average over the whole period (Hefesto's `peakdir`: ∫ max(0, s·i) dt / T) or over only the time spent in that direction? Does CB mean "copper/barrier", "current-based", or something else?
- Options: (a) average over the whole window T (specified). This is the usual EDA convention, in which the average of the positive part and the average of the negative part are each taken over the full period. (b) Average over only the time spent in the dominant direction. This gives larger values, so it is more conservative.
- Recommendation: (a), because it matches common EM-tool practice and the wording "the lower peak … is neglected". Record the interpretation in the bundled gf180 rule file's `note`, and confirm it with the gf180mcu-pdk maintainers. If they confirm (b), change the `peakdir` definition in `reliability-em-check`.

### Q-RELIABILITY-6: [platform] Using extresist for power grids
- Kind: platform
- Magic behavior: `extresist` builds networks one net at a time. It stores resistances as single-precision floats in mΩ, merges nodes, and was designed for signal nets with a driver device as origin. Without the `all`/`force` options it applies cutoffs (capability `extresist-command`).
- Evidence: openspec/specs/extresist-network/spec.md (Purpose, units; Requirement "Start tile and origin"); openspec/specs/extresist-command/spec.md (Requirement "Command options")
- Questions: performance and memory on chip-level supply nets with 10⁶–10⁷ tiles. The precision of float mΩ for very low resistances, since a 1 mΩ wide strap has only about 7 significant digits. The origin for a net with many pads and no driver.
- Options: (a) reuse the extresist construction exactly, with reductions off (specified); (b) a separate grid extractor that uses the same tile-resistor formulas in double precision, with a regular mesh for wide straps; (c) (a) for cells and macros, (b) for full chips.
- Recommendation: (a) for the first release, because it keeps resistances identical to Magic-compatible output. Measure on a Caravel-sized user area, and move to (c) if construction exceeds the performance target. The origin choice does not affect the IR solve, because sources are set by supply definitions.

### Q-RELIABILITY-7: Hierarchy, black boxes and hardened macros
- Kind: ambiguity
- Magic behavior: `extresist` has a `blackbox` option that skips subcells with ports. Magic does not otherwise distribute currents through abstract views.
- Evidence: openspec/specs/extresist-command/spec.md (option `blackbox`); librelane `OpenROAD.IRDropReport` step (librelane/steps/openroad.py:2193-2260), which uses OpenROAD PSM for digital blocks.
- Options: (a) treat black-box subcells and LEF/GDS-only macros as current sinks at their power pins, with currents from a table or an estimate (specified); (b) import per-macro IR results from an OpenROAD PSM run (desktop only, digital flow P-17) and combine them; (c) require full flattening.
- Recommendation: (a) now, and (b) as a desktop feature of the digital-flow integration. Record macro currents in the block import metadata.

### Q-RELIABILITY-8: Load currents from simulation
- Kind: ambiguity
- Magic behavior: none.
- Evidence: openspec/specs/simulation-ui-and-crossprobe/spec.md (Requirement "Probing device currents": `@m.xdut.xm1[id]` exists only with `all+currents`, and PDK wrapper subcircuits change the path); openspec/specs/simulation-netlist-preparation/spec.md (Requirement "Parasitic modes")
- Questions: which ngspice vectors give the supply-terminal current of every device type (MOSFET `id`/`is`/`ib`, BJT `ic`/`ie`, resistors, capacitors, diodes)? Can source and bulk currents be saved for PDK devices wrapped in subcircuits? What is the accuracy of the decoupled `sim` method against `cosim` when drops reach 10 % of VDD? `sim` ignores the feedback of the drop on device current.
- Options: (a) the decoupled `sim` method by default, with `cosim` for sign-off (specified); (b) `cosim` by default.
- Recommendation: (a). Capture golden cases REL-IR-07 and REL-IR-08 to measure the gap, and revisit the 10 % target of Requirement "Accuracy targets".

### Q-RELIABILITY-9: Sparse solver implementation and license
- Kind: platform
- Magic behavior: none.
- Evidence: openspec/specs/reliability-ir-drop/spec.md (Requirement "Grid solver"); openspec/specs/hefesto-licensing/spec.md
- Options: (a) Eigen (MPL-2.0) SimplicialLLT and ConjugateGradient with IncompleteCholesky; (b) SuiteSparse CHOLMOD, which is LGPL/GPL depending on the module and not suitable for iPad static linking; (c) KLU, which is already present through ngspice but is LGPLv2 (Q-SIM-1); (d) Hefesto's own supernodal Cholesky.
- Recommendation: (a). MPL-2.0 is file-level copyleft and compatible with Apache-2.0 distribution and the App Store, provided Eigen's source stays unmodified or changes are published. Check the determinism requirement, bit-identical results across thread counts, with Eigen's threading disabled or fixed.

### Q-RELIABILITY-10: Redistributing signal-net currents
- Kind: ambiguity
- Magic behavior: none.
- Evidence: openspec/specs/reliability-em-check/spec.md (Requirement "Branch current sources")
- Questions: the redistribution uses one net voltage waveform for every node, the node capacitances from `extresist`'s area-proportional distribution, and ignores coupling capacitance to neighbours. How large is the error on clock and long signal nets compared with `cosim`?
- Options: (a) keep `redistribute` as the default for signal nets, marked `approximate` (specified); (b) require `cosim` for signal-net EM; (c) use the node voltages from a `cosim` run when one exists, and otherwise `redistribute`.
- Recommendation: (c). Capture golden case REL-EM-09 to quantify the error.

### Q-RELIABILITY-11: Effects not modelled
- Kind: ambiguity
- Magic behavior: none.
- Evidence: openspec/specs/reliability-em-check/spec.md (Requirement "Modelling limitations"); gf180mcu drm_14_6_1.rst (slots for metal wider than 30 µm)
- Questions: should Hefesto give short-length (Blech) immunity credit when a PDK publishes a jL product? Should it model current crowding at via junctions, or self-heating with an RMS-to-ΔT model?
- Options: (a) none in the first release (specified, and stated in every report); (b) add optional `blech` fields (a jL threshold per layer) to the rule file schema in schema 2.
- Recommendation: (a) now. Revisit (b) when a supported PDK publishes jL data.

### Q-RELIABILITY-12: gf180mcu 6 kÅ top metal row
- Kind: ambiguity
- Magic behavior: none.
- Evidence: gf180mcu table 14.3 lists a 6 kÅ top metal. open_pdks builds no variant with it (gf180mcu/Makefile.in:40-63, where "Otherwise, the top metal is 0.6um thick" is unused by A–D).
- Options: bundle the row only as documentation | omit it.
- Recommendation: bundle the row in a commented `note`, and do not apply it to any shipped variant.
