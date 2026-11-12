# SCRIPT-AUX-03: default start; info commands magic::add_menu; check for a listener on 12946; set batch_mode under -dnull -noconsole
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {info commands magic::add_menu}
probe {info exists batch_mode}
probe {set batch_mode}
probe {catch {socket localhost 12946} e}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
