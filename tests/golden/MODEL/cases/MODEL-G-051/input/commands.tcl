# MODEL-G-051: metal1 squares touching at a corner; select net at 1 1.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 2 2} result; puts $result
catch {paint metal1} result; puts $result
catch {box 2 2 4 4} result; puts $result
catch {paint metal1} result; puts $result
catch {box 1 1 1 1} result; puts $result
catch {select net} result; puts $result
catch {what -list} result; puts $result
catch {select bbox} result; puts $result
quit -noprompt
