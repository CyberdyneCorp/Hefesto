# LEFDEF-130: `lef write -hide` on cell with pins and internal metal1/metal2. Captures: full LEF.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/lefdef.tech -noprompt}
probe {snap internal}
probe {load hide}
probe {lef write -hide}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
