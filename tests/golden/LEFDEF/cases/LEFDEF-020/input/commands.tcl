# LEFDEF-020: NONDEFAULTRULE with LAYER/WIDTH/SPACING/WIREEXTENSION, HARDSPACING, USEVIA, MINCUTS, PROPERTY, a VIA, then a duplicate rule name; DEF net using the rule. Captures: DEF read wire widths in .mag; warnings.
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
probe {lef read input/ndr.lef}
probe {def read input/ndr}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
