# NL-HIER-01: 3-level hierarchy top/mid/inv; hierarchy on
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
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
