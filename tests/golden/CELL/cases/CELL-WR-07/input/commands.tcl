# CELL-WR-07: Ports with use only, class only, all three attributes, use=reset; save.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 40 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 2 2 2 2} result; puts $result
catch {label u} result; puts $result
catch {port make} result; puts $result
catch {port use power} result; puts $result
catch {box 12 2 12 2} result; puts $result
catch {label c} result; puts $result
catch {port make} result; puts $result
catch {port class input} result; puts $result
catch {box 22 2 22 2} result; puts $result
catch {label all} result; puts $result
catch {port make} result; puts $result
catch {port use ground} result; puts $result
catch {port class output} result; puts $result
catch {port shape abutment} result; puts $result
catch {box 32 2 32 2} result; puts $result
catch {label r} result; puts $result
catch {port make} result; puts $result
catch {port use reset} result; puts $result
catch {save a} result; puts $result
quit -noprompt
