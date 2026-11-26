# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Unit tests of the branch selection of run_refruns.py (task 3.3), on hand-written results.

    python3 -m unittest discover -s tools/golden/refruns -v
"""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import run_refruns as rr  # noqa: E402


def res(name: str, status: str = "ok", **values: float) -> rr.Result:
    return rr.Result(name, status, dict(values))


def precedence_runs(last_wins: bool) -> dict[str, rr.Result]:
    """The D-22 runs of all three PDKs, with the later definition winning or not."""
    runs = [res("sky130-d22-override", switch=1.0 if last_wins else 0.0, spread=0.02 if last_wins else 0.0, i1=4e-4),
            res("sky130-d22-baseline", switch=0.0, spread=0.0, i1=4e-4),
            res("gf180mcu-d22-override", **{"global": 1.0 if last_wins else 0.0},
                switch=1.0 if last_wins else 0.0, spread=0.002 if last_wins else 0.0, i1=5e-4),
            res("gf180mcu-d22-baseline", **{"global": 0.0}, switch=0.0, spread=0.0, i1=5e-4),
            res("ihp-d22-override", switch=0.5 if last_wins else 0.85, i1=2e-4 if last_wins else 3e-4),
            res("ihp-d22-baseline", switch=0.85, i1=3e-4),
            res("gf180mcu-d27-hs", **{"in": 4e-4}, ip=1.7e-4),
            res("gf180mcu-d27-hsa", **{"in": 4e-4}, ip=1.7e-4),
            res("sky130-section-tt", switch=0.0, i1=4e-4, spread=0.0),
            res("sky130-d26-without", switch=0.0, i1=4e-4, spread=0.0)]
    return {r.name: r for r in runs}


class DecisionTest(unittest.TestCase):
    def test_later_definition_wins_selects_the_override_branches(self):
        """cap:simulation-monte-carlo req:Statistical functions in ngspice (informative): D-22 (a)/(b)."""
        d = rr.decide(precedence_runs(last_wins=True))
        self.assertEqual(d["Q-SIMADV-3"]["branch"], "a")
        self.assertEqual(d["Q-SIMADV-4"]["d22"], "a")
        self.assertEqual(d["Q-SIMADV-4"]["d27"], "a")
        self.assertEqual(d["Q-SIMADV-9"]["branch"], "b")

    def test_first_definition_kept_selects_the_other_branches(self):
        d = rr.decide(precedence_runs(last_wins=False))
        self.assertEqual(d["Q-SIMADV-3"]["branch"], "b")
        self.assertEqual(d["Q-SIMADV-4"]["d22"], "b")
        self.assertEqual(d["Q-SIMADV-9"]["branch"], "a")

    def test_spread_without_the_override_is_outside_the_rule(self):
        """A baseline spread (statistical .param drawn per reference, no patch 0005) fits no branch."""
        runs = precedence_runs(last_wins=True)
        runs["sky130-d22-baseline"] = res("sky130-d22-baseline", switch=0.0, spread=0.01, i1=4e-4)
        d = rr.decide(runs)
        self.assertIsNone(d["Q-SIMADV-3"]["branch"])
        self.assertIsNone(d["Q-SIMADV-9"]["branch"])

    def test_mixed_precedence_between_pdks_is_outside_the_rule(self):
        runs = precedence_runs(last_wins=True)
        runs["ihp-d22-override"] = res("ihp-d22-override", switch=0.85, i1=3e-4)
        self.assertIsNone(rr.decide(runs)["Q-SIMADV-9"]["branch"])

    def test_ngbehavior_difference_without_failure_is_outside_the_rule(self):
        """D-27: hs and hsa both clean but different: no branch says which one is right."""
        runs = precedence_runs(last_wins=True)
        runs["gf180mcu-d27-hs"] = res("gf180mcu-d27-hs", **{"in": 3e-4}, ip=1.7e-4)
        self.assertIsNone(rr.decide(runs)["Q-SIMADV-4"]["branch"])
        runs["gf180mcu-d27-hs"] = res("gf180mcu-d27-hs", status="failed")
        self.assertEqual(rr.decide(runs)["Q-SIMADV-4"]["d27"], "b")

    def test_nomodcheck_branch_follows_the_run_without_it(self):
        """Q-SIM-8: (a) if sky130 combined fails without ng_nomodcheck, (b) otherwise."""
        runs = precedence_runs(last_wins=True)
        self.assertEqual(rr.decide(runs)["Q-SIM-8"]["ng_nomodcheck"], "b")
        runs["sky130-d26-without"] = res("sky130-d26-without", status="failed")
        self.assertEqual(rr.decide(runs)["Q-SIM-8"]["ng_nomodcheck"], "a")

    def test_skipped_loads_select_no_descriptor_branch(self):
        runs = precedence_runs(last_wins=True)
        runs["ihp-lib-cornerRES-res_typ"] = res("ihp-lib-cornerRES-res_typ", status="skipped")
        self.assertIsNone(rr.decide(runs)["Q-SIM-8"]["branch"])
        runs["ihp-lib-cornerRES-res_typ"] = res("ihp-lib-cornerRES-res_typ", loaded=1.0)
        self.assertEqual(rr.decide(runs)["Q-SIM-8"]["descriptors"], "a")


class DeckTest(unittest.TestCase):
    def test_every_planned_deck_renders(self):
        """Every run's deck resolves all its placeholders; decks are Hefesto-authored (SPDX header)."""
        roots = {"SKY130": "/r/s", "GF180": "/r/g", "IHP": "/r/i"}
        for run in rr.plan():
            text = rr.render((rr.DECKS / run.deck).read_text(encoding="utf-8"), dict(run.subs, **roots))
            self.assertTrue(text.startswith("* SPDX-License-Identifier: Apache-2.0"), run.deck)
            self.assertIn("HREF", text)

    def test_parse_log_reads_values_errors_and_warnings(self):
        values, errors, warnings = rr.parse_log("HREF i1=0.0004\nWarning: x\nError: no such variable\nHREF bad=nan?\n")
        self.assertEqual(values, {"i1": 0.0004})
        self.assertEqual(errors, ["Error: no such variable"])
        self.assertEqual(warnings, 1)


if __name__ == "__main__":
    unittest.main()
