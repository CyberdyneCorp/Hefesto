# MISC-ANT-08: 150 distinct gate nets on 200 devices (the first 50 nets twice): the progress line after 100 distinct gate nets, once
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
probe {antennacheck}
probe {feedback count}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
