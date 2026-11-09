# CELL-WR-03: magscale 1 2 cell: flabel with odd size and offset, element at odd coordinates, even paint; save.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/a} result; puts $result
catch {save a_out} result; puts $result
quit -noprompt
