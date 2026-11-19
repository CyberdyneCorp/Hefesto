# OASIS-090: oasis nodup t, oasis flatten 5 and their queries (as gds), oasis compress 12 and circlepoints 4 (refused, values kept), oasis write a b (nothing written) and a bare oasis (top.oas).
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
probe {load top}
probe {box 0 0 4 2}
probe {paint metal1}
probe {gds nodup t}
rep {gds noduplicates}
probe {gds flatten 5}
rep {gds flatten}
# (no Magic counterpart)
specrep {6}
# (no Magic counterpart)
specrep {64}
# (no Magic counterpart)
specrep {0}
# (no Magic counterpart)
specrep {1}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
