# SCRIPT-BOX-05: units internal, box (0,0)-(100,20): ruler hello; box (0,0)-(20,100): ruler; element inbox and the configuration of each element
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
snap internal
box 0 0 100 20
probe {ruler hello}
box 0 0 20 100
probe {ruler}
box 0 0 200 200
foreach e [lsort [element inbox]] { probe [list element configure $e] }
probe {element inbox}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
