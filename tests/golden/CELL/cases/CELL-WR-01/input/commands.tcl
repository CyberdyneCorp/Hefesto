# CELL-WR-01: scmos: poly, metal1, a triangle and two instances; save.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/scmos -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load leaf} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save leaf} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 4 20} result; puts $result
catch {paint poly} result; puts $result
catch {box 10 0 30 6} result; puts $result
catch {paint metal1} result; puts $result
catch {box 40 0 50 10} result; puts $result
catch {splitpaint nw metal1} result; puts $result
catch {box 100 0 100 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {box 200 0 200 0} result; puts $result
catch {getcell leaf 90} result; puts $result
catch {save top} result; puts $result
quit -noprompt
