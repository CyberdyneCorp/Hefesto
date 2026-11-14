# MASK-GR-051: readonly with datestamp no, and readonly with datestamp 1700000000.
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
probe {gds readonly yes}
probe {cellname list allcells}
probe {gds datestamp no}
probe {gds read r1}
probe {cellname timestamp r1}
probe {gds datestamp 1700000000}
probe {gds read r2}
probe {cellname timestamp r2}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
