# SCRIPT-API-17: set PDKPATH /pdk/x; addpath $PDKPATH/lib; path search; then set PDKPATH /other; addpath $PDKPATH/lib2
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {set PDKPATH /pdk/x}
probe {addpath $PDKPATH/lib}
probe {path search}
probe {set PDKPATH /other}
probe {addpath $PDKPATH/lib2}
probe {path search}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
