# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# The library round trip of openspec/changes/m5-mask-gds-cif/design.md D19, timed (task 1.1).
# Run in a fresh session with the PDK's rc, e.g.
#   PDK_ROOT=<version dir> D19_IN=<lib>.gds D19_OUT=out.gds \
#     magic -dnull -noconsole -rcfile <variant>.magicrc d19_roundtrip.tcl
# It prints one line "D19 <in> read_ms=<n> write_ms=<n> tops=<n> structures=<n> status=<ok|...>"
# and quits. The wrapper places every top structure at the origin; GOLDEN's MASK-RT cases fix the
# final placement. Steps (D19): gds read; wrapper with a pinned timestamp; gds library yes; gds write.

# Magic's interpreter in the capture image has no `clock` command, so the times come from Tcl's `time`
# (microseconds for one run of the script).
proc d19_ms {script} {
    set us [lindex [uplevel 1 [list time $script 1]] 0]
    return [expr {$us / 1000}]
}

proc d19_report {status read_ms write_ms tops structures} {
    puts stdout "D19 $::env(D19_IN) read_ms=$read_ms write_ms=$write_ms tops=$tops structures=$structures status=$status"
    flush stdout
}

proc d19_run {} {
    random seed 1
    drc off
    set read_ms [d19_ms {set failed [catch {gds read $::env(D19_IN)} err]}]
    if {$failed} {
        d19_report "read-error:[string map {" " _} $err]" -1 -1 0 0
        return
    }
    set tops {}
    foreach cell [cellname list topcells] {
        if {$cell ne "(UNNAMED)" && $cell ne "d19_wrapper"} {
            lappend tops $cell
        }
    }
    set structures [llength [cellname list allcells]]
    load d19_wrapper
    box 0 0 0 0
    foreach cell $tops {
        getcell $cell
    }
    cellname timestamp d19_wrapper 0
    gds library yes
    set write_ms [d19_ms {set failed [catch {gds write $::env(D19_OUT)} err]}]
    if {$failed} {
        d19_report "write-error:[string map {" " _} $err]" $read_ms -1 [llength $tops] $structures
        return
    }
    d19_report ok $read_ms $write_ms [llength $tops] $structures
}

d19_run
quit -noprompt
