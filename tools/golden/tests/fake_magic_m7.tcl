# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# A stand-in for the Magic commands the M7 case scripts use (tools/golden/authoring/m7_cases.py),
# run under tclsh by tests/test_m7_cases.py. It models only where the commands write their files,
# so the test can check that every output a case.toml lists is produced by its script and that
# every rename finds its file:
#   - extract all writes input/<cell>.ext for the cell last loaded (ExtFileOpen: next to the .mag);
#   - save rewrites input/<cell>.mag;
#   - ext2spice writes <cell>.spice, <cell>.cdl in format cdl, or the -o file (ext2spice.c:875-885);
#   - ext2sim writes <cell>.sim, plus .al with alias on and .nodes with labels on, and
#     <cell>.ext.sim (.al, .nodes) with extresist on (ext2sim.c).
# Option words (the first word an option name) only change the modelled state.

# Tcl 8.5 (the macOS runners' /usr/bin/tclsh) has no lmap, which EX-064's script uses; Magic's
# and Hefesto's Tcl (8.6) do.
if {[info commands lmap] eq ""} {
    proc lmap {var list body} {
        upvar 1 $var item
        set result {}
        foreach item $list {
            lappend result [uplevel 1 $body]
        }
        return $result
    }
}

namespace eval fake {
    variable cell ""
    variable spice [dict create format spice3]
    variable sim [dict create alias off labels off extresist off]
}

proc addpath {args} {}
proc tech {args} {}
proc drc {args} {}
proc box {args} {}
proc scalegrid {args} {}
proc feedback {args} { return 0 }
proc quit {args} { exit 0 }
proc load {name} { set fake::cell $name }
proc save {args} { fake::touch input/$fake::cell.mag }

proc fake::touch {path} {
    set f [open $path a]
    close $f
}

proc extract {args} {
    if {[lindex $args 0] eq "all"} { fake::touch input/$fake::cell.ext }
}

# The run's root cell and -o file: the last word that is no flag and no flag value.
proc fake::run_words {words valued} {
    set cell ""
    set out ""
    for {set i 0} {$i < [llength $words]} {incr i} {
        set w [lindex $words $i]
        if {$w eq "-o"} {
            incr i
            set out [lindex $words $i]
        } elseif {[lsearch -exact $valued $w] >= 0} {
            incr i
        } elseif {[string index $w 0] ne "-"} {
            set cell $w
        }
    }
    return [list $cell $out]
}

proc ext2spice {args} {
    set opts {default format rthresh cthresh merge extresist resistor scale short subcircuits hierarchy
              blackbox renumber global lvs help}
    set first [lindex $args 0]
    if {[lsearch -exact $opts $first] >= 0} {
        if {$first eq "format" && [llength $args] > 1} { dict set fake::spice format [lindex $args 1] }
        if {$first eq "lvs"} { dict set fake::spice format ngspice }
        return
    }
    lassign [fake::run_words $args {-f -J -y -c -r -p -s -S -t -T -n -N}] cell out
    if {$out eq ""} {
        set out $cell.[expr {[dict get $fake::spice format] eq "cdl" ? "cdl" : "spice"}]
    }
    fake::touch $out
}

proc ext2sim {args} {
    set opts {alias labels default format rthresh cthresh merge extresist help}
    set first [lindex $args 0]
    if {[lsearch -exact $opts $first] >= 0} {
        if {$first in {alias labels extresist} && [llength $args] > 1} {
            dict set fake::sim $first [lindex $args 1]
        }
        return
    }
    lassign [fake::run_words $args {-a -l -f -J -y -c -r -p -s -S -t -T -n -N}] cell out
    set stem [expr {[dict get $fake::sim extresist] eq "on" ? "$cell.ext" : $cell}]
    fake::touch [expr {$out eq "" ? "$stem.sim" : $out}]
    if {[dict get $fake::sim alias] eq "on"} { fake::touch $stem.al }
    if {[dict get $fake::sim labels] eq "on"} { fake::touch $stem.nodes }
}
