# TECH-G45: Painted layout; tech load -quiet (usage) and -noprompt (the 'no' dialog answer is not authored).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 4 4} result; puts $result
catch {paint metal1} result; puts $result
catch {tech load input/tiny -quiet} result; puts $result
catch {tech load input/tiny -noprompt} result; puts $result
catch {tech name} result; puts $result
quit -noprompt
