# LEFDEF-023: `lef datestamp`, `lef datestamp 12345`, read LEF, `lef datestamp default`, `lef datestamp abc`, `def datestamp 1`. Captures: printed messages; cell timestamps in .mag headers.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/lefdef.tech -noprompt}
probe {snap internal}
probe {lef datestamp}
probe {lef datestamp 12345}
probe {lef datestamp}
probe {lef read input/x.lef}
probe {lef datestamp default}
probe {lef datestamp}
probe {lef datestamp abc}
probe {lef datestamp}
probe {def datestamp 1}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
