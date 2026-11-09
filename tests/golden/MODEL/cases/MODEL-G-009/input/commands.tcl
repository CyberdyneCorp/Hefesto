# MODEL-G-009: Child (metal1 0 0 10 20, label N) placed with getcell in 8 orientations; flattened.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 10 20} result; puts $result
catch {paint metal1} result; puts $result
catch {box 5 10 5 10} result; puts $result
catch {label N} result; puts $result
catch {save child} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {getcell child 0} result; puts $result
catch {box 40 0 40 0} result; puts $result
catch {getcell child 90} result; puts $result
catch {box 80 0 80 0} result; puts $result
catch {getcell child 180} result; puts $result
catch {box 120 0 120 0} result; puts $result
catch {getcell child 270} result; puts $result
catch {box 160 0 160 0} result; puts $result
catch {getcell child v} result; puts $result
catch {box 200 0 200 0} result; puts $result
catch {getcell child h} result; puts $result
catch {box 240 0 240 0} result; puts $result
catch {getcell child 90v} result; puts $result
catch {box 280 0 280 0} result; puts $result
catch {getcell child 90h} result; puts $result
catch {save top} result; puts $result
catch {load top} result; puts $result
catch {flatten flat} result; puts $result
catch {load flat} result; puts $result
catch {save flat} result; puts $result
quit -noprompt
