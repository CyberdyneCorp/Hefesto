# MASK-OP-009: pdiff with mixed right neighbours: bloat-max and bloat-min with negative values.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/masktest -noprompt}
probe {snap internal}
probe {gds datestamp 0}
probe {load top}
probe {box 0 0 10 20}
probe {paint pdiff}
probe {box 10 0 14 10}
probe {paint poly}
probe {box 10 10 14 20}
probe {paint metal1}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
