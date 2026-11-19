# LEFDEF-112: writeall on a top with 3 children having different LEFproperties keys. Captures: PROPERTYDEFINITIONS key order.
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
probe {load top}
probe {lef writeall}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
