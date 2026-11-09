# MODEL-G-034: 3-level hierarchy with labels and ports; flatten.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
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
catch {save top} result; puts $result
catch {load top} result; puts $result
catch {flatten flat} result; puts $result
catch {load flat} result; puts $result
catch {save flat} result; puts $result
quit -noprompt
