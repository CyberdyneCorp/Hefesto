# MODEL-G-030: Arrays: 4x2 of a 10x10 child, negative x separation, rotated 90; load and save.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/arr} result; puts $result
catch {save arr} result; puts $result
quit -noprompt
