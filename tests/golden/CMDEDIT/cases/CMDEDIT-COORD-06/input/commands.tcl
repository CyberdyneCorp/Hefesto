# CMDEDIT-COORD-06: grid 10 20 5 5; box values in grid units; grid what.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {grid 10 20 5 5} result; puts $result
catch {box values 2g 2g 3g 3g} result; puts $result
catch {box values} result; puts $result
catch {grid what} result; puts $result
quit -noprompt
