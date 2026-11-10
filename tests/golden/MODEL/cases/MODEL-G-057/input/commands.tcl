# MODEL-G-057: Load a cell, undo; paint metal2, flatten -doinplace, undo (Q-MODEL-28).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/c} result; puts $result
catch {undo} result; puts $result
catch {save c1} result; puts $result
catch {load leaf} result; puts $result
catch {box 0 0 5 5} result; puts $result
catch {paint metal1} result; puts $result
catch {save leaf} result; puts $result
catch {load top} result; puts $result
catch {box 100 0 100 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint metal2} result; puts $result
catch {flatten -doinplace leaf_0} result; puts $result
catch {undo} result; puts $result
catch {save top} result; puts $result
quit -noprompt
