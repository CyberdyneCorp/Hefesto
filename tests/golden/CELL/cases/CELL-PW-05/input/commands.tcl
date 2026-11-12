# CELL-PW-05: children under $PDK_ROOT (Tcl variable) and under pdk2, whose path has PDK_ROOT's value as a character prefix: the use paths written.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
catch {unset env(PDK_ROOT)}
set PDK_ROOT [pwd]/input/pdk
load top
box 0 0 0 0
probe {getcell [pwd]/input/pdk/lib/c1}
probe {getcell [pwd]/input/pdk2/lib/c2}
probe {save top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
