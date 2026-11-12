# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# A test device library for the gencell framework unit tests: the procedure
# contract of capability pcell-toolkit ("Library and device procedure
# contract") with a generated device (res), a type-switched pair
# (res/res2), a device whose _draw fails (bad), one without _check (nocheck)
# and a fixed (nocell) device placing the library cell `fixlib`.

namespace eval tpdk {
    variable converted {}
    variable calls {}
}

# ---- res: a generated device ----------------------------------------------
proc tpdk::res_defaults {} {
    return {w 1 l 2 nf 1 compatible {res res2}}
}

proc tpdk::res_convert {parameters} {
    set tpdk::converted $parameters
    set result {}
    foreach {key value} $parameters {
        if {$key == "mult"} {
            dict set result nf $value
        } else {
            dict set result $key $value
        }
    }
    return $result
}

proc tpdk::res_dialog {parameters} {
    magic::add_entry w "Width (um)" $parameters
    magic::add_entry l "Length (um)" $parameters
    magic::add_selectlist gencell "Device type" {res res2} $parameters res
    magic::add_checkbox guard "Guard ring" $parameters
    magic::add_message info "Info" $parameters
    magic::add_entry nf "Fingers" $parameters
    magic::add_dependency tpdk::res_dep res tpdk nf
}

proc tpdk::res_dep {pname parameters} {
    lappend tpdk::calls "dep $pname"
    dict set parameters l [expr {[dict get $parameters nf] * 2}]
    return $parameters
}

proc tpdk::res_check {parameters} {
    lappend tpdk::calls check
    if {[dict get $parameters w] < 1} {
        dict set parameters w 1
    }
    catch {set magic::info_val "checked"}
    return $parameters
}

proc tpdk::res_draw {parameters} {
    set w [expr {int([dict get $parameters w] * 10)}]
    box values 0 0 $w 10
    paint m1
    return
}

# ---- res2: the type switch target -----------------------------------------
proc tpdk::res2_defaults {} {
    return {w 3 l 3 nf 1}
}
proc tpdk::res2_convert {parameters} { return $parameters }
proc tpdk::res2_dialog {parameters} { magic::add_entry w "Width" $parameters }
proc tpdk::res2_check {parameters} { return $parameters }
proc tpdk::res2_draw {parameters} {
    box values 0 0 30 30
    paint m2
    return
}

# ---- bad: _draw raises an error -------------------------------------------
proc tpdk::bad_defaults {} { return {w 1} }
proc tpdk::bad_convert {parameters} { return $parameters }
proc tpdk::bad_dialog {parameters} { magic::add_entry w "Width" $parameters }
proc tpdk::bad_check {parameters} { return $parameters }
proc tpdk::bad_draw {parameters} { error "bad layer" }

# ---- nocheck: no _check procedure (sg13g2 rm6/rm7) ------------------------
proc tpdk::nocheck_defaults {} { return {w 2} }
proc tpdk::nocheck_convert {parameters} { return $parameters }
proc tpdk::nocheck_dialog {parameters} { magic::add_entry w "Width" $parameters }
proc tpdk::nocheck_draw {parameters} {
    box values 0 0 20 20
    paint m1
    return
}

# ---- fix: a fixed (nocell) device placing the library cell fixlib ----------
proc tpdk::fix_defaults {} {
    return {nocell 1 nx 1 ny 1 deltax 0 deltay 0}
}
proc tpdk::fix_convert {parameters} { return $parameters }
proc tpdk::fix_dialog {parameters} { magic::add_entry nx "X Repeat" $parameters }
proc tpdk::fix_check {parameters} { return $parameters }
proc tpdk::fix_draw {parameters} {
    set nx [dict get $parameters nx]
    set ny [dict get $parameters ny]
    set inst [getcell fixlib]
    if {$nx > 1 || $ny > 1} {
        array $nx $ny
    }
    return $inst
}
