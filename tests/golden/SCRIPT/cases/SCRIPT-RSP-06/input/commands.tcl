# SCRIPT-RSP-06: readspice nofile.sp
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {readspice nofile.sp}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
