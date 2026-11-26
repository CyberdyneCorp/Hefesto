# MISC-PLOT-01: one m1 box (0,0)-(10,10), style postscript with `m1 10 S` and colour `10 0 0 0 255`; plot postscript a.ps m1 (the prologue copied from magicps.pro)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plottest -noprompt
load top
box 0 0 10 10
probe {plot postscript a.ps m1}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
