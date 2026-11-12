# SCRIPT-PCELL-10: magic::gencell_makecell toy::nfet w 1 l 0.15 nf 1
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
probe {magic::gencell_makecell toy::nfet w 1 l 0.15 nf 1}
probe {cellname list children top}
probe {cellname list allcells}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
