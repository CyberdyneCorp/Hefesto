# SCRIPT-PDK-10: sky130A rc file copied with tech load pointing to a missing file
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
set f [open input/pdk/sky130A/libs.tech/magic/sky130A.magicrc]
set rc [string map {sky130A.tech missing.tech} [read $f]]
close $f
set f [open missing.magicrc w]
puts -nonewline $f $rc
close $f
proc ::sourced? {} { puts [list device-script [llength [info procs ::sky130::*]]] }
probe {tech name}
puts before-rc
flush stdout
source missing.magicrc
puts after-rc
sourced?
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
