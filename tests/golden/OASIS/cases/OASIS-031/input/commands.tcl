# OASIS-031: CIRCLEs of radius 0, 10 and 1000 read with circlepoints 8, 64 and 4096: the polygon of oasis-read's vertex formula (Q-OASIS-31), not KLayout's or gdstk's.
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
probe {gds datestamp 1}
# (no Magic counterpart)
probe {gds read circ8}
# (no Magic counterpart)
probe {gds read circ64}
# (no Magic counterpart)
probe {gds read circ4096}
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
