# LEFDEF-111: Cell with LEFproperties `CDL "inv x" MASK 1` and `A 1 B` variants. Captures: PROPERTYDEFINITIONS and PROPERTY lines.
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
probe {lef write inv}
probe {load inv2}
probe {lef write inv2}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
