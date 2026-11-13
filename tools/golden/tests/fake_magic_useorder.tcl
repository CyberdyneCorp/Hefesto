# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# A stand-in for the Magic commands the use-order probe calls, for running the
# MASK-UO-* scripts under a plain tclsh (tools/golden/tests/test_use_order_probe.py).
# It models no bplane: every cell keeps its uses newest first. It writes real GDS
# and CIF files, so the probe's decoders and the scripts' flow are exercised without
# Magic. Never used for capture.

namespace eval ::fake {
    variable box {0 0 0 0}
    variable root ""
    variable selected ""
    variable sizes
    variable counts
    variable uses
    array set sizes {}
    array set counts {}
    array set uses {}
}

proc addpath {args} {}
proc tech {args} {}
proc snap {args} {}
proc drc {args} {}
proc expand {args} {}
proc quit {args} {exit 0}

proc box {args} {
    if {[llength $args] == 4} {
        set ::fake::box $args
    }
    return $::fake::box
}

proc ::fake::size {def} {
    variable sizes
    if {![info exists sizes($def)]} {
        set sizes($def) {0 0 0 0}
        set f [open input/$def.mag r]
        foreach line [split [read $f] "\n"] {
            if {[lindex $line 0] eq "rect"} {
                set sizes($def) [lrange $line 1 4]
            }
        }
        close $f
    }
    return $sizes($def)
}

proc getcell {def} {
    lassign $::fake::box x y
    lassign [::fake::size $def] - - w h
    foreach use $::fake::uses($::fake::root) {
        if {[lindex $use 1] eq $def && [lindex $use 2] == $x && [lindex $use 3] == $y} {
            puts stderr "Can't place a cell on an exact copy of itself."
            return ""
        }
    }
    set n [expr {[info exists ::fake::counts($def)] ? $::fake::counts($def) : 0}]
    set ::fake::counts($def) [expr {$n + 1}]
    set id ${def}_$n
    set ::fake::uses($::fake::root) [linsert $::fake::uses($::fake::root) 0 [list $id $def $x $y]]
    set ::fake::box [list $x $y [expr {$x + $w}] [expr {$y + $h}]]
    return $id
}

proc select {args} {
    if {[lindex $args 0] eq "cell"} {
        set ::fake::selected [lindex $args 1]
    }
}

proc delete {} {
    set kept {}
    foreach use $::fake::uses($::fake::root) {
        if {[lindex $use 0] ne $::fake::selected} {
            lappend kept $use
        }
    }
    set ::fake::uses($::fake::root) $kept
}

proc cellname {list what name} {
    if {$what eq "childinst"} {
        return [lsort [lmap use $::fake::uses($name) {lindex $use 0}]]
    }
    set ids {}
    foreach use $::fake::uses($::fake::root) {
        if {[lindex $use 1] eq $name} {
            lappend ids [lindex $use 0]
        }
    }
    return $ids
}

proc load {name} {
    set ::fake::root $name
    if {[info exists ::fake::uses($name)]} {
        return
    }
    set ::fake::uses($name) {}
    if {[file isfile input/$name.mag]} {
        set f [open input/$name.mag r]
        foreach line [split [read $f] "\n"] {
            switch -- [lindex $line 0] {
                use {set id [lindex $line end]; set def [lindex $line 1]}
                transform {set ::fake::uses($name) [linsert $::fake::uses($name) 0 \
                               [list $id $def [lindex $line 3] [lindex $line 6]]]}
            }
        }
        close $f
    }
}

proc paint {layer} {
    if {[info exists ::fake::sizes($::fake::root)]} {
        lassign $::fake::box x1 y1 x2 y2
        lassign $::fake::sizes($::fake::root) a b c d
        set ::fake::sizes($::fake::root) [list [expr {min($a, $x1)}] [expr {min($b, $y1)}] \
                                              [expr {max($c, $x2)}] [expr {max($d, $y2)}]]
    }
}

proc cif {args} {
    if {[lindex $args 0] eq "write"} {
        ::fake::write_cif [lindex $args 1].cif
    }
}

proc gds {op name} {
    ::fake::write_gds $name.gds
}

# --- writers -----------------------------------------------------------------

proc ::fake::rec {type datatype {data ""}} {
    return [binary format Scc [expr {4 + [string length $data]}] $type $datatype]$data
}

proc ::fake::str {type text} {
    if {[string length $text] % 2} {
        append text "\0"
    }
    return [rec $type 6 $text]
}

proc ::fake::children {} {
    return [lsort -unique [lmap use $::fake::uses($::fake::root) {lindex $use 1}]]
}

proc ::fake::write_gds {path} {
    set out [rec 0 2 [binary format S 600]][rec 1 2 [binary format S12 [lrepeat 12 0]]]
    append out [str 2 lib] [rec 3 5 [string repeat "\0" 16]]
    foreach def [children] {
        append out [rec 5 2 [binary format S12 [lrepeat 12 0]]] [str 6 $def]
        append out [rec 8 0] [rec 13 2 [binary format S 49]] [rec 16 3 [binary format I10 {0 0 0 4 4 4 4 0 0 0}]]
        append out [rec 43 2 [binary format S 61]] [str 44 not-a-use] [rec 17 0] [rec 7 0]
    }
    append out [rec 5 2 [binary format S12 [lrepeat 12 0]]] [str 6 $::fake::root]
    foreach use $::fake::uses($::fake::root) {
        lassign $use id def x y
        append out [rec 10 0] [str 18 $def] [rec 16 3 [binary format II $x $y]]
        append out [rec 43 2 [binary format S 61]] [str 44 $id] [rec 17 0]
    }
    append out [rec 7 0] [rec 4 0] [string repeat "\0" 6]
    set f [open $path wb]
    puts -nonewline $f $out
    close $f
}

proc ::fake::write_cif {path} {
    set f [open $path w]
    puts $f "( @@user : fake );\n( @@tool : fake Magic );"
    set number 0
    foreach def [children] {
        set numbers($def) [incr number]
        puts $f "DS $number 50 2;\n9 $def;\nL CMF;\n    B 8 8 4 4;\nDF;"
    }
    puts $f "DS [incr number] 50 2;\n9 $::fake::root;"
    foreach use $::fake::uses($::fake::root) {
        lassign $use id def x y
        puts $f "91 $id;\nC $numbers($def) R 1 0 T [expr {$x * 4}] [expr {$y * 4}];"
    }
    puts $f "DF;\nC $number;\nEnd"
    close $f
}
