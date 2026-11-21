# NL-SIM-05: ext2sim extresist on with inv.res.ext: the output file names (inv.ext.sim) and contents
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2sim extresist on}
probe {ext2sim inv}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
