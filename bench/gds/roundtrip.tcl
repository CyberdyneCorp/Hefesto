# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# One pass of the library round trip of openspec/changes/m5-mask-gds-cif/design.md D19, timed, for the
# equivalence report of bench/gds/gds_equivalence.py (perf.yml gds-roundtrip). Run in a fresh session
# with the PDK's rc. The driver writes a script that sets the Tcl variables rt_in, rt_out and rt_first
# (hefesto-cli gives scripts a synthesized `env`, so they are not passed in the environment) followed by
# this file, and runs it with
#   PDK_ROOT=<version dir> PDK=<variant> \
#     hefesto-cli --no-log --trust-pdk <variant> --magic -dnull -noconsole -rcfile <variant>.magicrc pass.tcl
# rt_first (empty for none) is read first when rt_in places cells defined in another file of its library (the ef_io
# wrappers, design D19 "Task 1.1 facts"). The steps are D19's: gds read; a wrapper placing every top
# structure, its timestamp pinned to 0; gds library yes; gds write. It prints one line
# "RT read_ms=<n> write_ms=<n> tops=<n> status=<ok|...>" and quits.

proc rt_ms {script} {
    set us [lindex [uplevel 1 [list time $script 1]] 0]
    return [expr {$us / 1000}]
}

proc rt_report {status read_ms write_ms tops} {
    puts stdout "RT read_ms=$read_ms write_ms=$write_ms tops=$tops status=$status"
    flush stdout
}

proc rt_run {} {
    random seed 1
    drc off
    set read_ms [rt_ms {
        set failed 0
        if {$::rt_first ne ""} {
            set failed [catch {gds read $::rt_first} err]
        }
        if {!$failed} {
            set failed [catch {gds read $::rt_in} err]
        }
    }]
    if {$failed} {
        rt_report "read-error:[string map {" " _} $err]" -1 -1 0
        return
    }
    set tops {}
    foreach cell [cellname list topcells] {
        if {$cell ne "(UNNAMED)" && $cell ne "rt_wrapper"} {
            lappend tops $cell
        }
    }
    load rt_wrapper
    box 0 0 0 0
    foreach cell $tops {
        getcell $cell
    }
    cellname timestamp rt_wrapper 0
    gds library yes
    set write_ms [rt_ms {set failed [catch {gds write $::rt_out} err]}]
    if {$failed} {
        rt_report "write-error:[string map {" " _} $err]" $read_ms -1 [llength $tops]
        return
    }
    rt_report ok $read_ms $write_ms [llength $tops]
}

rt_run
quit -noprompt
