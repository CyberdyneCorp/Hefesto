# SCRIPT-API-15: load lib/inv.mag from /w, cd /tmp, save inv
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load input/lib/inv
probe {cellname list filepath inv}
file mkdir out
cd out
probe {cellname list filepath inv}
probe {save inv}
probe {pwd}
cd ..
probe {file exists out/inv.mag}
probe {file exists input/lib/inv.mag}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
