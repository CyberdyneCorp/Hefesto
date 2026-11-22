# NL-SPICE-03: capacitors of 0, 1e-19, 0.9999e-13, 1e-13, 1.0001e-10, 1e-7, 0.01, 999.9, 1000 and 1e9 F and MOSFET widths 1..20000 with scale off: the SI text
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice scale off}
probe {ext2spice top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
