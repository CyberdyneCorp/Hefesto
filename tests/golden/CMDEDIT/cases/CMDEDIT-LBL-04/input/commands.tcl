# CMDEDIT-LBL-04: Ports 1 and 4; port make on C; port make 4 on D; class, connections, renumber, first/last/next (Q-CMDEDIT-92).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 40 10} result; puts $result
catch {paint metal1} result; puts $result
catch {box 1 1 1 1} result; puts $result
catch {label A} result; puts $result
catch {port make 1} result; puts $result
catch {box 11 1 11 1} result; puts $result
catch {label B} result; puts $result
catch {port make 4} result; puts $result
catch {box 21 1 21 1} result; puts $result
catch {label C} result; puts $result
catch {port make} result; puts $result
catch {box 31 1 31 1} result; puts $result
catch {label D} result; puts $result
catch {port make 4} result; puts $result
catch {port A class input} result; puts $result
catch {port A class} result; puts $result
catch {box 1 1 1 1} result; puts $result
catch {select area labels} result; puts $result
catch {port connections} result; puts $result
catch {port renumber} result; puts $result
catch {port first} result; puts $result
catch {port last} result; puts $result
catch {port next 1} result; puts $result
catch {save a} result; puts $result
quit -noprompt
