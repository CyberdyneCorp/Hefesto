# MASK-GR-023: TEXT with STRANS+MAG 0.5/0/20000+ANGLE 45.7, WIDTH after and without PRESENTATION; gds magscale.
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
probe {gds read in}
probe {cellname list allcells}
probe {gds magscale}
probe {gds magscale 2}
probe {gds read in2}
probe {gds magscale default}
probe {gds magscale}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
