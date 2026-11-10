# CMDEDIT-COORD-03: tech fine; box values 0 0 1um+2um 1um (Q-CMDEDIT-2).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/fine -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box values 0 0 1um+2um 1um} result; puts $result
catch {box values} result; puts $result
quit -noprompt
