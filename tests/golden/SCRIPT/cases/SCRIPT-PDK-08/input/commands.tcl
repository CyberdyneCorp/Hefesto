# SCRIPT-PDK-08: GUI sky130A: open two wrappers
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
openwrapper
foreach w [lsort [windownames layout]] {
    set f [winfo toplevel $w]
    foreach m [lsort [info commands $f.*mbuttons.*]] { catch {puts [list $m [$m cget -text]]} }
    foreach m {pdk1 pdk2} { catch {set mm $f.titlebar.mbuttons.$m.toolmenu; for {set i 0} {$i <= [$mm index end]} {incr i} { catch {puts [list $m $i [$mm entrycget $i -label]]} } } }
}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
