# SCRIPT-PCELL-01: pure Tcl: magic::normalize_value over {2, 2.0, 2e0, 0.1500, 1.23456, -0.5, 0.0004, 10.5, 0x10, " 3 ", abc, inf}
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {magic::normalize_value 2}
probe {magic::normalize_value 2.0}
probe {magic::normalize_value 2e0}
probe {magic::normalize_value 0.1500}
probe {magic::normalize_value 1.23456}
probe {magic::normalize_value -0.5}
probe {magic::normalize_value 0.0004}
probe {magic::normalize_value 10.5}
probe {magic::normalize_value 0x10}
probe {magic::normalize_value " 3 "}
probe {magic::normalize_value abc}
probe {magic::normalize_value inf}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
