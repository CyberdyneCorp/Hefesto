# SPDX-License-Identifier: Apache-2.0
# Excerpt of open_pdks sky130/magic/sky130.tcl with common/pdk.tcl appended
# (RTimothyEdwards/open_pdks @ c6df28c1, as installed by ciel 5740c4a8:
# sky130A/libs.tech/magic/sky130A.tcl:282-284,350-353), the helpers the
# scripting-api "Script-defined procedures in the engine namespace" tests use.
# Copyright 2019-2024 Tim Edwards / efabless; Apache License 2.0.

proc magic::i2u { value } {
    return [expr {((round([magic::cif scale output] * 10000)) / 10000.0) * $value}]
}

proc magic::3digitpastdecimal {value} {
    set new [expr int([expr $value * 1000 + 0.5 ]) / 1000.0]
    return $new
}
