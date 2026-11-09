# CELL-WR-04: metal1 A, B and a 4-rect staircase: record order.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 20 5 30 15} result; puts $result
catch {paint metal1} result; puts $result
catch {box 40 0 44 4} result; puts $result
catch {paint metal1} result; puts $result
catch {box 42 4 46 8} result; puts $result
catch {paint metal1} result; puts $result
catch {box 44 8 48 12} result; puts $result
catch {paint metal1} result; puts $result
catch {box 46 12 50 16} result; puts $result
catch {paint metal1} result; puts $result
catch {save a} result; puts $result
quit -noprompt
