# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Open Question 3 probe (openspec/changes/m7-extraction tasks.md 6.4), run by
# Magic's Tcl on the capture image: for each argument x of args.txt it
# prints x, atan(x) (glibc's, through Tcl's expr) with 17 significant digits,
# and 0.6366 * atan(x) with C's %lg, the format of every .ext value. The
# output is compared with compare_atan_probe.py.
#   magic -dnull -noconsole -norc atan_probe.tcl   (args.txt in the cwd)
set in [open args.txt r]
set out [open atan_probe.out w]
while {[gets $in line] >= 0} {
    set x [string trim $line]
    if {$x eq ""} { continue }
    set a [expr {atan($x)}]
    puts $out [format "%s %.17g %lg" $x $a [expr {0.6366 * $a}]]
}
close $in
close $out
quit -noprompt
