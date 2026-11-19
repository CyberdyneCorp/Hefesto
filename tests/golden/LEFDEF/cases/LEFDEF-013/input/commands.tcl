# LEFDEF-013: MACRO with CLASS CORE WELLTAP, SYMMETRY X Y R90, SITE, two PROPERTY statements, SOURCE, TIMING section. Captures: .mag `string`/property lines.
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
probe {lef datestamp 100}
probe {lef read input/statements.lef}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
