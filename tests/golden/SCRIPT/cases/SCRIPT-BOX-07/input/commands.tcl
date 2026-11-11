# SCRIPT-BOX-07: genkey poly; genkey m1 6
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load genkey
box 0 0 0 0
probe {genkey poly}
probe {box 0 -100 0 -100}
probe {genkey m1 6}
probe {what -list}
box -500 -500 500 500
foreach e [lsort [element inbox]] { probe [list element configure $e] }
save genkey
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
