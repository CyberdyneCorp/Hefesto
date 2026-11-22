# NL-SPICE-08: hierarchical output with style substrates VSUBS!, VDD! and errsub; spice3, then cdl with SUB, VDD and GND set: the .global line
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice hierarchy on}
probe {ext2spice top}
set SUB VSUBS
set VDD vdd!
set GND gnd!
probe {ext2spice format cdl}
probe {ext2spice top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
