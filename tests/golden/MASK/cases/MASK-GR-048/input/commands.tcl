# MASK-GR-048: A readonly read with PDK_ROOT a prefix of the file path.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/masktest -noprompt}
probe {snap internal}
probe {gds datestamp 0}
probe {gds readonly yes}
probe {cellname list allcells}
probe {gds read $env(PDK_ROOT)/lib/v.gds}
probe {load v}
probe {property}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
