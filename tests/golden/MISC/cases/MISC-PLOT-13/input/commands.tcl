# MISC-PLOT-13: plot, plot help and plot foo in Magic's Cairo Tcl build (the option list with `svg`); plot versatec is not run (it would spool through lp)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {plot}
probe {plot help}
probe {plot foo}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
