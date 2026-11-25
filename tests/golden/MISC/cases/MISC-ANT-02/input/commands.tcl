# MISC-ANT-02: as MISC-ANT-01 with metal1 400 2200 and an ndiff tie-down (tiedown ndiff) on the plate through ndc: the diffusion branch, no violation
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
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
