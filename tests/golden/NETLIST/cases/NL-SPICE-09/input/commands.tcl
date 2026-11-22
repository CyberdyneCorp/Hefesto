# NL-SPICE-09: MOSFETs sharing diffusion, a source terminal attribute 12,14 and a gate attribute ext:aps; scale on and off: the + ad= lines, asub/psub and **devattr
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice top}
file rename -force top.spice top.scaleon.spice
probe {ext2spice scale off}
probe {ext2spice top}
file rename -force top.spice top.scaleoff.spice
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
