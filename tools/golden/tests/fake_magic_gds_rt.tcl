# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# A stand-in for the Magic commands tools/golden/harness/gds_rt/gds_rt.tcl uses (test_gds_rt.py).
# `fake::library <file> <cells> <tops>` declares what `gds read <file>` creates: every cell of
# `cells` exists afterwards and the ones in `tops` are top cells (a later read can make an earlier
# top cell a child by listing it in `fake::child`). Every command is logged with `fake::log`.

namespace eval fake {
    variable libraries
    variable cells {(UNNAMED)}
    variable children {}
    variable log {}
}

proc fake::log {args} {
    variable log
    lappend log [join $args " "]
}

proc fake::library {file cells tops {children {}}} {
    variable libraries
    set libraries($file) [list $cells $tops $children]
}

proc gds {option args} {
    fake::log gds $option {*}$args
    if {$option ne "read"} {
        return
    }
    set file [file tail [lindex $args 0]]
    if {![info exists fake::libraries($file)]} {
        error "Cannot open $file"
    }
    lassign $fake::libraries($file) cells tops children
    foreach cell $cells {
        if {[lsearch -exact $fake::cells $cell] < 0} {
            lappend fake::cells $cell
        }
    }
    lappend fake::children {*}$children
}

proc cellname {option args} {
    fake::log cellname $option {*}$args
    if {$option eq "timestamp"} {
        return
    }
    if {[lindex $args 0] eq "allcells"} {
        return $fake::cells
    }
    # topcells: every cell that is not a child, in reverse creation order (hash order stand-in)
    set tops {}
    foreach cell [lreverse $fake::cells] {
        if {[lsearch -exact $fake::children $cell] < 0 && $cell ne "(UNNAMED)"} {
            lappend tops $cell
        }
    }
    return $tops
}

proc load {name} {
    fake::log load $name
    lappend fake::cells $name
}

proc box {args} {
    fake::log box {*}$args
}

proc getcell {name} {
    fake::log getcell $name
    lappend fake::children $name
}

proc drc {args} {
    fake::log drc {*}$args
}
