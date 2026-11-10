# CMDEDIT-BOX-04: box north 5 (Q-CMDEDIT-8).
# Reference script (Magic, Tcl): one result line per command.
catch {tech load input/tiny -noprompt} result; puts $result
catch {load a} result; puts $result
catch {box 0 0 10 10} result; puts $result
catch {box north 5} result; puts $result
catch {box values} result; puts $result
quit -noprompt
