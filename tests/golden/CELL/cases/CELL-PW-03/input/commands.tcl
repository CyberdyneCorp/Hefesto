# CELL-PW-03: Child in the same directory; two instances.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load c} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint metal1} result; puts $result
catch {save c} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell c} result; puts $result
catch {box 10 0 10 0} result; puts $result
catch {getcell c} result; puts $result
catch {save top} result; puts $result
quit -noprompt
