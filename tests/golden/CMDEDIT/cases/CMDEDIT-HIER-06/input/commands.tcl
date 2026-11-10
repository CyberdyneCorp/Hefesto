# CMDEDIT-HIER-06: 3-level hierarchy; flatten f1, -nolabels f2, -dotoplabels f3, -dobox f4, f1 again, -doinplace mid_0 (Q-CMDEDIT-31).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {load leaf} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 5 5 5} result; puts $result
catch {label L} result; puts $result
catch {port make} result; puts $result
catch {save leaf} result; puts $result
catch {load mid} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {box 2 2 2 2} result; puts $result
catch {label M} result; puts $result
catch {save mid} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell mid} result; puts $result
catch {box 50 0 50 0} result; puts $result
catch {getcell mid} result; puts $result
catch {box 1 1 1 1} result; puts $result
catch {label T} result; puts $result
catch {flatten f1} result; puts $result
catch {flatten -nolabels f2} result; puts $result
catch {flatten -dotoplabels f3} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {flatten -dobox f4} result; puts $result
catch {flatten f1} result; puts $result
catch {flatten -doinplace mid_0} result; puts $result
catch {writeall force} result; puts $result
quit -noprompt
