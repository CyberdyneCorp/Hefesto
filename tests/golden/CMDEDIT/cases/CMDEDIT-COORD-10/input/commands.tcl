# CMDEDIT-COORD-10: metal1 -3 0 3 4, box 0 0 3 5; scalegrid 1 2; save; scalegrid 2 1; save.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box -3 0 3 4} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 3 5} result; puts $result
catch {scalegrid 1 2} result; puts $result
catch {box values} result; puts $result
catch {save a1} result; puts $result
catch {scalegrid 2 1} result; puts $result
catch {box values} result; puts $result
catch {save a2} result; puts $result
quit -noprompt
