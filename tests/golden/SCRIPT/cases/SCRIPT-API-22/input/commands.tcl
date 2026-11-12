# SCRIPT-API-22: tech version with a version containing spaces; tech lambda; drc list count; drc list count total; drc listall why
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/ver -noprompt
probe {tech version}
probe {tech lambda}
probe {drc list count}
probe {drc list count total}
probe {drc listall why}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
