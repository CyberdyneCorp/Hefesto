# SCRIPT-API-06: catch {box frobnicate}; catch {load nosuch -fail -silent}; catch {magic::shownet}
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {box frobnicate}
probe {load nosuch -fail -silent}
probe {magic::shownet}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
