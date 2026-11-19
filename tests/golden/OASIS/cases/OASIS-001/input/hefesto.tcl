# OASIS-001: Integers 0, 127, 128, 300, 16383, 16384, a non-minimal 0, a negative 0, 2^63 (dropped) and a 65-bit overflow; reals of types 0-7 as magnifications and a zero divisor; strings with illegal bytes.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
proc rep {script} {
    set code [catch {uplevel #0 $script} result]
    set f [open report.txt a]
    puts $f [list $code $result]
    close $f
}
proc specrep {value} {
    set f [open report.txt a]
    puts $f [list 0 $value]
    close $f
}
probe {drc off}
probe {tech load input/oastest -noprompt}
probe {snap internal}
probe {oasis datestamp 1}
probe {oasis read ints}
probe {oasis read over}
probe {oasis read reals}
probe {oasis read rdiv}
probe {oasis read strs}
probe {load anchor}
probe {box 0 0 1 1}
probe {paint metal1}
rep {lsort [cellname list allcells]}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
