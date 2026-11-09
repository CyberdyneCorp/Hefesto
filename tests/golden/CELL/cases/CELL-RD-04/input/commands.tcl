# CELL-RD-04: magscale 1 2 file in a fresh session, then a file without magscale holding rect 0 0 3 5.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/a} result; puts $result
catch {box} result; puts $result
catch {load input/b} result; puts $result
catch {box} result; puts $result
catch {save a_out} result; puts $result
catch {load a} result; puts $result
catch {save a2} result; puts $result
quit -noprompt
