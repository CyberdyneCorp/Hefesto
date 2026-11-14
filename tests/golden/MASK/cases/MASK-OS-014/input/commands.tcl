# MASK-OS-014: bloat-all present or not, scalefactor 10 nanometers: cuts of an odd-sized contact.
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
probe {load top}
probe {box 0 0 7 5}
probe {paint pdc}
probe {cif ostyle ba}
probe {gds write ba}
probe {cif ostyle nb}
probe {gds write nb}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
