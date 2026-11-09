# CELL-RD-05: magscale 1 2 under a cifoutput gridlimit that forbids refinement; odd and negative coordinates.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tinyg -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/a} result; puts $result
catch {cellname list allcells} result; puts $result
catch {save a_out} result; puts $result
quit -noprompt
