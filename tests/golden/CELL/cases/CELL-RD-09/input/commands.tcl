# CELL-RD-09: fake tech; section << m3c2 >> with one rect.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/fake -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/a} result; puts $result
catch {cellname list allcells} result; puts $result
catch {save a_out} result; puts $result
quit -noprompt
