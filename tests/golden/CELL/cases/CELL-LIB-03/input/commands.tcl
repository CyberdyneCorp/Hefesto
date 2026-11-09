# CELL-LIB-03: Technology mismatch with paint loaded (tiny vs a scmos cell); load -force.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint metal1} result; puts $result
catch {load input/s} result; puts $result
catch {load input/s -force} result; puts $result
catch {cellname list allcells} result; puts $result
quit -noprompt
