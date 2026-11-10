# CMDEDIT-COORD-12: fill t, fill no, fill ne.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 -5 4 0} result; puts $result
catch {paint metal1} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {fill t} result; puts $result
catch {fill no} result; puts $result
catch {fill ne} result; puts $result
quit -noprompt
