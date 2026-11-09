# MODEL-G-015: via1 0 0 10 10, then metal1 0 0 20 10.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint via1} result; puts $result
catch {box 0 0 20 10} result; puts $result
catch {paint metal1} result; puts $result
catch {save a} result; puts $result
quit -noprompt
