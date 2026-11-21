# NL-FLAT-16: legacy fet records with 2 and 3 terminals and ext:l= / ext:w= gate attributes, LL defined with -s; ext2sim
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2sim -s LL=3 top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
