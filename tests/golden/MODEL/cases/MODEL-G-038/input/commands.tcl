# MODEL-G-038: Point labels at 2 2, 15 15, 28 28, 2 28 in a 30x30 cell, no justification.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 30 30} result; puts $result
catch {paint metal1} result; puts $result
catch {box 2 2 2 2} result; puts $result
catch {label a} result; puts $result
catch {box 15 15 15 15} result; puts $result
catch {label b} result; puts $result
catch {box 28 28 28 28} result; puts $result
catch {label c} result; puts $result
catch {box 2 28 2 28} result; puts $result
catch {label d} result; puts $result
catch {save a} result; puts $result
quit -noprompt
