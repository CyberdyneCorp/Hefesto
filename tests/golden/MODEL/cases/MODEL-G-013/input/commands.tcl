# MODEL-G-013: ndiff 0 0 10 10, pdiff 5 0 15 10.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint ndiff} result; puts $result
catch {box 5 0 15 10} result; puts $result
catch {paint pdiff} result; puts $result
catch {save a} result; puts $result
quit -noprompt
