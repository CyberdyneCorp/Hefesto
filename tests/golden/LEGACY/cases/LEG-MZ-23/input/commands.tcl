# LEG-MZ-23: iroute search, search rate 100, search width x, unknown parameter, too many arguments
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {iroute search}
probe {iroute search rate 100}
probe {iroute search rate}
probe {iroute search width 200}
probe {iroute search width x}
probe {iroute search rate -1}
probe {iroute search nosuch 1}
probe {iroute search rate 1 2}
probe {iroute search}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
