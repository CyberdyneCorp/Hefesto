# SCRIPT-PCELL-08: toy _draw raising error bad layer
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
probe {magic::gencell toy::bad B1}
probe {cellname list allcells}
probe {instance list exists B1}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
