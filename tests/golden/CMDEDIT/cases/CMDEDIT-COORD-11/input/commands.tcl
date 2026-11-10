# CMDEDIT-COORD-11: grid multiple 300; grid multiple; grid 0 5; grid 0; grid state (Q-CMDEDIT-7).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {grid multiple 300} result; puts $result
catch {grid multiple} result; puts $result
catch {grid 0 5} result; puts $result
catch {grid 0} result; puts $result
catch {grid state} result; puts $result
quit -noprompt
