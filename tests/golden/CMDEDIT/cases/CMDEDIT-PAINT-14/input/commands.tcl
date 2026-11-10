# CMDEDIT-PAINT-14: metal1 over the box; spliterase sw metal1; spliterase ne.
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {paint metal1} result; puts $result
catch {spliterase sw metal1} result; puts $result
catch {save s1} result; puts $result
catch {spliterase ne} result; puts $result
catch {save s2} result; puts $result
quit -noprompt
