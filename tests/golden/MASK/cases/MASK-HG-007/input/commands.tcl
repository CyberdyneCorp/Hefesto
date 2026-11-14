# MASK-HG-007: Child pdiff under parent metal3 (as nwell): PSD = pdiff and-not metal3, feedback and output.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/masktest -noprompt}
probe {snap internal}
probe {gds datestamp 0}
probe {load top}
probe {gds write out}
probe {feedback count}
probe {feedback save fb.txt}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
