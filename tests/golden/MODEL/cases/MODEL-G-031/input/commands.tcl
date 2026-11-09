# MODEL-G-031: Children: paint + point label, label only, empty, flabels; parent use boxes (Q-MODEL-6, Q-MODEL-19).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load c1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 15 15 15 15} result; puts $result
catch {label p} result; puts $result
catch {save c1} result; puts $result
catch {load c2} result; puts $result
catch {box 3 4 3 4} result; puts $result
catch {label q} result; puts $result
catch {save c2} result; puts $result
catch {load c3} result; puts $result
catch {save c3} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell c1} result; puts $result
catch {box 100 0 100 0} result; puts $result
catch {getcell c2} result; puts $result
catch {box 200 0 200 0} result; puts $result
catch {getcell c3} result; puts $result
catch {box 300 0 300 0} result; puts $result
catch {getcell input/fl} result; puts $result
catch {save top} result; puts $result
quit -noprompt
