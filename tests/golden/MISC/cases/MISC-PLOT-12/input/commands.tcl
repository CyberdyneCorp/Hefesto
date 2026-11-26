# MISC-PLOT-12: plot svg v under the Cairo display (-d XR): v.svg written for the whole window; compared on the root element, the file name and the numeric width and height (svg-shape)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plottest -noprompt
load top
catch {set w [lindex [windownames layout] 0]; if {![winfo viewable $w]} {tkwait visibility $w}; update}
box 0 0 100 100
probe {plot svg v}
probe {file exists v.svg}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
