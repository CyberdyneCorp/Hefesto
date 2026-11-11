# SCRIPT-GUI-13: after magic::enable_tools: tool, tool type, macro list per tool, tool info
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {magic::enable_tools}
probe {tool}
foreach t {box wiring netlist pick} { probe [list tool $t]; probe {tool type}; probe {llength [macro list]}; probe {tool info} }
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
