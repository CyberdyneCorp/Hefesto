# SCRIPT-API-13: print text with $abc (undefined) and $VDD (defined) via echo/engine messages, e.g. cellname create a\$abc; cellname list allcells
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
set VDD vdd!
probe {cellname create a$abc}
probe {cellname create b$VDD}
probe {cellname list allcells}
probe {echo {x$abc y$VDD}}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
