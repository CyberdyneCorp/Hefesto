# MASK-HG-025: A hierarchical write followed by cif statistics twice.
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
probe {gds write out}
probe {cif statistics}
probe {cif statistics}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
