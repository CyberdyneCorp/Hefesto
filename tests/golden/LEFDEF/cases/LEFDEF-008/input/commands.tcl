# LEFDEF-008: LEF with VERSION, UNITS 2000, PROPERTYDEFINITIONS, SPACING, BEGINEXT/ENDEXT sections and an unknown top-level statement, then a MACRO, END LIBRARY and a trailing MACRO. Captures: list of cells created; messages.
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
probe {lef read input/structure.lef}
probe {cellname list allcells}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
