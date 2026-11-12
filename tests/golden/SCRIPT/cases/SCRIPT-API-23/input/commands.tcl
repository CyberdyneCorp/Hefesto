# SCRIPT-API-23: *bypass box values then select cycling check; read the command counter through a log file
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
box 0 0 10 10
set n0 [macro list]
probe {*bypass box values}
select top cell
probe {*bypass select}
probe {select}
set f [open cmdlog.txt w]
close $f
probe {history info}
probe {history}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
