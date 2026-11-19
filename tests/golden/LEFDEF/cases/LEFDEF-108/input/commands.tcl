# LEFDEF-108: Cell with LEFclass "CORE TIEHIGH", LEFsymmetry "X Y R90", LEFsite unit; no site cell. Captures: CLASS/SYMMETRY/SITE lines.
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
probe {load inv}
probe {lef write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
