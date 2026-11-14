# MASK-GW-039: No cifoutput style loaded: gds write x, the error and the empty file.
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
probe {box 0 0 4 2}
probe {paint metal1}
probe {gds write x}
probe {file exists x.gds}
probe {file size x.gds}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
