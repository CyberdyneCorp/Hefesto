# SCRIPT-PCELL-09: magic::gencell toy::nosuch x; magic::gencell {} with nothing selected; odd argument count
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
source input/toy.tcl
probe {magic::gencell toy::nosuch x}
probe {select clear}
probe {magic::gencell {}}
probe {magic::gencell toy::nfet M1 w}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
