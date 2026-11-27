# LEG-MZ-02: a technology whose mzrouter section has a layer line before `style`, two route layers on one plane, a malformed contact line and a non-positive cost; iroute layers, iroute contacts
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/mzbad -noprompt
probe {iroute layers}
probe {iroute contacts}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
