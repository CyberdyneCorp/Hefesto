# SCRIPT-GUI-01: GUI startup with no args; winfo children .layout1.pane.top; windownames layout
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {winfo children .layout1.pane.top}
probe {windownames layout}
probe {winfo children .layout1}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
