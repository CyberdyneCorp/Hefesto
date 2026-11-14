# MASK-HG-020: A style with the net operator: two cells written in one session, the first write after startup.
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
probe {gds write a}
probe {load b}
probe {gds write b}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
