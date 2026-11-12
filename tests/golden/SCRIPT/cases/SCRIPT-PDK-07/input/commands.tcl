# SCRIPT-PDK-07: each PDK, NULL graphics: for every device in the PDK menus (36 sky130, 39 gf180mcu, 21 sg13g2), magic::gencell <ns>::<dev> X<i> with defaults; save each generated cell
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
set f [open input/pdk/sky130A/libs.tech/magic/sky130A.tcl]
set devices [regexp -all -inline {magic::gencell ([^\s"]+)} [read $f]]
close $f
set devices [dict values $devices]
set i 0
foreach dev $devices {
    load top_[incr i]
    probe [list magic::gencell $dev X$i]
    probe [list cellname list children top_$i]
    foreach c [cellname list children top_$i] { probe [list cellname list property $c] ; probe [list save $c] }
}
tech load input/pdk/gf180mcuD/libs.tech/magic/gf180mcuD.tech -noprompt
source input/pdk/gf180mcuD/libs.tech/magic/gf180mcuD.magicrc
random seed 1
set f [open input/pdk/gf180mcuD/libs.tech/magic/gf180mcuD.tcl]
set devices [regexp -all -inline {magic::gencell ([^\s"]+)} [read $f]]
close $f
set devices [dict values $devices]
set i 0
foreach dev $devices {
    load gtop_[incr i]
    probe [list magic::gencell $dev X$i]
    probe [list cellname list children gtop_$i]
    foreach c [cellname list children gtop_$i] { probe [list cellname list property $c] ; probe [list save $c] }
}
tech load input/pdk/ihp-sg13g2/libs.tech/magic/ihp-sg13g2.tech -noprompt
source input/pdk/ihp-sg13g2/libs.tech/magic/ihp-sg13g2.magicrc
random seed 1
set f [open input/pdk/ihp-sg13g2/libs.tech/magic/ihp-sg13g2.tcl]
set devices [regexp -all -inline {magic::gencell ([^\s"]+)} [read $f]]
close $f
set devices [dict values $devices]
set i 0
foreach dev $devices {
    load itop_[incr i]
    probe [list magic::gencell $dev X$i]
    probe [list cellname list children itop_$i]
    foreach c [cellname list children itop_$i] { probe [list cellname list property $c] ; probe [list save $c] }
}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
