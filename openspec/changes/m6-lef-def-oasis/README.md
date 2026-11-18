# m6-lef-def-oasis

M6 LEF/DEF and OASIS (`lef-tech-section`, `lef-read`, `lef-write`, `def-read`, `def-write`, `oasis-format`, `oasis-read`, `oasis-write`, `oasis-command`) covers:
- the tech `lef` section and the LEF layer table;
- LEF read, with abstract views and annotation;
- LEF write, with `-hide`, `-pinonly`, `-toplayer` and antenna areas;
- DEF read and DEF write for place-and-route flows;
- the OASIS codec, with CBLOCK compression and validation;
- OASIS read and write equivalent to the GDS path, with read-only vendor views and transcoding;
- the `lef`, `def` and `oasis` commands;
- KLayout (`strm2oas`, `strmxor`) and gdstk validation in the capture image;
- the 419-byte OASIS reference file, confirmed by a third-party reader;
- the 143 LEFDEF and 51 OASIS golden cases;
- the M6 exit check.

The 99 open questions that blocked M6 were approved on 2026-11-13 (`docs/decisions/m6-decision-table.md`). The plan's Open Questions 1–6 and Q-LEFDEF-114 were decided by the user on 2026-11-18, each as recommended (Open Question 3 with its fallback (b)).
