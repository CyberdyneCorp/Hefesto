# MISC-FL-01: label A FreeSans 10 0 0 0 center m1 at (0,0) on metal1; save (flabel line).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box -20 -20 20 20} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 0 0} result; puts $result
catch {label A FreeSans 10 0 0 0 center m1} result; puts $result
catch {save a} result; puts $result
quit -noprompt
