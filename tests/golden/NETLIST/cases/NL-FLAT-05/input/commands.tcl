# NL-FLAT-05: parameters nfet l=l w=w a1=as p1=ps x=xpos*0.5 l1=lt+2 and a device with verbatim l1=4 foo=1; scale on and off: the parameter text order and values
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice scale on}
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
