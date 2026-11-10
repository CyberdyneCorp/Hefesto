# MISC-MH-07: Child with a hint placed with rotation 90; flatten into a new cell; flatten -doinplace the instance.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/t -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load child} result; puts $result
catch {box 0 0 10 4} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X 0 0 6 4} result; puts $result
catch {save child} result; puts $result
catch {load top} result; puts $result
catch {box 20 0 20 0} result; puts $result
catch {getcell child 90} result; puts $result
catch {save top} result; puts $result
catch {load top} result; puts $result
catch {flatten flat} result; puts $result
catch {load flat} result; puts $result
catch {save flat} result; puts $result
catch {property MASKHINTS_X} result; puts $result
catch {load top} result; puts $result
catch {flatten -doinplace child_0} result; puts $result
catch {property MASKHINTS_X} result; puts $result
catch {save top} result; puts $result
quit -noprompt
