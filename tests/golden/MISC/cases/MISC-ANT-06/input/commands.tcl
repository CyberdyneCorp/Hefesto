# MISC-ANT-06: a 10x10 via1 with squares 10 20 20 at scalefactor 10 (Q-MISC-4, compat: 4 units of cut area), via1 surface 0.1 and metal1 surface 400: the contact sum's debug lines
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
probe {feedback find 4}
probe {box values}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
