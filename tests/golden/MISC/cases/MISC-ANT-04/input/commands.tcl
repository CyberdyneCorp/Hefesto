# MISC-ANT-04: units microns, height metal1 1.0 0.36 (thickness 3.6 units at 0.1 micron per unit), metal1 sidewall 10 none: an isolated 100x10 metal1 wire on the gate; the debug effective ratio
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
