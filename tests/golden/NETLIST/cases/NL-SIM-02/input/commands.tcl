# NL-SIM-02: bjt, diode, devres, devcap and subckt devices: the device lines
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2sim top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
