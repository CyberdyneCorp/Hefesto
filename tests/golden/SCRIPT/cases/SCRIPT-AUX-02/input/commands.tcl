# SCRIPT-AUX-02: mazeroute f.net with iroute stubbed to log its arguments
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
proc magic::iroute {args} { puts [list iroute {*}$args]; return {} }
probe {mazeroute input/f.net}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
