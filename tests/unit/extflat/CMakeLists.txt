# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the extflat area (the `.ext` read-and-flatten subset; Open
# Question 1, decided (a)). Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md
# D1/D2, D10). Every *_test.cpp here is compiled into hefesto_extflat_tests
# (data: tests/data/extflat, which also holds GOLDEN's `.ext` probes).

hefesto_add_area_tests(extflat)
