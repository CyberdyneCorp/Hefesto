# NL-FLAT-18: names with , = [ ] . # ! in the leaf and in the instance prefix; formats spice3, hspice and cdl, then ext2sim -t !#,=[
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
file rename -force top.spice top.spice3.spice
probe {ext2spice format hspice}
probe {ext2spice top}
file rename -force top.spice top.hspice.spice
probe {ext2spice format cdl}
probe {ext2spice top}
probe {ext2sim -t !#,=\[ top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
