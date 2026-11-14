# MASK-OP-015: A ring with a hole of area 100: close 100, close 101 and close alone.
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
probe {box 0 0 3 3}
probe {paint metal1}
probe {box 1 1 2 2}
probe {erase metal1}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
