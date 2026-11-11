# SCRIPT-GUI-04: GUI: box 0 0 1.5um 2um; box remove; pointer motion events
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {box 0 0 1.5um 2um}
update
probe {.layout1.titlebar.pos cget -text}
probe {.layout1.titlebar.caption cget -text}
probe {box remove}
update
probe {.layout1.titlebar.pos cget -text}
event generate .layout1.pane.top.magic <Motion> -x 50 -y 60
update
probe {.layout1.titlebar.pos cget -text}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
