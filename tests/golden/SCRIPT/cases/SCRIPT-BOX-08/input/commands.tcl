# SCRIPT-BOX-08: GUI: run label A12, then press + twice
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
box 0 0 10 10
paint m1
box 5 5 5 5
probe {label A12 c m1}
event generate .layout1.pane.top.magic <Motion> -x 100 -y 100
event generate .layout1.pane.top.magic <KeyPress> -keysym plus
event generate .layout1.pane.top.magic <KeyPress> -keysym plus
update
select area label
probe {what -list}
probe {macro list .}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
