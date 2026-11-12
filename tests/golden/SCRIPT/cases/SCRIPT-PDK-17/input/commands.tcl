# SCRIPT-PDK-17: each PDK: GUI start under Xvfb with the rc file; dump the File and DRC menu entries and the pdk1/pdk2 menu entries of .layout1
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
proc menus {} { foreach m {file drc pdk1 pdk2} { set mm .layout1.titlebar.mbuttons.$m.toolmenu; if {[catch {$mm index end} n] || $n eq {none}} { puts [list $m none]; continue }; for {set i 0} {$i <= $n} {incr i} { catch {puts [list $m $i [$mm entrycget $i -label]]} } } }
menus
tech load input/pdk/gf180mcuD/libs.tech/magic/gf180mcuD.tech -noprompt
source input/pdk/gf180mcuD/libs.tech/magic/gf180mcuD.magicrc
random seed 1
menus
tech load input/pdk/ihp-sg13g2/libs.tech/magic/ihp-sg13g2.tech -noprompt
source input/pdk/ihp-sg13g2/libs.tech/magic/ihp-sg13g2.magicrc
random seed 1
menus
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
