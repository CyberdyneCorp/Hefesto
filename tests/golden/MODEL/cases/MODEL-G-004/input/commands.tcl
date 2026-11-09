# MODEL-G-004: Three disjoint metal1 rectangles.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 20 0 30 30} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 20 10 30} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save a} result; puts $result
quit -noprompt
