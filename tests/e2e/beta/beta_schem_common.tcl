# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Helpers of the Desktop Beta schematic scripts (beta_inv_sch.tcl, beta_tb_sch.tcl; m9-lvs-simulation
# task 20.2, design D19 "Beta flow inputs"). Only existing `schematic` commands are used: new, place,
# setprop, label, save, symbol make, netlist.
#
# run_beta.py prepends, before this file, the array PIN: PIN(<symbol>,<pin>) is the pin's position
# {x y} relative to the symbol origin, read from the .sym file of every symbol the scripts place
# (rotation 0, no flip). Connections are made by placing a net label on each pin, so no wire is drawn.

# Nets that are ports of the schematic, with their label kind; the first label of such a net is
# the port, later labels of the same net are plain net labels.
set ::PORTS {}
set ::ported {}

# The label kind for a label of `net`.
proc label_kind {net} {
    if {![dict exists $::PORTS $net] || $net in $::ported} {
        return pin
    }
    lappend ::ported $net
    return [dict get $::PORTS $net]
}

# Labels every pin of `pinmap` (pin net ...) of an instance of `sym` placed at (x, y).
proc connect {sym x y pinmap} {
    foreach {pin net} $pinmap {
        lassign $::PIN($sym,$pin) px py
        schematic label [expr {$x + $px}] [expr {$y + $py}] $net -kind [label_kind $net]
    }
}

# Places `sym` at (x, y), sets the properties `props` (key value ...), labels its pins with
# `pinmap` and returns the instance name.
proc device {sym x y props pinmap} {
    set name [schematic place $sym $x $y]
    foreach {key value} $props {
        schematic setprop instance $name $key $value
    }
    connect $sym $x $y $pinmap
    return $name
}

# The pin map of a transistor: `roles` maps d, g, s, b to nets; `names` (from the PDK file) maps
# d, g, s, b to the symbol's pin names.
proc fet_pins {names roles} {
    set out {}
    foreach {role net} $roles {
        lappend out [dict get $names $role] $net
    }
    return $out
}
