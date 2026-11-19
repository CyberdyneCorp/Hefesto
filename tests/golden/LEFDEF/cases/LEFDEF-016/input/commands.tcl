# LEFDEF-016: tech `contact pc cont1` with a shrinking grid so the contact area is filled; macro PORT RECT on cont1. Captures: painted cut rectangle in .mag.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech lambda 1 2}
probe {tech load input/lefdef.tech -noprompt}
probe {snap internal}
probe {lef datestamp 100}
probe {lef read input/cport.lef}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
