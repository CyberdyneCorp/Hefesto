# CMDEDIT-MISC-05: random seed 42; random; random seed 42; random.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {random seed 42} result; puts $result
catch {random} result; puts $result
catch {random seed 42} result; puts $result
catch {random} result; puts $result
quit -noprompt
