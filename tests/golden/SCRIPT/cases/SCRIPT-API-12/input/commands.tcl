# SCRIPT-API-12: GUI: .layout1.pane.top.magic box values; .layout1.pane.top.magic bo values
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {.layout1.pane.top.magic box values}
probe {.layout1.pane.top.magic bo values}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
