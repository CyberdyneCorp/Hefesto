# SCRIPT-GUI-12: set Opts(backupinterval) 1000; crashbackups start; wait 2.5 s; crashbackups stop; crashbackups disable; crashbackups resume; after info
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
box 0 0 10 10
paint m1
set Opts(backupinterval) 1000
probe {crashbackups start}
probe {llength [after info]}
after 2500 {set done 1}
vwait done
probe {crashbackups stop}
probe {llength [after info]}
probe {crashbackups disable}
probe {crashbackups resume}
probe {llength [after info]}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
