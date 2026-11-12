# SCRIPT-PCELL-15: sg13g2: magic::gencell sg13g2::rm6 R1
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
load top
probe {magic::gencell sg13g2::rm6 R1}
probe {cellname list children top}
foreach c [lsort [cellname list children top]] { probe [list cellname list property $c] }
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
