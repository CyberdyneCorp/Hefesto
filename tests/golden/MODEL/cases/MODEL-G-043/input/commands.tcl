# MODEL-G-043: Ports b, A10, A2, VDD, vdd, VDD and non-port c; port renumber (Q-MODEL-9, Q-MODEL-10).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/mt -noprompt} result; puts $result
catch {snap internal} result; puts $result
catch {load a} result; puts $result
catch {box 1 1 1 1} result; puts $result
catch {label b} result; puts $result
catch {port make} result; puts $result
catch {box 3 1 3 1} result; puts $result
catch {label A10} result; puts $result
catch {port make} result; puts $result
catch {box 5 1 5 1} result; puts $result
catch {label A2} result; puts $result
catch {port make} result; puts $result
catch {box 7 1 7 1} result; puts $result
catch {label VDD} result; puts $result
catch {port make} result; puts $result
catch {box 9 1 9 1} result; puts $result
catch {label vdd} result; puts $result
catch {port make} result; puts $result
catch {box 11 1 11 1} result; puts $result
catch {label VDD} result; puts $result
catch {port make} result; puts $result
catch {box 20 1 20 1} result; puts $result
catch {label c} result; puts $result
catch {port renumber} result; puts $result
catch {save a} result; puts $result
quit -noprompt
