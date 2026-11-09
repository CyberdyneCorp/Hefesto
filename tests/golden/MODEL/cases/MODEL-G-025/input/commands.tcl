# MODEL-G-025: tech locked metal2, then paint via1.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {tech locked metal2} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint via1} result; puts $result
catch {save a} result; puts $result
quit -noprompt
