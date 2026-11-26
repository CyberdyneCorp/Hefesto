# LEG-MZ-21: iroute help, help route, version, verbosity, verbosity 0 and 2, an unknown and an ambiguous subcommand
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {iroute help}
probe {iroute help route}
probe {iroute version}
probe {iroute verbosity}
probe {iroute verbosity 0}
probe {iroute verbosity}
probe {iroute verbosity 2}
probe {iroute verbosity}
probe {iroute verbosity x}
probe {iroute verbosity 1 2}
probe {iroute foo}
probe {iroute s}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
