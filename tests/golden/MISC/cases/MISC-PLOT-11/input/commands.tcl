# MISC-PLOT-11: a technology with no plot section; plot pnm x and plot postscript x.ps (the warning, then the default PNM styles and an empty PostScript drawing)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/noplot -noprompt
load top
box 0 0 100 100
probe {plot pnm x}
probe {plot postscript x.ps}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
