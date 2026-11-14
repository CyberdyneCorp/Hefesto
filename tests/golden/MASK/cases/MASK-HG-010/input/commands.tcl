# MASK-HG-010: Parent 300x100 with interacting children at x 139..141, stepsize unset and stepsize 50.
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
probe {cif ostyle nostep}
probe {gds write nostep}
probe {cif ostyle step50}
probe {gds write step50}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
