# MISC-MH-10: Two-level hierarchy with a hint at each level (leaf X inside mid, mid Y); flatten -doinplace the mid instance: Magic copies only Y, Hefesto also X (Q-MISC-14).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/t -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {drc off} result; puts $result
catch {load leaf} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_X 0 0 2 2} result; puts $result
catch {save leaf} result; puts $result
catch {load mid} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint m1} result; puts $result
catch {property MASKHINTS_Y 0 0 3 3} result; puts $result
catch {box 6 0 6 0} result; puts $result
catch {getcell leaf} result; puts $result
catch {save mid} result; puts $result
catch {load top} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint m1} result; puts $result
catch {box 20 0 20 0} result; puts $result
catch {getcell mid} result; puts $result
catch {save top} result; puts $result
catch {load top} result; puts $result
catch {flatten -doinplace mid_0} result; puts $result
catch {property MASKHINTS_X} result; puts $result
catch {property MASKHINTS_Y} result; puts $result
catch {save top} result; puts $result
quit -noprompt
