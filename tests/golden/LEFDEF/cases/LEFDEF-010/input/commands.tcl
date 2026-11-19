# LEFDEF-010: technology LEF with VIA (met1/via/met2 RECTs in that order), VIARULE of the same name, VIA redefined, VIA via rule (CUTSIZE...), then a DEF that places that via. Captures: `def read` result .mag (via paint) and warnings.
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
probe {lef read input/vias.lef}
probe {def read input/vias}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
