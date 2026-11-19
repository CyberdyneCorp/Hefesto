# OASIS-059: The reference cell under a units-angstroms style (unit 10000), with layernames no, and in a style whose metal2 layer has no calma statement (skipped).
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
proc rep {script} {
    set code [catch {uplevel #0 $script} result]
    set f [open report.txt a]
    puts $f [list $code $result]
    close $f
}
proc specrep {value} {
    set f [open report.txt a]
    puts $f [list 0 $value]
    close $f
}
probe {drc off}
probe {tech load input/oastest -noprompt}
probe {snap internal}
probe {load top}
probe {box 0 0 4 2}
probe {paint metal1}
probe {box 10 10 12 12}
probe {paint metal2}
probe {oasis compress 0}
probe {cif ostyle ang}
probe {oasis write ang.oas}
probe {cif ostyle gds}
probe {oasis layernames no}
probe {oasis write nolayers.oas}
probe {oasis layernames yes}
probe {cif ostyle noc}
probe {oasis write nocalma.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
