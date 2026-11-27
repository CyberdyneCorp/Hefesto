# MISC-PLOT-19: an empty area (Empty region, no plot); pnmbackground 0; pnmdownsample 1
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plottest -noprompt
load top
box 200 200 300 300
probe {plot pnm empty 50}
probe {file exists empty.pnm}
box 0 0 100 100
probe {plot parameters pnmbackground 0}
probe {plot pnm bg 50}
probe {plot parameters pnmdownsample 1}
probe {plot pnm ds 50}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
