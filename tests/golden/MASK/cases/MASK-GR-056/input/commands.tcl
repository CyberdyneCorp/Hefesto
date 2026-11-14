# MASK-GR-056: Option queries and prefix forms: gds noduplicates t, gds ord on, gds flatten.
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
probe {gds noduplicates}
probe {gds noduplicates t}
probe {gds noduplicates}
probe {gds ord on}
probe {gds ordering}
probe {gds flatten}
probe {gds flatten 5}
probe {gds flatten}
probe {gds rescale}
probe {gds readonly}
probe {gds drccheck}
probe {gds savepaths}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
