# CELL-WR-11: save top.tcl of a cell with paint, a split tile, an instance, labels, ports and properties.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load leaf} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save leaf} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 20 4} result; puts $result
catch {paint metal1} result; puts $result
catch {box 30 0 40 10} result; puts $result
catch {splitpaint ne poly} result; puts $result
catch {box 50 0 50 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {box 2 2 2 2} result; puts $result
catch {label A} result; puts $result
catch {port make} result; puts $result
catch {property s v} result; puts $result
catch {save top.tcl} result; puts $result
quit -noprompt
