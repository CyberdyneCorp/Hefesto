# TECH-G19: ndiff painted over a 4x4 pc (Q-TECH-23).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint pc} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint ndiff} result; puts $result
catch {save a} result; puts $result
quit -noprompt
