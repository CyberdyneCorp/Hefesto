# MISC-ANT-01: partial surface model, metal1 400 none: an nfet gate 2x10 through poly and pc to a 100x100 metal1 plate; antennacheck debug, antennacheck; debug lines and the feedback areas (gate, pc, plate) with their text
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/anttest -noprompt
drc off
load top
probe {antennacheck debug}
probe {antennacheck}
probe {feedback count}
probe {feedback find 1}
probe {box values}
probe {feedback find 2}
probe {box values}
probe {feedback find 3}
probe {box values}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
