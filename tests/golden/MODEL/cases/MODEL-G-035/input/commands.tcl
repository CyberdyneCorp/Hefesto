# MODEL-G-035: 2x2 array of a child with label out; flatten -doinplace inv_0 (Q-MODEL-7).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load input/top} result; puts $result
catch {flatten -doinplace inv_0} result; puts $result
catch {save top} result; puts $result
quit -noprompt
