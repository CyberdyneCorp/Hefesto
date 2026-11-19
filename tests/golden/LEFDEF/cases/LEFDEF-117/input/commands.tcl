# LEFDEF-117: Tcl `set VDD vccd1; set GND vssd1`, pins with default use. Captures: USE lines, no antenna.
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
probe {load pwr}
probe {set VDD vccd1}
probe {set GND vssd1}
probe {lef write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
