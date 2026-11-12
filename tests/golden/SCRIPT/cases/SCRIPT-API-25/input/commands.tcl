# SCRIPT-API-25: fresh session: info commands magic::spice2float; then source the sky130A device script and run magic::3digitpastdecimal 0.12345, catch {3digitpastdecimal 1}
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
probe {info commands magic::spice2float}
set PDKPATH input/pdk/sky130A
source input/pdk/sky130A/libs.tech/magic/sky130A.tcl
probe {magic::3digitpastdecimal 0.12345}
probe {3digitpastdecimal 1}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
