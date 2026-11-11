# SCRIPT-PDK-13: sky130A: snap lambda; run sky130::sky130_fd_pr__cap_mim_m3_1_draw via magic::gencell; record snap, tech unlocked, llength $boxstack before and after; also run label B c li; select area label; port ...
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
load top
snap lambda
set boxstack {}
probe {snap}
probe {tech unlocked}
probe {llength $boxstack}
probe {magic::gencell sky130::sky130_fd_pr__cap_mim_m3_1 C1}
probe {snap}
probe {tech unlocked}
probe {llength $boxstack}
load empty
box 0 0 0 0
probe {label B c li}
probe {select area label}
probe {port make}
probe {port first}
probe {setlabel text}
probe {setlabel layer}
probe {port B index}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
