# SCRIPT-PDK-16: sky130A: create sky130_fd_pr__npn_05v5_W1p00L1p00 with nx 2 ny 3 deltax 1; then open the edit dialog (Xvfb) and dump magic::gencell_getparams plus the parameters passed to _dialog; Apply without ch...
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
load top
probe {magic::gencell sky130::sky130_fd_pr__npn_05v5_W1p00L1p00 Q1 nx 2 ny 3 deltax 1}
select cell Q1
probe {array -list count}
probe {array -list pitch}
magic::gencell {}
probe {magic::gencell_getparams}
catch {.params.buttons.apply invoke}
select cell Q1
probe {array -list count}
probe {array -list pitch}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
