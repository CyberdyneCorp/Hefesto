# SCRIPT-API-24: start without console; info commands unknown; foo with tcl_foo defined
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {info commands unknown}
probe {info commands tcl_unknown}
proc tcl_foo {args} {return [list tcl_foo {*}$args]}
probe {foo a b}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
