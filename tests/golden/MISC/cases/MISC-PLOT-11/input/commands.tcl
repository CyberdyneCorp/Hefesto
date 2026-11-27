# MISC-PLOT-11: a technology with no plot section; plot postscript x.ps, then plot pnm x (the warning, then the default PNM styles); Magic's exit status is recorded because its first capture ended in SIGSEGV (exit 139) right after the PNM warning
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
probe {plot postscript x.ps}
probe {plot pnm x}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
