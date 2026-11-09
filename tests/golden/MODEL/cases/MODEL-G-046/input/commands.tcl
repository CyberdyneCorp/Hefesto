# MODEL-G-046: FIXED_BBOX, integer n, compat false; save; scalegrid 1 2; save.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {property FIXED_BBOX 0 0 100 200} result; puts $result
catch {property integer n 7} result; puts $result
catch {save a1} result; puts $result
catch {property compat false} result; puts $result
catch {save a2} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {save a3} result; puts $result
quit -noprompt
