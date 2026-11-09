# MODEL-G-041: Label on metal1 2 2 4 4, then via1 0 0 6 6.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 2 2 4 4} result; puts $result
catch {paint metal1} result; puts $result
catch {box 2 2 4 4} result; puts $result
catch {label L} result; puts $result
catch {box 0 0 6 6} result; puts $result
catch {paint via1} result; puts $result
catch {save a} result; puts $result
quit -noprompt
