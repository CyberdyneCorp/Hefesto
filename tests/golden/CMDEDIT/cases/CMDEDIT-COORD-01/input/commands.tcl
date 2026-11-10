# CMDEDIT-COORD-01: tech fine; units internal; box values with {2 + 3 * 4}.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/fine -noprompt} result; puts $result
catch {load a} result; puts $result
catch {units internal} result; puts $result
catch {box values 0 0 {2 + 3 * 4} 1} result; puts $result
catch {box values} result; puts $result
quit -noprompt
