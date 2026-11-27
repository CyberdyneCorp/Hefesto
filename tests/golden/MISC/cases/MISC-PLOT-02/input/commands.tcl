# MISC-PLOT-02: pattern style `poly,pc 12 5`, X style for contacts; an L-shaped poly of two tiles, a pc and an m2c contact; plot postscript b.ps
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plottest -noprompt
load top
box 0 0 60 30
probe {plot postscript b.ps}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
