# SCRIPT-PCELL-05: toy: create M1, M2 with the same parameters; change M1 to w 3; then change M2 to w 3
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
probe {magic::gencell toy::nfet M1 w 2}
probe {magic::gencell toy::nfet M2 w 2}
probe {allcells}
select cell M1
rotate 90
probe {cellname list allcells}
probe {magic::gencell toy::nfet M1 w 3}
probe {cellname list allcells}
probe {instance list orientation}
probe {magic::gencell toy::nfet M2 w 3}
probe {cellname list allcells}
foreach c [lsort [cellname list allcells]] { probe [list cellname list parents $c] }
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
