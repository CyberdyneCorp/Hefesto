# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Security review of the schematic script paths (task 12.1 of
# openspec/changes/m8-schematic; design D14): hostile `tcleval` values,
# launchers, Tcl symbol generators and `xschemrc` files against
# hefesto-security "Script sandbox enforcement" and "Schematic script
# permission model". Owner: SECURITY. Every *_test.cpp here is compiled into
# hefesto_schem_security_tests.

hefesto_add_area_tests(schem_security DATA_DIR schem/security)
