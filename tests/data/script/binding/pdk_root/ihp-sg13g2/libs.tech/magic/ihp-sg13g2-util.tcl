# SPDX-License-Identifier: Apache-2.0
# Excerpt of IHP-Open-PDK ihp-sg13g2/libs.tech/magic/ihp-sg13g2-util.tcl:691-694
# (IHP-GmbH/IHP-Open-PDK @ 5e6d592e), the helper the scripting-api
# "Cross-PDK helper redefinition" test redefines.
# Copyright 2024 IHP PDK Authors; Apache License 2.0.

proc magic::i2u { value } {
    set value [expr {((round([magic::cif scale output] * 10000)) / 10000.0) * $value}]
    return [format "%.3f" $value]
}
