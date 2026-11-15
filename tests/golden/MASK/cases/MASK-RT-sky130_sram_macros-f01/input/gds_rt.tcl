# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# The library GDS round trip of openspec/changes/m5-mask-gds-cif/design.md D19 (task 13.7).
# Copied into the input/ of every MASK-RT-* case (tools/golden/authoring/mask_rt_cases.py) and
# sourced by its commands.tcl, which Magic and hefesto-cli both run in a fresh session with the
# PDK's rc and `random seed 1`. tools/golden/gds_rt_report.py sources it again for Hefesto's
# second read and write (the non-gating equivalence report).
#
# Steps (D19): `gds read` of the file with the PDK's default read options; a wrapper cell that
# places every top structure the read created (sorted by name, each at the origin), its
# timestamp pinned to 0 (BGNLIB's creation date); `gds library yes`; `gds write`. Progress is
# printed as `mask-rt ...` lines; a failing command is reported, never raised, so the run
# always reaches `quit`.

namespace eval rt {
    variable wrapper mask_rt_wrapper
}

proc rt::say {args} {
    puts [join [concat mask-rt $args] " "]
    flush stdout
}

# Reads one GDS file; returns 1 on success.
proc rt::read_file {path} {
    if {[catch {gds read $path} err]} {
        rt::say read-error $path $err
        return 0
    }
    return 1
}

# The top cells that are not in the list `before` (the cells that existed before the read).
proc rt::new_tops {before} {
    foreach cell $before {
        set old($cell) 1
    }
    set tops {}
    foreach cell [cellname list topcells] {
        if {![info exists old($cell)]} {
            lappend tops $cell
        }
    }
    return [lsort $tops]
}

# The round trip of `path` into `out` (.gds). `defs` are files read first in the same session:
# the libraries whose cells `path` places without defining them (the ef_io wrappers, D19).
proc rt::roundtrip {path out {defs {}}} {
    variable wrapper
    foreach def $defs {
        rt::read_file $def
    }
    set before [cellname list allcells]
    if {![rt::read_file $path]} {
        return
    }
    set tops [rt::new_tops $before]
    rt::say tops [llength $tops] cells [expr {[llength [cellname list allcells]] - [llength $before]}]
    load $wrapper
    box 0 0 0 0
    foreach cell $tops {
        if {[catch {getcell $cell} err]} {
            rt::say getcell-error $cell $err
        }
    }
    cellname timestamp $wrapper 0
    gds library yes
    if {[catch {gds write $out} err]} {
        rt::say write-error $err
    }
}

# File `index` (0-based) of the `count` GDS files of directory `dir`, in sorted order.
proc rt::library_file {dir count index} {
    set files [lsort [glob -nocomplain -directory $dir -tails *.gds *.gds.gz *.gds2]]
    if {[llength $files] != $count} {
        rt::say file-count [llength $files] expected $count
    }
    return [lindex $files $index]
}

# The round trip of file `index` of the library staged in `dir` (a MASK-RT case's input).
proc rt::run_library {dir count index out} {
    set file [rt::library_file $dir $count $index]
    if {$file eq ""} {
        rt::say no-file $index
        return
    }
    rt::say file $file
    set defs {}
    if {[string match *_ef_io* $file]} {
        foreach def [lsort [glob -nocomplain -directory $dir -tails *_fd_io.gds *_fd_io.gds.gz]] {
            lappend defs [file join $dir $def]
        }
    }
    rt::roundtrip [file join $dir $file] $out $defs
}
