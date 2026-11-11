# SCRIPT-PDK-15: ihp-sg13g2 bipolar generator: labels and ports present in the generated npn13G2 cell; stderr
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
foreach c [lsort [cellname list children top]] { load $c; select top cell; probe {port first}; probe {port last}; probe {what -list} }
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
