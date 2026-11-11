# SCRIPT-API-05: cellname list top; units MICRONS; tech layer *; tech lock poly; tech locked; cif scale out
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {cellname list top}
probe {units MICRONS}
probe {units}
probe {tech layer *}
probe {tech lock poly}
probe {tech locked}
probe {cif scale out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
