# SCRIPT-PDK-14: ihp-sg13g2 in an edit cell top: magic::gencell sg13g2::npn13g2 Q1 and magic::gencell sg13g2::diodevdd_2kv D1 with npn13g2_base/diodevdd_2kv not in memory
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
load top
probe {magic::gencell sg13g2::npn13g2 Q1}
probe {magic::gencell sg13g2::diodevdd_2kv D1}
probe {cellname list window}
probe {cellname list self}
probe {cellname list children top}
foreach c [cellname list allcells] { probe [list cellname list property $c] }
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
