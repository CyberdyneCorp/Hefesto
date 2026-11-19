# LEFDEF-304: `def write -hide 5`, `def write a b`, `def writeall`, `def nocheck`, `def`. Captures: messages, absence of files.
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
probe {load top}
probe {def write -hide 5}
probe {def write a b}
probe {def writeall}
probe {def nocheck}
probe {def}
probe {lsort [glob -nocomplain *.def]}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
