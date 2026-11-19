# LEFDEF-011: tech whose cifoutput generates `via` with squares (size/border known); LEF VIA with a cut smaller than the magic cut; DEF placing it. Captures: warnings and painted contact size in .mag.
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
probe {lef read input/tiny.lef}
probe {def read input/smallcut}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
